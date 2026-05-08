# Chapter 80 — Mesa, radv, ACO, NIR

If the kernel is half of an AMD GPU stack, **Mesa** is the other half. It's the open-source OpenGL/Vulkan/OpenCL implementation. For AMD, the marquee components are **`radeonsi`** (OpenGL), **`radv`** (Vulkan), and **ACO** (a hand-rolled compiler). PMTS engineers in graphics live here.

## Mesa structure (quick map)

```
mesa/
├── src/
│   ├── compiler/
│   │   ├── glsl/        ← GLSL frontend
│   │   ├── nir/         ← NIR (Mesa's portable IR) — main IR
│   │   └── spirv/       ← SPIR-V frontend
│   ├── amd/
│   │   ├── common/      ← shared AMD code (NIR lowering, surfaces, AC reg model)
│   │   ├── compiler/    ← ACO compiler
│   │   ├── llvm/        ← LLVM AMDGPU backend frontend (alternative to ACO)
│   │   ├── vulkan/      ← radv (Vulkan)
│   │   └── ci/          ← per-target test rules
│   ├── gallium/
│   │   ├── drivers/
│   │   │   ├── radeonsi/   ← OpenGL/EGL/OpenCL on AMD
│   │   │   └── ...
│   │   ├── frontends/      ← VAAPI, VDPAU, OpenCL frontends
│   │   └── winsys/amdgpu/  ← libdrm_amdgpu glue
│   └── intel/, broadcom/, freedreno/, etc.
```

The split: `gallium` is a state-tracker framework for OpenGL/OpenCL drivers; `radv` is a standalone Vulkan driver. Both call into `src/amd/common/` and the compiler.

## NIR — Mesa's IR

**NIR (NIR Intermediate Representation)** is Mesa's portable shader IR, used by every driver. Frontends (GLSL, SPIR-V, HIPCC) emit NIR; backends consume it.

NIR features:

- SSA form for vector and scalar values.
- Block-structured control flow (`if/loop/break/continue`).
- Generic intrinsics for things like `image_load`, `barrier`, `vote`.
- Lowering passes: convert high-level ops into target-friendly ops.

Key NIR passes for AMD:

- `nir_lower_io`: split stage outputs/inputs into per-component locations.
- `nir_lower_subgroups`: implement subgroup ops (ballot, shuffle, vote).
- `nir_opt_dce`, `nir_opt_cse`, `nir_opt_loop_unroll` — generic optimizations.
- `ac_nir_lower_ngg`: AMD-specific NGG lowering.
- `ac_nir_lower_resinfo`, `ac_nir_lower_image`: AMD-specific image-fetch lowering.

ACO (and the LLVM backend) consume the final NIR and produce machine code.

## ACO — the AMD-compute optimizer

ACO is a from-scratch shader compiler written for AMD targets. Started as a community/alternative to LLVM (which has a huge surface area and slower compile). ACO focuses on:

- Fast compile time.
- Good register allocation, especially for wave32.
- AMD-specific IR (`aco_ir.h`).
- Generates AMDGCN/RDNA ISA directly (no LLVM dependency).

