# Chapter 45 — GEM — buffer objects

A GPU is, more than anything, a buffer-eating machine. Vertex buffers, textures, framebuffers, command buffers, ring buffers, descriptor sets, intermediate render targets — every GPU operation reads or writes a buffer. **GEM** (Graphics Execution Manager) is DRM's framework for naming, owning, sharing, and mapping those buffers.

## 45.1 What a GEM object is

A `struct drm_gem_object` is a kernel object representing one buffer:

```c
struct drm_gem_object {
    struct kref         refcount;
    unsigned int        handle_count;
    struct drm_device  *dev;
    struct file        *filp;
    struct kobject     *kobj;

    struct dma_resv    *resv;          /* fences attached to this buffer */
    struct dma_resv     _resv;
    size_t              size;
    int                 name;          /* legacy global handle */
    /* … */
};
```

Driver embeds it inside the driver's BO struct:

```c
struct amdgpu_bo {
    struct ttm_buffer_object  tbo;     /* TTM embeds drm_gem_object */
    /* … placement, mappings, metadata … */
};
```

Hierarchy: `drm_gem_object` < `ttm_buffer_object` < `amdgpu_bo`.

## 45.2 Handles vs. objects

Userspace doesn't see a kernel pointer — it sees a small **handle** (uint32). Handles are allocated per `drm_file`; they are NOT shared across processes.

To pass a buffer to another process: convert handle → fd via PRIME (`DRM_IOCTL_PRIME_HANDLE_TO_FD`), pass the fd over a Unix socket, then the receiver does `DRM_IOCTL_PRIME_FD_TO_HANDLE` to get a handle valid in *its* `drm_file`.

## 45.3 The lifecycle

1. `DRM_IOCTL_AMDGPU_GEM_CREATE` (or generic `DUMB_CREATE`):
   - Allocate a BO of the requested size and placement.
   - Allocate a handle in the file's idr.
   - Return the handle.
2. `DRM_IOCTL_GEM_CLOSE`:
   - Drop the handle. If last handle, drop the kref. Last kref triggers `gem_free_object`.
3. **Mmap path**:
   - `DRM_IOCTL_AMDGPU_GEM_MMAP` returns a fake offset.
   - User does `mmap(fd, …, offset)`; kernel resolves offset → BO and installs `vm_ops->fault`.
4. **VA bind path**:
   - `DRM_IOCTL_AMDGPU_GEM_VA` maps the BO into the process's GPUVM at a given GPU VA.
5. **Submit path**:
   - User builds an IB referencing GPU VAs.
   - `DRM_IOCTL_AMDGPU_CS` validates and submits.

## 45.4 amdgpu BO placement

Each BO has a **placement**: VRAM, GTT, CPU, or hybrid. Common flags:

```c
AMDGPU_GEM_DOMAIN_CPU        /* system memory, CPU access primary */
AMDGPU_GEM_DOMAIN_GTT        /* system memory mapped via IOMMU */
AMDGPU_GEM_DOMAIN_VRAM       /* GPU local */
AMDGPU_GEM_DOMAIN_DOORBELL   /* small doorbell pages */
AMDGPU_GEM_DOMAIN_OA         /* off-chip APU memory */
```

A BO can have *preferred* and *allowed* domains. TTM (next chapter) chooses where to actually place it, may evict to make room, may keep it pinned.

## 45.5 Reserving and fences

A BO can be in flight (GPU is reading/writing) when userspace tries to use it. Synchronization uses **`dma_resv`**:

```c
struct dma_resv {
    struct dma_fence       *fence_excl;   /* exclusive (writer) */
    struct dma_resv_list   *fence;        /* shared (readers)   */
    struct ww_mutex         lock;
};
```

When a job starts, it adds its fence to the BOs it touches (write → exclusive, read → shared). When another job submits, it waits for the existing fences before issuing its work.

This is the foundation for cross-process synchronization without explicit semaphores.

## 45.6 The mmap path in depth

User:

```c
struct drm_amdgpu_gem_mmap mmap_req = { .handle = handle };
ioctl(fd, DRM_IOCTL_AMDGPU_GEM_MMAP, &mmap_req);
void *cpu = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mmap_req.addr_ptr);
```

Kernel:

