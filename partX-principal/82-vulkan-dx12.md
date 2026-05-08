# Chapter 82 — Vulkan & DX12 deep

Modern explicit graphics APIs (Vulkan, DirectX 12, Metal) push state management, synchronization, and memory layout decisions out of the driver and into the application. This *should* mean less work for the driver. In practice, it just means *different* work, and the surface area of "things the driver must do well" is enormous.

PMTS engineers in the graphics group think in Vulkan/DX12 idioms even when working on hardware.

## Why explicit APIs

OpenGL/DX11 hid almost everything: state changes, memory placement, synchronization, descriptor management. Drivers became huge optimizers that figured out what apps "really meant." Vulkan/DX12 say: the application *tells* the driver exactly what it wants, no inference.

Wins:

- Lower per-call CPU overhead (fewer driver decisions).
- Multi-thread submission.
- Predictable behavior (same code = same perf across drivers).
- Fine-grained memory control.

Costs:

- Apps must manage many things they used to ignore (barriers, layouts, descriptors).
- Misuse → undefined behavior or hard-to-debug crashes.

## Pipeline state objects (PSOs)

A PSO snapshots most of the rendering state into one object: shaders, vertex format, blend, depth, MSAA. Apps build these once, bind them many times. The driver compiles shaders during PSO build (slow); subsequent binds are cheap.

This is why many AAA games have a "compiling shaders" loading screen — they're warming the PSO cache.

PSO compilation cost is the **#1 driver perceived issue**. Mesa/radv ACO improvements over the years have largely targeted this.

## Descriptor sets / descriptors / descriptor heaps

Resources (textures, buffers, samplers) are bound through **descriptors**. Each descriptor is a hardware-specific structure (32 or 64 bytes on AMD) describing the resource: address, size, format, swizzle, sampler state.

Vulkan models this with **descriptor sets** (groupings of descriptors); DX12 with **descriptor heaps + tables/root-descriptors**.

AMD reads descriptors via the **scalar load (SMEM)** path — a wave's scalar unit fetches descriptors that all lanes share, then VGPRs fetch resources. Caching matters; descriptor pool layout matters.

**Bindless** descriptors (Vulkan `descriptorIndexing`, DX12 root signatures + heaps) let shaders index into a giant descriptor table at runtime. Modern engines (Unreal 5) use bindless heavily. The compiler must generate appropriate descriptor-indexing code.

## Synchronization

Both Vulkan and DX12 force the app to express dependencies:

- **Pipeline barriers / Resource barriers**: "before subsequent ops can execute, complete previous ops, flush their caches, and transition resource layouts."
- **Events / Splits barriers**: barrier whose wait is in a separate command than the signal.
- **Semaphores / Fences**: cross-queue and CPU↔GPU sync.

Driver translates these into PM4: `RELEASE_MEM` (with cache-flush mask), `WAIT_REG_MEM`, `ACQUIRE_MEM`. Wrong barriers = corruption or stalls.

### Image layouts

In Vulkan, an image can be in many layouts: `GENERAL`, `COLOR_ATTACHMENT_OPTIMAL`, `SHADER_READ_ONLY_OPTIMAL`, `TRANSFER_SRC_OPTIMAL`, etc. Layout dictates internal compression state (DCC on/off, HTILE compressed). Transitioning layouts may require hardware decompresses.

PMTS-level art: making layout transitions cheap (skip the decompress when the next stage understands the compression).

## Render passes (Vulkan classic) vs dynamic rendering

Vulkan 1.0 had **render pass** objects: declare attachment topology and dependencies up-front. Tile-based GPUs (Mali, Adreno) love this. AMD doesn't really benefit; the abstraction was painful.

**Vulkan 1.3 dynamic rendering** removes the render pass, allowing direct begin/end. Most modern Vulkan apps use this.

DX12 always had a more direct model.

## Pipeline barriers — the bug factory

