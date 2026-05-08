# OST 7 — Page replacement algorithms

> When RAM is full, which page do you evict? The choice can affect performance by an order of magnitude.

## The setup

A process accesses pages over time: A, B, C, A, D, B, A, ... If RAM fits 3 pages and we want a 4th, we must evict one. The evicted page may be re-fetched later, costing a page fault.

**Goal**: minimize page faults.

## The optimal (Belady's MIN)

**Evict the page that will not be used for the longest time in the future.**

Provably optimal — proven by Belady (1966).

But it requires **future knowledge**. So: it's our **upper bound**; real algorithms approximate it.

## FIFO (First-In, First-Out)

Evict the oldest page.

Simple. **Belady's anomaly**: more memory can sometimes cause **more** faults under FIFO (counter-intuitive). Avoid FIFO.

## LRU (Least Recently Used)

Evict the page **not used for longest** in the past.

Approximates optimal under temporal locality (assumes recent past predicts near future). Generally good.

Implementation:
- **Naive**: timestamp on each access. Scanning is O(pages) per replacement.
- **Better**: linked list — most recent at front. On access, move to front. Plus a hash map for O(1) lookup. (DSA-26.)
- **Hardware-supported**: PTE has "accessed" bit; OS scans periodically.

## Approximate LRU — the practical reality

Real systems can't update a list on **every** memory access (too expensive). They use **clock-style** or **second-chance** algorithms.

### Clock algorithm (NRU, "Not Recently Used")

Pages in a circular list. A "hand" sweeps:
- If page's "accessed" bit is 0 → evict.
- If 1 → clear it; move on.

Cheap; approximates LRU. Used in Linux (with tweaks).

### Two-handed clock
Two hands; the leading hand clears bits, trailing hand evicts. Tunable rate.

### Page-aging (Linux)
Each page has an "age" counter, periodically decayed. Access bumps age. Evict lowest age.

## LFU (Least Frequently Used)

Evict the page with the smallest access count.

Pros: handles "hot" pages well.
Cons: pages with one-time burst of accesses become hard to evict.

Often combined with aging (decay counter).

## Working-set algorithm

Compute the set of pages accessed in a recent window (e.g., last 1 second). Keep working set in RAM; evict outside.

Predicts thrashing — if total working sets > RAM, system is doomed.

## Modern variants

### ARC (Adaptive Replacement Cache)
Maintains two LRU lists (recently used, frequently used) plus "ghost" lists tracking what was evicted recently. Adapts the partition between them. Used by ZFS, IBM DB2.

### 2Q
Splits cache into "in" (newcomers) and "out" (proven hot). Approximates LRU's ability while being simpler.

### TinyLFU / W-TinyLFU
Recent (last decade) approach. Tracks frequency via count-min sketch. Used in modern Java caches (Caffeine, Guava).

### LIRS
Distinguishes pages by **recurrence interval** (how often used). Better than LRU for some loop-heavy workloads.

## Linux's actual policy

Linux maintains **two LRU lists** per memory zone:
- **Active list**: pages thought to be hot.
- **Inactive list**: pages possibly cold; first to be evicted.

Page transitions:
- Newly mapped → inactive.
- Accessed once → moved to active.
- Active page not accessed for a while → demoted to inactive.
- Inactive + not accessed → eligible for eviction.

This is approximately a **two-handed clock with frequency awareness**.

In `mm/vmscan.c`, `mm/swap.c`. Read these once you finish Part V.

## Anonymous vs file-backed

Linux's eviction logic differs:
- **File-backed** (page cache for regular files): clean → just drop; dirty → write back, then drop.
- **Anonymous** (heap, stack): need a swap area to write to before eviction.

Both go on LRU lists. Tunable balance via `vm.swappiness` (0 = avoid swap, 100 = swap freely).

## Major vs minor faults

- **Minor fault**: page already in RAM (e.g., lazily mapped). Cheap; just fix PTE.
- **Major fault**: page must be fetched from disk. Slow.

Minimizing **major faults** is the goal of page replacement.

## Thrashing

When the working set exceeds RAM, the system spends most time swapping pages in/out — making no real progress. Throughput collapses.

Mitigations:
- More RAM.
- Reduce process count.
- Better algorithms (working-set aware).
- The OOM killer (Linux: kills a process when memory pressure is severe).

## In databases

DB engines often **manage their own buffer pool**, ignoring the OS page cache (with `O_DIRECT`). They tune replacement (often LRU-K, ARC) for SQL workloads. The OS page cache becomes a thin shim.

## Common pitfalls

1. **Mistaking LRU for optimal**. LRU works on locality; for sequential huge scans (bigger than RAM), LRU is the worst — every page evicts before re-use. ARC and 2Q help.
2. **Using ages too coarsely**. Age = 1 vs Age = 0 distinguishes nothing useful at scale.
3. **Bypass the cache** with O_DIRECT and forget that **you** now have to manage replacement.

## Try it

1. Simulate FIFO, LRU, OPT (oracular optimal) on a hand-crafted access trace. Count faults for each.
2. Reproduce Belady's anomaly: a trace + page count where FIFO gets **more** faults with **more** memory.
3. Implement an LRU cache with O(1) ops (DSA-26 again). Measure hit rate on a real trace (e.g., a website's request log).
4. Read `mm/vmscan.c` introduction comments. Identify the two-list model.
5. Tune `vm.swappiness` on a test box. Run a memory-heavy workload; observe swap usage with `vmstat`.

## Read deeper

- OSTEP chapters 21–24 (Beyond Physical Memory).
- Megiddo & Modha "ARC: A Self-Tuning, Low Overhead Replacement Cache" (2003).
- Linux kernel `Documentation/admin-guide/mm/` — workingset.rst, transhuge.rst.

Next → [File system theory & journaling](OST-08-fs-theory.md).
