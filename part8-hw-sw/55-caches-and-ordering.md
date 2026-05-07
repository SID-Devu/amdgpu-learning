# Chapter 55 — Caches, coherence, and memory ordering

The single most-asked AMD/NVIDIA/Qualcomm interview topic outside of basic C. We deepen Chapter 23 here with hardware specifics.

## 55.1 Cache structure

Modern CPUs have multi-level caches:

- **L1d / L1i** per core, ~32 KB each, ~4 cycles, 8-way associative.
- **L2** per core, ~1 MB, ~12 cycles.
- **L3** shared across cores, MBs, ~30 cycles.

Cache lines: 64 bytes (essentially universal for x86 and ARM).

A line may be in one of (MESI / MOESI):

- **M** — modified (dirty, I have the only copy).
- **O** — owner (modified, but shared).
- **E** — exclusive (clean, only mine).
- **S** — shared (clean, others may have).
- **I** — invalid (no copy).

When core 1 writes, it must transition the line to M, evicting others' copies via a **coherence protocol** (snoops or directories). Bouncing a hot line between cores is expensive — it's the cost of contended atomics.

## 55.2 Coherence vs. consistency

- **Coherence**: every memory location appears to have a single global ordering; once written, all readers eventually see the new value.
- **Consistency** (memory model): how reads and writes to *different* locations may be reordered.

x86: TSO (Total Store Order). Strong: stores from one core appear in the same order on all cores. Loads can be reordered with earlier stores to different addresses.

ARM: more permissive. Loads/stores can be reordered freely unless barriered.

POWER, Alpha, RISC-V: even more permissive.

## 55.3 The store buffer

When a core stores, the value goes to a **store buffer** before reaching cache. This lets the core continue executing without waiting. Other cores don't see the store until it drains.

The store buffer is the reason `mfence` exists — to drain it.

## 55.4 Memory barriers

| Barrier | x86 spelling | ARM spelling |
|---|---|---|
| Compiler only | `__asm volatile("":::"memory")` | same |
| Read | (free on x86) | `dmb ld` |
| Write | (free on x86) | `dmb st` |
| Full | `mfence` / `lock`-prefixed | `dmb sy` |
| Acquire load | (free on x86) | `ldar` |
| Release store | (free on x86) | `stlr` |

C++/`<atomic>` orders compile to the right thing per arch. The kernel has `smp_*mb`, `smp_load_acquire`, etc.

## 55.5 The infamous "double-checked locking"

```c
if (!instance) {
    lock();
    if (!instance) instance = create();
    unlock();
}
return instance;
```

In Java pre-5 this was broken because the publish wasn't release. In modern C/C++ with atomic<T> + acquire/release, it works. In the kernel, prefer `cmpxchg` or `static_branch_*`.

## 55.6 The CPU↔device case

Caches are not coherent with **all** devices. PCIe is, in a sense, by default coherent on x86 (Snoop, ACS), but:

- Whether a device sees CPU writes from the CPU's caches depends on the device's transaction type and the IOMMU.
- For *write-combined* CPU mappings, writes are buffered — `wmb()` flushes.
- For *uncached*, writes are immediate.

amdgpu manages this with `wmb()` after writing descriptors and before doorbell.

## 55.7 GPU caches

The GPU has L1 caches per CU, L2 globally, sometimes Infinity Cache (very large). These are NOT coherent with the CPU.

To make GPU writes visible to CPU:

- **Flush** GPU L2 (cache flush packet, like `RELEASE_MEM` with EOP).
- The CPU read after fence-signaled is then guaranteed to see the data.

Driver responsibility: emit cache-flush packets in IB epilogues. amdgpu does. If you forget, you get garbage.

## 55.8 IOMMU and coherence

With IOMMU, devices can choose coherent or non-coherent traffic. For GPU traffic to system memory (GTT), x86 systems usually use snooped writes — the CPU caches stay consistent. For VRAM traffic, the CPU isn't involved.

ARM systems sometimes use I/O coherency (ACE-Lite). When designing drivers for ARM SoC GPUs (Qualcomm Adreno via msm), you must explicitly handle.

## 55.9 Practical guidance

- For shared CPU-CPU data: use C++ `std::atomic` or kernel atomics; pick correct order.
- For CPU↔device shared data via DMA: trust `dma_alloc_coherent` for descriptors, use `dma_sync_single_for_*` on streaming.
- For CPU↔GPU command rings: use WC mappings and explicit barriers.
- For perf-critical code: use `perf stat` to measure cache-miss rate; restructure data layout for locality.

## 55.10 Exercises

1. Write code that increments two adjacent `int`s from two threads. Pad them to separate cache lines. Compare throughput.
2. On x86, write a release-acquire publish/consume; verify with TSan.
3. On ARM (in QEMU or a real device), repeat — note that without barriers, the same code is broken.
4. Read `Documentation/atomic_t.txt` and `Documentation/memory-barriers.txt`.
5. Read AMD's GPU "Cache Hierarchy" section in the architecture white-paper for whichever generation you have.

---

Next: [Chapter 56 — PCIe deep dive](56-pcie.md).
