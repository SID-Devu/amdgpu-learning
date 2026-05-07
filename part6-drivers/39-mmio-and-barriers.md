# Chapter 39 — MMIO, register access, barriers

You've already used `readl`/`writel`. This chapter goes deeper: the memory model of MMIO, write coalescing, posted writes, and how to talk safely to GPU registers.

## 39.1 What MMIO is

Memory-Mapped I/O. The chipset and PCIe routing make some physical address ranges resolve not to DRAM but to a device. CPU loads/stores to those addresses become bus transactions to the device.

The CPU's caches typically *don't* cache MMIO ranges; they're marked **UC** (uncached) by the kernel.

To use:

```c
void __iomem *base = ioremap(phys, size);
/* … readl/writel … */
iounmap(base);
```

Or the `pcim_iomap_regions` (Chapter 37) for PCI BARs.

## 39.2 The accessors

```c
u8  readb(addr);   void writeb(u8  v, addr);
u16 readw(addr);   void writew(u16 v, addr);
u32 readl(addr);   void writel(u32 v, addr);
u64 readq(addr);   void writeq(u64 v, addr);
```

Why not just `*(u32*)addr = v`? Two reasons:

1. **`__iomem` annotation** — sparse static analysis catches accidental dereferences.
2. **Endianness and barrier semantics** — these are encoded in the accessor.

Variants:

- `readl_relaxed`/`writel_relaxed` — no DMA barrier; faster; you must add manual barriers.
- `ioread32`/`iowrite32` — alternate name; same thing.
- `readsl`/`writesl` — fixed-source/dest "string" loops; useful for FIFOs.

## 39.3 Posted writes

PCIe writes are **posted** by default: the CPU's write returns as soon as the write reaches the root complex's outbound buffer. The data has not yet reached the device; the device hasn't acknowledged.

Implication: a sequence like

```c
writel(START, regs + CTRL);
status = readl(regs + STATUS);
```

…does not guarantee the device has *seen* the START write before the STATUS read. The read pushes the write through (because reads are non-posted) — that's the famous "read after write to flush a posted write" idiom.

Your code:

```c
writel(value, regs + CTRL);
(void)readl(regs + CTRL);   /* flush posted write */
```

amdgpu uses register-poll waits everywhere; the read implicitly flushes.

## 39.4 Memory barriers between CPU and device

The kernel has DMA barriers:

- `wmb()` — write memory barrier; ensures stores complete before subsequent stores.
- `rmb()` — read memory barrier.
- `mb()`  — full barrier.

`writel` already includes the necessary write barrier on most architectures. `writel_relaxed` does not.

Pattern:

```c
/* CPU: write descriptor in DRAM */
desc->addr = ...;
desc->len  = ...;
wmb();                                /* descriptor visible */
writel(cur_idx, regs + RING_HEAD);    /* doorbell */
```

Without the `wmb()`, the device might fetch the descriptor with old contents.

For "device wrote to memory; CPU reads":

```c
fence_val = readl(regs + FENCE);   /* device has updated mem before this */
rmb();
data = ring->shared[fence_val % N];
```

amdgpu's IB handlers and fence read paths follow exactly this.

## 39.5 Write combining

Some MMIO ranges are mapped **WC** (write-combining). The CPU's write-combining buffer accumulates 64-byte aligned writes and flushes them in a burst. Used for:

- Large frame-buffer writes (CPU drawing into VRAM scanout area).
- Command rings in WC system memory.

Write a partial 64 bytes? It still flushes when the buffer is full or you hit a barrier. Use `__iowrite32_copy` / `memcpy_toio` to leverage WC for big copies.

```c
memcpy_toio(dst_iomem, src_kernel, len);
```

For doorbell pages, WC is wrong (you want the write to go through immediately). Use UC.

## 39.6 Endianness

`readl` is **always** little-endian (most platforms; explicitly specified in the API). For a big-endian device on a little-endian CPU, swap manually:

```c
u32 v = be32_to_cpu(__raw_readl(addr));
```

amdgpu hardware is little-endian; you don't worry about this.

## 39.7 Address validation

When userspace passes a register offset (via ioctl/debugfs), validate it:

```c
if (offset > pci_resource_len(pdev, 0) - 4) return -EINVAL;
```

Otherwise userspace can read MMIO outside your BAR — which may map to *another device* and crash the system or read sensitive data.

amdgpu has explicit "register whitelists" for what userspace can read via the `AMDGPU_INFO` ioctl. You will read this code in Part VII.

## 39.8 Debugging MMIO

- `lspci -s 01:00.0 -v` — see BARs and memory regions.
- `cat /proc/iomem` — system address map.
- `mmiotrace` — kernel feature to log all MMIO accesses to a debugfs file. Build with `CONFIG_MMIOTRACE=y`.
- `devmem2 0xfb00000c 32` — read a register from userspace (root only). Dangerous; use only on devices you understand.

For amdgpu, set `amdgpu.mmio_debug=1` parameter to log register accesses. Useful when debugging hangs.

## 39.9 amdgpu register naming

Each ASIC family has thousands of registers, defined in headers like `gc_10_3_0_offset.h`, `gc_10_3_0_sh_mask.h`. The macros `RREG32_SOC15`, `WREG32_SOC15`, `WREG32_FIELD_SOC15`, etc. abstract over family.

```c
WREG32_SOC15(GC, 0, mmCP_RB_CNTL, value);
```

The driver builds register addresses dynamically based on chip family and instance. We'll dive in Chapter 48.

## 39.10 Exercises

1. In your edu driver, add a `relaxed` variant that drops the implicit barrier; measure throughput. Confirm the regular variant orders correctly.
2. Map an MMIO BAR with `ioremap_wc` and `memcpy_toio` 16 KB. Time it vs `readl`-loop equivalent.
3. Read `arch/x86/include/asm/io.h::readl` to see the exact instruction emitted.
4. `cat /proc/iomem` — find your GPU's BARs by name.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_device.c::amdgpu_device_indirect_rreg`. Identify the register-access primitive amdgpu uses when MMIO BAR isn't directly accessible (over PCIe config-space MMIO bridge).

---

Next: [Chapter 40 — Power management, runtime PM, suspend/resume](40-power-management.md).
