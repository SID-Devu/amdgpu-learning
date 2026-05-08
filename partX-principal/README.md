# Part X — The Senior / Principal / PMTS Track

> **You are competing with someone who has spent 24 years in this stack.** This part is the map of what they know.

This part is *not* a tutorial. It is a reference. Each chapter is a dense overview of an area where staff- and principal-level engineers spend years. You should:

1. **Read each chapter once for shape** — get the vocabulary, see the diagrams, know the parts.
2. **Pick one or two areas as your specialty** and go deep using the references in each chapter.
3. **Re-visit** the rest as you encounter the topics in your day job.

You will not master Part X in a year. **Nobody masters all of Part X.** The 24-year PMTS you're competing with is *deep* in 4–6 of these areas and *fluent* in the rest. That's the realistic target.

## Sections

### Hardware foundations (the silicon side)
| # | Chapter |
|---|---|
| 64 | [GPU microarchitecture: RDNA](64-microarch-rdna.md) |
| 65 | [GPU microarchitecture: CDNA & matrix engines](65-microarch-cdna.md) |
| 66 | [Wavefronts, occupancy, VGPRs/SGPRs](66-waves-occupancy-gprs.md) |
| 67 | [GPU memory subsystem: HBM, GDDR, controllers](67-memory-subsystem.md) |
| 68 | [GPU cache hierarchy & coherence](68-gpu-cache-hierarchy.md) |
| 69 | [PM4 packets, command processor, firmware engines](69-pm4-firmware.md) |

### The graphics pipeline, deeply
| # | Chapter |
|---|---|
| 70 | [Geometry pipeline: vertex → tess → geom → mesh shaders](70-geometry-pipeline.md) |
| 71 | [Rasterization, hierarchical Z, primitive culling](71-rasterization.md) |
| 72 | [Pixel pipeline: RBE, ROPs, blending, MSAA](72-pixel-rbe.md) |
| 73 | [Ray tracing hardware](73-ray-tracing.md) |

### Compute, runtime, queues
| # | Chapter |
|---|---|
| 74 | [KFD, HSA, AQL packets, MES](74-kfd-hsa-aql.md) |
| 75 | [CWSR, priorities, preemption](75-cwsr-preempt.md) |
| 76 | [ROCm runtime deep](76-rocm-runtime.md) |

### Display, multimedia, power
| # | Chapter |
|---|---|
| 77 | [Display Core (DCN) deep dive](77-display-core-dcn.md) |
| 78 | [Multimedia (VCN), codecs](78-multimedia-vcn.md) |
| 79 | [SMU, DPM, voltage/frequency, telemetry](79-smu-power.md) |

### Userspace stack deep
| # | Chapter |
|---|---|
| 80 | [Mesa, radv, ACO, NIR](80-mesa-radv-aco.md) |
| 81 | [LLVM AMDGPU backend](81-llvm-amdgpu-backend.md) |
| 82 | [Vulkan & DX12 deep](82-vulkan-dx12.md) |

### Driver advanced topics
| # | Chapter |
|---|---|
| 83 | [SR-IOV and GPU virtualization](83-sriov-virt.md) |
| 84 | [VM faults, page tables, recovery](84-vm-fault-recovery.md) |
| 85 | [Security: PSP, FW signing, attestation, SEV](85-security-psp.md) |

### Performance & debug at scale
| # | Chapter |
|---|---|
| 86 | [SQTT, RGP, perf counters, RGD](86-sqtt-rgp.md) |
| 87 | [Cross-stack debug & regression triage](87-cross-stack-debug.md) |

### Engineering at staff/principal level
| # | Chapter |
|---|---|
| 88 | [Customer / ISV / game studio interaction](88-customer-isv.md) |
| 89 | [Maintainership and upstream leadership](89-maintainership.md) |
| 90 | [Silicon bring-up, post-silicon validation](90-silicon-bringup.md) |
| 91 | [Cross-vendor architecture: NVIDIA, Apple, ARM, Imagination](91-cross-vendor.md) |
| 92 | [The PMTS career arc — the 24-year view](92-pmts-career.md) |

## How to use this part for FTE conversion

The realistic plan, given you are a fresh contractor:

| Year | Goal | Part X focus |
|---|---|---|
| Year 0–1 | Become a competent junior. Ship 5+ patches. | Read 64–69, 84, 86, 89 once. Don't try to internalize all of it. |
| Year 1–2 | FTE conversion. Specialize. | Pick **one** sub-area (e.g., GPUVM, or RBE, or display) and become *the person* on your team for it. Re-read its Part X chapters monthly. |
| Year 2–4 | Senior engineer. | Add a second specialty. Start reviewing patches. Cover 2–3 areas. |
| Year 4–8 | Staff engineer. | Cross-cut: lead a feature spanning 3+ subsystems. Cover 6–8 areas of Part X. |
| Year 8+ | Principal / PMTS. | Architecture, silicon influence, mentoring. Cover 10+ areas; deep in some you helped *design*. |

The 24-year PMTS started exactly the same way. Do not skip steps.

## What this part is not

- It's not a substitute for AMD's internal architecture documents (which are NDA-protected and 1000s of pages).
- It's not a tutorial — most chapters have minimal exercises.
- It's not exhaustive — some areas (e.g., specific HW codecs, PSP firmware internals) are intentionally surface-level because the knowledge is gated behind NDA.

It *is* the most useful pre-NDA map of "what you'll be expected to know" that exists outside AMD.

➡️ Start with [Chapter 64 — GPU microarchitecture: RDNA](64-microarch-rdna.md).
