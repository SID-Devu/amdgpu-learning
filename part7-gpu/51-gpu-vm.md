# Chapter 51 — VM, page tables, and unified memory

GPUVM is amdgpu's most intricate piece. Per-process page tables mapping user GPU virtual addresses to physical pages (VRAM, GTT, host pinned). This is the Linux MMU, but for GPUs.

## 51.1 Why per-process VM

Without per-process VM:

- Two processes' GPU buffers would share a single address space; collisions possible; no isolation.

With per-process VM (since Vega/Navi):

- Each process has its own GPU virtual memory.
- The hardware MMU enforces.
- Processes can use the same GPU VAs without conflict.
- A bug in one process can't read another's memory.

Implementation: each process gets a **VMID** (a small integer). The GPU's MMU has a table indexed by VMID giving the page-table root. Submissions tag the VMID; the engine uses that VMID's page tables.

VMIDs are scarce (16 GFX VMIDs typical). The driver multiplexes (VMID 0 reserved for kernel, others rotated among contexts via flush + re-bind).

## 51.2 The structures

```c
struct amdgpu_vm {
    struct rb_root_cached   va;          /* allocated ranges */
    struct list_head        evicted;     /* BOs to re-bind */
    struct list_head        moved;
    struct list_head        relocated;
    struct list_head        idle;
    struct list_head        invalidated;
    struct list_head        freed;
    /* page directory tree */
    struct amdgpu_vm_pt     root;
    struct dma_fence       *last_update;
    /* … */
};

struct amdgpu_vm_pt {
    struct amdgpu_vm_bo_base    base;
    struct amdgpu_vm_pt        *entries;
    /* … */
};
```

The PT tree is recursively defined, mirroring the hardware page-table format (4 levels for 48-bit GPUVM, etc.).

Each `amdgpu_bo_va` is a per-mapping struct: which BO maps where in this VM, with what flags.

```c
struct amdgpu_bo_va {
    struct amdgpu_vm_bo_base    base;
    /* … */
    struct list_head            valids;
    struct list_head            invalids;
};
```

## 51.3 Mapping a BO

`DRM_IOCTL_AMDGPU_GEM_VA` with `op=MAP`:

1. Allocate a `bo_va` mapping range in the VM's `va` rb-tree.
2. Append to `bo->valids`/`invalids`.
3. Schedule a page-table update — actually writing PTEs is deferred.

`amdgpu_vm_bo_update()` performs PT updates, either via SDMA copy job (default — fast, async) or CPU writes (`amdgpu_vm_use_cpu_for_update`). PT updates produce a fence; subsequent CSes wait on it.

PT updates are batched per-VM: the kernel coalesces many incremental mappings into one SDMA submission.

## 51.4 The PDE / PTE format

Generation-specific. RDNA 3 uses 64-bit entries with bits for: present, valid, system/local, MTYPE (cache type), R/W, fragment size, large-page, address. Look in `include/asm-generic/sizes.h`-style headers in `drivers/gpu/drm/amd/include/`.

## 51.5 Eviction and re-binding

When TTM evicts a BO from VRAM to GTT (or system), its PTEs become invalid (the GPU physical pages have moved). The driver must:

1. Mark the BO's mappings as invalid.
2. Add the BO to `vm->evicted`.
3. On the next CS for this VM, walk `evicted` and re-update PTEs to point at the new physical pages.

This is the *most* concurrency-fraught path. Reading `amdgpu_vm.c::amdgpu_vm_handle_moved` is a rite of passage.

## 51.6 Userptr BOs and MMU notifiers

Userptr maps user pages into GPUVM. If the user munmaps or mremaps, those pages may go away. The kernel registers a **MMU interval notifier**:

```c
struct mmu_interval_notifier {
    /* registered on user's mm covering [start, end) */
    bool (*invalidate)(struct mmu_interval_notifier *,
                       const struct mmu_notifier_range *,
                       unsigned long cur_seq);
};
```

When the user changes mappings, the kernel calls `invalidate`. amdgpu must:

1. Stall any in-flight GPU work using the BO.
2. Unmap from GPUVM.
3. Possibly retry with new pages.

Code: `amdgpu_mn.c`. Treacherous because it can be called from any context, with various locks held by the MM core.

## 51.7 Shared Virtual Memory (SVM)

In ROCm, you can let the GPU access *any* address in the user's process address space. The GPU faults on missed pages; the kernel resolves via the same path the CPU's page-fault handler uses; pages get pinned and mapped on demand.

This needs:

- IOMMU SVA (Shared Virtual Addressing) — the IOMMU walks the CPU's page tables.
- KFD's SVM API for finer control.

Code: `drivers/gpu/drm/amd/amdkfd/kfd_svm.c`. Currently the most rapidly evolving part of the AMD GPU driver.

## 51.8 The GMC

GMC (GPU Memory Controller) is the IP block that owns:

- VRAM aperture, its size, its base.
- GTT aperture (system memory window).
- VMID flushing.
- The on-chip page-walk hardware.

`gmc_v*.c` per-generation files. Setting up GMC is one of `hw_init`'s steps for the IP.

## 51.9 Page faults

When the GPU dereferences an unmapped GPUVA, it raises a page fault. Two flavors:

- **Recoverable**: the kernel resolves (e.g. SVM), tells the GPU to retry.
- **Fatal**: the kernel tags the fault and signals an error. Affected job is canceled; reset may follow.

Page faults are delivered through the IH ring as a special cookie. `amdgpu_irq.c` and `gmc_v*.c::process_interrupt` handle them.

If you ever see "VM_L2 RDREQ" in dmesg, that's the GPU reporting a VM page fault.

## 51.10 Concurrency

GPUVM updates can happen from:

- CS ioctl threads (mapping new BOs).
- TTM eviction (re-mapping moved BOs).
- KFD context (process address-space changes).
- Reset path (rebuilding everything).

All of these synchronize through:

- `amdgpu_vm.lock` (`mutex` + `dma_resv` for the root).
- ww_mutex on each BO's resv.
- Per-VM update fences.

Reading `amdgpu_vm.c` is reading distributed-systems-grade synchronization.

## 51.11 Summary mental model

- Per-process GPU page tables, like CPU page tables, but updated by SDMA.
- TTM moves BOs; VM keeps PTEs current.
- Userptr + MMU notifiers tie GPUVM to CPU mm.
- VMID is a scarce hardware resource; flushed and recycled.
- Faults arrive via IH ring; recoverable or fatal.

## 51.12 Exercises

1. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c::amdgpu_vm_init`. Watch how the root PT is allocated.
2. Read `amdgpu_vm.c::amdgpu_vm_bo_update`. Identify the PT walking and PTE write path.
3. Read `amdgpu_mn.c::amdgpu_mn_invalidate_hsa`. Understand what stalls the GPU on userspace munmap.
4. Read `drivers/gpu/drm/amd/amdkfd/kfd_svm.c::svm_range_set_attr`. Skim; this is hard code.
5. Trace `amdgpu_vm_update_ptes` with bpftrace under a workload.

---

Next: [Chapter 52 — Userspace: libdrm, Mesa, ROCm, KFD](52-userspace.md).
