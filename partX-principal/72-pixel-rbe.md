# Chapter 72 — Pixel pipeline: RBE, ROPs, blending, MSAA

The pixel shader's output doesn't go straight to memory. It funnels through the **Render Back End (RBE)** — a fixed-function block that does depth testing, blending, MSAA resolve, and compression-aware writes.

RBE is where memory bandwidth is most actively managed. PMTS engineers spend years tuning RBE behavior.

## Block diagram of the RBE

```
PS wave outputs (color, depth, coverage)
              │
              v
      +------------------+
      |  Pre-shader Z/   |   EarlyZ (when applicable)
      |   stencil tests  |
      +------------------+
              │
              v   (PS runs if needed, on the CUs)
              │
              v
      +------------------+
      |  Post-shader Z/  |
      |   stencil tests  |
      +------------------+
              │
              v
      +------------------+
      |  Coverage / MSAA |
      +------------------+
              │
              v
      +------------------+
      |    Blending      |
      +------------------+
              │
              v
      +-----------------------+
      | Color / Depth caches  |   CB$, DB$
      +-----------------------+
              │
              v
      +-----------------------+
      | DCC/HTILE/CMASK paths |   compression engines
      +-----------------------+
              │
              v
            VRAM
```

CB = Color Buffer pipe, DB = Depth Buffer pipe. Each has its own caches, separate from the shader caches.

## ROPs ("Raster OPerations" units)

A **ROP** is the unit that takes a quad of post-shader pixels and writes them to memory after testing/blending. AMD GPUs have many ROPs distributed across shader engines. ROP count is often advertised in marketing specs ("64 ROPs").

ROP throughput limits fillrate: how many pixels/sec you can write. With heavy blending or MSAA resolve, ROPs can become the bottleneck even when shaders are idle.

## Depth and stencil

Depth test happens twice (potentially): pre-shader (EarlyZ when allowed) and post-shader (always). DB pipe handles:

- depth-test (less, less-equal, greater, etc.),
- depth write (with mask),
- HTILE updates,
- depth compression.

Stencil is a per-sample 8-bit value stored interleaved with depth (typical D24S8 or D32_S8 formats). Operations: keep, replace, increment, decrement, increment-wrap, decrement-wrap, invert.

Common uses for stencil:

- shadow volumes (legacy),
- decal stencil,
- masking deferred-shading lighting passes,
- mirror/portal effects.

## Blending

The CB pipe applies the blend equation:

```
final = src * srcFactor  OP  dst * dstFactor
```

`OP` is add/sub/min/max. Factors are zero/one/srcAlpha/oneMinusSrcAlpha/etc. The hardware reads the destination value, computes, writes back. Blending is a read-modify-write; with order-dependent blending (typical alpha), it serializes pixel writes within a pixel.

**Independent blend** allows different blend state per RT. **Dual-source blending** (two outputs from PS into one RT) supports advanced effects like correct subpixel AA.

## DCC (Delta Color Compression)

AMD's tile-level lossless RT compression. For each tile (e.g., 8×8 pixels in BC-style layouts), a small metadata block stores either "this tile is a constant" or "delta-encoded against a reference." When a write would change the tile from "compressed" to "non-compressible," the hardware silently decompresses.

Properties:

- **Lossless** — pixel-exact.
- **Bandwidth saving**: typical 30-60% on real frames.
- **Correct on read**: any consumer aware of DCC can read directly; non-aware needs **decompress** pass.

DCC requires extra metadata buffers (called **DCC keys**) tied to the RT. Modifier-aware compositors (KWin, Mutter on recent Mesa) can scan out DCC-compressed buffers without intermediate decompression.

DCC is on by default in Mesa for Vulkan/RADV; some old games or unusual access patterns disable it.

## CMASK and FMASK

