# Chapter 74 — KFD, HSA, AQL packets, MES

The compute path on AMD GPUs is fundamentally different from the graphics path. Graphics goes through DRM (`/dev/dri/card*`, libdrm, Mesa). Compute goes through **KFD** (`/dev/kfd`) using the **HSA** model with **AQL packets** scheduled by **MES**.

Senior compute / runtime engineers at AMD live in this chapter.

## The dual-driver story

Historically:

- `/dev/dri/cardN` — the DRM/amdgpu node, used by Mesa, Vulkan, OpenGL, OpenCL-on-Mesa.
- `/dev/kfd` — the KFD node, used by ROCr (HSA runtime), HIP, ROCm tools.

Both nodes talk to the same `amdgpu` kernel driver, but expose different ioctls and a different programming model. KFD has **always-on user-mode queues**; DRM uses kernel-mediated submission. Recent MES-driven kernels are blurring the line, but the userspace API split remains.

## What HSA is

**Heterogeneous System Architecture (HSA)** — an industry standard from the early 2010s, championed by AMD. The full vision (cross-vendor unified compute) didn't materialize, but AMD's runtime (ROCr) is HSA-shaped.

Core HSA ideas:

- **Agents**: each device (CPU, GPU) is an agent.
- **Queues**: software-visible ring buffers per process per agent.
- **AQL (Architected Queuing Language)**: standardized 64-byte packets describing work.
- **Signals**: hardware-coordinated 64-bit values for sync.
- **Shared virtual memory** (SVM): CPU and GPU share addresses.

ROCr is the user-space runtime implementing HSA on AMD. HIP (CUDA-API-shaped) sits above ROCr. PyTorch, TensorFlow, JAX run on top.

## AQL packet format

Every AQL packet is **64 bytes**, with a 16-bit header and packet-type-specific body.

```
+--------+--------+----------------------------+
| header |  type  |    type-specific data      |
+--------+--------+----------------------------+
   16b      16b              480 bits
```

Packet types include:

| Packet | What it does |
|---|---|
| `KERNEL_DISPATCH` | run a compute kernel with grid dimensions |
| `BARRIER_AND` | wait for all listed signals to reach 0 |
| `BARRIER_OR` | wait for any |
| `AGENT_DISPATCH` | run a runtime-internal task on the kernel agent |
| `INVALID` | written into the slot until the producer is ready (consumer waits) |

Kernel dispatch carries: grid size, workgroup size, kernel object pointer, kernarg pointer, completion signal handle, barrier flags.

## The user-mode queue model

A queue is a **ring buffer in shared memory**. Producer (the host runtime, in user mode) writes packets at `write_index`. Consumer (the GPU's MES firmware, or older HW's HQD) consumes at `read_index`. A **doorbell** notifies the GPU.

```
+-------------------------------------+
| AQL packet 0 (INVALID until ready)  |
+-------------------------------------+
| AQL packet 1                        |
+-------------------------------------+
| AQL packet 2                        |
+-------------------------------------+
| ...                                 |
+-------------------------------------+
| AQL packet N-1                      |
+-------------------------------------+
   ^                  ^
   read_index         write_index
   (consumed by HW)   (advanced by host)

Doorbell write rings the queue's doorbell (PCIe write to a magic page).
```

To submit:

1. Reserve a slot (atomic increment of `write_index`).
2. Fill all packet fields **except** `header.type`.
3. Memory barrier.
4. Atomically write `header.type = KERNEL_DISPATCH` (transitions packet from INVALID to ready).
5. Ring the doorbell.

Steps 4+5 must be atomic-in-meaning so the GPU can't see a partially-filled packet.

## Doorbells

A **doorbell** is a small PCIe-mapped page where each queue has its own dword. Writing to the doorbell wakes up the GPU scheduler. The mapping is per-process and protected by VMID/PASID — process A can't ring process B's doorbell.

Kernel allocates doorbell pages and grants `mmap` access to userspace. Each queue gets a slot; thousands of queues share one doorbell page.

## MES: scheduling user-mode queues

**MES (Micro Engine Scheduler)** is firmware running on a dedicated microcontroller in the GPU. It manages the **runlist**: set of queues currently mapped to hardware. Each VMID slot can have at most one mapped queue per pipe per ME instance.

When a queue gets work (doorbell rings):

1. MES checks if the queue is mapped (has a real HW slot).
2. If yes, MES schedules execution: dispatches workgroups based on AQL packet.
3. If no, MES decides whether to swap out a lower-priority queue and map this one.

Scheduling considers:

- Per-queue priority (HSA `HSA_QUEUE_PRIORITY_*`).
- Per-process priority (set via KFD ioctl).
- Wave time-slicing for long-running kernels.
- CWSR (chapter 75) for preemption.

