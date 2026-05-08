# Chapter 83 — SR-IOV and GPU virtualization

In data centers and cloud, one physical GPU often serves many tenants. **SR-IOV (Single-Root I/O Virtualization)** is the PCIe standard that lets a single GPU expose multiple **virtual functions (VFs)**, each appearing as an independent device to a virtual machine. AMD's GPUs implement SR-IOV in firmware + hardware + a sophisticated kernel/host driver split.

PMTS engineers in the cloud team and the security team live in this chapter.

## The problem

A bare-metal driver assumes it owns the GPU. A cloud customer wants:

- 8 customers sharing one MI300, each in their own VM.
- Strong isolation: customer A can't see customer B's framebuffer or VRAM.
- Performance: near-native, not "10× slower because everything is paravirtualized."
- Live migration support (in some scenarios).

SR-IOV is the answer. The hardware exposes a **PF (physical function)** that the **host hypervisor** drives, plus N **VFs**, one per VM.

## PCIe-level model

```
            Host hypervisor (KVM / Hyper-V / VMware)
                       │
                       │ PCIe config space access
                       v
           +---------------------------------+
           | GPU (one PCI device)            |
           | +-----------------------------+ |
           | | PF: BDF 03:00.0             | |  driven by host PF driver
           | | VF: BDF 03:00.1, 03:00.2,...| |  driven by guest VF driver
           | +-----------------------------+ |
           +---------------------------------+
```

Each VF has:

- Its own PCIe BDF.
- Its own MSI-X vectors.
- Its own BAR aperture (mapped from a slice of the PF's resources).
- Its own VMID for GPU work.

VFs use a **subset** of the GPU's resources — partitioned engines, partitioned VRAM, partitioned compute units. The host PF driver decides the partitioning.

## Host vs guest drivers

In `amdgpu`:

```
adev->virt.caps == AMDGPU_SRIOV_CAPS_*
adev->virt.ops  == per-PF or per-VF callback table
```

The same driver compiles for both modes; runtime detection switches behavior.

**Host (PF) driver**:

- Initializes hardware fully.
- Allocates VRAM/GFX time slices to VFs.
- Mediates access to shared resources (DCN display, video encode pools).
- Listens for VF faults / health.
- Manages reset of individual VFs without disturbing others.

**Guest (VF) driver**:

- Cannot do many init operations (PSP boot, SMU programming).
- Talks to the PF driver via the **MailBox** interface — a small register-level RPC.
- Asks the PF for resources (memory, doorbell slots) when it can't allocate directly.

## The Mailbox

VF↔PF communication goes through MMIO mailbox registers. Protocol is essentially:

```
VF writes: command, arg, "request"
PF interrupts, reads, services
PF writes: result, "complete"
VF reads result
```

Commands include: "get firmware version," "request reset," "I'm ready," "I'm gone." All initialization handshakes use mailbox.

This is a security boundary. The PF driver must validate every mailbox message — a malicious guest could send malformed commands trying to crash the PF or escape the VM.

## VRAM and address-space partitioning

The PF carves out:

- A piece of VRAM dedicated to each VF (e.g., MI300X 192 GB → 8 VFs × 24 GB).
- A range of GPU virtual address space.
- A subset of HW queues / doorbells.

The VF's VRAM size is hard-fixed per scheduler config; resizing requires VM stop. Some platforms allow CMSA / MIG-style sub-partitions of compute units, exposed as different VF profiles.

## Time-slice vs space-slice

Two scheduling models:

| Model | How |
|---|---|
| **Time-slice** | All VFs share *all* compute units; MES rotates work between them every few ms. Maximizes per-VF peak performance, but cross-VF interference. |
| **Space-slice** | Each VF gets dedicated CUs / SDMA / VCN. No interference, lower peak per VF. Used in cloud with strict QoS guarantees. |

AMD's MI300X partitioning supports both depending on platform config.

## Reset isolation

Customer A's VM hangs the GPU. Should customer B suffer?

- **Function-level reset (FLR)** of just the offending VF if hardware allows.
- Otherwise: **mode-1 reset** of the whole GPU (everyone takes a hit).

The PF driver tries hardest for FLR. RAS errors that mark a single VF's region "bad" can be quarantined.

## Live migration

Migrating a running VM with a passthrough GPU is technically possible:

1. Quiesce the VF (stop new submissions).
2. Snapshot VF state (VRAM, queue state, fence values).
3. Transfer to destination host.
4. Restore VF state.

In practice, GPU live migration is rare today — too much state, too much work. Cloud usually evacuates GPU VMs on planned maintenance instead.

## MIG-like vs partition

NVIDIA "MIG" pre-creates partitions at boot. AMD's approach is more flexible — partition profiles can be reconfigured (with downtime). For training (long-lived jobs), partition once and don't change. For inference (many small tenants), pick the right profile.

## Security implications

SR-IOV is the single biggest GPU security concern:

- A VF must not access another VF's memory.
- A VF must not crash the PF.
- A VF must not see another VF's compute results in caches.
- A VF must not extract decryption keys from the PSP.

Every IH / mailbox / queue path is reviewed for cross-tenant leak. Bugs are CVEs (e.g., past advisory: VF VMID reuse without TLB flush).

## Display & multimedia in SR-IOV

Most cloud configs disable display (no monitor on a server). Some:

- Use a separate "GIM" (Graphics Infrastructure Manager) host service that renders for VFs.
- Stream encode via VCN to the network (cloud gaming).
- Disable VCN in PF profile for compute-only deployments.

Display blocks are tricky because they're inherently global (one HDMI/DP cable per chip). Cloud gaming uses VCN encode + RDP/PCoIP/Parsec/AMD Cloud SDK to stream.

## Tooling

- `lspci -vv` shows VF/PF relationships (`Capabilities: SR-IOV`).
- `cat /sys/bus/pci/devices/<BDF>/sriov_numvfs` — current VF count.
- `echo 8 > sriov_numvfs` enables 8 VFs (needs IOMMU enabled).
- `virsh attach-device` (libvirt) hands a VF to a VM.
- AMD's `amdgpu-virt` debug knobs in sysfs.

## What kernel code to read

- `drivers/gpu/drm/amd/amdgpu/amdgpu_virt.c` — virtualization helpers.
- `drivers/gpu/drm/amd/amdgpu/amdgv_*.c` (when present) — virtualization-specific paths.
- `drivers/gpu/drm/amd/amdgpu/mxgpu_*` — per-chip SR-IOV mailbox implementations.
- `drivers/pci/iov.c` — Linux PCI SR-IOV core.

## What you should be able to do after this chapter

- Define PF and VF.
- Sketch host+guest driver split.
- Describe the mailbox protocol.
- Distinguish time-slice from space-slice scheduling.
- Reason about reset isolation and security boundaries.

## Read deeper

- Linux SR-IOV docs: `Documentation/PCI/pci-iov-howto.rst`.
- AMD GIM (Graphics Infrastructure Manager) repository for host-side VM cloud SDK.
- Linux KVM docs on PCI passthrough.
- AMD GPUOpen blog: "MI300 partitioning."
- Past CVE advisories for AMD GPU SR-IOV (read the patches; learn the patterns).
