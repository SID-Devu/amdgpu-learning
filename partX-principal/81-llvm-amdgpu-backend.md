# Chapter 81 — LLVM AMDGPU backend

LLVM is the compiler infrastructure used by Clang, Mesa (legacy radeonsi), and ROCm (HIP). Its **AMDGPU backend** generates AMDGCN/RDNA machine code from LLVM IR. Understanding it is required if you want to work on shader/compute compilers at AMD.

## Why both LLVM and ACO?

Mesa's `radv` uses ACO. ROCm/HIP uses LLVM. Two reasons LLVM still dominates compute:

1. **Massive infrastructure**: LLVM has decades of optimization passes (loop transforms, vectorizers, SLP, register allocators, SSA-based passes, autovec). Hard to reimplement.
2. **Cross-vendor flow**: HIP, OpenCL, OpenMP offload all use LLVM. Reusing the same backend simplifies development.

Graphics moved to ACO because Vulkan compile latency matters. Compute is throughput-bound; LLVM's slower compile is OK because kernels are AOT-compiled.

## LLVM source structure (AMDGPU side)

```
llvm-project/
└── llvm/lib/Target/AMDGPU/
    ├── AMDGPU.h                ← public-ish header
    ├── AMDGPU.td               ← TableGen — instruction definitions
    ├── AMDGPUSubtarget.cpp     ← per-chip features
    ├── SIInstructions.td       ← VOP, SOP, SMEM, MUBUF, etc. instructions
    ├── SIInstrInfo.cpp         ← instruction info
    ├── SIRegisterInfo.cpp      ← VGPR/SGPR allocation
    ├── SIShrinkInstructions.cpp ← compress 64-bit forms when possible
    ├── SIMemoryLegalizer.cpp   ← emits the right cache flush sequences
    ├── GCNHazardRecognizer.cpp ← inserts s_waitcnt and other waits
    ├── AMDGPUInstructionSelector.cpp
    ├── AMDGPULegalizerInfo.cpp
    ├── AMDGPULowerKernelArguments.cpp
    └── ...
```

Reading `SIInstructions.td` gives you the canonical list of ISA opcodes per RDNA/CDNA gen.

## Compile flow

LLVM IR (LL or BC) → AMDGPU backend → ELF object file containing AMDGCN code.

Stages:

1. **IR-level passes**: GVN, CSE, LICM, loop-unroll, autovec, AMDGPU-specific lowering.
2. **Instruction Selection (ISel)**: lowers LLVM IR to MIR (machine IR), with virtual registers.
3. **Register allocation**: assigns VGPRs/SGPRs.
4. **Scheduling**: orders instructions to minimize stalls (latency, RAW/WAW hazards).
5. **Hazard recognition**: inserts `s_waitcnt`, NOPs.
6. **Memory legalizer**: inserts `buffer_wbinvl1`, `s_dcache_wb`, etc., per memory model.
7. **Final emission**: produces ELF.

Each stage has dozens of files and configuration knobs.

## TableGen — the instruction database

`AMDGPU.td` and friends use **TableGen**, LLVM's declarative DSL, to describe:

- All instructions (mnemonics, encodings, operand types, latency, predicates).
- Calling conventions.
- Register classes.
- Patterns to match LLVM IR onto instructions during ISel.

A snippet of TableGen looks like:

```td
def V_ADD_F32_e32 : VOP2_Pseudo<"v_add_f32",
    (outs VGPR_32:$vdst),
    (ins VSrc_f32:$src0, VGPR_32:$src1)>;
```

The build runs `llvm-tblgen` to generate C++ from these. PMTS-level work often involves modifying `.td` files when a new ISA gen lands.

## Memory model handling

Compute kernels use C++ atomics with memory orders. LLVM's `SIMemoryLegalizer.cpp` translates these into AMD-specific sequences:

```
atomic load with acquire        → load + s_waitcnt + invalidate caches
atomic store with release       → wb caches + s_waitcnt + store
atomic_thread_fence(seq_cst)    → wb + invalidate + barrier
```

The exact sequence depends on the gfx target and memory scope. **Read this file once.** It is the operational definition of the AMD GPU memory model.

## Wave size handling

LLVM tracks wave size (`-mattr=+wavefrontsize32` or `+wavefrontsize64`) per function or module. It influences:

