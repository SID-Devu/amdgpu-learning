# Chapter 19 — Virtual memory, paging, TLBs, MMUs

GPU drivers live and die on memory. To write `amdgpu_vm.c` (the GPU's page tables) you must already have a perfect mental model of the CPU's page tables. We build it now.

## 19.1 Why virtual memory

Early machines had one program, one physical address space. Multitasking + protection requires that:

1. Two processes can both write to "address 0x1000" without conflict.
2. A process cannot read another's memory.
3. Memory can be over-committed (DRAM is finite).

Virtual memory solves all three. Every process sees its own virtual address space, mapped by hardware (the **MMU**) to physical memory through **page tables**.

## 19.2 Pages

Physical memory is divided into fixed-size **frames** (4 KiB on x86 by default; 16 KiB or 64 KiB possible on some ARM). Virtual memory is divided into matching **pages**.

The MMU's job: given a virtual address, look up the page table to find the physical frame.

Address format on x86_64 with 4 KiB pages, 4-level paging:

```
 63                                                                12 11           0
 ┌────────────────────────────────────────────────────────────────┬──────────────┐
 │ canonical sign-extended bits | PML4(9) | PDPT(9) | PD(9) | PT(9)│  page offset │
 └────────────────────────────────────────────────────────────────┴──────────────┘
```

The 48 low bits of address split into 4× 9-bit indices into 4 levels of tables, plus a 12-bit offset within the 4 KiB page. Each table is itself a page (4 KiB) of 512× 8-byte entries.

Modern x86 supports 5-level paging (`la57`, 57-bit virtual). Most systems still use 4-level.

## 19.3 A page table walk

```
                       virtual address
   ┌────┬─────┬─────┬─────┬─────┬───────┐
   │ ext│ L4  │ L3  │ L2  │ L1  │ off   │
   └────┴──┬──┴──┬──┴──┬──┴──┬──┴───────┘
           │     │     │     │
           ▼     ▼     ▼     ▼
        PML4  →PDPT  → PD  →  PT
                                  │
                                  ▼
                             physical frame
                                  +
                                offset
```

Each table entry contains the physical address of the next-level table or the final frame, plus flag bits:

| Bit | Name | Meaning |
|---|---|---|
| 0 | P | Present |
| 1 | R/W | Writable |
| 2 | U/S | User-accessible |
| 3 | PWT | Write-through caching |
| 4 | PCD | Cache disable |
| 5 | A | Accessed (set by HW) |
| 6 | D | Dirty (set by HW for last-level entries) |
| 7 | PS | Page size (1 = huge page at this level) |
| … | | |
| 63 | XD | Execute disable |

The kernel manipulates these bits. A driver rarely touches them directly, but `mmap` for device memory ends up flipping `PCD`, `PWT` to control caching.

## 19.4 Huge pages

Setting `PS=1` at L2 makes a 2 MiB page; at L3, 1 GiB. Reduces TLB pressure for big buffers. The kernel exposes them via `madvise(MADV_HUGEPAGE)` and transparent huge pages (THP).

GPU drivers use huge mappings on the GPU side (GPUVM) to represent big buffers efficiently. We will revisit in Chapter 51.

## 19.5 The TLB

The translation lookaside buffer is a hardware cache of recent virtual→physical translations. Without it, every memory access would do 4 (or 5) extra DRAM reads to walk the page table.

Sizes vary; on a Zen 4 core, dTLB has ~64 4 KiB entries (L1d) and 3 K (L2). TLB misses are *expensive* (200+ cycles). Cache-friendly data structures and huge pages help.

When the kernel changes a page-table entry, it must invalidate the affected TLB entries:

- `INVLPG addr` — flush one entry.
- `mov %cr3, %cr3` — flush all (except global pages).
- IPI to other cores — every CPU with the mapping must flush its TLB.

The IPI cost is why `munmap` of a shared mapping is one of the slowest things you can do in Linux.

## 19.6 The kernel's page-table view

Every process has its own page tables, but the **upper half** of every process's PML4 points to the same kernel page tables. So the kernel is mapped into every process at the same VAs.

Switching processes only requires updating the lower half (one CR3 reload). This is fast.

Newer Linux uses **KPTI** (Kernel Page Table Isolation, Meltdown mitigation) which actually swaps in a kernel-only page table on syscall entry. Cost: more TLB flushing.

## 19.7 Page faults

Every memory access goes through the MMU. If the entry is missing or doesn't have the right perms, the CPU generates a **page fault** exception (vector 14 on x86). The kernel handler:

1. Reads `CR2` (faulting virtual address) and the error code.
2. Finds the VMA covering the address.
3. If the VMA exists and the access is legal, allocates a page (or reads from a file) and installs the PTE.
4. If illegal, sends `SIGSEGV` to the process.

GPU drivers piggyback on this for **demand-faulted GPU buffers**. A user `mmap`s a device handle; when the user touches the buffer, a page fault enters the driver's `vm_ops->fault` callback, which allocates and inserts the page. We will write one in Chapter 36.

## 19.8 Cache attributes

Each PTE has bits controlling caching for that page. Combined with the **PAT** (Page Attribute Table) the OS can pick:

- **WB** (write-back) — default, fully cached.
- **WT** (write-through) — write-through cache.
- **UC** (uncached) — every access goes to DRAM/MMIO.
- **WC** (write-combining) — uncached but writes coalesce in a small buffer; great for streaming write-only mappings (frame buffers).

For MMIO (device registers), the kernel uses UC. For framebuffers and GPU command buffers being written by the CPU to be read by the GPU, WC. `ioremap()` chooses; `ioremap_wc()` forces WC.

The wrong cache attribute on a GPU buffer will silently corrupt data (the CPU thinks the cache is in sync; the GPU sees stale memory). This is the most insidious driver bug.

## 19.9 The page allocator

The kernel's lowest-level memory allocator is the **buddy allocator**:

- Free pages are kept in `MAX_ORDER` lists, indexed by power-of-two size (1, 2, 4, …, 1024 pages).
- Allocate by splitting a higher-order block; free by coalescing buddies.

Above it: **slab/SLUB** for fixed-size objects, **kmalloc** for variable, **vmalloc** for non-contiguous virtually-mapped memory. We treat each in Part V.

GPU memory has its own allocator (TTM in amdgpu, Chapter 46). It mirrors the CPU's buddy idea on the device side.

## 19.10 Reverse mappings and `struct page`

Every physical page has a corresponding `struct page` in a giant array (`mem_map`). This struct stores: refcount, mapcount, flags, owner. Drivers that pin pages (for DMA) frequently use `struct page *` — `get_user_pages_fast()` returns them.

To translate kernel virtual → physical: `__pa(addr)`. Reverse: `__va(paddr)` (works for the direct-mapped region only).

To translate kernel virtual → `struct page *`: `virt_to_page(addr)` (direct-map). For vmalloc'd: `vmalloc_to_page(addr)`.

These helpers are everywhere in DMA code.

## 19.11 NUMA briefly

On multi-socket boxes (and modern AMD chiplet designs), DRAM is split into **nodes**. Accessing memory on a remote node is slower. The kernel tries to keep allocations local and offers `numa_alloc_local`/`set_mempolicy`.

For GPUs on a multi-socket host, you allocate DMA memory close to the device's PCIe root complex. amdgpu does this via `dev_to_node(dev)`.

## 19.12 IOMMUs

The IOMMU is the MMU for devices. It maps the device's view of memory ("DMA addresses" or "IOVA") to physical RAM. Without IOMMU, a malicious or buggy device could DMA anywhere. With IOMMU:

- Device sees only what the kernel maps (`dma_map_*`).
- Each device (or device group) can have its own IO page table.
- Protection at the PCIe level.

AMD's IOMMU is "AMD-Vi"; Intel's is VT-d. ARM uses "SMMU." All implement the same idea.

GPU drivers must be IOMMU-aware: `dma_map_*` returns IOVA; the GPU is programmed with IOVA, not physical. The IOMMU translates back to physical when the GPU does DMA.

## 19.13 Exercises

1. Write a userspace program that does `mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)`; read `/proc/self/maps` to see the new VMA.
2. `cat /proc/meminfo`. Identify `MemTotal`, `MemAvailable`, `Slab`, `KReclaimable`, `HugePages_Total`.
3. Write a microbenchmark that measures the cost of a TLB miss: stride through a 1 GiB array in 4 KiB steps. Compare to a 1 MiB array.
4. Read `Documentation/x86/x86_64/mm.rst` from the kernel source. Identify each address-range comment.
5. Read `Documentation/admin-guide/mm/concepts.rst` for the kernel's MM overview.

---

Next: [Chapter 20 — Scheduling — CFS, deadline, real-time](20-scheduling.md).
