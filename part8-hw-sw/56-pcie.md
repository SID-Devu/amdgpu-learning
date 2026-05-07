# Chapter 56 — PCIe deep dive

Almost every interview at AMD/NVIDIA/Qualcomm asks about PCIe. We took a tour in Chapter 37; here we go deep enough to discuss bandwidth, ordering, and pitfalls.

## 56.1 PCIe is a layered packet protocol

```
┌──────────────────────────┐
│  Software API            │   pci_dev, dma_*
├──────────────────────────┤
│  Transaction Layer       │   TLPs (Transaction Layer Packets)
├──────────────────────────┤
│  Data Link Layer         │   reliability, DLLPs, ACK/NAK, flow ctrl
├──────────────────────────┤
│  Physical Layer          │   8b/10b or 128b/130b, lanes
└──────────────────────────┘
```

A PCIe transaction is a **TLP**: header + optional payload, addressed to an endpoint by Bus/Device/Function or by memory address.

## 56.2 Lanes and generations

Bandwidth per lane (per direction):

| Gen | Encoding | Raw GT/s | Effective GB/s/lane |
|---|---|---|---|
| 1 | 8b/10b | 2.5 | 0.25 |
| 2 | 8b/10b | 5.0 | 0.5 |
| 3 | 128b/130b | 8.0 | 0.985 |
| 4 | 128b/130b | 16.0 | 1.97 |
| 5 | 128b/130b | 32.0 | 3.94 |
| 6 | PAM4+FEC | 64.0 | 7.56 |
| 7 | PAM4+FEC | 128.0 | 15.13 |

Cards advertise width (x1/x2/x4/x8/x16). Aggregate = lanes × per-lane.

A modern GPU is x16 PCIe Gen4 or Gen5: ~32 GB/s or ~63 GB/s per direction.

`lspci -vv -s 01:00.0 | grep -E 'LnkSta|LnkCap'` reports actual vs. capable.

## 56.3 TLP types

- **Memory read/write** (MRd / MWr).
- **I/O read/write** (legacy x86 port I/O, rare today).
- **Configuration read/write** (cfgrd0/cfgwr0).
- **Message** (interrupts, errors, hot-plug).
- **Completion** for non-posted requests.

Most GPU traffic is MWr (descriptor + payload writes from GPU to host RAM, doorbell writes from CPU to GPU) and MRd (GPU fetching commands, fences).

## 56.4 Posted vs. non-posted

- **Posted**: write goes, no completion expected. Includes Memory writes.
- **Non-posted**: requires a completion. Reads (memory and config) and config writes.

A read pushes posted writes through ordering rules → fence semantics on PCIe.

## 56.5 Ordering rules (briefly)

- Within a TC (Traffic Class), strong ordering by default for posted-vs-posted: writes don't pass writes.
- Reads can pass writes only with **Relaxed Ordering** bit set.
- Completions are unordered with each other in some cases.

GPU drivers assume default (strong) ordering. RO is rarely used.

## 56.6 BARs

Each device has up to 6 BARs declaring address windows:

```
BAR[i]:
  bit 0: 0=memory, 1=I/O
  bit 1-2: 64-bit (10) or 32-bit (00)
  bit 3: prefetchable (1) — write-combinable
  bits 4+: address (read after writing 1s gives size mask)
```

GPU has typically:

- BAR0 (Memory, prefetchable, large): VRAM aperture (or part of it).
- BAR1: doorbell/MMIO mix (varies by gen).
- BAR2: registers.
- BAR5: ROM.

Resizable BAR (ReBAR / SAM): firmware tells the chipset to allocate a 16 GB BAR0 on systems supporting it; the CPU can map all of VRAM.

## 56.7 ATS, PRI, ACS

- **ATS (Address Translation Service)**: device caches address translations. Reduces IOMMU pressure for GPUs.
- **PRI (Page Request Interface)**: device asks the OS to handle a page fault. Enables SVM.
- **ACS (Access Control Services)**: prevents peer-to-peer DMA between devices unless OS allows.

amdgpu, with KFD, uses ATS+PRI for SVM on platforms that support it. ACS interacts with VFIO IOMMU groups (security boundary).

## 56.8 Speed / latency

Round-trip MMIO read latency over PCIe is ~hundreds of ns to ~1 µs depending on platform. **Avoid hot-path reads.** Writes are posted, much faster.

DMA for bulk transfer is ~1 µs setup, then GB/s steady-state. Doorbells are µs-scale.

## 56.9 Common PCIe issues

- **Link not at full speed**: BIOS, PCIe re-training, faulty cable/slot. Diagnose with `lspci -vv`.
- **Hot-plug failure**: ACS, PCIe slot config. Linux's `setpci` and DSDT tweaks help.
- **DMA mask too restrictive**: SWIOTLB engaged, slowness.
- **Bandwidth-bound workload**: PCIe gen too low, too few lanes.

## 56.10 Multi-GPU and P2P

Two GPUs on the same root complex can DMA directly between each other (peer-to-peer) without going through host RAM. amdgpu supports P2P DMA with explicit setup (`amdgpu_p2p` flags). Used by RCCL for multi-GPU collectives.

ACS interferes (it forces P2P traffic through the root complex). On servers, BIOS has knobs to disable ACS for performance, accepting reduced isolation.

## 56.11 Reading a real GPU's PCIe state

```
lspci -vv -s 01:00.0
01:00.0 VGA compatible controller: Advanced Micro Devices, Inc. [AMD/ATI] Navi 31 [Radeon RX 7900 XTX] (rev c8) (prog-if 00 [VGA controller])
    Subsystem: Sapphire Technology Limited Navi 31 [Radeon RX 7900 XTX]
    Flags: bus master, fast devsel, latency 0, IRQ 156
    Memory at 7c000000000 (64-bit, prefetchable) [size=16G]   ← VRAM via ReBAR
    Memory at 7d000000000 (64-bit, prefetchable) [size=2M]
    I/O ports at 1000 [size=256]
    Memory at fcc00000 (32-bit, non-prefetchable) [size=512K] ← MMIO regs
    Capabilities: [48] Vendor Specific Information
    Capabilities: [50] Power Management version 3
    Capabilities: [58] Express (v2) Endpoint, MSI 00
    LnkSta: Speed 16GT/s, Width x16
    Capabilities: [a0] MSI: Enable+ Count=1/1 Maskable+ 64bit+
    Capabilities: [c0] MSI-X: Enable+ Count=8 Masked-
    Capabilities: [120] AER: ...
    Capabilities: [200] Resizable BAR <?>
    Kernel driver in use: amdgpu
```

Read every line. Each capability has a code path in the kernel.

## 56.12 Exercises

1. `lspci -vv` on your machine. Identify each capability for your GPU.
2. `cat /proc/iomem | grep amdgpu`. Identify each BAR.
3. Read `Documentation/PCI/pcieaer-howto.rst` (Advanced Error Reporting).
4. Read `drivers/pci/setup-bus.c::pci_resize_resource` (ReBAR).
5. With `pcm-pcie` (Intel; runs on AMD too) measure GPU PCIe bandwidth under a workload.

---

Next: [Chapter 57 — Performance: profiling, flamegraphs, GPU profilers](57-performance.md).
