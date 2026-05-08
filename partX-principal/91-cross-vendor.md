# Chapter 91 — Cross-vendor architecture: NVIDIA, Apple, ARM, Imagination

You're competing not just with peers at AMD but with engineers at every other GPU vendor. The world is small; people move between companies; the same engineering ideas pop up in different costumes.

PMTS-level engineers are fluent in *all* major GPU architectures. They read each company's whitepapers when they're released, attend HotChips talks, and know the differences cold.

## The roster

| Vendor | Lines | Where it dominates |
|---|---|---|
| **NVIDIA** | GeForce, RTX, A/H/B series datacenter, Tegra | gaming high-end, AI, autonomous |
| **AMD** | Radeon, Instinct | gaming, datacenter AI (rising), HPC |
| **Intel** | Arc (dGPU), Xe iGPU, Habana | iGPU, attempting dGPU & AI |
| **Apple** | M-series GPUs | Mac platform; tightly coupled to OS |
| **ARM Mali** | Cortex-coupled mobile | Android phones, embedded |
| **Imagination Tech (PowerVR)** | embedded, automotive | tile-based mobile/embedded |
| **Qualcomm Adreno** | Snapdragon mobile | Android flagship phones |
| **Samsung Xclipse** | based on AMD RDNA IP | Samsung phones |
| **Huawei Ascend / GPU** | datacenter | China internal |
| **MediaTek** | various | mobile |
| **Cerebras / SambaNova / Groq / Tenstorrent** | non-GPU AI | training, inference accelerators |

## NVIDIA — the standard you measure against

NVIDIA's GPU architecture vocabulary:

| AMD term | NVIDIA term |
|---|---|
| Compute Unit (CU) / WGP | **Streaming Multiprocessor (SM)** |
| Wavefront (32 or 64) | **Warp** (32, fixed) |
| VGPR / SGPR | (combined) registers per SM |
| LDS | **Shared Memory** |
| L0/L1 vector $ | L1 cache (per SM) |
| L2 $ | L2 cache (chip-wide) |
| Infinity Cache | (none — but "L2 large" on H100/B100 is similar) |
| HBM | HBM (same standard) |
| MFMA / matrix engine | **Tensor Core** |
| Ray-accelerator | **RT Core** |
| MES | **GSP (GPU System Processor)** firmware |
| KFD / HSA | **CUDA driver / kernel module** |
| AQL packet | **CUDA stream commands** |
| ROCm | **CUDA / cuDNN / cuBLAS / NCCL** |
| Mesa radv | **NVIDIA Vulkan driver** (closed) |
| amdgpu kernel | **nvidia.ko** (closed; nouveau open) |
| Open kernel modules | recently NVIDIA opened a "GSP-only" kernel module |

NVIDIA strengths:

- Tensor Cores are extremely well integrated; cuBLAS/cuDNN have decade+ optimization.
- CUDA ecosystem mindshare is unmatched.
- NVLink for multi-GPU.
- Early to RT Cores.
- Stable, mature drivers (closed-source).
- Massive software investment in ML frameworks.

