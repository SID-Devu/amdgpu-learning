# Chapter 42 — GPU architecture for software engineers

You can't write GPU drivers without a clear picture of the hardware. This chapter is a fast tour, AMD-specific where it matters.

## 42.1 What a GPU is

A GPU is a wide SIMD machine optimized for *throughput*. CPUs minimize latency; GPUs maximize throughput.

Trade-offs:

- CPU: 8–96 cores, deep pipelines, big caches, branch predictors.
- GPU: hundreds to thousands of "lanes", shallow pipelines, small per-core caches but enormous register files, hardware multithreading to hide memory latency.

Modern AMD GPUs (RDNA / CDNA):

- **Compute Units (CUs)** — the basic block. Each has 64 ALUs (lanes) organized as 4 SIMDs (RDNA: 32-wide × 2 sub-issue; CDNA: 64-wide).
- **Wavefronts (waves)** — group of 32 (RDNA) or 64 (CDNA) lanes executing in lock-step.
- **Workgroups** — collections of waves that share **LDS** (Local Data Share, a.k.a. shared memory in CUDA terms).
- **Shader Engines / Shader Arrays** — clusters of CUs, sharing fixed-function units.
- **L1, L2 caches** — L1 per CU; L2 globally shared, banked.
- **Memory controllers** — connected to GDDR / HBM.
- **DCN** (Display Core Next) — display engines, modesetting.
- **VCN** (Video Core Next) — video encode/decode.

A high-end GPU has dozens of CUs and an enormous (~2 TB/s) memory subsystem.

## 42.2 The blocks you'll touch

Inside the chip, the units of interest to a driver writer:

- **GFX (graphics)** — graphics + general compute pipeline. Has the **CP** (Command Processor) that fetches and dispatches commands.
- **MEC** (Micro-Engine Compute) — older compute engine; queue-driven.
- **MES** (Micro-Engine Scheduler) — newer firmware-based scheduler.
- **SDMA** — System DMA engine; copy buffers between GTT/VRAM/host.
- **DCN** — display.
- **VCN** — video.
- **PSP** (Platform Security Processor) — secure firmware loader.
- **SMU** — System Management Unit; power/thermal/voltage.
- **IH** — Interrupt Handler (the hardware that delivers IRQs).
- **GMC / VMID** — GPU Memory Controller; VM IDs for multi-process.

amdgpu has a directory per IP block: `gfx_v10_0.c`, `sdma_v4_4_2.c`, `vcn_v3_0.c`, `mes_v11_0.c`, etc. The driver is structured around these.

## 42.3 The command-processor model

The CPU produces work; the GPU consumes it via a ring buffer of "packets." Common packet types:

- **PM4** (graphics CP packets) — the GFX language. Examples:
  - `INDIRECT_BUFFER` — jump to an IB and execute it.
  - `WRITE_DATA` — write a dword to memory (often used as fence).
  - `WAIT_REG_MEM` — poll a memory location for a value.
  - `RELEASE_MEM` — write fence + flush caches.
  - `ACQUIRE_MEM` — invalidate caches.
- **SDMA packets** — copy, fill, write.
- **VCN packets** — video.
- **MES packets** — submit/scheduler control.

A "command buffer" is a sequence of packets in memory. Userspace builds one (an **IB** — Indirect Buffer), then submits via ioctl. The kernel:

1. Validates the IB (relocations, register white-list).
2. Wraps it with prologue/epilogue PM4 (cache flushes, fence).
3. Pushes onto a ring (or hands to MES).
4. Triggers a doorbell.

We dedicate Chapter 49 to this.

## 42.4 The ISA (briefly)

AMD's GPU ISA — RDNA, CDNA, GCN — is documented in PDFs ("RDNA 3 ISA", "CDNA 3 ISA"). You don't need to read it day one, but if you do GFX bring-up or compute optimization, you will.

The userspace shader compiler (LLVM AMDGPU backend) emits ISA. The kernel rarely cares about ISA contents — it just shovels code into VRAM and tells the CP "here's a shader at address X."

## 42.5 GPU memory architecture

Memory the GPU can see, in priority order:

1. **VRAM** (GDDR6/HBM): high-bandwidth, GPU-local, *not* CPU-coherent.
2. **GTT** (Graphics Translation Table): system RAM the GPU can access via the IOMMU; CPU-shared.
3. **System memory via userptr**: pinned user pages mapped into GPU VM.
4. **DOORBELL**: tiny, special PCIe pages mapped into the GPU's MMIO.

GPU virtual addresses are *per-process* on modern AMD parts — every userspace process has its own GPUVM with its own VMID. The GPU's MMU walks page tables prepared by the kernel.

When userspace allocates a BO (buffer object), the kernel chooses placement (VRAM, GTT, or both with eviction) and maps it into the process's GPUVM. We cover in Chapter 51.

## 42.6 The whole stack, top to bottom

```
    OpenGL / Vulkan / OpenCL / HIP             (Mesa, ROCclr)
                ▲
   userspace driver: IB build, BO mgmt          (mesa/radeonsi, rocclr)
                ▲
        libdrm + ioctl                          ( libdrm_amdgpu )
                ▲ syscall
   ─────────────┼───────────────
   amdgpu.ko   ▼
     │
     ├── DRM core: file_ops, ioctl dispatch
     ├── GEM: buffer object handles
     ├── TTM: VRAM/GTT placement, eviction
     ├── DRM scheduler: per-engine queues
     ├── IRQ: IH ring drain → tasklet
     ├── PM: DPM, runtime pm
     ├── DC: display modesetting
     ├── psp: firmware load + security
     ├── gfx/sdma/vcn/mes: per-IP ring submit
     └── KFD: HSA / ROCm path (separate fd)
                │ MMIO + DMA + IRQ
                ▼
              GPU silicon
```

We will build out each box in subsequent chapters.

## 42.7 The KFD path (ROCm)

AMD has *two* ways into the GPU from userspace:

- **DRM/render** path (`/dev/dri/renderD*`): graphics, Vulkan, OpenGL, OpenCL via Mesa/Radeon, etc.
- **KFD path** (`/dev/kfd`): for ROCm — HSA queues, signals, SVM. Used by HIP, OpenCL ROCm runtime.

KFD is in `drivers/gpu/drm/amd/amdkfd/`. It shares device with amdgpu (KFD is initialized by amdgpu). The KFD interface offers user-mode queues — userspace fills doorbells directly and the GPU's MES runs them. This is how ROCm achieves low submission overhead.

## 42.8 What a GPU "kernel" is

In compute parlance, a **kernel** is a small function that runs on the GPU, dispatched as a grid of workgroups. CUDA calls them `__global__` functions; HIP/OpenCL/CUDA all call them kernels.

In Linux/driver parlance, "kernel" means the OS kernel.

We disambiguate by context. In Part VII, "GPU kernel" or "compute kernel" or "shader" = the workgroup function. The OS kernel = "the kernel."

## 42.9 Bring-up overview (preview of Chapter 48)

When amdgpu binds to a GPU at boot:

1. Detect ASIC family from PCI ID.
2. Map BARs.
3. Enable PCIe.
4. Init PSP and load firmware (every IP has a `.bin`).
5. Init GMC, VMIDs, page tables.
6. Init GFX (CP), SDMA, VCN, JPEG, MES, etc.
7. Init DC (display).
8. Init DPM/SMU.
9. Register DRM device, expose ioctls and `/dev/dri/cardN` + `/dev/dri/renderDN`.

Each step has hundreds of lines. The driver is *enormous* because GPUs are enormous.

## 42.10 Exercises

1. `lspci -vv -s 01:00.0` (your GPU's BDF). Identify all BARs and capabilities.
2. `cat /sys/class/drm/card*/device/uevent`.
3. Read `Documentation/gpu/amdgpu/index.rst` from the kernel.
4. Skim the AMD RDNA 3 architecture white-paper (publicly available). You don't need to memorize; orient yourself.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu.h` — the omnibus header. Identify `struct amdgpu_device` and its many sub-structs.

---

Next: [Chapter 43 — The Linux graphics stack from app to silicon](43-graphics-stack.md).