1. `amdgpu_gem_mmap_ioctl` returns a fake offset that encodes the GEM object.
2. `drm_gem_mmap` (in `file_operations.mmap`) looks up the offset in the device's `vma_manager`, finds the GEM object, calls `drv->vm_ops->fault` on access.
3. `amdgpu_gem_fault` resolves where the page lives (VRAM / GTT / migrated) and installs the PTE.

For a write-combined CPU mapping into VRAM, the PTE has WC attribute. For GTT, it can be cached. The driver must pick correctly.

## 45.7 The VA bind path (GPU-side mapping)

A BO must be mapped into the GPU's address space before the GPU can dereference it.

```c
struct drm_amdgpu_gem_va va = {
    .handle    = handle,
    .operation = AMDGPU_VA_OP_MAP,
    .flags     = AMDGPU_VM_PAGE_READABLE | AMDGPU_VM_PAGE_WRITEABLE,
    .va_address= 0x100000000ULL,
    .offset_in_bo = 0,
    .map_size  = size,
};
ioctl(fd, DRM_IOCTL_AMDGPU_GEM_VA, &va);
```

Kernel updates the per-process GPUVM page tables to map `va_address` → BO's pages. We discuss in Chapter 51.

A BO can have multiple VA mappings (in different processes, different addresses). The driver maintains a list of `amdgpu_bo_va_mapping`.

## 45.8 dma-buf export/import

Export:

```c
struct drm_prime_handle p = { .handle = handle, .flags = O_RDWR };
ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &p);
/* p.fd is a dma-buf fd. */
```

Import (other process or other driver):

```c
struct drm_prime_handle p = { .fd = received_fd };
ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &p);
/* p.handle is a new GEM handle in this drm_file. */
```

The dma-buf is what's sent over Wayland / VAAPI / V4L2 / Vulkan external memory.

amdgpu's `gem_prime_import` and `gem_prime_export` drive this.

## 45.9 BO list

A submission may touch dozens of BOs. The kernel needs to validate (lock, get into fast memory, attach fences) every BO before running the work. Userspace prepares a **BO list** ahead of time:

```c
struct drm_amdgpu_bo_list_in {
    uint32_t operation;
    uint32_t list_handle;
    uint32_t bo_number;
    uint32_t bo_info_size;
    uint64_t bo_info_ptr;        /* user pointer to entries */
};
```

Each entry is `(handle, priority)`. The kernel hashes them, sorts by priority for eviction decisions.

Modern amdgpu has the **inline BO list** (passed in the CS ioctl directly) as well — avoids the round-trip to create a BO list handle.

## 45.10 Userptr BOs

A "userptr" BO is a BO that wraps existing user pages. The kernel uses `get_user_pages` to pin them and maps them into GPUVM.

Used by: HIP `hipHostMalloc`, OpenGL EXT_buffer_storage, scientific codes.

The kernel must register an MMU notifier (`mmu_interval_notifier`): if the user munmaps or mremaps, the kernel must unmap from GPUVM and possibly stall in-flight work.

`drivers/gpu/drm/amd/amdgpu/amdgpu_mn.c` implements this. Beautiful, treacherous code.

## 45.11 Reserved and pinned BOs

Some BOs cannot move:

- The page tables themselves.
- Doorbells.
- The CSA (Compute Shader Associations) buffer.
- BOs being scanned out (display) — must stay in VRAM.

Kernel pins via `amdgpu_bo_pin()`. Counts; unpinning lets TTM reclaim.

## 45.12 GEM helpers and "shmem" path

Lighter drivers (vkms, simpledrm, panfrost) use `drm_gem_shmem_helper.c` for buffer management — system-memory only, no migration. Faster to implement.

amdgpu's needs (VRAM/GTT/eviction) require TTM, which is heavier but more capable.

## 45.13 Exercises

1. Write a userspace program that opens `/dev/dri/renderD128`, calls `DRM_IOCTL_AMDGPU_GEM_CREATE` to allocate 64 KB GTT BO, mmaps it, writes a pattern, frees.
2. Extend it to map the BO into GPU VA at 0x100000000 via `DRM_IOCTL_AMDGPU_GEM_VA`.
3. Export to dma-buf fd; in another process, import as GEM handle; mmap; verify the same memory.
4. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_gem.c::amdgpu_gem_create_ioctl`. Trace how a request flows to TTM.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_mn.c::amdgpu_mn_invalidate_range_start`. This is one of the hardest files in amdgpu; just orient yourself.

---

Next: [Chapter 46 — TTM, the memory manager amdgpu uses](46-ttm.md).
