# OST 6 — Memory theory: paging, segmentation, swapping

> The OS lies to every process: each thinks it has the whole address space to itself. Here's how the lie is sustained.

## The illusion

Every process sees a contiguous range of addresses, e.g., 0x0 .. 0x7fffffffffff (128 TB on Linux x86-64). Reality:
- Memory is **fragmented** physical RAM.
- **Pieces** (pages) of the process are mapped where physical RAM has room.
- Some pages may be **on disk** (swapped out) until needed.
- All processes share the same physical RAM but **don't see** each other's.

The hardware **MMU** + OS **page tables** maintain the illusion.

## Address translation

Virtual address → physical address. The MMU translates on every memory access (assisted by TLB caching).

Two main schemes historically:

### Paging
Memory divided into **fixed-size pages** (typically 4 KB; bigger for huge pages). Both virtual and physical address spaces are paginated. A **page table** maps virtual page → physical frame.

Example, 4 KB pages, 48-bit virtual address:
- bits [47:39] index level 1 (PML4)
- bits [38:30] index level 2 (PDPT)
- bits [29:21] index level 3 (PD)
- bits [20:12] index level 4 (PT)
- bits [11:0] offset within page.

Each level holds 512 entries (9 bits each). Walking 4 levels gives ~256 GB of indirect address space; the kernel may set up the same on multiple processes' page tables.

This is the **multi-level page table** structure used by x86-64. Also called "radix tree of page tables." Other architectures use slightly different splits (ARM's 4-level, RISC-V's Sv39/Sv48).

### Segmentation
Memory divided into **variable-size segments** (code, data, stack). Address = (segment, offset). x86 had it; modern Linux x86-64 has flat segments and uses only paging. Segmentation is mostly historical.

### Combined
80386 supported both. Most systems today use paging only.

## Page table entries (PTEs)

Each PTE typically holds:
- Physical frame number.
- Present bit (else, page fault on access).
- Read/Write/Execute permissions.
- User/Supervisor (kernel can access, user can or can't).
- Accessed (set by hardware on read).
- Dirty (set on write).
- NX bit (no execute).

## Page faults

On any access not satisfied by current page tables, the CPU raises a **page fault**. The OS handler decides:

- **Demand-paged**: the page exists logically but isn't loaded yet → load from disk, fix PTE, retry.
- **Copy-on-write**: page is shared but writer wants to modify → copy + remap.
- **Stack growth**: access just below the stack → expand stack, retry.
- **Swap-in**: page was swapped to disk → fetch back.
- **Bad address**: not part of any valid mapping → SIGSEGV the process.

## TLB (Translation Lookaside Buffer)

Cache of recent virtual-to-physical translations. Hit → ~1 cycle. Miss → page table walk (4 memory loads on x86, possibly all the way to RAM if not cached).

**TLB shootdown**: when the OS unmaps a page, must invalidate the TLB on every CPU that might have it. Expensive — IPI-based. A common scalability bottleneck.

ASIDs / PCIDs: hardware tag in the TLB so different processes' TLB entries don't collide. Reduces flushing on context switch.

## Page sizes

- 4 KB: standard.
- 2 MB: x86-64 large page.
- 1 GB: x86-64 huge page (1 GB pages).
- 64 KB / 1 MB / 16 GB on ARM, depending on translation granule.

Bigger pages → fewer TLB entries needed → lower TLB pressure for big workloads. But more **internal fragmentation** if you use only part. Linux's THP (Transparent Huge Pages) tries to give big pages opportunistically.

## Swapping

When physical RAM fills, the OS may **swap out** less-used pages to disk:
- mark PTE non-present;
- write page contents to swap area;
- on next access, page-fault → swap back in.

A modern Linux thin distinction:
- **Page cache**: pages backed by files. "Drop them" means just discard; can be re-read from file.
- **Anonymous pages**: pages not backed by files (heap, stack, malloc). Need a swap area to evict.

## Working set & locality

The **working set** of a process is the set of pages it's actively using. If the working set fits in RAM, performance is good. If not, **thrashing** — constant swapping, the system grinds.

Programs exhibit **temporal** and **spatial** locality:
- Recently accessed pages likely accessed again soon.
- Nearby pages likely accessed together.

Caches and prefetchers exploit this.

## Memory zones (Linux)

Physical memory is divided into zones with different properties:

- **DMA32**: for devices that can only address 32-bit addresses.
- **DMA**: even more restricted (legacy 24-bit).
- **NORMAL**: most RAM; freely usable.
- **HIGHMEM**: on 32-bit, RAM beyond what kernel can directly map.
- **MOVABLE**: pages that can be migrated (for memory hot-unplug, defrag).

Allocators (buddy, slab) consult zone preferences.

## NUMA

Multiple memory nodes per machine; access to remote nodes is slower. Linux's NUMA-aware allocators try to allocate on the local node. Page migration may move pages closer to the CPU using them.

## Allocation strategies (the OS picks frames)

- **Buddy allocator**: power-of-two sized blocks; split on alloc, merge on free. Fast, low fragmentation. Linux page allocator is buddy.
- **Slab / SLUB**: caches of small fixed-size objects; built on top of pages. (Part V/III.)
- **vmalloc**: virtually contiguous, physically scattered — good for big allocations that can't get a contiguous frame.

## Real address spaces (Linux x86-64)

```
0x0000000000000000  user space (lower half, ~128 TB)
0x0000800000000000  ... (canonical hole; sign extension)
0xffff800000000000  kernel mapping starts
0xffff800000000000  direct map of all physical memory
0xffffea0000000000  vmemmap (struct page array)
0xffffffff80000000  kernel image, text, modules
```

Userspace processes see the lower half. Kernel is mapped to upper half (always present) so syscalls don't need a page table swap.

## What about virtualization?

A guest OS runs in a VM. It has its own page tables (guest virtual → guest physical). The hypervisor adds a second translation (guest physical → host physical), via **EPT** (Intel) / **NPT** (AMD). This is "**nested paging**" — two levels of MMU walks. SR-IOV GPUs and other PCIe passthrough need IOMMU support too.

## What you should remember

- Every memory access is **virtually** addressed; the MMU translates with caching.
- Page tables are a hierarchical data structure; most are kernel-managed; some have hardware semantics.
- Page faults are normal; the OS handles them.
- TLB is critical to speed; flushes are expensive.
- The kernel chooses what stays in RAM (page cache + LRU); the next chapter is about which pages to evict.

## Try it

1. Read `/proc/self/maps` and decode the regions you see.
2. Watch a long-running program with `pmap -X PID`. What's its biggest segment?
3. Cause a deliberate page fault: `mmap` a file but `MADV_DONTNEED` it; access pages and watch in `/proc/PID/status` (RSS / faults).
4. Use `time` (with -v) on a workload; look at "minor page faults" vs "major page faults." Major = went to disk.
5. Try `numactl --hardware` (if NUMA system). Identify nodes and distances.
6. Read OSTEP chapters on Address Spaces (13–17), Beyond Physical Memory (21–24).

## Read deeper

- OSTEP chapters 13–24 — entire memory section.
- *What Every Programmer Should Know About Memory* (Drepper, 2007) — deep, classic.
- `Documentation/admin-guide/mm/` and `mm/` source in the kernel.

Next → [Page replacement algorithms](OST-07-page-replacement.md).