NVIDIA weaknesses (from AMD's POV):

- Closed-source drivers; harder to debug for users.
- Per-GPU pricing high; ROI argument open.
- Power efficiency story shifting as AMD catches up.
- Vendor-lockin frustrates the open-source ecosystem.

## Apple — vertical integration

Apple GPUs (in M-series chips) are **TBDR (tile-based deferred renderers)** with custom ISA. Tightly integrated with macOS / Metal / Apple's compilers. Strengths:

- Unified memory (CPU + GPU share LPDDR5X with hardware coherence).
- Excellent power efficiency.
- Metal API designed alongside hardware → minimal overhead.
- Apple Silicon GPUs scale from 8-core (laptop) to ~80-core (Ultra).

Weakness: locked to Apple platform; no Linux support (though Asahi project is making progress); no AI training-class part yet.

Architecture insight: TBDR partitions the screen into tiles. All draws are sorted by tile. Within a tile, depth is determined first, then only visible pixels are shaded. Saves bandwidth massively on overdraw-heavy scenes (mobile games).

## ARM Mali

Mali GPUs (in mobile SoCs from Mediatek, Samsung Exynos, Google Tensor) are TBDR (Bifrost / Valhall / 5th-gen architectures). Open-source Linux driver available (Panfrost, then Panthor for Valhall+). Performance per watt is good; raw compute lower than desktop.

Architecture quirks:

- Quad-based execution (every "core" runs 4-wide vec).
- Cooperative caching across cores.
- Tile memory exposed to shaders for transient data.

ARM is also a key partner — they license the architecture, and AMD competes/coexists in the Android space.

## Imagination Tech (PowerVR)

The original tile-based renderer (1990s onwards). PowerVR Series-A through Series-9X. Used in early iPhones, Apple drew heavy inspiration. Now mostly automotive and embedded. Open-source drivers via "PVRSGX" and modern "powervr" kernel driver.

## Qualcomm Adreno

Mobile flagship. Forked from the AMD/ATI XGS — historical link! Adreno 600/700/800 series. Mostly closed userland; Mesa's `freedreno` is the open-source Vulkan/OpenGL driver, and very good. Architecture: tile-based with some unique tricks (low-power "bucket" mode for video).

## Intel Arc / Xe

Intel's serious dGPU push. Open-source drivers (`xe` kernel driver, Mesa `iris` and `anv` Vulkan). Architecture: Xe Cores → Vector Engines + Matrix Engines (XMX). Matrix engines do BF16/INT8 matrix.

Intel's open-source story is excellent — they upstream aggressively. Their performance has improved release-over-release. Watch them.

## How a PMTS uses cross-vendor knowledge

- **Standards work**: in Khronos Vulkan committee, you negotiate with NVIDIA's representatives to design extensions that work for both. Knowing their architecture's idiosyncrasies makes you a better negotiator.
- **Borrowing ideas**: NVIDIA's Tensor Cores were the public proof that matrix engines were the way; AMD's MFMA followed. NVIDIA's RT Cores; AMD's RA. The industry moves together.
- **Recruitment**: many AMD seniors are ex-NVIDIA, ex-Intel, ex-Apple. The conversation shifts when you understand where they came from.
- **Customer comparisons**: a customer says "NVIDIA does X better." You need to know whether they're right and what to do about it.

## Reading whitepapers

Every gen, vendors release architecture whitepapers (free PDFs):

- AMD: GPUOpen "RDNA 1/2/3/4 architecture," "CDNA 1/2/3."
- NVIDIA: "Volta," "Ampere," "Hopper," "Blackwell" architecture whitepapers.
- Apple: M-series HotChips talks (no full whitepaper; reverse-engineered details by Asahi team).
- Intel: Xe architecture whitepapers.

Read every one in your area when it appears. Compare. Notice the trade-offs each vendor made.

## Industry standards work

Khronos:

- **Vulkan** specification working group.
- **OpenCL** WG.
- **SPIR-V** intermediate language.
- **glTF** asset format.

PCI-SIG:

- PCIe spec.
- CXL (Compute Express Link).

VESA:

- DisplayPort versions (1.4, 2.0, 2.1).
- AdaptiveSync (the basis for FreeSync / G-Sync compatibility).

PMTS engineers from AMD attend, contribute proposals, vote. Junior engineers attend a couple of sessions to learn how decisions get made.

## What you should be able to do after this chapter

- Translate AMD vocabulary to NVIDIA / Apple / ARM equivalents.
- Identify each vendor's strengths and weaknesses.
- Read a competitor whitepaper and summarize architecture for your team.
- Know which standards bodies shape the industry.
- Recognize ex-vendor colleagues by their approach to problems.

## Read deeper

- HotChips proceedings (every year, free PDF) — the gold standard.
- ChipsAndCheese.com — independent chip analyses.
- Asahi Linux project blog — Apple GPU reverse engineering.
- Mesa project — has drivers for almost every vendor; reading them comparatively is enlightening.
- Khronos Vulkan extension list — see who pushes which features.
