# Chapter 64 — GPU microarchitecture: RDNA

This chapter is the *physical structure* of an AMD graphics GPU at the level a senior engineer must be able to draw on a whiteboard. We focus on **RDNA** (Radeon DNA — RDNA 1/2/3/4 — desktop and mobile graphics). CDNA (compute) is chapter 65.

## Top-level block diagram

```
                     +-------------------------------------+
                     |           Host (CPU / PCIe)         |
                     +--+----------------------+-----------+
                        |                      |
                  PCIe TLPs               Doorbells / IRQ
                        v                      ^
                  +----------+           +-----------+
                  |  IH      |<----------|  Engines  |
                  |(Interrupt|           +-----------+
                  | Handler) |
                  +----+-----+
                       |
   +-------------------+-----------------------------------+
   |             AMD GPU SoC                              |
   |                                                      |
   |  +-----------+   +-----------+   +-------------+     |
   |  | GFX (CP)  |   | SDMA      |   | VCN (codec) |     |
   |  | + MEC/MES |   | (DMA)     |   |             |     |
   |  +-----+-----+   +-----+-----+   +------+------+     |
   |        |               |                |            |
   |        v               v                v            |
   |  +---------------------------------------------+     |
   |  |     Shader Engines (SE0..SE_n-1)            |     |
   |  |   each containing:                          |     |
   |  |     - Workgroup Processors (WGPs)           |     |
   |  |     - Compute Units (CUs) made of SIMDs     |     |
   |  |     - LDS (Local Data Share)                |     |
   |  |     - L0$ / L1$                             |     |
   |  +---------------------------------------------+     |
   |                       |                              |
   |                       v                              |
   |              +----------------+                      |
   |              |   GL2 / L2$    |                      |
   |              +----------------+                      |
   |                       |                              |
   |                       v                              |
   |              +----------------+                      |
   |              |  GMC (memory   |                      |
   |              |  controller)   |                      |
   |              +----------------+                      |
   |                       |                              |
   +-----------------------+------------------------------+
                           |
                  +-----------------+
                  |  HBM / GDDR     |
                  |  (VRAM)         |
                  +-----------------+
```

Memorize this. Every conversation at AMD references one of those boxes.

## The hierarchy from top to bottom

### 1. SoC

The "GPU chip" is a system-on-chip with multiple **IP blocks**: GFX (graphics+compute), SDMA (DMA copy engines), VCN (video encode/decode), DCN (display), SMU (power), PSP (security). Each is a separate piece of silicon with its own firmware and its own kernel-driver IP block (`amd_ip_funcs`, see chapter 48).

### 2. Shader Engine (SE)

A shader engine is a coarse cluster of CUs/WGPs sharing a primitive front-end (geometry/rasterization) and a render back-end (RBE). RDNA chips have 1 to 6+ SEs. Big GPUs have multiple **Shader Arrays (SAs)** per SE; each SA holds a number of WGPs.

### 3. Workgroup Processor (WGP) — the RDNA innovation

In **GCN** (the older arch) the unit was the **Compute Unit (CU)**. RDNA introduced the **WGP**, which is **two CUs paired** sharing a 128-KB LDS, an instruction cache, a scalar cache, and a texture path. RDNA's "wave32" mode uses the WGP-pair to issue 32 lanes per cycle; legacy "wave64" uses 64-lane execution that splits work across the WGP halves.

(In docs you may see "WGP mode" vs "CU mode"; CU mode treats the two CUs of a WGP as independent units with their own LDS halves. WGP mode pairs them and shares the full 128 KB.)

### 4. Compute Unit (CU)

Inside a CU, the actual ALUs:

- **2× SIMD32** (or **4× SIMD16** in GCN). Each SIMD32 is a 32-lane vector ALU. It executes 32 work-items in lockstep — a "wave32."
- **1× scalar ALU** for control flow, address calc, branch decisions.
- **VGPR file** (vector general-purpose registers) — typically 1024 VGPRs of 32 bits per SIMD, dynamically allocated per wave (more VGPRs/wave → fewer waves resident → lower occupancy; chapter 66).
- **SGPR file** (scalar registers) — ~108 per wave.
- **Branch/exec mask** — 32-bit (or 64-bit in wave64) execution mask saying which lanes are "alive" through divergent branches.
- **LDS** — fast local memory, 64 KB per CU (or 128 KB shared per WGP).

