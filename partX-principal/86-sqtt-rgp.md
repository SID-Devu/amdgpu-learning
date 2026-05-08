# Chapter 86 — SQTT, RGP, perf counters, RGD

Performance work on AMD GPUs is dominated by a few profilers and a *lot* of hardware counter knowledge. Knowing what each tool sees and where its blind spots are is what separates serious profilers from people running `time ./game`.

## The tools

| Tool | Workload | What it shows |
|---|---|---|
| **RGP (Radeon GPU Profiler)** | Graphics (Vulkan/DX12) | wave-level timeline, occupancy, stalls, cache hits |
| **rocprofiler-sdk / rocprof** | Compute (HIP) | hardware counters per kernel, traces |
| **omniperf** | Compute | roofline, occupancy, cache hit views, AI focus |
| **omnitrace** | Compute + system | unified CPU+GPU timeline |
| **RGD (Radeon GPU Detective)** | Crashes | post-mortem GPU dump analysis |
| **PIX (Microsoft)** | DX12 graphics | similar to RGP, Windows-side |
| **AMD AMF tools** | Multimedia | encode/decode profiling |

PMTS-level engineers know all of these. Junior engineers usually know one.

## SQTT (Shader Quality Trace)

**SQTT** is the hardware feature that captures **per-wave events** in a circular buffer. Each event records:

- Wave issued / completed instruction
- Memory access started / completed
- Cache hit / miss
- Wave preempted
- LDS bank conflict
- Stall reasons

The volume is enormous (millions of events for a few-millisecond capture). Hardware writes them into a buffer; software post-processes.

RGP and Microsoft PIX both consume SQTT. RGD uses SQTT for crash forensics.

## How RGP capture works

```
1. App runs, RADEON-PROFILER hotkey pressed (or programmatic capture).
2. driver opens SQTT buffers in VRAM.
3. enables SQTT collection on shader engines.
4. captures one or more frames.
5. disables collection; exports raw data.
6. user opens .rgp file in RGP UI.
```

In radv (Mesa): set `RADV_DEBUG=trace` or use `RADEON_DEBUG=hang` then trigger via Alt+Shift+P (with the RGP hotkey daemon).

Captures are huge (100s of MB per frame). Post-processing reconstructs:

- Per-wave timeline.
- Pipeline state per draw.
- Shader code with lane-count hot spots.
- Memory access patterns.
- Cache hit/miss rates.

## Hardware perf counters

The GPU has hundreds of hardware counters: per shader engine, per CU, per memory channel, per RBE. Examples:

- `SQ_PERF_SEL_WAVES`: wave issues.
- `SQ_PERF_SEL_CYCLES`: SIMD active cycles.
- `TCC_HIT`, `TCC_MISS`: L2 hits/misses (texture cache cluster).
- `CB_PERF_SEL_*`: render backend counters.
- `GL2_*`: L2 cache cluster counters.

Tools program counter selects (via PM4 SET_UCONFIG_REG packets), let kernels run, then read accumulated counts. Counters are limited (e.g., 16 simultaneous), so multiple passes may be needed for full coverage.

`rocprofiler-sdk` (the modern AMD profiler API) wraps this in:

```yaml
metrics:
  - WAVES
  - L2CACHE_HIT_RATE
  - VALU_BUSY
```

```bash
rocprofv3 --kernel-trace --metrics-set=foo my_app
```

## SPM — Streaming Performance Monitor

SPM streams selected counters to a buffer at a fixed sample interval (e.g., every 1024 cycles). Lets you build **counter timelines** synchronized with SQTT — see how L2 hit rate evolves over a frame.

Used by RGP's "Frame Summary" view.

## Reading an RGP capture

For a graphics workload:

1. **Frame Summary**: total time, hottest stages.
2. **Pipeline Overview**: time per pipeline (geometry vs raster vs PS vs RBE).
3. **Most expensive draws**: top 10 draws by GPU time.
4. **Wave occupancy**: timeline showing waves on each SE/CU.
5. **Shader analysis**: per-shader stats (VGPR count, occupancy, cycles, stalls).
6. **Cache views**: hit rates per level, per draw.
7. **Async compute**: are compute and graphics overlapping?

A first-pass profile takes 30 seconds; a thorough one takes hours. Identifying *which* of N findings to actually address is the art.

## Reading a rocprof / omniperf capture

For compute:

1. Total kernel time, top kernels.
2. Per-kernel: VGPRs, SGPRs, scratch, occupancy.
3. Cache hit rates per level.
4. Memory bandwidth realized vs peak.
5. Roofline: where does this kernel sit (memory-bound vs compute-bound)?
6. Wave issue stalls broken down by reason (memory, scalar, branch).

omniperf's roofline view is famous — plots arithmetic intensity vs achievable performance vs HW peak.

## Hot-path identification: a checklist

When given a profile, walk down this list:

1. **Is the GPU saturated?** If GPU activity < 90%, find why (CPU-bound, sync-bound, page-flip waits).
2. **Where's the time?** Top draw / kernel / IP block.
3. **Is it memory-bound?** Compare BW realized vs peak. If saturated, optimize accesses; reduce footprint.
4. **Is it compute-bound?** Look at stalls. VALU stalls = compiler issue or true math limit.
5. **Is it occupancy-limited?** Spilling, LDS, VGPRs.
6. **Is it serial?** Bad async scheduling or barrier oversync.

Each finding gets one fix. Profile again.

## RGD — crash forensics

When a GPU hangs or VM-faults, RGD analyzes the post-mortem dump:

- Last submitted IBs.
- Faulting wave PC and registers.
- Page-table state at fault time.
- Cache state.

Used by AMD's customer support to handle game-crash reports. PMTS-level engineers help train the heuristics RGD uses to isolate likely causes.

## Capture tips

- **Capture short**: 1-3 frames is plenty. SQTT buffer fills fast.
- **Stable conditions**: same scene, same input, same clocks (force `pp_dpm_*` to a fixed level). Variability ruins comparisons.
- **Background noise**: kill compositors / browsers; their work shows up too.
- **Compare A/B**: capture before-fix and after-fix, diff in RGP.

## Driver instrumentation

The driver exposes useful trace points:

- `tracepoint:amdgpu_cs_ioctl` — every CS submission.
- `tracepoint:amdgpu_vm_*` — VM events.
- `tracepoint:dma_fence_*` — fence events.
- `tracepoint:drm_sched_*` — scheduler events.

Use `trace-cmd` or `perf trace` to capture:

```bash
trace-cmd record -e 'amdgpu_cs_ioctl' -e 'dma_fence_*' my_app
trace-cmd report
```

## Counter combinations to learn

```
cycles_active / cycles_total      → utilization
valu_busy_cycles / cycles_active  → ALU efficiency
tcc_hit / (tcc_hit + tcc_miss)    → L2 hit rate
gl2_hit_rate                      → effective with chip varies
mem_accesses / kernel_time        → BW used
```

Memorizing 8-10 derived metrics is enough to talk shop.

## What you should be able to do after this chapter

- Define SQTT, SPM, RGP, RGD.
- Capture an RGP profile of a Vulkan game and explain at least one finding.
- Use rocprof to extract a counter set for a HIP kernel.
- Reason about memory-bound vs compute-bound.
- Identify when occupancy is the issue.

## Read deeper

- RGP user guide and the AMD GPUOpen tutorials.
- ROCm rocprofiler-sdk documentation.
- AMD blog: "Optimizing your graphics on Radeon."
- omniperf docs: the roofline model walkthrough.
- Reading other AAA engines' RGP captures (some shared online).
