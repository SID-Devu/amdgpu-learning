# Chapter 38 — DMA, IOMMU, coherent vs streaming mappings

A device that can't DMA is useless. amdgpu DMAs constantly: pulling commands from system memory, pushing rendered frames to a scanout buffer in VRAM, copying textures, even running compute kernels via the SDMA engine. This chapter is the foundation.

## 38.1 What DMA is

The CPU could copy data between RAM and a device, but that wastes cycles. **Direct Memory Access** lets the device move data on its own — it has its own bus master that issues memory transactions. The CPU just programs "from here, this many bytes, to here" and goes to do something else.

Three actors:

- **CPU**: prepares buffers, kicks the DMA, optionally waits.
- **Device**: reads/writes memory.
- **IOMMU** (when present): translates *device-side* addresses to physical RAM.

## 38.2 The two DMA mappings

| Type | API | Use |
|---|---|---|
| **Coherent** | `dma_alloc_coherent` | Long-lived buffers shared in both directions; descriptors, ring buffers, completion areas. |
| **Streaming** | `dma_map_single`, `dma_map_sg`, `dma_map_page` | One-shot transfers; data buffers; the device "owns" the buffer until you unmap. |

Coherent memory is allocated by the kernel, mapped both into the CPU and the device, and is **automatically synchronized** (typically uncached or write-combining). No explicit flushes.

Streaming requires you to:

- map the buffer (`dma_map_single` returns a `dma_addr_t` — what the device sees),
- transfer ownership to the device,
- do the DMA,
- transfer back to the CPU via `dma_sync_*` if you re-read,
- unmap when done.

Why streaming? Coherent memory is expensive on platforms that need uncached or noncacheable mappings. For one-shot transfers, the cache flush + map is cheaper.

## 38.3 Coherent example

```c
dma_addr_t dma_handle;
void *cpu_addr = dma_alloc_coherent(&pdev->dev, size, &dma_handle, GFP_KERNEL);
if (!cpu_addr) return -ENOMEM;

/* Tell device its physical-ish address (after IOMMU translation, this is what it sees). */
writel(lower_32_bits(dma_handle), regs + DMA_LO);
writel(upper_32_bits(dma_handle), regs + DMA_HI);

/* CPU and device can read/write cpu_addr; no flushes. */

dma_free_coherent(&pdev->dev, size, cpu_addr, dma_handle);
```

`dma_handle` is a **device-side address** (often called "bus address" or "DMA address"). With an IOMMU enabled, it's a virtual address in the device's IOMMU domain. Without IOMMU, it's the physical address (or platform-dependent translation).

## 38.4 Streaming example

```c
dma_addr_t dma_addr = dma_map_single(&pdev->dev, kbuf, size, DMA_TO_DEVICE);
if (dma_mapping_error(&pdev->dev, dma_addr)) return -ENOMEM;

writel(lower_32_bits(dma_addr), regs + DMA_LO);
writel(upper_32_bits(dma_addr), regs + DMA_HI);
writel(size,                    regs + DMA_LEN);
writel(START,                   regs + DMA_CTL);

/* device DMAs … completes via IRQ */

dma_unmap_single(&pdev->dev, dma_addr, size, DMA_TO_DEVICE);
```

Direction:

- `DMA_TO_DEVICE` — CPU writes, device reads.
- `DMA_FROM_DEVICE` — device writes, CPU reads.
- `DMA_BIDIRECTIONAL` — both.

Direction tells the kernel what cache flushes to do.

## 38.5 Scatter-gather

For a big buffer that's many physically-discontiguous pages (e.g. a userspace mmap'd buffer):

```c
struct scatterlist sg[N];
sg_init_table(sg, N);
for (i = 0; i < N; i++) sg_set_page(&sg[i], pages[i], PAGE_SIZE, 0);

unsigned int nents = dma_map_sg(&pdev->dev, sg, N, DMA_TO_DEVICE);
/* program device with scatter list */
dma_unmap_sg(&pdev->dev, sg, N, DMA_TO_DEVICE);
```

`dma_map_sg` may *coalesce* adjacent entries when the IOMMU can. Always use the returned `nents`, not your input `N`.

For modern code, prefer `sg_table` plus the `dma_buf_map_attachment` paths.

## 38.6 DMA mask

A device can DMA to a limited address range. For a 32-bit DMA-only device, only 4 GiB. Set the mask early:

