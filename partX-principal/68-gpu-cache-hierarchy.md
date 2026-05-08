# Chapter 68 — GPU cache hierarchy & coherence

GPUs and CPUs share concepts (L1, L2, lines, hit rate) but their cache *semantics* are different. Misunderstanding this is the source of most GPU memory bugs.

## The hierarchy on RDNA (gen-typical, varies)

```
                    [VGPRs / SGPRs]                  per-lane / per-wave
                          │
                  [L0 vector $ ~32 KB]               per WGP
                  [L0 scalar $ ~16 KB]               per WGP
                  [L0 instr  $ ~32 KB]               per WGP
                          │
                   [L1 $ ~128 KB]                    per Shader Engine
                          │
                   [L2 / GL2 several MiB]            chip-wide
                          │
              [Infinity Cache (L3) tens-hundreds MiB] (RDNA2+ desktop)
                          │
                       [VRAM]
```

Naming has shifted across gens (the old "L1" became the new "L0", the old "GL2" became "L2"). Whitepaper-cite the specific gen when you discuss; don't guess.

## On CDNA

```
[VGPRs] / [SGPRs] / [LDS 64-128KB per CU]
        │
   [L1 vector $] per CU       (smaller than RDNA's WGP L0)
        │
   [L2 $] chip-wide, large    (no Infinity Cache, but big L2)
        │
       [HBM]
```

CDNA leans more on raw HBM bandwidth, less on cache layers.

## Cache-line size

Most AMD GPU caches use **64-byte** lines (a wave32 with each lane reading a `u16` = 64 bytes total — matches perfectly). CDNA may have wider effective fetch granularity due to the 64-lane wave. Always 64 B unless the spec says otherwise; design data layouts around it.

## Coherence: the GPU model

CPUs have **MESI-style coherence**: every cache snoops every other, writes propagate transparently. GPUs **do not** scale this — too many cache instances, too high cost. Instead:

- Within a single wave/CU: cache is coherent with itself.
- Between waves on the same CU/SE: depends on level and gen; often coherent at the L1/L2 boundary with explicit instructions.
- Between waves across the chip: not automatically coherent; **explicit invalidate / write-back instructions** required.
- Between GPU and CPU: zero coherence by default; must use coherent mappings or flush.

Translated to programmer-visible rules:

1. After a global write, before another wave reads the same address, you must:
   - On many AMD gens: write-back the producer's L1 (`buffer_wbinvl1`) **and** invalidate the consumer's L1.
   - Atomics handle this implicitly (they go to L2 or VRAM).
2. **`memory_scope_workgroup`** — flush only enough to keep the workgroup consistent (cheaper).
3. **`memory_scope_device`** — flush so any wave on this device sees it.
4. **`memory_scope_system`** — flush so the CPU sees it (slow; only for completion fences).

The compiler emits the right wait/flush combos based on the C++ atomic memory orders or HIP `__threadfence_*`. As a driver dev, you must understand what's emitted.

## Wait counters and barriers

GPU memory ops are *issued* but their completion is asynchronous. The wave keeps a few counters:

- **`vmcnt`** — count of outstanding vector memory ops (loads/stores).
- **`lgkmcnt`** — outstanding LDS / GDS / scalar / constant-mem ops.
- **`expcnt`** — outstanding export (output) ops.

Synchronizing is `s_waitcnt vmcnt(0)` etc. — wait until that counter reaches the given value. The compiler inserts these.

Workgroup barriers (`s_barrier`) ensure all waves of a workgroup reach a point and that LDS writes are visible. They include the right `s_waitcnt`.

## Caching policy — `glc`, `slc`, `dlc`

Memory ops on RDNA carry policy bits:

- **glc (globally coherent)** — bypass L0/L1, go to L2; for inter-CU communication.
- **slc (system-level coherent)** — bypass L2; for CPU-coherent paths.
- **dlc (device-level coherent / disable L1)** — RDNA1+ extra knob.

The compiler emits these per access. For example, atomics use `glc`. Streaming loads (won't be re-read) might use `slc` to avoid polluting L2. RDNA3 adds even finer policy control.

A senior engineer reads HSACO and notes the policy bits; wrong policy can cause subtle correctness bugs (e.g., a producer wave's writes not visible because the consumer reads from a stale L1).

## Texture cache (TC / TCC / TCP)

Reads through the texture path go via a separate cache — TCP/TCC — optimized for 2D locality and format conversion. Image-typed reads benefit from TCC's 2D-aware addressing (Z-order/Morton ordering on the lines). Buffer reads use the regular vector $ path.

For graphics: textures live in TCC; render targets are written through RBE which has its own color cache (CB) and depth cache (DB) — chapter 72.

## Cache misses and how to fix them

Common patterns:

| Pattern | Cause | Fix |
|---|---|---|
| Strided / scattered read | Non-coalesced or non-2D-local layout | Restructure data; transpose; tile |
| Read-then-write same line many times | False sharing / atomics on adjacent words | Pad to line; use private accumulator |
| Double-buffered streaming | Producer/consumer on same data | Use `glc` and explicit wait |
| Texture cache thrash | Working set > TCC | Tile/swizzle textures; reduce footprint |

## Inspecting cache behavior

- **rocprof / omniperf** counters: `L1_HIT_RATE`, `L2_HIT_RATE`, `TCC_MISS`, `TCC_HIT`.
- **RGP** for graphics: shows shader memory stalls, cache hit graphs.
- **umr** can dump some counters live.
- Microbenchmark with `streamBench` / `babelstream` to know your peak.

## Coherence with the CPU (for SVM / ATS)

When SVM (Shared Virtual Memory) is in play, GPU and CPU share an address space with hardware coherence between IOMMU-backed pages and GPU caches. Implementation:

- IOMMU translates GPU device VAs to host PAs.
- Coherent fabric links GPU caches to CPU cache hierarchy (Infinity Fabric on AMD APUs, NVLink/CCIX/CXL on others).
- Atomics with `system` scope use this path.

MI300A (CDNA3 APU) is the most coherent today — Zen 4 CPU and CDNA GPU share HBM with hardware coherence; you can pass pointers freely.

For discrete GPUs over PCIe, "coherence" usually means *managed* coherence: explicit fences and flushes hidden behind a runtime API.

## Implications for the driver

The kernel driver:

- Sets up MTYPE bits in page-table entries that select cache policy per-page (CDNA has many MTYPEs).
- Programs shader engine cache config registers (cache size partitions, prefetch policies).
- Issues blanket cache flushes during reset or page-table changes.
- Picks pgprot for `mmap` carefully (write-combined for streaming, cached for read-mostly host buffers).

`amdgpu_vm_pte.c` and `amdgpu_gmc_*.c` are the relevant files.

## What you should be able to do after this chapter

- Draw the cache hierarchy of a recent RDNA chip.
- Explain why GPUs aren't fully MESI-coherent.
- Use vmcnt/lgkmcnt vocabulary in conversation.
- Reason about glc/slc/dlc bits.
- Know what 2:4 sparse and `MTYPE` mean.

## Read deeper

- AMD ISA manuals (RDNA2/3, CDNA3) — especially sections on memory operations.
- "Atomics on GPUs" papers (any year).
- `drivers/gpu/drm/amd/amdgpu/gmc_v*.c` — MTYPE programming.
- LLVM AMDGPU `SIMemoryLegalizer.cpp` — emits the wait/flush sequences. **Read this file.** It is the operational definition of the GPU memory model.
- `mesa/src/amd/common/ac_nir_lower_global_access.c` — Mesa's similar lowering.