Pre-MES: kernel did this in software via "process queue manager" with limited slots. MES scales much better.

## KFD ioctls — the compute API

`/dev/kfd` exposes ioctls that are *complement* to amdgpu's:

| Ioctl | Purpose |
|---|---|
| `AMDKFD_IOC_CREATE_QUEUE` | allocate a user queue (gets back a doorbell offset) |
| `AMDKFD_IOC_DESTROY_QUEUE` | tear down a queue |
| `AMDKFD_IOC_SET_MEMORY_POLICY` | per-VMID memory policies |
| `AMDKFD_IOC_GET_CLOCK_COUNTERS` | timestamps for profiling |
| `AMDKFD_IOC_ALLOC_MEMORY_OF_GPU` | allocate GPU-visible memory (VRAM or coherent system) |
| `AMDKFD_IOC_MAP_MEMORY_TO_GPU` | bind memory to a GPU VM |
| `AMDKFD_IOC_SVM` | manage shared virtual memory regions and migration policies |

All of these manipulate kernel-side `struct kfd_process` / `struct kfd_process_device` state. The KFD source is in `drivers/gpu/drm/amd/amdkfd/`.

## Kernel object & code object

A compiled HIP kernel is an **HSACO** (HSA code object) — an ELF blob containing AMDGCN machine code, kernel descriptors, metadata. ROCr loads it into VRAM and produces a **kernel object pointer** — passed in `KERNEL_DISPATCH` packets.

Compute waves spawned by the GPU read code from VRAM via the L1 instruction cache. Updating the same kernel object (e.g., during JIT recompile) requires icache invalidation.

## Signals

HSA signals are 64-bit memory locations tracked with their producer/consumer semantics. Used for:

- Completion notification (kernel dispatch carries a completion signal handle).
- Barriers (wait until signal == X).

Two flavors:

- **Default signals** — a memory location plus a kernel-managed waiter event.
- **Doorbell signals** — backed by GPU doorbells; very low-latency for inter-queue sync.

ROCr exposes them via `hsa_signal_*` API. Internally they are atomic 64-bit values.

## SVM (Shared Virtual Memory)

In SVM mode, GPU and CPU share an address space. Allocations made by `malloc()` (or `hsa_amd_svm_attribute_set`) are accessible from both, with the kernel handling page faults via:

- **HMM (Heterogeneous Memory Management)** in Linux — provides page migration callbacks.
- **MMU notifiers** — invalidate GPU mappings when CPU side mprotects/unmaps.
- **GPU page faults** — when GPU touches an unmigrated page, IOMMU/GMC raises a fault, kernel handles it (chapter 84).

SVM is heavy machinery and where the most subtle compute bugs live. CDNA APUs (MI300A) make this trivial because of hardware coherence; discrete GPUs need explicit migration.

## Userspace stack

```
Application (Python / C++)
       │
   PyTorch / TF / JAX / HIP code
       │
       v
  HIP runtime (libamdhip64.so)
       │
       v
  ROCr / HSA runtime (libhsa-runtime64.so)
       │
       v
  KFD ioctls + doorbells (no syscall on submit)
       │
       v
   amdgpu kernel (KFD module) + amdgpu IH/MES
       │
       v
   GPU hardware
```

Once a queue is created, **submission has zero syscalls**. AQL packet write + doorbell write = the entire submission path. This is critical for AI workloads issuing thousands of kernels/sec.

## Driver responsibilities (KFD)

Kernel must:

- Manage `kfd_process` lifecycle (per-process resources).
- Allocate/deallocate user queues, doorbell slots, runlist entries.
- Coordinate VMID allocation with the rest of `amdgpu`.
- Implement SVM with HMM/MMU notifiers.
- Implement `kfd_dev_dbg_*` for debug events when rocgdb attaches.
- Handle GPU page faults (recover or signal app).

These are deep waters. `drivers/gpu/drm/amd/amdkfd/kfd_*.c` is the codebase.

## What you should be able to do after this chapter

- Explain the dual-node (DRM + KFD) architecture.
- Describe the AQL packet format.
- Walk through user-mode queue submission step-by-step.
- Define MES, runlist, doorbell, kernel object.
- Describe how SVM uses HMM and MMU notifiers.

## Read deeper

- AMD HSA runtime (open-source ROCr): <https://github.com/ROCm/ROCR-Runtime>.
- HSA spec PDFs (HSA Foundation, public).
- `drivers/gpu/drm/amd/amdkfd/kfd_chardev.c` — every ioctl handler.
- `drivers/gpu/drm/amd/amdkfd/kfd_process_queue_manager.c`.
- `drivers/gpu/drm/amd/amdkfd/kfd_svm.c` — SVM with HMM.
- AMD GPUOpen blog: "AQL packet processing," "MES queues."
