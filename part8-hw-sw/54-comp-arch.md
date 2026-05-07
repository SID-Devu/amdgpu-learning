# Chapter 54 — Computer architecture for systems programmers

You don't need a master's degree in computer architecture, but to write fast driver code you must know how a CPU executes your C, where time goes, and where the silicon is hiding from you.

## 54.1 What a single core actually does

A modern x86_64 core (Zen 4 / Golden Cove) runs at ~5 GHz, executes 4–6 micro-ops per cycle. That means: a single core does roughly **20 billion small operations per second**. Latency vs. throughput:

- Add two registers: 1 cycle, throughput 4/cycle.
- Multiply: 3 cycles, throughput 1/cycle.
- L1 hit: 4 cycles.
- L2 hit: 12 cycles.
- L3 hit: 30 cycles.
- DRAM: 100–300 cycles.
- Locked atomic: 10–30 cycles uncontended.
- Branch mispredict: 15–20 cycles wasted.
- TLB miss + page-walk: 100–500 cycles.

Memory is the bottleneck. Code that fits in L1 is 100x faster than code that misses to DRAM.

## 54.2 The pipeline

A core pipelines its work:

1. **Fetch** instructions from L1I.
2. **Decode** into micro-ops.
3. **Rename** registers (so independent ops can run in parallel).
4. **Issue** to execution units.
5. **Execute**.
6. **Retire** in program order.

Modern cores can have 100+ in-flight micro-ops. They speculate past unresolved branches; they execute out-of-order; they predict cache misses. All of this is invisible to your C, but it determines whether your code runs at 10% or 80% of peak.

## 54.3 The memory hierarchy

```
   ┌───────────┐
   │ Registers │   ~16 GP + 32 SIMD; 0 cycles
   ├───────────┤
   │   L1d     │   ~32 KB; 4 cycles
   │   L1i     │   ~32 KB
   ├───────────┤
   │    L2     │   ~512 KB-1MB; 12 cycles
   ├───────────┤
   │    L3     │   ~32 MB shared; 30 cycles
   ├───────────┤
   │   DRAM    │   GBs; 100-300 cycles
   ├───────────┤
   │ NVMe / GPU│   ~µs / ms
   └───────────┘
```

Cache lines are **64 bytes**. Loads bring a whole line. Two threads writing different fields in the same line cause **false sharing** (Chapter 23) — performance dies because the line bounces.

## 54.4 Branch prediction and speculation

Branches are predicted by the BPU. Mispredict = pipeline flush. Branchless code (`cmov`, masking) is sometimes faster than branchy.

Speculative execution = the CPU runs ahead of unresolved branches. Spectre/Meltdown-class attacks abused this. The kernel mitigates with retpolines, IBRS, etc. — visible as slight perf cost on every kernel call.

## 54.5 Out-of-order and barriers

The CPU may reorder loads/stores within its model. On x86_64 (TSO):

- Loads don't get reordered with earlier loads.
- Stores don't get reordered with earlier stores.
- A store can be reordered with a *later* load to a different address.

For SMP visibility, the kernel uses barriers (`smp_mb`, `smp_rmb`, `smp_wmb`). For a single core, it's the as-if rule.

## 54.6 SIMD

Modern x86 has SSE (128-bit), AVX (256-bit), AVX-512 (512-bit). ARM has NEON (128) and SVE (variable). One AVX-512 instruction does 8 `double`s in parallel.

GPU "SIMD" is much wider (32 or 64 lanes). The CPU's SIMD is tiny by comparison but used in libc memcpy/memset, hot kernels, video codecs, etc.

For driver work you rarely write SIMD intrinsics; the compiler auto-vectorizes simple loops. For runtime work (Mesa shader compiler, video codecs), you'll see lots of intrinsics.

## 54.7 ARM/AArch64 differences

- 31 GP regs (`x0-x30`), all 64-bit. `sp` separate.
- LDP/STP load/store pair instructions.
- Weakly-ordered memory model — needs `dmb`, `ldar`, `stlr` for SC.
- 4 KB / 16 KB / 64 KB pages possible.
- Fixed 4-byte instruction width (no variable-length encoding).

If you write driver code that needs to run on Qualcomm SoCs, you'll know AArch64. amdgpu is x86-mostly today but a few amdgpu APUs run on ARM in some form factors.

## 54.8 RISC-V briefly

The newcomer. Open ISA. Few production GPU-attached systems but presence growing. Same general patterns as ARM: weak ordering, RISC, paged.

## 54.9 GPU architecture vs. CPU

Already in Chapter 42. Re-anchor here:

- CPU: low latency, deep cache, branch prediction, OoO.
- GPU: high throughput, hardware multithreading hides DRAM latency, SIMD-of-SIMD.

GPUs hate divergent control flow within a wave. CPUs handle it fine.

## 54.10 What this means for you

- **Profile, don't guess.** `perf` will tell you where cycles go.
- **Cache > algorithm**, often. A linear scan of a vector beats a tree until n is large.
- **Avoid false sharing.** Pad hot per-CPU/per-thread data to a cache line.
- **Use atomics correctly.** Wrong memory order will be slow on ARM, "work" on x86.
- **For MMIO**: the CPU caches don't help; every read is a PCIe transaction. Batch where possible.

## 54.11 Exercises

1. Write a microbenchmark that times accessing a 64 MB array vs a 16 KB array in a tight loop. Compare; relate to L3 vs. L1.
2. Write code that has false sharing; pad to fix; measure.
3. Run `perf stat -e cache-misses,branch-misses your_program` — interpret.
4. Disassemble (`objdump -d`) a hot inner loop; identify the schedule of micro-ops.
5. Read AMD's "Software Optimization Guide for Zen 4." Skim — internalize that out-of-order is real.

---

Next: [Chapter 55 — Caches, coherence, and memory ordering](55-caches-and-ordering.md).