ACO is the default for radv. The LLVM backend remains as a fallback (and is mandatory for some cases — e.g., RT shaders, which require complex CFG transforms ACO doesn't yet have).

### ACO IR

ACO's IR sits between NIR and AMDGCN ISA. Each instruction is one of:

- **VOPx** (vector op forms): `v_add_f32`, `v_mul_f32`, `v_mfma_*`, etc.
- **SOPx** (scalar op forms).
- **SMEM, MUBUF, MIMG, FLAT** load/store forms.
- **EXP** (export — vertex outputs / pixel outputs).
- Pseudo-ops for spills, copies, parallel-copies during regalloc.

Compile pipeline (simplified):

```
NIR → instruction selection → live-range analysis → register allocation
    → scheduling → assembly → ELF
```

### Why ACO matters

- 30–50% faster compile times than LLVM in many cases (matters for shader compile stutter in games).
- Can produce equally good or better code on common shader shapes.
- AMD-aware — knows about wave size, NGG, MFMA, mode bits.
- Easier to extend by Mesa contributors than LLVM.

PMTS-level work in ACO: improving regalloc, adding new MFMA forms, lowering new Vulkan extensions, fixing pathological shaders.

## radv — Vulkan on AMD

radv implements the Vulkan API:

- `vkCreateDevice` etc. → opens `/dev/dri/renderD*`.
- Pipeline state objects (PSOs) → compile + layout shaders into ISA.
- Descriptor sets → AMD descriptor format (image/sampler/SSBO descriptor structs in 32-byte blobs).
- Command buffers → assemble PM4 streams.
- Submit → `amdgpu_cs_submit` ioctl.
- Queues, fences, semaphores → drm_syncobj / dma_fence.

radv is impressively fast. Several AAA games run *better* on radv than on AMDVLK (AMD's official Vulkan), because radv's compiler and lower-overhead command-buffer building win on workloads that hammer the API.

### radv source

| File | Role |
|---|---|
| `radv_device.c` | device init, capability query |
| `radv_pipeline*.c` | pipeline (PSO) creation, shader compile entry |
| `radv_cmd_buffer.c` | recording PM4 |
| `radv_descriptor_set.c` | descriptor management |
| `radv_meta_*.c` | "meta" passes for clears, decompresses |
| `radv_image.c` | DCC/HTILE/CMASK setup |
| `radv_acceleration_structure.c` | RT BVH builder |

## radeonsi — OpenGL on AMD

radeonsi implements OpenGL 4.6, OpenGL ES, EGL on top of Gallium. Compiler = LLVM AMDGPU backend (older code path; some pieces use ACO via `amd/llvm`).

OpenGL is much harder to make fast than Vulkan because of API state changes and validation. radeonsi has years of legacy and is genuinely impressive code.

## OpenCL on Mesa: rusticl

The new OpenCL frontend, written in Rust. Replaces the old `clover`. Uses NIR → ACO/LLVM. Aimed at filling the OpenCL gap for users who don't want full ROCm.

## Compositor / WSI

**WSI (Window System Integration)** binds Vulkan/EGL outputs to Wayland or X11 surfaces. Mesa has WSI code in `src/vulkan/wsi/` (Vulkan WSI extensions) and EGL platforms.

WSI cooperates with KMS atomic on the kernel side. Direct scan-out from Vulkan to KMS planes (no compositor copy) is supported and increasingly used.

## How a draw call becomes silicon (Mesa view)

```
1. App: vkCmdBindPipeline + vkCmdDraw
2. radv: append PM4 dwords to a command buffer BO
   - SET_CONTEXT_REG for blend, depth, viewport
   - SET_SH_REG for shader resources (descriptor sets)
   - DRAW_INDEX_2 packet
3. App: vkQueueSubmit
4. radv: submit BO list + IB to amdgpu_cs ioctl
5. Kernel: validate, schedule via DRM scheduler
6. CP runs IB on the silicon
```

PMTS-level optimization: minimize the work in step 2. Cache state vectors, hash them, reuse precomputed PM4 sequences. radv has many such tricks (`radv_emit_*` functions reuse cached state).

## Testing

Mesa CI runs:

- `dEQP-VK` and `dEQP-GLES`: Khronos test suites (10s of thousands of tests).
- `piglit`: OpenGL test suite.
- `crucible`: Vulkan-specific tests.
- Game replay (Chrome's `vk_replay`).

CI runs on real AMD hardware in shared test farms (FreeDesktop CI). Failing a CI run blocks merge.

## How to contribute

1. Find a small bug — check Mesa's GitLab issue tracker, filter `radv` label.
2. Clone Mesa: `git clone https://gitlab.freedesktop.org/mesa/mesa.git`.
3. Build: `meson setup build -Dvulkan-drivers=amd -Dgallium-drivers=radeonsi`; `ninja -C build`.
4. Reproduce, fix, test (run dEQP locally for the affected feature).
5. Submit MR on GitLab. Be ready for review iterations.

Mesa is one of the most welcoming open-source communities in graphics. Junior contributions land regularly.

## What you should be able to do after this chapter

- Sketch the Mesa source tree.
- Define NIR, ACO, LLVM AMDGPU backend.
- Distinguish radv, radeonsi, rusticl.
- Describe a draw-call's path from API to ISA.
- Know where to find a specific feature (RT, DCC, descriptors).

## Read deeper

- Mesa source (you must skim this).
- Bas Nieuwenhuizen's blog on radv internals.
- Connor Abbott's writings on ACO design.
- "Mesa 3D: Behind the scenes" FOSDEM talks (multiple years).
- `mesa/docs/` — surprisingly good architecture docs.
