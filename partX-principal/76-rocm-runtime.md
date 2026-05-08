# Chapter 76 — ROCm runtime deep

ROCm is AMD's "CUDA equivalent" — a stack of libraries, runtimes, compilers, and tools that lets developers write and run GPU compute on AMD hardware. PMTS engineers in the compute team work somewhere in this stack.

## The big picture

```
Application: PyTorch / TensorFlow / JAX / Blender / DCC apps / scientific code
                  │
                  v
       ┌───────────────────────────┐
       │ Frameworks integration:    │  PyTorch ROCm backend, XLA on ROCm
       │ MIOpen, hipBLASLt, RCCL    │
       └─────────────┬──────────────┘
                     │
                     v
       ┌───────────────────────────┐
       │  HIP runtime               │  CUDA-API-shaped C++ for AMD
       │  (libamdhip64.so)          │
       └─────────────┬──────────────┘
                     │
                     v
       ┌───────────────────────────┐
       │  ROCr / HSA runtime        │  HSA agents, queues, signals
       │  (libhsa-runtime64.so)     │
       └─────────────┬──────────────┘
                     │
       Doorbells / KFD ioctls
                     │
                     v
       ┌───────────────────────────┐
       │  amdgpu kernel + amdkfd    │
       └─────────────┬──────────────┘
                     │
                     v
                  GPU silicon
```

Plus a forest of libraries:

| Library | Role |
|---|---|
| **rocBLAS** | dense linear algebra (GEMM, BLAS levels 1-3) |
| **hipBLASLt** | newer GEMM with offline-tuned solutions |
| **rocSPARSE** | sparse linear algebra |
| **rocFFT** | FFT |
| **rocSOLVER** | linear solvers (LU, QR, SVD) |
| **MIOpen** | DNN primitives (conv, pool, batchnorm) |
| **RCCL** | NCCL-shape collectives over xGMI/IB/PCIe |
| **Composable Kernel** | templated C++ kernel generators |
| **ROCgdb / rocprofiler-sdk / omniperf / omnitrace** | debug + perf tools |

## HIP — the CUDA-shaped API

HIP is C++ with kernel launch syntax very close to CUDA:

```cpp
__global__ void saxpy(float a, const float *x, float *y, int n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) y[i] += a * x[i];
}

int main() {
    float *dx, *dy;
    hipMalloc(&dx, N*sizeof(float));
    hipMalloc(&dy, N*sizeof(float));
    hipMemcpy(dx, hx, N*sizeof(float), hipMemcpyHostToDevice);
    saxpy<<<(N+255)/256, 256>>>(2.0f, dx, dy, N);
    hipMemcpy(hy, dy, N*sizeof(float), hipMemcpyDeviceToHost);
    hipFree(dx); hipFree(dy);
}
```

`hipify` is a tool that translates CUDA source to HIP source — usually an automated `s/cuda/hip/` plus a few semantic fixes. AI frameworks rely on this.

Key semantic differences vs CUDA:

- Wave size 64 (CDNA) or 32 (RDNA) — not always 32 like NVIDIA. Reductions/shfl semantics shift.
- No `__syncwarp` (use `__builtin_amdgcn_wave_barrier()` or HIP equivalents).
- Cooperative groups subset.
- Different driver APIs for stream/event semantics in some edge cases.

## hipcc and the compiler chain

```
*.hip → hipcc (driver) → clang (with -x hip)
                         → produces both host code (compiled normally)
                         and device code (compiled to AMDGCN)
                         linked into a fat binary

Device code path:
  *.hip → frontend (clang) → LLVM IR → AMDGPU backend → AMDGCN ISA
                                                      → ELF inside the host binary
```

At runtime, ROCr extracts the AMDGCN code object for the matching gfx target, loads it, hands a kernel object pointer to dispatch.

The AMDGPU backend is shared between graphics and compute (chapter 81).

## Streams and events

Like CUDA, HIP has streams (queues) and events (signals).

- A stream is an HSA queue.
- A default stream is created per-thread (or per-context, configurable).
- Events are HSA signals.
- `hipStreamSynchronize`, `hipEventRecord` map straight to HSA primitives.

Async memcpy uses SDMA queues internally — the runtime selects the right engine.

## Multi-GPU

