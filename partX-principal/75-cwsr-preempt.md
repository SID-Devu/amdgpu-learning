# Chapter 75 — CWSR, priorities, preemption

A modern GPU runs work for many processes (and many queues per process). Each kernel can take milliseconds to hours. Without preemption, an evil or just-long compute kernel would block all interactive graphics. Solving this is **CWSR**: Compute Wave Save / Restore.

This chapter is dense and important — preemption interacts with reset, scheduling, security, and performance.

## The problem

Consider:

- Process A: training a 30-second compute kernel (BFS over 10 GB of graph).
- Process B: a Wayland compositor wanting to render the next frame in 16 ms.

If A's wave occupies the GPU and we can't interrupt it, B misses the deadline → user-visible stutter. Worse: in some platforms (laptops, VR), the watchdog will eventually reset the GPU thinking it hung — interrupting A *and* breaking everyone.

We need to *preempt*: stop A, start B, later resume A from where it left off.

## Granularity options

GPU preemption can happen at different granularities:

| Granularity | Meaning | Cost | Used for |
|---|---|---|---|
| **Pipeline boundary** | Wait for current draw/dispatch to finish, then switch | low overhead, high latency | OK for most graphics |
| **Wavefront boundary** | Drain in-flight waves, switch | medium | better for mixed loads |
| **Mid-wave (CWSR)** | Save wave state mid-instruction, switch, restore later | high overhead, low latency | required for real-time + long compute |

CWSR is the heaviest, used when latency requirements are tight.

## What CWSR saves

When a wave is preempted mid-execution, the hardware/RLC must save:

- All VGPRs (~1024×4 B = 4 KB per SIMD wave).
- All SGPRs (~108×4 B = ~432 B per wave).
- LDS contents (up to 64-128 KB per workgroup).
- Wave PC, exec mask, status registers, scc, vcc.
- Scratch context.

The save area is in **VRAM** (or sometimes GTT), pre-allocated by the runtime. RLC firmware copies wave state to/from this area when preemption happens. Restore is the same in reverse.

CWSR images are **chip-specific** in layout — every gen has its own format documented in AMD's headers.

## When CWSR runs

The host requests preemption via PM4 packet (`PACKET3_PREEMPT`) on the GFX or compute ring, or via MES API for user queues. Hardware:

1. Selects waves to preempt (by priority criteria).
2. RLC writes their save areas.
3. Idle indication signals back to MES/CP.
4. New work runs (via newly mapped queue or new dispatch).
5. When the preempted work is to resume, RLC reads the save areas and the waves continue.

## Time-slicing

Without CWSR, very-long-running kernels would block everything. The kernel watchdog (TDR — Timeout Detection and Recovery, chapter 53) considers a long-running kernel as "hung" and resets the GPU.

Time-slicing combats this: every N milliseconds, MES preempts and yields to other queues. Long-running kernels see "pauses" but eventually finish. The watchdog measures *forward progress*, not wall time, so legitimate long kernels don't trigger it.

## Priorities

ROCr exposes:

- `HSA_QUEUE_PRIORITY_LOW`
- `HSA_QUEUE_PRIORITY_NORMAL`
- `HSA_QUEUE_PRIORITY_HIGH`
- `HSA_QUEUE_PRIORITY_REALTIME` (rare, requires permissions)

Vulkan/DX exposes context priorities. Internally these map to MES scheduler priority bands.

The **realtime** priority is special: it preempts everything else aggressively, used for compositors, VR, low-latency audio (where AMD GPU is doing mixing).

## CU masking and partitioning

Sometimes you want **physical isolation** instead of time-slicing. Two mechanisms:

- **CU mask**: a bitmap saying which CUs a queue may run on. Process A gets CU0..CU15, Process B gets CU16..CU31. Hard partition.
- **Compute partitioning** (CDNA-2/3): an entire XCD or SE can be carved out as a logical sub-device.

CU masks are popular in datacenter (e.g., shared GPU between containers). Set via KFD `set_cu_mask` ioctl.

## Realtime + display interaction

Display (chapter 77) operates on hard real-time deadlines (vblank intervals). Page flips need a few hundred microseconds of GPU compute (composition). The graphics scheduler escalates these to high priority and uses CWSR if a long compute task is mid-flight.

Mistakes in this code path produce visible glitches — frame skips, tearing, "compositor hang." Reviewing display ↔ compute scheduling fixes is a senior responsibility.

## What kernels and runtimes worry about

- **Save area sizing**: too small → CWSR fails; too big → wastes VRAM.
- **Restore correctness**: every shadow register must round-trip exactly. Bugs here are catastrophic.
- **Atomic write windows**: queue manipulation must be atomic from MES's perspective. Race → wrong queue runs.
- **Priority inversion**: low-prio queue holds a resource the high-prio one needs. Detect and avoid.

## Preemption + reset

If preemption itself hangs (RLC firmware bug, hardware glitch), the whole reset machinery (chapter 53) kicks in. Tiered reset:

1. **Soft reset**: just the offending IP (compute), preserves graphics.
2. **Mode-1 reset**: more of the GPU.
3. **Mode-2 reset**: full reset, all userspace contexts get `VK_ERROR_DEVICE_LOST`.

CWSR-induced hangs typically lead to soft reset.

## Debug interactions

When `rocgdb` attaches to a HIP process, it can:

- Set breakpoints inside running compute waves.
- Cause a wave to halt at a breakpoint.
- Step through GPU code instruction-by-instruction.

This requires the kernel to inject debug interrupts and use CWSR-like state save to read VGPR/SGPR/LDS values. The KFD debug ioctls (`AMDKFD_IOC_DBG_*`) implement this. It's a beautiful pile of complexity.

## Performance considerations

- CWSR overhead per preemption: dozens of microseconds. Not free.
- Frequent preemption hurts throughput for the preempted workload.
- The scheduler tries to minimize preemptions consistent with priority.
- **Avoid mixing realtime with long compute** if you can place them on different GPUs.

## Driver responsibilities

- Allocate CWSR buffers per-process, per-queue.
- Program RLC tables that say where to find per-chip state.
- Coordinate with MES on queue priorities.
- Handle CWSR failures (saved wave can't be restored: hangs → reset).
- Expose KFD ioctls for CU masks and priority.

`drivers/gpu/drm/amd/amdkfd/kfd_packet_manager.c`, `kfd_priv.h`, `kfd_mqd_manager_*.c`, and `gfx_v*.c` (per-gen RLC programming) are the relevant files.

## What you should be able to do after this chapter

- Explain why GPUs need preemption.
- Define CWSR and what it saves.
- Describe priority bands.
- Differentiate CU mask vs time-slicing.
- Reason about preemption + reset interactions.

## Read deeper

- AMD GPUOpen: "GPU Compute Preemption," "Realtime Compute on Radeon."
- `drivers/gpu/drm/amd/amdkfd/kfd_mqd_manager_*.c` — MQD (Memory Queue Descriptor) handling.
- `drivers/gpu/drm/amd/amdgpu/gfx_v11_0.c` — RLC microcode load + CWSR setup.
- `rocgdb` documentation (the GPU debug API).
- HSA runtime spec, "queue priority and preemption" section.
