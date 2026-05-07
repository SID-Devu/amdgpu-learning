# Chapter 27 — GPU concurrency: fences, queues, doorbells

GPU programming is *fundamentally* a multithreaded problem: thousands of GPU lanes, several command queues, multiple engines (graphics, compute, copy), and an asynchronous host. The synchronization primitives you've learned in CPU-land map onto GPU-land but with new names. Internalizing this map is the gateway into Part VII.

## 27.1 The mental model

The CPU produces work; the GPU consumes it.

```
┌────────┐     submit cmd        ┌──────────┐
│  CPU   │ ───────────────────▶  │   Ring   │ ─▶  GPU CP fetches
│  app   │                       │  buffer  │
└────────┘                       └──────────┘
   │                                  │
   │             completion           │
   │       ◀──────────────────────────┘
   ▼
 fence signaled
```

The **ring buffer** is a shared memory queue between CPU (producer) and GPU (consumer). The CPU writes commands into the ring and updates the **write pointer**. The GPU's command processor (CP) reads from the **read pointer** and advances. Same SPSC pattern as Chapter 23.

## 27.2 Doorbells

The GPU might be napping. To kick it: ring a "doorbell" — write to a special MMIO/PCIe address that wakes the CP and tells it new work is available. Doorbells are a *notification* mechanism, not data.

amdgpu maps a doorbell page to userspace for fast userspace submission (userspace queues / KFD). When userspace writes the new write-pointer value to that page, the GPU is notified directly without a syscall. Each process has its own doorbell page (protected by IOMMU + the GPU's own per-process VM).

## 27.3 Fences

A **fence** is a "this work has completed" signal. The CPU waits on a fence; multiple GPU operations can have multiple fences. Fences are the GPU equivalent of C++ futures.

In Linux, the universal kernel-side fence type is `struct dma_fence`:

```c
struct dma_fence {
    spinlock_t                *lock;
    const struct dma_fence_ops *ops;
    u64                        seqno;
    unsigned long              flags;
    /* … */
};
```

`dma_fence` has methods:

- `dma_fence_wait()` — block until signaled.
- `dma_fence_signal()` — mark as signaled; wake waiters.
- `dma_fence_add_callback()` — register a callback to fire on signal.
- `dma_fence_get()` / `dma_fence_put()` — refcount.

Drivers implement the `ops` (`enable_signaling`, `signaled`, `wait`, `release`) for their fence subclass.

In amdgpu: `struct amdgpu_fence` wraps a hardware sequence number written into a memory location by the GPU. When the value reaches a target, the fence is signaled.

## 27.4 Sync objects (`drm_syncobj`)

A **drm_syncobj** is a userspace handle to a fence. It allows fences to flow through ioctls and across drivers. Vulkan's semaphores are usually backed by syncobjs; modern Mesa uses `DRM_IOCTL_SYNCOBJ_*` heavily.

Two flavors:

- **Binary syncobj** — like a single fence: signaled or not.
- **Timeline syncobj** — a monotonically increasing 64-bit value; "wait for value ≥ N" (Vulkan timeline semaphores).

## 27.5 dma-buf and cross-driver sharing

A `dma-buf` is a kernel object representing memory shared between drivers. amdgpu can export a dma-buf, V4L2 (camera) can import it; zero-copy. Each dma-buf carries a list of fences (for synchronization across drivers).

Vulkan external memory and Wayland zero-copy clients use dma-bufs.

## 27.6 Memory ordering CPU↔GPU

The CPU's writes to system memory are not magically visible to the GPU. Three ordering domains:

1. **CPU caches** — the CPU's writes might still be in store buffers or L1.
2. **PCIe write buffers** — even after the writes hit DRAM, the GPU might have buffered them in its own memory controller.
3. **GPU's own caches** — the GPU has L1, L2 caches that may be stale relative to its memory controller.

To make a CPU write visible to the GPU:

- Write to memory.
- `wmb()` (kernel) — ensures store ordering.
- Issue a doorbell (an MMIO write) — this acts as a heavy barrier on PCIe.
- The GPU CP, on processing, may need to invalidate its L2 (via a packet like `KCACHE_INVALIDATE`).

To make a GPU write visible to the CPU:

- GPU executes a `RELEASE_MEM` packet (writes a fence value to a known memory address with a flush).
- CPU reads the fence; on seeing the value, the corresponding data is also globally visible.

This is the per-engine version of acquire/release.

In amdgpu's `IB` (Indirect Buffer) submission, the prologue/epilogue contains cache invalidate / writeback packets. They are not optional. If you forget them, you get random graphical corruption — the most painful class of GPU bugs.

## 27.7 Coherent vs. write-combining vs. uncached for GPU

CPU-mapped GPU memory has three common attributes:

| Attribute | Use |
|---|---|
| **WB (write-back, cacheable)** | rare for GPU memory; risk of CPU cache inconsistency |
| **WC (write-combining)** | command buffers, vertex buffers — write-only from CPU |
| **UC (uncached)** | doorbells, MMIO registers, fence read-back |

GTT memory in amdgpu is often allocated as WC. Carving up: `_PAGE_PCD | _PAGE_PWT` etc. (all hidden behind `ioremap_wc`/`pgprot_writecombine`).

When you do `mmap` on a GPU buffer in userspace, the kernel chooses page protection based on the buffer's flags. Choose wrong, you corrupt data.

## 27.8 GPU scheduler in the kernel

The DRM scheduler (`drivers/gpu/drm/scheduler/sched_main.c`) sits between userspace submissions and hardware queues:

- Each hardware queue has a `drm_gpu_scheduler`.
- Each open file context has a `drm_sched_entity`.
- Submitting a job creates a `drm_sched_job` with input fences (deps) and an output fence.
- Scheduler picks ready jobs, respects round-robin or priority across entities.

We dedicate Chapter 50 to this. It is one of the trickiest pieces of code to read in amdgpu and one of the most-asked-about in interviews.

## 27.9 Memory operations from the GPU's perspective

The GPU has its own MMU (the GPUVM). Each process's GPU buffers are mapped at a per-process GPU virtual address. Buffers can live in:

- **VRAM** (GPU local memory).
- **GTT** (system memory mapped via the IOMMU, accessible to both CPU and GPU).
- **CPU pinned pages** (via `userptr` BOs — the kernel pins user memory and maps it into GPU VM).

Migrating between these is expensive. TTM (Chapter 46) is the placement engine.

## 27.10 Practical takeaways for our future driver work

1. CPU↔GPU is a producer-consumer system; SPSC ring buffers, fences, doorbells.
2. Memory barriers between CPU and GPU are explicit; the OS APIs hide most.
3. Most GPU bugs are concurrency bugs in disguise — wrong fence ordering, wrong cache flush, wrong placement.
4. Userspace drivers do their own scheduling, but the **kernel** scheduler exists to enforce isolation between processes.

## 27.11 Exercises

1. Read `include/linux/dma-fence.h` from the kernel. Identify the ops, the flags, the seqno.
2. Read `drivers/gpu/drm/scheduler/sched_main.c::drm_sched_main` once. Don't worry about understanding everything; pick out the loop and the call to `run_job`.
3. Build a userspace SPSC queue between two threads using shared memory and futex-based fences. Note how it mirrors a GPU command ring.
4. With libdrm, write a tiny program that opens `/dev/dri/card0` and queries `DRM_IOCTL_AMDGPU_INFO`. We'll use libdrm in Part VII.

---

End of Part IV. Move to [Part V — Linux kernel foundations](../part5-kernel/28-kernel-tree-and-build.md).