- **CMASK (Color Mask)**: 1-bit-per-sample (or 4-bit-per-tile) buffer that records whether a sample equals the "fast-clear color" — enables 1-cycle clears.
- **FMASK (Fragment Mask)**: only for MSAA RTs; encodes which sample at each pixel got which shader output. Lets the resolve pass do the right thing without per-sample stores.

## Fast clears

Clearing an RT to a fixed color is a frequent operation. The naive "write every pixel" path costs `width × height × bpp` bytes. AMD's fast clear:

1. Set the CMASK to "all clear."
2. Store the clear color in a small register (per RT).
3. Future reads through CB return the clear color when CMASK says so.

Cost: O(metadata size) instead of O(pixel count). Massive win.

The driver/UMD must emit the clear and update metadata correctly, including coordination with subsequent compositors.

## MSAA on the back end

Multi-sample paths in RBE:

- **Coverage**: which samples does this fragment cover? (1, 2, 4, 8 samples per pixel.)
- **Storage**: typically one shaded color per pixel + per-sample coverage; FMASK encodes which sample got which color when there's overlap.
- **Resolve**: at end-of-frame, average per-sample colors into 1× color buffer for display.

MSAA bandwidth = N× single-sample. RBE must maintain throughput; modern AMD parts have lots of CB area dedicated to MSAA.

## VRS (Variable Rate Shading) interaction

VRS reduces shading rate to coarser-than-pixel (1×2, 2×1, 2×2, 4×4). The PS runs once for a VRS region, and the RBE writes the same color to all covered pixels. Coverage and depth are still per-sample.

Three sources of VRS rate:

1. **Per-draw**: API call sets a global rate.
2. **Per-primitive**: vertex shader output `SV_ShadingRate` per primitive.
3. **Per-screen-region**: a low-res "shading rate image" attached to the framebuffer.

Common use: foveated rendering for VR.

## Pixel shader output paths

A PS can write:

- **Up to 8 RT colors** (`SV_Target0..7`).
- **Depth**: rare (`SV_Depth`); kills EarlyZ.
- **Stencil**: very rare (`SV_StencilRef` in DX12).
- **UAV writes** (storage images / structured buffers): goes through the regular shader memory path, **not** RBE.
- **Discard / kill**: drops the fragment (no write); may interact with EarlyZ.

UAV writes from PS are useful for compute-graphics interop (e.g., GPU particles updated from PS). Order isn't guaranteed across pixels of the same triangle; if you need ordering, atomics or **ROV (Raster-Order Views)** — a hardware feature on some chips that orders UAV writes per-pixel.

## Cross-stage dependencies

When a render pass writes to a texture and a subsequent draw reads it, the producer's RBE writes must drain to memory and downstream caches must invalidate before the consumer can fetch. The driver/UMD inserts:

- **Cache flushes** (RB→L2 writeback).
- **Stage barriers** (Vulkan `vkCmdPipelineBarrier`).
- **Decompress** if consumer doesn't understand DCC.

Mis-emitting barriers is one of the top game-driver bug categories.

## Driver responsibilities

UMD work:

- Choose CB compression mode (CMASK on/off, DCC on/off, FMASK on/off, fast-clear color register).
- Emit fast-clear PM4 sequences.
- Manage Vulkan barriers → PM4 cache flush events.
- Pick the right shadow/depth-only path.

Kernel work: BO allocation for metadata, atomic update of metadata BO when scanned out (compositor/DRM).

## What you should be able to do after this chapter

- Explain RBE, CB, DB.
- Define DCC, CMASK, FMASK, HTILE.
- Walk through a fast-clear sequence.
- List PS output kinds.
- Reason about MSAA bandwidth.

## Read deeper

- AMD whitepapers on RBE/DCC.
- `mesa/src/amd/common/ac_surface.c` — surface metadata.
- `mesa/src/amd/vulkan/radv_image.c`, `radv_meta_clear.c`, `radv_meta_decompress.c`.
- DX12 / Vulkan barrier specs (Microsoft, Khronos).
- "A Trip Through the Graphics Pipeline" — RBE chapters.