Most game crashes / corruption on Linux Vulkan are barrier bugs:

- App didn't barrier between a write and a subsequent read → race.
- App over-synchronized → lost performance ("driver is slow") even though correctness is fine.

`VK_LAYER_KHRONOS_validation` catches many. radv has internal asserts and `RADV_DEBUG=*` knobs.

## Compute in Vulkan

Vulkan compute is a strict subset of compute shading: a Vulkan compute pipeline ⇒ AMDGCN compute kernel running on a compute ring. Same hardware as graphics compute; just different API surface.

Many engines use Vulkan compute for postprocess, animation, particles. Some use it for everything (Path of Exile 2's renderer is heavily compute).

## Mesh and ray tracing in Vulkan

Both as KHR extensions, both supported by radv:

- `VK_EXT_mesh_shader` (now KHR-tier in Vulkan 1.4 era).
- `VK_KHR_ray_tracing_pipeline` and `VK_KHR_ray_query`.

Both require recent radv + ACO + RDNA2+.

## DX12 on Linux

DX12 doesn't run natively on Linux; you need a translation layer:

- **VKD3D / VKD3D-Proton**: translates DX12 to Vulkan. Used by Steam Proton and Wine.
- Performance often near-native because the APIs are similar.

PMTS-level engineers at AMD also work with Microsoft on the Windows D3D12 driver (separate codebase, NDA).

### Translation challenges

- DX12 root signatures don't map 1:1 to Vulkan descriptor layouts.
- Some DX12 features need Vulkan extensions Mesa doesn't yet support.
- Resource barriers vary in semantics.

VKD3D maintains a translation layer that's grown to ~95% feature coverage.

## Drawing best practices (what every AAA team learns)

1. **Batch draws** with indirect draw + bindless.
2. **Use compute for postprocess** rather than full-screen quads with PS.
3. **Layout transition coalescing** — fewer, broader barriers.
4. **PSO caching** — ship pipeline cache binaries with the game.
5. **Async compute** — overlap shadow rendering / tone mapping with main pass.
6. **Avoid render-pass-load-clear** — use fast-clear via meta path.
7. **Place transient resources in tile-memory tiers** — even on AMD, reuse memory for per-frame buffers.

A senior engineer can recite these from memory.

## Driver architecture impact

When PSO compilation lags, the driver may:

- Cache PSOs to disk between game runs (`~/.cache/mesa_shader_cache`).
- Pre-compile in the background using a "shader threadpool."
- Fall back to a simpler shader (PSO "stuttering" mitigation).

When barriers stack up:

- Coalesce adjacent barriers into one PM4 fence.
- Emit cache flushes lazily (only flush masks that change).

When descriptor sets change rapidly:

- Cache the descriptor write layout, hash, and reuse PM4 sequences.

These optimizations are the bread and butter of game-driver work.

## Validation, tools

- **`VK_LAYER_KHRONOS_validation`** — must run during dev.
- **RGP** (chapter 86) — captures GPU timeline.
- **PIX** (Microsoft, DX12) — equivalent for Windows.
- **Nsight Graphics** — supports Vulkan and DX12; works on AMD too.
- **vktrace / vkreplay** — capture and replay Vulkan workloads.

CI for Mesa/radv runs these against many real games (anonymized replays) to catch regressions.

## What you should be able to do after this chapter

- Define PSO, descriptor set, pipeline barrier.
- Distinguish Vulkan render passes from dynamic rendering.
- Reason about barrier + image layout interactions.
- Sketch how DX12 maps to Vulkan via VKD3D.
- Recognize common best-practice patterns.

## Read deeper

- Vulkan spec (vulkan.org/spec).
- DX12 reference (Microsoft Learn, gpuopen.com).
- "Vulkan Tutorial" by Alexander Overvoorde — best free intro.
- AMD GPUOpen blog series on Vulkan.
- VKD3D source on Wine GitLab.
- "The Road to Vulkan" — Khronos talks.
