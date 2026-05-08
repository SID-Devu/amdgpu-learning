# Chapter 71 — Rasterization, hierarchical Z, primitive culling

The rasterizer takes a primitive (post-clipped, post-projected) and figures out which pixels (or sub-pixel samples) it covers. It then schedules pixel-shader waves on the CUs. This is fixed-function silicon — you don't program it directly, but you live with its rules forever.

## What rasterization actually computes

Given a triangle defined by its three screen-space vertices, the rasterizer:

1. Computes the bounding box.
2. For each pixel/sample inside the bounding box, evaluates **edge functions** to test inside/outside.
3. For inside samples, computes barycentric coordinates and interpolates per-vertex attributes.
4. Emits a **pixel quad** (2×2 group of pixels) to the pixel shader, with attribute-interpolated values, derivative info (`ddx`/`ddy`), and a coverage mask.

It does this in parallel across multiple **rasterizers** per shader engine. Throughput is measured in pixels (or quads) per clock.

## Pixel quads — the 2×2 unit

Pixel shaders execute on **2×2 quads** even at the edge of triangles. Why? Because hardware needs neighbors for derivative computations (`ddx`/`ddy`) used in mip-level selection for textures. So even a 1-pixel triangle occupies a full quad, with 3 of the 4 lanes "helper invocations" that don't write final color but participate in derivative math.

This means small triangles are wasteful — you're paying for 4 lanes' worth of shading to color 1 pixel. **Small-triangle culling matters.**

## Hierarchical Z (HiZ) and Hi-Stencil

Before pixel shading runs, the **HiZ** unit can reject entire pixel tiles by comparing the triangle's depth range against the pre-computed minimum/maximum Z of each tile in the depth buffer.

```
Screen tile (e.g., 8x8 pixels) has stored:
   ZMIN, ZMAX

Triangle covers tile with depths [zlo, zhi].
- If zhi < ZMIN  → entire tile passes (closer than everything stored).
- If zlo > ZMAX  → entire tile rejected (entirely behind everything stored).
- Otherwise     → fall through to per-pixel Z test.
```

HiZ buffers are compressed metadata maintained alongside the depth buffer.

**EarlyZ** is the per-pixel depth test that runs before the pixel shader (when possible), skipping shader invocations whose Z fails. Disabled if the shader writes Z, uses discard heavily, or has side effects. Modern shaders use `[earlydepthstencil]` (HLSL) / `layout(early_fragment_tests)` (GLSL) to force EarlyZ when safe.

A senior engineer always asks: "is EarlyZ enabled here? If not, why?"

## Render targets, depth, MSAA

Rasterizer outputs land in render targets (RTs) and a depth buffer. Multiple render targets (MRT) — up to 8 typical. Each may have:

- **Format** (R8G8B8A8_UNORM, R16G16B16A16_FLOAT, R32_UINT, ...).
- **Compression**: AMD calls this **DCC (Delta Color Compression)**. Lossless tile-level compression that reduces bandwidth on render and texture-fetch.
- **Tiling**: GPUs don't store textures linearly; they use 2D-swizzled tiling for cache locality. AMD's **DCC + Display tiles** swizzling has many flavors per gen.

### MSAA (Multi-sample antialiasing)

Multiple **samples per pixel** (2/4/8) at the depth/coverage level, but the pixel shader runs *once per pixel* (not per sample). The single shaded color is broadcast to covered samples. Resolve at end-of-frame compresses to 1×.

**EQAA** (Enhanced Quality AA) increases coverage samples beyond color samples for cheaper AA.

MSAA is bandwidth-heavy. RDNA introduced **VRS (Variable Rate Shading)** — shade at coarser-than-pixel rate (e.g., 1 shader invocation per 2×2 block) for areas with low detail. Saves shader work without proportionally hurting visible quality.

## Primitive culling, again — at the rasterizer level

Even after geometry-stage culling, the rasterizer can reject:

- Primitives outside the scissor.
- Sample-mask zero coverage.
- Conservative-rasterization edge cases.

**Conservative rasterization** (rare feature) marks a pixel as covered if *any part* of the triangle touches it. Used for voxelization, occlusion culling.

## Tile-based vs immediate-mode

Mobile GPUs (Mali, Adreno, PowerVR) are **tile-based deferred renderers (TBDR)**: they bin all primitives into screen tiles, then process tiles serially. Saves bandwidth.

AMD desktop GPUs are **immediate-mode** with tiles being a cache/tiling concern, not a rendering primitive. RDNA introduced **DSBR (Draw Stream Binning Rasterizer)** as a hybrid: defers rasterization within a small stream window to extract primitive overlap, then rasterizes; smaller wins than full TBDR but no app changes needed.

## RDNA-specific: WGP-pair issuing

Pixel-shader waves run on the same WGPs as compute. The graphics scheduler picks WGPs for new PS waves based on RBE bandwidth and CU occupancy. Senior debugging looks at "PS wave issue stalls" to detect rasterizer-front-end bottlenecks.

## Common rasterization perf issues

| Symptom | Likely cause | Fix |
|---|---|---|
| GPU bound on tiny triangles | Sub-pixel geometry | Proper LOD / mesh-shader cull |
| Z-bound | EarlyZ disabled | Avoid `discard`; mark `earlydepthstencil` |
| RBE bandwidth pegged | Heavy MSAA / no DCC | Enable DCC, reduce MSAA, use VRS |
| Overdraw shows in RGP | No depth pre-pass | Add depth-only pass for opaque geo |
| Quad utilization < 50% | Small triangles | Mesh-shader cluster cull |

## Driver responsibilities

Kernel: nothing rasterization-specific; it just allocates BOs and submits IBs.

UMD (Mesa, PAL, Windows driver):

- Configure RBE state (PA_SC_*, DB_*, CB_* registers) from API state.
- Pick depth/color compression (DCC) modes per RT.
- Manage **HTILE** (HiZ metadata) and **CMASK** (color compression metadata) buffers.
- Emit clears that update HTILE/DCC metadata in O(1) instead of touching every pixel.
- Submit fast-clear PM4 sequences.

A *lot* of UMD optimization lives in metadata management.

## Display interaction

The final color RT often becomes a presented surface:

- KMS receives a `drm_framebuffer` referencing the BO.
- Display Core (DCN, chapter 77) reads it. DCN supports DCC-compressed inputs, so the render output doesn't need a decompression pass before scanout — nice bandwidth saving.

Cross-driver dma-buf scenarios (composited via Wayland) require care: the consumer must understand the modifier (tiling+DCC layout). DRM **format modifiers** describe these layouts so consumer drivers know what they're getting.

## What you should be able to do after this chapter

- Explain pixel quads and helper invocations.
- Describe HiZ and EarlyZ in one sentence each.
- Define DCC, HTILE, CMASK and what each compresses.
- Know when EarlyZ gets disabled.
- Explain DSBR vs TBDR.
- Identify rasterizer perf issues from a profile.

## Read deeper

- AMD GPUOpen: "DCC primer," "Render Backend Engine (RBE)," "DSBR."
- `mesa/src/amd/common/ac_surface.c` — surface tiling/compression metadata.
- `mesa/src/amd/vulkan/radv_meta_*.c` — fast-clear, decompress meta paths.
- `drivers/gpu/drm/amd/include/asic_reg/db_*.h`, `cb_*.h` — depth and color register sets.
- "A Trip Through the Graphics Pipeline" — Fabian Giesen's classic 13-part series. Required reading.
