# Chapter 65 — GPU microarchitecture: CDNA & matrix engines

CDNA (Compute DNA) is AMD's *compute-only* GPU line — the **Instinct** accelerators. No display, no RBE, no ray tracing units. Everything that gets cut goes back into more compute, more memory bandwidth, and **matrix engines** for AI.

If you work on consumer graphics, CDNA is "the cousin." If you work on data center, AI, or HPC, CDNA is your daily life.

## How CDNA differs from RDNA

| Feature | RDNA | CDNA |
|---|---|---|
| Target | Gaming, content creation, mobile | HPC, AI training/inference |
| Display block (DCN) | yes | **no** |
| RBE / blending | yes | **no** |
| Ray-tracing accel | yes (RDNA2+) | no |
| Wave size | wave32 default, wave64 supported | wave64 native (uses 4×SIMD16) |
| Core unit | WGP (paired CUs) | CU |
| Matrix engine | small AI accel (RDNA3+) | **MFMA / large MFMA** is the marquee feature |
| Memory | GDDR / Infinity Cache | HBM2/HBM2e/HBM3 |
| Form factor | PCIe card, APU | OAM module, MI series PCIe |
| Driver path | amdgpu (DRM/GFX rings) | amdgpu + KFD (HSA queues) |

CDNA is closer to the GCN ancestor in many ways (wave64, 4×SIMD16) than to RDNA. Mental model: CDNA = "GCN with matrix engines bolted on, and HBM."

## CDNA lineage

| Gen | Architecture | Products | Headline |
|---|---|---|---|
| Pre-CDNA | Vega 20 / GCN5 | MI50, MI60 (2018) | First matrix accel (V_DOT, packed FP16) |
| CDNA 1 | Arcturus | MI100 (2020) | First true matrix engine (MFMA), no display |
| CDNA 2 | Aldebaran | MI200 (MI210/250/250X) | Multi-die (2 GCDs), Infinity Fabric, FP64 matrix |
| CDNA 3 | Aldebaran successor | MI300 series (MI300X, MI300A) | APU+GPU package, 8-die, HBM3, sparse FP8 |
| CDNA 4+ | future | MI350+ | even bigger AI focus |

## The matrix engine: MFMA

The single biggest CDNA feature is the **MFMA (Matrix Fused Multiply-Add)** family of instructions. They compute a small dense matrix product C += A × B in *one* hardware instruction.

```
v_mfma_f32_16x16x16f16   v[acc], v[a], v[b], v[acc]
```

Reads:

- `v[a]` is a 16×16 block of FP16 values (distributed across the wave's lanes).
- `v[b]` is a 16×16 block of FP16.
- Multiplies into a 16×16 FP32 accumulator.
- Adds into `v[acc]` and writes back.

A wave executing this in parallel keeps the matrix multipliers fed for many cycles. **This is the engine of every modern AI workload.**

Variants: different shapes (16×16×16, 32×32×8), data types (FP16, BF16, FP32, FP64, INT8, INT4, FP8 on MI300+), accumulation widths.

You don't write MFMA by hand often. You either:

- Use **rocBLAS / rocSPARSE / rocFFT / MIOpen / hipBLASLt** which ship hand-tuned kernels.
- Use **Composable Kernel** (CK) — AMD's templated C++ library that emits MFMA-laden HIP kernels.
- Write HIP and let the compiler pick MFMA via intrinsics or pattern matching.
- Build a kernel in Triton or PyTorch's compiler, which emit MFMA via ROCm.

### Sparse MFMA (CDNA 3)

MI300 introduced 2:4 structured sparsity — if 2 of every 4 weights are zero, the matrix engine can skip them and double effective throughput. Critical for AI inference.

## Memory subsystem on CDNA

- **HBM2/2e/3** stacked DRAM, gigabytes-wide. ~1.5 to 5+ TB/s on modern parts.
- **Infinity Fabric** between dies on multi-die parts.
- **xGMI** links between GPUs (8 GPUs per host, all-to-all on flagship platforms).
- No Infinity Cache on CDNA. The HBM bandwidth is high enough.

Multi-GPU is *the* CDNA topic. RCCL (the AMD analog of NCCL) does collectives — all-reduce, broadcast, all-gather — over xGMI rings/trees.

## Software differences vs RDNA

### KFD path

CDNA workloads almost always go through **KFD** (`/dev/kfd`) and the HSA runtime. User-mode queues, AQL packets, MES scheduling. See chapter 74.

Graphics drivers (`/dev/dri/card*`) are still there but rarely used on a headless MI300.

### Compiler

LLVM AMDGPU backend has separate code generation paths for `gfx9*` (CDNA1/2), `gfx940/942` (CDNA 3 family), `gfx1030+` (RDNA2/3). MFMA intrinsics live behind `__builtin_amdgcn_mfma_*`. When you write HIP, the compiler decides; when you write inline GCN/RDNA assembly, you use the actual mnemonic.

### Tools

- `rocm-smi` — telemetry (clock, voltage, temp, ECC).
- `rocprofiler-sdk` / `rocprof` — perf counters, kernel traces.
- `omniperf`, `omnitrace` — higher-level analysis tools.
- `rocgdb` — GPU debugging (single-stepping wavefronts!).

We cover these in chapter 86.

## Multi-die, chiplets, packaging (CDNA 2 onward)

MI200 was AMD's first multi-die compute GPU: two **GCDs** (Graphics Compute Dies) connected by an Infinity Fabric link. Software sees them either as two separate devices ("HOST" mode) or one logical device with NUMA-style memory access ("XGMI" mode). Both modes have their bug surface area.

MI300X is even more aggressive: 8 compute chiplets ("XCDs") + 4 I/O dies + 8 HBM stacks on one package. MI300A adds a CPU chiplet (Zen 4) sharing the same HBM coherently — a true APU at data-center scale.

The driver implications:

- VRAM is split across HBM stacks; allocator must NUMA-balance.
- Cross-die latency matters; placement affects perf.
- Reset domains: a single XCD can reset, others stay alive (helpful for cluster reliability).

## ECC

CDNA always has ECC on HBM and key SRAMs (vs RDNA which often doesn't). The RAS subsystem (chapter 53) reports correctable errors (CE) and uncorrectable errors (UE), with EEPROM-backed bad-page tables.

## What you should be able to do after this chapter

- Tell apart RDNA and CDNA in one sentence each.
- Explain what MFMA does and why it dominates AI throughput.
- Sketch an MI300 package: chiplets, HBM stacks, fabric.
- Name three RCCL collectives and roughly when each is used.
- Say what "sparse 2:4" means and which gen introduced it.

## Read deeper

- AMD CDNA whitepapers (CDNA1, CDNA2, CDNA3) — public.
- MI300 architecture deck (HotChips 2023 talk is excellent).
- LLVM AMDGPU backend `lib/Target/AMDGPU/AMDGPUGenSubtargetInfo.inc` (autogen) shows feature flags per gfx target.
- Composable Kernel (`github.com/ROCm/composable_kernel`) — read a couple of kernel templates to see how MFMA gets emitted.
- ROCm RCCL repository.
