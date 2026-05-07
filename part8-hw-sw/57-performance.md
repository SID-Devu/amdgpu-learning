# Chapter 57 — Performance: profiling, flamegraphs, GPU profilers

You can't optimize what you can't measure. This chapter teaches the toolset for both CPU-side driver perf and GPU-side workload perf.

## 57.1 The scientific method

1. **Have a hypothesis** ("submission overhead is the bottleneck").
2. **Measure** with a tool appropriate to the hypothesis.
3. **Confirm or reject.**
4. **Optimize the actual hotspot, not what you guessed.**

90% of perf claims I've seen in industry were wrong before measurement.

## 57.2 perf

CPU sampling profiler.

```bash
sudo perf record -F 999 -ag -- sleep 5
sudo perf report
```

Top consumers, per function. With `-g`, callgraphs.

`perf top` — live view.

`perf stat` — counters: cycles, instructions, cache misses, branch misses.

```bash
sudo perf stat -e cycles,instructions,cache-misses,cache-references \
                ./your_program
```

`perf trace` — strace replacement, much faster.

## 57.3 Flame graphs

```bash
sudo perf record -F 99 -ag -- sleep 30
sudo perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

Open in browser. Hot stacks are wide. The clearest visualization.

Run during a Vulkan game; identify amdgpu functions in the hot path.

## 57.4 ftrace

Kernel function tracer (Chapter 41). For driver-internal latency:

```bash
sudo trace-cmd record -p function_graph -l 'amdgpu_*'
```

Examine in `kernelshark`.

## 57.5 bpftrace / bcc

Dynamic tracing. One-liner:

```bash
sudo bpftrace -e '
kprobe:amdgpu_cs_ioctl { @start[tid] = nsecs; }
kretprobe:amdgpu_cs_ioctl /@start[tid]/ { @lat = hist(nsecs - @start[tid]); delete(@start[tid]); }'
```

Get a histogram of CS ioctl latency.

## 57.6 GPU profiling

For ROCm:

- `rocprof --hip-trace --hsa-trace ./your_app` — collect kernel times, HIP events, HSA-side.
- Output JSON; visualize in `chrome://tracing`.

For graphics:

- **`renderdoc`** — frame capture and profiling.
- **`Radeon GPU Profiler (RGP)`** + `radeonprofiler` — AMD's official.
- **`gputrace`** — Mesa-internal.

Identify per-shader cost, draw call cost, memory pressure.

## 57.7 GPU performance counters

Modern GPUs expose hundreds of counters: cache hits, ALU utilization, wave occupancy, memory bandwidth. AMD's interface: `rocprof -i input.txt`.

Counters tell you *why* a shader is slow:

- Low **VALU utilization** + high stall: memory-bound.
- High register pressure → low occupancy: cut register usage.
- LDS bank conflicts: rearrange access pattern.

## 57.8 Where time goes in amdgpu

Typical CS ioctl breakdown (rough):

| Phase | µs |
|---|---|
| ioctl entry, copy_from_user | 1 |
| BO list resolve + ww_mutex | 1–10 |
| IB validation | 0.5 |
| build sched job | 0.5 |
| schedule_main wakeup | 1 |
| ring emit + doorbell | 1 |

So 5–15 µs per submit on a not-very-contended system. KFD user-mode queues bypass the ioctl: <1 µs.

For graphics, draw-call rate is bound by GPU (state changes, shader compiles), not submission.

## 57.9 Memory bandwidth

DRAM bandwidth: ~25 GB/s per channel (DDR4-3200). Modern desktops have 2 channels: 50 GB/s aggregate. Workstations 4-8 channels. GPU memory: 1–2 TB/s for HBM3 / GDDR6X.

PCIe Gen4 x16: ~32 GB/s per direction. A GPU-bound application moving data in and out of VRAM is gated here.

`pcm-memory` and AMD's `uprof` can measure DRAM bandwidth.

## 57.10 Power and thermal

`rocm-smi` shows clocks, voltages, temperatures, power. Throttling shows up as drops in `sclk` (shader clock). `journalctl -k | grep dpm` for kernel-side PM logs.

Many "perf bugs" are actually thermal throttling under inadequate cooling.

## 57.11 Practical tips

- **Always rebuild with optimizations** (`-O2`); profiling `-O0` code is meaningless.
- **Disable address randomization** for reproducible profiles: `setarch x86_64 -R`.
- **Pin the workload** to a CPU set; keep noise out: `taskset 0xff`.
- **Run for long enough**; tools need samples.
- **Compare apples to apples**; always have a baseline measurement.

## 57.12 Exercises

1. Profile `vkcube` with `perf record -ag`. Identify the top 5 functions.
2. Profile `glmark2 -bbuild`; flamegraph.
3. Use bpftrace to histogram `amdgpu_cs_ioctl` latency under load.
4. Run `rocprof --stats` on a HIP sample (`rocm-examples` repo); identify the longest kernel.
5. Use `rocm-smi -f` while running a workload; observe if clocks throttle.

---

Next: [Chapter 58 — Debugging: gdb, kgdb, crash, ftrace, dmesg, lockdep](58-debugging.md).