- Vector register width (effectively).
- DPP/permute lane patterns.
- Active-lane mask handling.
- Subgroup/wavefront-level intrinsics.

Compute kernels on RDNA can pick wave32 for short kernels with high parallelism, wave64 for long kernels with high register pressure. The HIP frontend sometimes guesses; explicit `__attribute__((amdgpu_waves_per_eu))` forces.

## Register allocation challenges

VGPRs are dynamically allocated per wave. The compiler picks the *minimum* count; the runtime tells the GPU. Allocating too few → spilling. Allocating too many → reduced occupancy.

LLVM uses a "sliding window" approach: try multiple register counts, pick the best estimated occupancy/perf trade.

Pathological cases:

- **Long basic blocks** with many live values → high VGPR pressure.
- **Loop-carried** values across many iterations.
- **Inline kernels with deep callees**.

Senior engineer work: re-reading allocator logs (`-Rpass=regalloc`), identifying spill culprits, restructuring kernels.

## Optimization levels

| Flag | What it does |
|---|---|
| `-O0` | no opts, slow code, fast compile, debuggable |
| `-O1` | basic |
| `-O2` | most game-relevant |
| `-O3` | aggressive (loop unroll, autovec) |
| `-Os` | size |
| `-Ofast` | `-O3 + -ffast-math` (relaxed FP semantics) |

Most production HIP code uses `-O3`. Be careful with `-ffast-math`: it can change results subtly and cause divergent output between platforms.

## AMDGCN-specific intrinsics

`__builtin_amdgcn_*` exposes raw ISA features. Examples:

- `__builtin_amdgcn_mbcnt_lo`, `_hi` — compute wave lane id.
- `__builtin_amdgcn_ds_bpermute` — DS permute (fast lane permute via LDS).
- `__builtin_amdgcn_mfma_*` — matrix engine.
- `__builtin_amdgcn_buffer_load_format_*` — typed buffer loads.
- `__builtin_amdgcn_image_*` — texture path.
- `__builtin_amdgcn_s_setreg_*` — set hardware control registers.

PMTS engineers use these regularly to wring every cycle out of a hot kernel.

## Code object layout

LLVM emits an ELF (an "HSACO" — HSA code object) with sections:

- `.text` — machine code.
- `.rodata` — constants.
- `.note.amd.amdgpu` — metadata (wave size, kernel descriptors, kernel argument layout).
- `.debug_*` — debug info (if `-g`).

`roc-obj-ls`, `llvm-readelf`, and `llvm-readobj` inspect HSACOs:

```bash
clang -x hip mykernel.hip -O3 -emit-static-lib -o mykernel.a
roc-obj-ls -V mykernel.a
roc-obj-extract -t gfx942 mykernel.a   # extract for one target
llvm-readobj --notes extracted.hsaco
```

## Multi-arch fat binaries

A HIP build typically targets multiple gfx archs in one binary (`gfx906;gfx908;gfx90a;gfx940;gfx942;gfx1030;gfx1100`). Each is a separate compilation; the runtime picks the right code object at load time based on the device.

The wrapper format is `clang_offload_bundle` magic. If you see weird "no compatible code object" errors, your binary doesn't include the running GPU's gfx target.

## ROCm-LLVM vs upstream LLVM

AMD ships its own LLVM fork with a few extra patches (newer mfma intrinsics, faster turn-around on backend changes). The patches eventually upstream. As a contributor: prefer upstream; AMD will pick it up.

## What you should be able to do after this chapter

- Identify the AMDGPU backend's location in LLVM.
- Define ISel, regalloc, scheduling, legalizer in one sentence each.
- Read TableGen at a glance.
- Use `__builtin_amdgcn_*` intrinsics deliberately.
- Reason about wave size, occupancy, spilling at the LLVM level.

## Read deeper

- LLVM source: `llvm/lib/Target/AMDGPU/`. **Just read it.**
- LLVM docs: `llvm.org/docs/AMDGPUUsage.html`.
- AMD blog: "Compiler optimization on AMDGPU."
- Talks at LLVM dev meeting on AMDGPU topics (yearly).
- `llvm-mc`, `llvm-objdump`, `llvm-readobj` man pages — use them.
