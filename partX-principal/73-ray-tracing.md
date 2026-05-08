# Chapter 73 — Ray tracing hardware

Ray tracing on AMD started with **RDNA 2** (RX 6000 series, 2020). RDNA 4 is the third generation. Each gen significantly improves per-ray throughput, traversal performance, and BVH features. PMTS-level engineers in the graphics group must speak this fluently.

## What ray tracing actually computes

Given a ray (origin + direction) and a scene of geometry, find:

- **First hit** (or "any hit," depending on shader): which triangle/primitive does the ray first intersect?
- Trigger the appropriate **shader** (closest-hit, miss, any-hit, intersection) with hit info.

The naive approach (test every triangle) is O(N) per ray — unusable. The standard solution is a **BVH (Bounding Volume Hierarchy)**: a tree of axis-aligned bounding boxes containing geometry. Traversal is roughly O(log N) per ray.

## The BVH

```
                Root AABB
                /       \
             AABB        AABB
            /    \      /    \
          ...   ...   ...   ...
          /\
        Leaf  Leaf       ← references to triangles
```

Each interior node has a few children (binary, quad, octree variants); each leaf references geometry.

A ray descends the tree:

1. Test ray against each child AABB.
2. Push hit children onto a traversal stack.
3. At leaves, intersect against actual triangles.
4. Return closest hit.

Modern GPUs have **hardware** for BVH traversal — the part that's expensive in software but a few cycles per node in HW.

## AMD's ray-accelerator approach

RDNA 2/3/4 add **Ray Accelerators** (RAs) inside each CU. Compared to NVIDIA's RT cores, AMD's RAs are *hybrid*: hardware for box and triangle intersection tests, **but traversal control flow runs in shader code**.

```
On AMD:
- Shader executes traversal loop.
- For each node, calls hardware "intersect 4 boxes" or "intersect triangle" intrinsic.
- Hardware returns hit/no-hit / barycentrics.
- Shader pushes/pops the stack, decides next node.
```

This means:

- The compiler emits the traversal loop ("inline traversal").
- Optimization (which node to visit next, sorting hits) is software-controlled.
- More flexible than fully-fixed-function approaches; can be slower if the loop is poorly compiled.

NVIDIA's RT cores do traversal in HW too; AMD trades flexibility for less area. Real-world performance differences are workload-dependent and gen-dependent.

## RDNA 3 and 4 improvements

RDNA 3:

- Larger LDS-resident traversal stack option.
- Better sorting of hit candidates.
- More instructions per ray.

RDNA 4:

- Major increase in box-tests-per-cycle.
- Improved BVH traversal compaction.
- Better hit/miss latency hiding.

Comparable to NVIDIA's per-gen RT improvements; the gap has narrowed.

## DXR / VKR — the API picture

**DirectX Raytracing (DXR)** and **Vulkan KHR_ray_tracing_pipeline** define the same conceptual model:

- App builds **acceleration structures (AS)**: bottom-level (BLAS, per mesh) and top-level (TLAS, the scene).
- App writes **HLSL/GLSL shaders** for the various RT shader stages.
- App dispatches a "trace rays" call: width × height × depth threads, each tracing rays.

Shader stages:

| Stage | When |
|---|---|
| **Ray generation** (RGen) | First, per ray-thread; emits primary rays |
| **Closest-hit** (CHit) | When a ray's closest hit is found |
| **Any-hit** (AHit) | For each hit considered (alpha-tested geometry) |
| **Miss** | When the ray misses everything |
| **Intersection** (Isect) | Custom primitive types (procedural geometry) |
| **Callable** | App-defined utility shaders, like function calls |

These compile into PSOs (DX) or pipeline objects (Vulkan) bundled with shader binding tables (SBTs) the GPU uses to dispatch the right shader for each hit.

## Inline ray tracing (a.k.a. "Ray Queries")

DXR 1.1 / Vulkan `VK_KHR_ray_query` allow ray traversal **inside** a regular pixel/compute shader without a full RT pipeline:

