# Chapter 84 — VM faults, page tables, recovery

GPU memory is **virtualized**: every shader access goes through GPU page tables. When the address isn't mapped, you get a **page fault** — and unlike CPU page faults, GPU page faults can be brutal. The way amdgpu handles them is a recurring source of complex bugs and a key area for senior engineers to own.

## GPU page tables (recap from chapter 51)

```
GPU virtual address (48-bit typical)
        │
        v
   PD0 (top-level) ──┐
        │            │
        v            │
   PD1 (mid-level)   │
        │            │ walked by hardware
        v            │
   PT (page table)   │
        │            │
        v            │
   page (4 KiB / 2 MiB / 1 GiB)
        │            │
        v            v
  GPU PA → VRAM, or → IOMMU → host PA
```

Each PTE is 64 bits, encoding:

- Physical address.
- Cache policy (MTYPE bits — chapter 68).
- R/W/X permissions.
- Valid bit.
- TLB-tag info.
- Per-VMID owner.

Page tables are stored in VRAM, edited via SDMA writes, and flushed via TLB-flush PM4 packets.

## What "VM fault" means

A wave issues an address whose PTE has the **valid bit clear**, or fails permission checks. The hardware:

1. Aborts the load/store.
2. Records fault info (faulting VMID, address, access type, source ring, source CU/wave) in a fault log.
3. Raises an interrupt to the IH (Interrupt Handler) ring.

The kernel reads the IH ring, parses the fault payload, and decides what to do.

## Two flavors

### Fatal page faults

The default. The wave doesn't continue; the entire submission is in a bad state. Kernel:

1. Logs the fault (`dmesg`).
2. Marks the originating context as guilty.
3. Triggers a reset of the affected ring (and possibly broader).
4. The userspace process gets `VK_ERROR_DEVICE_LOST` / `hipDeviceUnreachable` / etc. on its next API call.

This is the classic "dmesg flood" people see when a game has a barrier bug.

### Recoverable page faults

CDNA + recent RDNA support **GPU page-fault recovery**: the wave retries the access after the kernel has fixed the mapping. Used for:

- **SVM** lazy migration: HMM brings the page in.
- **Userptr** invalidation re-bind.
- **HMM-MMu-notifier**-driven migration.

Implementation: hardware stalls the wave; kernel handles fault (allocates a page, updates PTE, flushes TLB); kernel signals "retry"; wave continues.

This requires:

- HW that supports retry semantics (gfx9, some gfx10+ for compute).
- Kernel page-fault handler wired up.
- The page tables updated *atomically* before resume — TLB shootdown must complete.

## The page-fault path in `amdgpu`

```
IH ring: fault entry written by GMC/IH
        │
        v
amdgpu_irq_dispatch()
        │
        v
gmc_v*.process_interrupt()
        │
        v
amdgpu_vm_handle_fault() / svm_range_restore_pages() / etc.
        │
        v
Either:
  - Resolve (find missing page, update PTE, retry)
  - Escalate (reset)
```

Multiple paths converge here. SVM has its own (`kfd_svm.c::svm_fault_handler`). DRM-context faults have another (`amdgpu_vm_fault_*`).

## Why recovery is hard

- The fault must be classified accurately. Was this an SVM page (kernel can resolve) or a wild pointer (must reset)?
- Multi-thread races: the same page might be faulted by many waves simultaneously.
- TLB shootdown must complete before resume — wrong ordering and the wave reads stale.
- ECC can corrupt PTEs themselves; fault may be due to bad memory.
- Per-VMID isolation must be respected.

A single bug in the fault path can corrupt other contexts' memory. **Reviewing patches here is non-negotiable.**

## TLB management

Each VMID slot has its own TLB. Flushing options:

- **Per-VMID TLB flush**: targeted, cheaper.
- **Global TLB flush**: nuclear, used after major re-mappings.
- **Specific-page TLB invalidation**: for SVM with retry support.

PM4 packet `EVENT_WRITE` with appropriate flags issues a TLB flush. The flush latency is in the hundreds of nanoseconds; do it sparingly.

## Eviction and re-binding

When VRAM gets full, TTM evicts BOs to GTT. The BO's PTEs in every VM that mapped it must be *invalidated* before the eviction completes; new mappings emerge after the next bind. This dance requires:

- Reservation lock (`ww_mutex`) on every VM holding the BO.
- Walk the bo_va list.
- Submit SDMA writes to update PTEs.
- Wait for completion.
- Free the original VRAM.

A senior engineer reviews eviction-path patches very carefully — race conditions here can leak memory or corrupt other workloads.

## Userptr and MMU notifiers

A "userptr BO" wraps a user-space malloc-ed buffer. The kernel pins the pages and maps them to GPU VA. But the user might `munmap()` or fork() those pages; the kernel registers an **MMU notifier** that fires on such events. The notifier:

- Removes the pages from GPU mappings.
- Marks the BO "evicted."
- Next GPU access faults; handler re-pins and re-maps.

This is critical for ROCm's "managed memory" pattern. Bugs here = silent memory corruption in user processes.

## SVM (Shared Virtual Memory) extras

SVM mappings have richer metadata:

- **Migration policies**: prefer-VRAM, always-system, etc.
- **Per-page access counters**: GPU logs which pages it touches; kernel uses this to migrate hot pages to VRAM.
- **SVM ranges**: contiguous virtual ranges with a single policy.

`kfd_svm.c` has this logic. Cross-NUMA scheduling becomes interesting on multi-die GPUs (MI200/MI300).

## Reset-and-recovery interactions (continued from chapter 53)

A GPU reset:

1. Quiesces all in-flight work.
2. Drops every in-flight fence with `EIO`.
3. Re-runs IP block init (per `amd_ip_funcs`).
4. Re-binds all BOs that survived the reset.
5. Resumes user processes — but their previously-pending work is gone (`VK_ERROR_DEVICE_LOST`).

For VM faults specifically, the reset path takes the guilty context and (if `amdgpu_gpu_recovery=1`) tries to keep other contexts alive. SR-IOV adds complexity (per-VF reset).

## Watching for faults in production

```bash
# Persistent log target:
journalctl -k -f | grep -i 'amdgpu.*fault'

# debugfs detail:
cat /sys/kernel/debug/dri/0/amdgpu_gpuvm_info
cat /sys/kernel/debug/dri/0/amdgpu_evict_vram
```

Or use `umr -O ihn -s gfx_0.0.0` to dump the IH ring directly.

## Common fault signatures

| dmesg pattern | Cause |
|---|---|
| "GFX VMID 1 PASID 0x... ... TC0 ... HUB CLIENT GMC ..." | wild pointer in shader |
| "process X has VM_FAULT at 0x..." | userspace bug, often barrier-related |
| "amdgpu: ... gmc invalidate during ..." | TLB flush path issue |
| Many faults on same address | SVM not migrating, or repeated wild access |

## What you should be able to do after this chapter

- Distinguish fatal from recoverable faults.
- Walk through the IH → handler → resolve/reset chain.
- Define VMID, TLB flush, MMU notifier in one sentence each.
- Read a fault dmesg line and identify the faulting client.
- Reason about eviction-path locking.

## Read deeper

- `drivers/gpu/drm/amd/amdgpu/amdgpu_vm*.c`.
- `drivers/gpu/drm/amd/amdgpu/gmc_v11_0.c` (or relevant gen).
- `drivers/gpu/drm/amd/amdkfd/kfd_svm.c`.
- Linux HMM docs (`Documentation/mm/hmm.rst`).
- AMD blog: "GPU Page Faults and Recovery."
