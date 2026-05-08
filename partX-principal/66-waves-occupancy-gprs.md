# Chapter 66 — Wavefronts, occupancy, VGPRs/SGPRs

The GPU's parallelism unit is the **wavefront** (or "wave"). Mastering occupancy — how many waves are simultaneously resident on the hardware — is the single biggest lever for compute performance. PMTS-level engineers think about this constantly.

## What a wave actually is

A **wave** is a group of work-items (lanes) that execute in lockstep on a SIMD. Lockstep means: every lane executes the same instruction at the same time. If lanes diverge (some take a branch, some don't), the SIMD masks off lanes that aren't supposed to execute and runs both sides — the dreaded **divergence cost**.

| Architecture | Wave size | SIMD width | Issue cycles |
|---|---|---|---|
| GCN | 64 | 16 | 4 |
| CDNA 1/2 | 64 | 16 | 4 |
| CDNA 3 (MI300) | 64 | 16 | 4 (with double-rate ops) |
| RDNA wave32 (default) | 32 | 32 | 1 |
| RDNA wave64 | 64 | 32 | 2 (issued back-to-back) |

A wave has its own:

- 32-bit (or 64-bit on wave64) **execution mask** (`exec`) — which lanes are alive.
- **PC** (program counter).
- VGPR allocation (private to this wave).
- SGPR allocation (private to this wave).
- A slot in the wave scheduler.

Multiple waves on the same SIMD time-slice. The scheduler picks a ready wave each cycle.

## Workgroup → wave mapping

A **workgroup** (compute) or **draw of N pixels** (graphics) is broken into waves by the hardware:

- Workgroup size 256, wave32 → 8 waves.
- Workgroup size 256, wave64 → 4 waves.
- Workgroup size 64, wave64 → 1 wave (this is the GCN-classic case).

Workgroup-level shared memory (LDS) is shared by all waves of the workgroup that land in the same CU/WGP. Synchronization between waves of a workgroup uses LDS barriers (`s_barrier`).

## Occupancy

**Occupancy** is the number of waves resident on each SIMD (or each WGP / CU, depending on convention). Higher occupancy = the scheduler has more work to hide latency with.

```
Latency hiding analogy:
- 1 wave on the SIMD, it's waiting for memory → SIMD idles.
- 8 waves on the SIMD, one waits for memory → scheduler runs another.
```

### What limits occupancy

A wave occupies *all* of:

1. **VGPRs** — RDNA SIMDs have ~1024 VGPRs each. If a shader uses 80 VGPRs/wave, max waves/SIMD = 1024/80 ≈ 12. The compiler quantizes VGPR counts (e.g., to multiples of 4).
2. **SGPRs** — usually less of a bottleneck; ~108 per wave; the file holds many.
3. **LDS** — per workgroup. If you allocate 32 KB LDS, only 2 workgroups can be resident in a 64 KB CU.
4. **Wave slots** — the scheduler has a hard cap on simultaneous waves (e.g., 16 or 20 per SIMD).
5. **Workgroup slots** — a separate cap on number of distinct workgroups.

The actual occupancy is the **minimum** of all these limits.

### Compiler reports occupancy

When you compile a HIP kernel:

```
hipcc -save-temps -O3 my_kernel.hip
# look at *.s file, see comments like:
;   VGPRs: 80
;   SGPRs: 32
;   ScratchSize: 0
;   Occupancy: 8
```

Or use `--save-temps` plus `roc-obj-ls`/`roc-obj-extract` to inspect the HSACO. RGP (chapter 86) shows occupancy per-wave per-time.

## Spilling (the enemy)

If a shader exceeds the architectural VGPR limit (e.g., wants 300 VGPRs, max is 256), the compiler **spills** registers to **scratch memory** (a per-wave region in VRAM, accessed via the scratch path). Every spill = a DRAM round-trip = horrible.

You'll see this as `ScratchSize` > 0 in compile output.

Mitigations (in order of preference):

1. **Reduce live ranges** — restructure code so fewer values are live simultaneously.
2. **Wave64** — twice the registers per work-item (since the wave is bigger).
3. **Reduce inlining** — sometimes hurts perf elsewhere; profile.
4. **Bigger workgroup, smaller per-thread state**.

A senior engineer reads HSACO and counts "live VGPRs" by hand to find spill culprits.

## Divergence

Different lanes taking different branches:

```c
if (lane_id < 16) {
    do_thing_a();
} else {
    do_thing_b();
}
```

The wave executes `do_thing_a` with half the lanes masked off, then `do_thing_b` with the other half masked off. **Cost ≈ both arms.** When divergence happens at every iteration, you've serialized the wave.

Compiler/runtime tricks:

- Reorder data so adjacent lanes take the same branch.
- Sort work by branch type before dispatch.
- Use predication for tiny branches (faster than masking).

## Wave-level intrinsics

Modern shaders use intrinsics that reach across lanes within a wave:

- `__shfl`, `__shfl_xor`, `__shfl_up`, `__shfl_down` — read another lane's value.
- `__ballot` — broadcast a per-lane bool to a 32/64-bit mask.
- `__any`, `__all` — quick reductions.
- DPP (Data-Parallel Primitives) instructions — fast lane permutations.

These exist because the hardware can copy values between lanes within the SIMD without going through LDS or memory. Use them for wave reductions, prefix sums, sorting.

## RDNA's dual-issue (RDNA 3+)

RDNA 3 added **VOPD** (vector op dual-issue): one instruction encodes two scalar-add or vector-mul-add type ops that execute on the same cycle. Compiler must pair eligible ops; you can hit ~2× throughput on float-heavy kernels. Doesn't apply to MFMA (CDNA-only).

## CDNA's MFMA pipeline

On CDNA, MFMA instructions occupy the matrix engine for many cycles. While MFMA is running, the SIMD can issue other vector ops (the matrix unit and SIMD are partially decoupled). The compiler interleaves MFMA with VGPR-to-VGPR shuffling and address computation to keep both fed. Hand-written kernels exploit this by software-pipelining loads, MFMAs, and stores.

## Practical occupancy tuning

| Symptom | Likely cause | Fix |
|---|---|---|
| Low utilization, high cache hit rate | Memory-bound, not occupancy-bound | Coalesce loads, reduce traffic |
| Scratch > 0 | Spilling | Reduce live VGPRs |
| Many waves but slow | Divergence | Reorder data |
| Few waves and slow | Low occupancy | Reduce VGPR/LDS use |
| LDS-bound | Bank conflicts | Pad arrays, swizzle indices |

A good debugging approach:

1. Look at occupancy in RGP / `rocprof`.
2. Check spill / scratch.
3. Look at wave issue stalls per cycle (HW counters).
4. Look at L2 hit rate vs HBM bandwidth.
5. Hypothesize, change *one* thing, measure.

## What the kernel driver has to know

- VGPR/SGPR/LDS limits per chip family — used to validate shader compiles.
- Scratch buffer allocation — per-wave private memory in VRAM.
- Reset/recovery behavior on hung waves (chapter 84).
- Per-context priority (graphics vs compute vs realtime) so a high-priority wave can preempt.

The driver does not micro-manage occupancy. The compiler decides VGPR count; the driver allocates scratch and validates.

## What you should be able to do after this chapter

- Define wave, workgroup, occupancy.
- Explain why high VGPR usage hurts performance.
- Identify spilling from a `.s` listing.
- Know roughly how to read RGP's occupancy view.
- Reason about divergence cost.

## Read deeper

- RDNA 3 ISA manual, §3 "Wavefront execution" and §6 "VGPRs/SGPRs."
- AMD blog: "RGP profiling for game developers" — shows occupancy and stalls.
- LLVM `AMDGPUSubtarget` & `SIRegisterInfo` — VGPR allocation logic.
- Mesa ACO: `aco_register_allocation.cpp` — independent allocator that often beats LLVM on wave32.
- `rocm/HIP-Examples` — kernels with comments about occupancy.
