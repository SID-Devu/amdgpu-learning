# Appendix — Glossary (every acronym you will hit)

**ABI** — Application Binary Interface. The contract between compiled artifacts (calling convention, struct layout, symbol names). Stable kernel↔userspace; *unstable* in-kernel.

**ACPI** — Advanced Configuration and Power Interface. Firmware tables describing the platform.

**APIC / LAPIC / IOAPIC** — Advanced Programmable Interrupt Controller. Routes interrupts on x86.

**ATS / PRI / PASID** — PCIe address translation services / page request interface / process address space ID. Used for IOMMU + SVM.

**BAR** — Base Address Register. PCIe device's window into memory or I/O space.

**BO** — Buffer Object. A piece of GPU-visible memory in DRM/GEM.

**CDNA / RDNA** — AMD's compute / graphics architectures (Instinct vs Radeon).

**CRTC** — Cathode-Ray Tube Controller (vestigial name). The display pipeline driving a connector.

**CS** — Command Submission. The ioctl that hands a GPU work batch to the kernel.

**DC** — Display Core. AMD's display block (also known as DCN).

**DKMS** — Dynamic Kernel Module Support. Out-of-tree module manager.

**DMA** — Direct Memory Access. Device reads/writes RAM without CPU.

**DRM** — Direct Rendering Manager. Linux GPU driver framework.

**EDID** — Extended Display Identification Data. Monitor capability blob.

**FLR** — Function Level Reset (PCIe). Per-function reset.

**GEM** — Graphics Execution Manager. DRM's BO management layer.

**GFP flags** — Get Free Page flags. Tell kmalloc/alloc_pages how it can sleep, what zone, etc.

**GMC** — Graphics Memory Controller (AMD). Handles GPU MMU, page tables.

**GPUVM** — Per-process GPU virtual address space.

**GTT** — Graphics Translation Table. System RAM made GPU-accessible via IOMMU/GART.

**hrtimer** — High-resolution timer in kernel.

**IB** — Indirect Buffer. A buffer of GPU commands referenced from a ring.

**IH** — Interrupt Handler block (AMD). Receives interrupts from GPU sub-blocks.

**IOMMU / AMD-Vi / VT-d / SMMU** — I/O memory management unit. Translates device DMAs.

**IP block** — Intellectual Property block. A discrete hardware unit on the GPU (GFX, SDMA, VCN, …).

**IRQ** — Interrupt Request.

**KFD** — Kernel Fusion Driver. AMD's compute/HSA path (`/dev/kfd`).

**KMD / UMD** — Kernel-Mode Driver / User-Mode Driver.

**KMS** — Kernel Mode Setting. The display side of DRM.

**MEC / MES** — Micro Engine Compute / Scheduler. AMD GPU firmware engines.

**MMIO** — Memory-Mapped I/O. Device registers accessed by load/store.

**MSI / MSI-X** — Message-Signaled Interrupts. PCIe interrupts as memory writes.

**NUMA** — Non-Uniform Memory Access. Memory closer to some CPUs than others.

**OPP** — Operating Performance Point. (Voltage, frequency) pair.

**PASID** — see ATS.

**PCIe** — PCI Express. The bus.

**PIMPL** — Pointer to IMPLementation. C++ pattern hiding members for ABI stability.

**PM4** — AMD GPU command packet format.

**PRIME** — DRM mechanism for sharing BOs between drivers via dma-buf.

**PSP** — Platform Security Processor (AMD). Boots GPU firmware.

**RAII** — Resource Acquisition Is Initialization. C++ resource pattern.

**RAS** — Reliability, Availability, Serviceability. ECC/UE handling.

**RCU** — Read-Copy-Update. Kernel concurrency primitive for read-mostly data.

**SDMA** — System DMA engine (AMD).

**SMU** — System Management Unit (AMD). Power/clock controller.

**SVM** — Shared Virtual Memory. CPU and GPU share the same VA space.

**TLB** — Translation Lookaside Buffer. MMU/SMMU page-table cache.

**TLP** — Transaction Layer Packet. PCIe wire packet.

**TTM** — Translation Table Manager. DRM's memory manager for VRAM-class devices.

**UE / CE** — Uncorrectable / Correctable Error (RAS).

**VA / PA** — Virtual / Physical Address.

**VCN** — Video Core Next (AMD video codec block).

**VFS** — Virtual File System. Linux's filesystem abstraction.

**VMID** — Virtual Memory ID. GPU's process tag, indexes the GPU page table register file.

**vblank** — Vertical Blanking Interval. The "between-frames" event.