### 5. Caches at the CU level

```
L0 instruction $    per WGP   ~32 KB
L0 scalar $         per WGP   ~16 KB
L0 vector / data $  per WGP   ~32 KB     (used to be called "L1$" in GCN)
L1 cache            per SE    ~128 KB    (shared between WGPs in the SE)
L2 / GL2            chip-wide several MiB
L3 / Infinity Cache chip-wide tens-to-hundreds of MiB (RDNA2/3/4 only)
```

The naming changed across generations. Always confirm with the specific chip's whitepaper. The **Infinity Cache** (L3) is a huge SRAM behind L2 to absorb VRAM bandwidth pressure — it's RDNA2's biggest win for high resolutions.

### 6. Front-end / fixed function

In addition to programmable shaders, each SE has fixed-function blocks:

- **Geometry processor / primitive assembler** — vertex fetch, primitive assembly, primitive culling (small-triangle/zero-area, frustum, backface).
- **Rasterizer** — turns a primitive into pixels.
- **Hierarchical Z (HiZ)** / **Stencil cull** — early reject before pixel shader.
- **RBE (Render Backend)** — depth/stencil/color writes, blending, MSAA resolve.

We dedicate chapters 70–72 to these.

## Wave32 vs wave64 (the RDNA shift)

GCN executed **wave64** (64 lanes per wave) on a 16-lane SIMD over 4 cycles. Worked, but a divergent or short shader wasted lanes.

RDNA introduced **wave32** as the default — a 32-lane SIMD that issues in 1 cycle. Wave64 is still supported (backward compat, some workloads). UMDs (Mesa, DXC, drivers) pick wave size per shader stage based on heuristics.

Implications:

- Compiler (ACO, LLVM) produces different code for wave32 vs wave64.
- Occupancy math (chapter 66) differs.
- LDS bank conflicts differ (chapter 67).

## RDNA generations at a glance

| Gen | Architecture name | First products | Notable feature |
|---|---|---|---|
| GCN1..5 | Southern Islands → Vega | HD 7000 (2012) → Vega (2017) | Wave64 only, 4× SIMD16, GDS |
| RDNA 1 | Navi 10/14 | RX 5700/5500 (2019) | First WGP/wave32, no Infinity Cache |
| RDNA 2 | Navi 21/22/23/24 | RX 6000 (2020) | Infinity Cache, ray accel units, mesh shaders, VRS |
| RDNA 3 | Navi 31/32/33 | RX 7000 (2022) | Chiplet (GCD+MCDs), dual-issue VOPD, AI accelerators |
| RDNA 4 | Navi 4x | RX 9000 (2025) | Improved RT, AI accel, monolithic |

You should know the rough release year and one differentiator each. Interviewers ask.

## How a triangle becomes pixels — the path through RDNA

1. CP receives draw IB. Vertex shader (VS, or merged "GS-NGG" / "ESGS" pipeline) is dispatched as wave32 across CUs.
2. VS reads vertex buffers via L0/L1/L2 caches; outputs go to LDS or directly to next stage.
3. Primitive assembler combines vertices into triangles. **Primitive culling** discards backfacing/zero-area early (NGG path).
4. Rasterizer emits pixel quads to **PS (pixel shader)** wavefronts.
5. PS waves run on CUs, sample textures via TC paths.
6. PS output → RBE: depth/stencil test, blend, write to color buffer in VRAM.

Every box is a place where a senior engineer has spent months optimizing.

## What you should be able to do after this chapter

- Sketch this diagram from memory in 90 seconds.
- Explain the difference between wave32 and wave64 in one sentence.
- Explain what an SE, WGP, CU are and why they exist as separate boxes.
- Name three caches and roughly their sizes.
- Identify, given a workload, which part of the pipeline is likely the bottleneck (geometry-heavy → front-end; high-res → RBE/bandwidth; small shaders → wave occupancy).

## Read deeper

- AMD whitepapers for each gen ("RDNA 1 architecture," "RDNA 3 architecture") — public, dense, **read all of them.**
- AMD ISA manuals (RDNA1, RDNA2, RDNA3.5 ISA documents) — the bible.
- ChipsAndCheese articles on RDNA (excellent independent analysis).
- `drivers/gpu/drm/amd/amdgpu/gfx_*.c` — kernel side for each chip family.
- `mesa/src/amd/common/` — userspace knows about the same blocks.