Multi-GPU on AMD uses:

- **xGMI / Infinity Fabric** for in-package or in-board GPU-GPU links (high bandwidth).
- **PCIe peer-to-peer** for cross-board.
- **RDMA over IB / RoCE** for cross-node clusters.

**RCCL** (`librccl.so`) implements:

- AllReduce
- AllGather
- ReduceScatter
- Broadcast
- AlltoAll
- ReduceScatter / All-to-All-v
- Send/Recv (point-to-point)

It picks topology-aware algorithms (rings, trees) and stages buffers via SDMA. PyTorch DDP / DeepSpeed / Megatron all rely on it.

## hipBLASLt and rocBLAS

GEMM is the most-tuned kernel in the world. Both libraries ship pre-tuned solutions for many shapes, indexed by (M, N, K, dtype, layout, gfx-arch).

hipBLASLt adds an offline solution-tuning workflow: an app records its hot shapes, you run a tuner, the tuner picks per-shape MFMA configs, fused activations, persistent kernels — all stored in a tuning DB. Then production runs hit the tuned path.

When PyTorch on AMD beats expectations on a model, it's usually because hipBLASLt has the shapes already tuned.

## MIOpen

Equivalent of cuDNN. Convolution, pooling, batchnorm, dropout, RNNs. Has multiple convolution algorithms (direct, GEMM, Winograd, implicit-GEMM, FFT) and a chooser that picks per (input shape, filter shape, layout, dtype). Auto-tuning at first run + cache.

## Composable Kernel (CK)

A C++ template metaprogramming library. You instantiate templates of "an MFMA-based GEMM with these tile sizes, this fusion, this prologue/epilogue," and CK emits an optimized HIP kernel. Used by AMD library teams to scale tuning effort across many ops without writing N×M hand-kernels.

If you want to write a *fast* fused kernel and don't want to touch ISA, CK is the tool.

## PyTorch on ROCm

PyTorch's CUDA backend is replicated as a ROCm backend — same Python API, same `torch.cuda.*` (yes, really named "cuda" even on AMD for compatibility), but underneath calls HIP runtime, hipBLASLt, MIOpen, RCCL.

A ROCm-aware PyTorch wheel is provided by AMD or community. Major models work out of the box on MI300; performance for some small / unusual shapes may need hipBLASLt tuning.

## Triton, JAX, TF, XLA

- **Triton**: JIT compiles Python tile-language to GPU. AMD upstream support is improving each release; targets MFMA on CDNA.
- **JAX**: via XLA, which has an AMDGPU backend.
- **TF**: ROCm builds maintained.

PMTS-level engineers spend cycles ensuring these toolchains pick MFMA, avoid unnecessary copies, choose the right layouts, and don't fall off cliffs.

## Debugging compute

- **rocgdb**: GPU-side gdb. Stop a kernel mid-flight, inspect VGPRs, step.
- **rocprof / rocprofiler-sdk**: hardware counters per kernel.
- **omnitrace**: kernel timeline + CPU profile interleaved.
- **omniperf**: roofline, occupancy, cache hit views.

We dive into these in chapter 86.

## Production realities

- **Driver/firmware version coupling**: ROCm release N expects amdgpu kernel ≥ X and firmware ≥ Y. Mismatch = breakage. Documentation of compat matrices is critical.
- **Container support**: ROCm ships container images; users mount `/dev/kfd` and `/dev/dri/render*`. Permissions/cgroups matter.
- **Reset behavior**: a process hang that triggers GPU reset kills everyone on that GPU. Multi-tenant scheduling needs to handle this.

## What you should be able to do after this chapter

- Sketch the ROCm stack from app to kernel.
- Distinguish ROCr, HIP, hipBLASLt, MIOpen, RCCL.
- Explain how a HIP kernel becomes machine code.
- Outline RCCL's role.
- Know which tools to reach for when a workload is slow.

## Read deeper

- ROCm documentation (rocm.docs.amd.com) — tutorial-quality.
- ROCr-Runtime, HIP, hipBLASLt, MIOpen, RCCL repos on github.com/ROCm.
- Composable Kernel readme + a couple of sample kernels.
- AMD blog "ROCm fundamentals" series.
- Latest MI300X performance papers and HotChips talks.
