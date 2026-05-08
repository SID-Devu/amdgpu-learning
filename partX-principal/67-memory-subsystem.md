# Chapter 67 — GPU memory subsystem: HBM, GDDR, controllers, coalescing

The GPU is a parallel arithmetic engine bolted to a *very* aggressive memory system. Bandwidth, not compute, is the bottleneck for most real workloads. This chapter is the memory side.

## Memory types

| Type | Where it lives | Role |
|---|---|---|
| **VGPRs** | inside SIMD | per-lane fastest |
| **SGPRs** | inside CU | per-wave scalar |
| **LDS** | per CU/WGP | workgroup-shared scratch ("shared memory" in CUDA terms) |
| **GDS** | chip-wide | global atomics, less used in modern UMDs |
| **L0 / L1 / L2** | per-WGP / per-SE / chip | caches |
| **Infinity Cache (L3)** | chip | huge SRAM behind L2 (RDNA2+ desktop) |
| **VRAM** | HBM stacks or GDDR chips | gigabytes of off-chip DRAM |
| **System RAM** (via GTT) | host DRAM | mapped to GPU through IOMMU/GART |

A wave can address all of those, but with very different latencies and bandwidths.

## VRAM: HBM vs GDDR

| | GDDR (5/5X/6/6X/7) | HBM (2/2e/3/3e) |
|---|---|---|
| Form factor | individual DRAM chips around the GPU on the PCB | stacked die on the same package as the GPU |
| Per-pin speed | very high (e.g., 21–32 Gbps GDDR6/6X/7) | medium (~3–6 Gbps) |
| Channel width | narrow (32-bit per chip, 256–384 bits aggregate) | very wide (1024 bits per stack) |
| Bandwidth | hundreds of GB/s | up to several TB/s |
| Power per bit | higher | lower |
| Cost | cheaper | expensive |
| Used in | RDNA gaming GPUs, APUs | CDNA, MI250/MI300, datacenter |

**Why both exist**: gaming workloads tolerate higher latency for cheaper memory. AI/HPC needs sustained TB/s, so the $$$ HBM stacks are worth it. RDNA2/3 high-end uses **Infinity Cache** as an SRAM staircase to bridge GDDR's bandwidth gap.

### HBM packaging

HBM stacks contain 4–12 DRAM dies vertically connected by **TSVs** (through-silicon vias) and connected to the GPU through a **silicon interposer** (or **CoWoS** at TSMC). The interposer carries the wide buses without needing PCB-level routing. This is why HBM is "expensive": you're paying for advanced packaging.

### GDDR particulars

- GDDR6X / GDDR7 uses PAM3 / PAM4 signaling (multi-level voltages per cycle).
- Memory controller has to handle high-frequency training, refresh, ZQ calibration.
- Power consumption is dominated by I/O at high speeds.

## Memory controllers

The GPU has multiple **memory channels** (HBM stack channels or GDDR chip channels). Each channel has its own controller. Together they form the **GMC (Graphics Memory Controller)**.

The kernel driver `amdgpu_gmc` handles:

- aperture programming (where in the GPU's address space VRAM and GTT live),
- VM page-table walker config,
- ECC scrubber init,
- bad-page handling.

A typical config:

```
VRAM aperture: 0x000000_00000000 .. 0x0000FF_FFFFFFFF   (16 TB virtual, mostly unmapped)
GTT  aperture: 0x010000_00000000 .. 0x01FFFF_FFFFFFFF
MMIO aperture: ...
```

GPUs have a **48-bit virtual address space** for the shader VM. CPU and GPU virtuals are *separate* unless using SVM / ATS (chapter 51, 75).

## Coalescing — the most important access pattern

When a wave issues a load `ld a[lane_id]`, every lane in the wave generates one address. Hardware **coalesces** them: if all 32 (or 64) addresses fall into a single cache line, it issues one cache-line read. If they're scattered, it issues many.

```
Coalesced (good):
  lane 0 reads &a[0]      ┐
  lane 1 reads &a[1]      ├ all in one 64-byte cache line, 1 transaction
  ...                     │
  lane 31 reads &a[31]    ┘

Scattered (bad):
  lane 0 reads &a[0]      ┐
  lane 1 reads &a[1024]   ├ 32 different cache lines, 32 transactions
  lane 2 reads &a[2048]   │
  ...
```

Strided access by 1 element of the right size = good. Random access = order of magnitude slower. AOS vs SOA layouts matter enormously here.

### Bank conflicts in LDS

LDS is divided into **banks** (typically 32 banks in RDNA, 4-byte wide each). Two lanes that hit the same bank with different addresses serialize. Memorize:

```
addr % 32  →  bank
```

If `lane_id` reads `lds[lane_id * 4]`, that hits bank 0 from every lane → 32-way conflict. Use `lds[lane_id * 4 + (lane_id / 4)]` or pad to break the stride.

This is the single biggest LDS performance trap; senior engineers profile bank conflict rates routinely.

## Address translation on the GPU

The GPU has its own MMU. Every shader memory access is a GPU virtual address. Translation:

```
GPU VA  →  GPUVM page tables  →  device physical (VRAM) or system PA (via IOMMU)
```

The page tables have several levels (PD0/PD1/PT in AMD terms). Walked by hardware. The driver builds and updates them via SDMA writes (chapter 51, 84).

**TLB** caches translations; flush is needed when mappings change. amdgpu issues TLB flushes as part of fence completion when VM pages are remapped.

## Cache coherence on the GPU

GPUs are *not* fully coherent the way CPUs are. Within one wave, after a write, you must explicitly invalidate or flush caches before another wave (or block) reads. Compute often uses:

- `s_waitcnt vmcnt(0) lgkmcnt(0)` — wait for outstanding memory ops.
- `buffer_wbinvl1` — write-back & invalidate L1 (varies per gen).
- `s_dcache_wb` — flush scalar cache.

The compiler emits these around atomics, barriers, kernel boundaries, and explicit `__threadfence` calls.

CPU↔GPU coherence:

- "Coherent" mappings (host-allocated, mapped with HSA_AMD_MEMORY_POOL_PINNED_FLAG_*) — slower but no manual flush.
- "Write-combined" mappings — burst writes only; reads are very slow.
- "Uncached" — for MMIO-like access.

The kernel driver picks the right pgprot/cache mode in `mmap` callbacks.

## Bandwidth math

Always do this back-of-envelope before optimizing.

```
Effective BW needed = (input bytes + output bytes) per kernel * launch frequency
Available BW       = HBM/GDDR peak * efficiency (50-80% real-world)

If needed >> available → memory-bound. Compute changes don't help.
If needed << available → compute-bound. Adding waves / better ALU code helps.
```

For an MI300X with ~5 TB/s HBM, a kernel that touches 50 GB once per call takes ≥10 ms just to move bytes — irreducible.

## VRAM layout decisions made by the kernel

- **VRAM (local memory)** — fastest GPU access; CPU has slower BAR-mapped access.
- **GTT (system memory mapped through IOMMU)** — CPU-fast, GPU has to round-trip over PCIe.
- **VRAM with CPU-write-combined coherent map** — for upload streams.
- **Visible PCI BAR** — historically limited to 256 MB; **Resizable BAR (rebar)** lifts this so the CPU can directly address all of VRAM. Modern systems should have rebar enabled.

Allocator (TTM, GEM, see chapters 45–46) picks placement based on `domain` flags.

## Memory hot paths a senior engineer cares about

1. **Texture sampling** — high-throughput TC/TCC path with format conversion + filtering.
2. **Image stores** — coalesced or atomic; impacts RBE bandwidth.
3. **Buffer atomics** — global atomic to L2 or to memory; serialization point.
4. **LDS reductions** — bank-aware indexing for parallel reductions.
5. **Streaming loads** — bypass caches with `glc` (NOLD/streaming) bits to avoid pollution.

## What you should be able to do after this chapter

- Sketch the memory hierarchy with rough sizes and bandwidths.
- Explain why HBM is more expensive but worth it for AI.
- Define coalescing and bank conflicts.
- Reason about whether a kernel is memory-bound vs compute-bound.
- Know the difference between VRAM, GTT, and resizable BAR.

## Read deeper

- AMD CDNA 3 architecture (HotChips slides) — best memory subsystem diagrams.
- HBM3 standard (JESD238) — terse but the source of truth.
- `drivers/gpu/drm/amd/amdgpu/gmc_v*.c` — GMC IP version for each chip.
- `drivers/gpu/drm/ttm/ttm_resource.c` — placement logic.
- ChipsAndCheese articles on caches/L2/Infinity Cache.