```c
ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(40));
if (ret) {
    dev_err(&pdev->dev, "no 40-bit DMA support\n");
    return ret;
}
```

If the mask isn't satisfiable, the kernel will use the bounce buffer (SWIOTLB) — slow. Modern GPUs are 40+ bit; amdgpu uses 44-bit.

## 38.7 The IOMMU

The IOMMU translates **device addresses → physical RAM**:

- Programs the device with IO virtual addresses (IOVAs).
- Maps IOVAs to RAM via IOMMU page tables managed by the kernel.
- Devices with bugs or hostile firmware can't reach memory you didn't map.

AMD-Vi: AMD's IOMMU. VT-d: Intel's. Both expose the same Linux API.

Group: devices behind the same IOMMU group share the same IOVA space (e.g., functions of a multi-function PCI device). For VFIO passthrough, you must pass the entire group.

For driver code, you mostly *don't* care: `dma_*` APIs hide IOMMU behind them. You only care if:

- A weird IOMMU configuration breaks transfers (e.g. SWIOTLB engaged).
- You're integrating with VFIO (PCIe passthrough).
- You're touching the SVM (Shared Virtual Memory) path: an IOMMU feature where a device shares the CPU's process page tables — used by KFD for ROCm.

## 38.8 SWIOTLB / bounce buffer

When the device DMA mask can't reach the buffer's physical address, the kernel allocates a **bounce buffer** in low memory, copies the data, then DMAs. Slow. Common with old 32-bit devices.

If you see `swiotlb` in dmesg under load, your device or kernel is missing IOMMU support. amdgpu users sometimes hit this on platforms with broken Re-bar configurations.

## 38.9 dma-buf

A cross-driver buffer-sharing primitive. One driver allocates and *exports*; another *imports* and gets DMA mappings into its own context. Used heavily for:

- Wayland and Vulkan WSI (zero-copy framebuffers).
- V4L2 → GPU (camera → display).
- amdgpu BO ↔ another DRM driver via PRIME.

We discuss in Chapter 45 alongside GEM. The key API:

- `dma_buf_export(...)` — exporter creates.
- `dma_buf_get(fd)` — importer attaches.
- `dma_buf_attach`, `dma_buf_map_attachment` — get a `sg_table`.
- `dma_buf_vmap` — kernel virtual mapping.

## 38.10 Cache management

For coherent memory: nothing to do.

For streaming with `_TO_DEVICE`: CPU's writeback caches are flushed before device reads. The kernel does this in `dma_map_*`.

For streaming with `_FROM_DEVICE`: CPU caches are invalidated after device writes. `dma_unmap_*` does it.

For mid-DMA reads/writes by CPU on a streaming buffer:

```c
dma_sync_single_for_cpu(&pdev->dev, dma_addr, size, DMA_FROM_DEVICE);
/* read */
dma_sync_single_for_device(&pdev->dev, dma_addr, size, DMA_FROM_DEVICE);
```

Forget these and you'll get random data corruption.

## 38.11 Minimal end-to-end DMA in our edu driver

QEMU's edu device supports DMA via registers `0x80..0x9C`. We add:

```c
/* allocate coherent buffer */
dma_addr_t dh;
void *kva = dma_alloc_coherent(&pdev->dev, 4096, &dh, GFP_KERNEL);

/* fill */
memset(kva, 0xAB, 4096);

/* program edu DMA: src=dh, dst=intra-device offset, len, direction */
writeq(dh,          e->bar0 + 0x80);
writeq(0x40000,     e->bar0 + 0x88);  /* device-internal address */
writel(4096,        e->bar0 + 0x90);
writel(0x1,         e->bar0 + 0x98);  /* fire */

/* wait for IRQ, etc. */

dma_free_coherent(&pdev->dev, 4096, kva, dh);
```

You can extend Chapter 37's edu driver to do this — a useful exercise.

## 38.12 Exercises

1. Add a DMA path to the edu driver from Chapter 37: allocate coherent, write a pattern, DMA into device, DMA back, verify.
2. Repeat with `dma_map_single` on a `kmalloc` buffer; observe SWIOTLB on machines without IOMMU support enabled in firmware.
3. Read `Documentation/core-api/dma-api.rst`.
4. Read `Documentation/driver-api/dma-buf.rst`.
5. Read `drivers/gpu/drm/amd/amdgpu/amdgpu_dma_buf.c`. We will revisit.

---

Next: [Chapter 39 — MMIO, register access, barriers](39-mmio-and-barriers.md).