```hlsl
RayQuery<RAY_FLAG_NONE> rq;
rq.TraceRayInline(scene, ...);
while (rq.Proceed()) { ... }
if (rq.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
    /* shade */
}
```

Inline RT is preferred when you don't need many shaders or complex SBTs. AMD's hardware loves this — compiler embeds the traversal loop directly.

## BVH build

AS construction itself is GPU-accelerated:

- Driver/runtime calls into a BVH builder kernel (provided by AMD's runtime / Mesa's radv).
- Builder partitions geometry, computes AABBs, lays out nodes in BVH-format buffers.
- App sees an opaque BO holding the AS.

The BVH layout is **AMD-private** — apps must rebuild ASes on driver update if AMD changes the format. Mesa and AMD's official driver may have incompatible BVH layouts; that's why an AS built by one isn't usable by the other.

Build modes:

- **Build** — initial.
- **Update** — refit AABBs after small per-frame motion (cheap).
- **Compact** — make a smaller copy after build to save memory.

## Performance considerations

Ray tracing throughput depends on:

1. **BVH quality** — how well it minimizes ray-box overlap. SAH (Surface Area Heuristic) builders are slower but produce better BVHs.
2. **Coherence** — adjacent rays should mostly visit the same nodes. Random rays = thrash.
3. **Wave occupancy** — RT shaders tend to have high VGPR usage; spills hurt a lot.
4. **Hit shader divergence** — different lanes hit different materials → divergent shader execution.
5. **Cache locality** — BVH traversal is pointer-chase-y; L0/L1 hit rate matters.

PMTS engineers spend tens of thousands of hours collectively on these.

### Mitigations

- **Sort rays** before tracing (by direction or origin) to improve coherence.
- **Bin shading** — sort hits into bins by material, then dispatch coherent shading kernels. (NVIDIA's "SER" feature; partially software-emulated on AMD.)
- **Reduce VGPR pressure** in hit shaders.
- **Use ray queries** for simple cases (avoids shader-binding-table indirection).
- **Avoid AnyHit** if possible (each AnyHit invocation re-enters shader).

## Driver responsibilities

Kernel:

- Allocate BVH BOs.
- Map them with right MTYPE (uncached on writes from BVH-builder kernel that is, then cached for reads — actually usually fully cached).
- Provide enough VRAM headroom (BVHs are big).

UMD (Mesa, PAL):

- Compile RT shaders (ACO/LLVM) with traversal loop inlined.
- Build the SBT.
- Build BVHs — radv currently does this with its own builder; AMD's PAL has another. Quality varies.
- Manage AS update vs rebuild.

A real PMTS contribution is improving the BVH builder. radv's builder has gotten substantially better over the last 2 years through community contributions.

## Lumen, RTGI, path tracers

Real-world workloads using AMD RT:

- **Cyberpunk 2077** path-tracing mode (RDNA3/4 territory).
- **Alan Wake 2**, **Indiana Jones** — heavy RT.
- **Unreal Engine 5 Lumen** — uses inline RT for software-traced and hardware-traced flavors.
- **Blender Cycles** — production path tracer with HIP RT backend.

Knowing what these workloads stress (BVH update cost? Shader divergence?) is the day-to-day of RT-team engineers.

## What you should be able to do after this chapter

- Explain BVH and traversal.
- Describe AMD's hybrid ray-accelerator approach.
- List the DXR shader stages.
- Define inline RT vs RT pipeline.
- Identify performance traps (divergence, BVH quality, VGPR pressure).

## Read deeper

- AMD GPUOpen "Ray Tracing in DXR" series.
- `mesa/src/amd/vulkan/radv_acceleration_structure.c` — radv BVH builder.
- "Ray Tracing Gems" volumes 1 and 2 (free PDFs).
- Microsoft DXR spec.
- Khronos `VK_KHR_ray_tracing_pipeline` and `VK_KHR_ray_query` specs.
- PIX / RGP captures of any RT-enabled game.
