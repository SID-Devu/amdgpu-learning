# Part DSA — Data Structures & Algorithms (in C)

> **Stage 4 of the [LEARNING PATH](../LEARNING-PATH.md). 8–10 weeks at 1 hr/day.**

This is the part most engineers skip and most engineers regret skipping. **Don't.**

## Why DSA is non-negotiable

Two reasons, both real:

1. **Every interview at AMD/NVIDIA/Qualcomm/Intel has a DSA round.** "Reverse a linked list." "Find the kth largest." "Implement an LRU cache." If you can't do these in 30 minutes in C, you don't get the offer. The hiring bar is the same for new grads and 24-year PMTS — both rounds have DSA.
2. **The Linux kernel is mostly data structures.** Every advanced concept in this book — fences, BO trees, GPUVM page tables, IH ring buffers, dma-buf trackers — is a data structure with constraints. If you've implemented a hash table once yourself, the kernel's `hashtable.h` reads like prose. If not, it reads like Sanskrit.

You have two reasons. Either alone is enough. Both together: **non-negotiable.**

## How DSA fits with the rest of this book

- We are at Stage 4. You've finished C (Part I).
- The next stage is **userspace systems programming** (Part USP), which uses DSA constantly (e.g., epoll uses red-black trees internally; you'll write hash tables to track file descriptors; etc.).
- Then OS theory (Part OST) uses DSA at every page (page replacement = LRU = doubly linked list + hash; CPU scheduling = priority queue = heap; etc.).
- Then the kernel — where DSA shows up in `list_head`, `rb_root`, `xarray`, `kfifo`, `radix_tree`, `interval_tree`, `idr/ida`.

So this part has **the highest leverage** of any part in the book. You will use what you learn here for the next 30 years.

## How to study this part

For every topic:

1. **Implement it from scratch in C.** No `qsort`, no library, no copying.
2. **Test it with at least 5 cases**, including empty, one, and corner.
3. **Compute the complexity** (time and space) and write it as a comment at the top.
4. **Solve 5 problems using it** on Leetcode or HackerRank, in C.
5. **Find where the Linux kernel uses it.** `git grep` it. Read 1 short usage.

If you do this for every chapter, you'll be ahead of >90% of new grads.

## Table of contents

| # | Chapter | Why it matters |
|---|---|---|
| 1 | [Big-O & complexity analysis](DSA-01-bigO.md) | The language all engineers speak |
| 2 | [Arrays — the foundation](DSA-02-arrays.md) | RAM is an array; everything starts here |
| 3 | [Strings (in C, properly)](DSA-03-strings.md) | C strings are tricky; master them |
| 4 | [Linked lists (single, double, circular)](DSA-04-linked-lists.md) | The kernel's `list_head` is everywhere |
| 5 | [Stacks & queues (incl. ring buffers)](DSA-05-stacks-queues.md) | Function calls, IH ring, kfifo |
| 6 | [Hash tables (chaining, open addressing)](DSA-06-hash-tables.md) | The most-used data structure in CS |
| 7 | [Binary trees & BSTs](DSA-07-trees.md) | Foundation for self-balancing trees |
| 8 | [Self-balancing trees: AVL, Red-Black](DSA-08-balanced-trees.md) | Linux `rbtree` is in this chapter |
| 9 | [B-trees and B+trees](DSA-09-btrees.md) | Filesystems and DBs use these |
| 10 | [Heaps & priority queues](DSA-10-heaps.md) | Schedulers, Dijkstra, top-K |
| 11 | [Graphs — representation & traversal (BFS/DFS)](DSA-11-graphs.md) | Dependency graphs, DRM scheduler deps |
| 12 | [Shortest paths: Dijkstra, Bellman-Ford, A*](DSA-12-shortest-paths.md) | Path planning, graph weights |
| 13 | [Tries](DSA-13-tries.md) | Strings, autocomplete, IP routing |
| 14 | [Sorting algorithms (all of them)](DSA-14-sorting.md) | When each is right; kernel's sort.c |
| 15 | [Searching: linear, binary, ternary, exponential](DSA-15-searching.md) | The most-asked interview pattern |
| 16 | [Recursion deep](DSA-16-recursion.md) | The mental model behind DP, trees, backtracking |
| 17 | [Dynamic programming](DSA-17-dp.md) | The interviewer's favorite |
| 18 | [Greedy algorithms](DSA-18-greedy.md) | When greedy works and when it doesn't |
| 19 | [Divide & conquer](DSA-19-divide-conquer.md) | The mindset behind merge sort, quicksort |
| 20 | [Bit manipulation tricks](DSA-20-bits.md) | Kernel uses tons of these |
| 21 | [Two pointers, sliding window, fast/slow](DSA-21-two-pointers.md) | Problem-solving patterns |
| 22 | [Backtracking](DSA-22-backtracking.md) | N-queens, sudoku, permutations |
| 23 | [Union-Find / DSU](DSA-23-union-find.md) | Kruskal, connectivity, image labeling |
| 24 | [Advanced trees: segment, Fenwick (BIT), interval](DSA-24-advanced-trees.md) | Linux `interval_tree` lives here |
| 25 | [String algorithms: KMP, Rabin-Karp, Z, suffix array](DSA-25-string-algos.md) | Pattern matching, dedup, search |
| 26 | [System-design DSA: LRU cache, bloom filter, skip list, consistent hashing](DSA-26-system-design-ds.md) | Every senior interview asks one |
| 27 | [Concurrency-aware DS: lock-free queue, RCU, hazard pointers](DSA-27-concurrent-ds.md) | The bridge to kernel concurrency |
| 28 | [DSA in the Linux kernel — guided tour](DSA-28-kernel-ds-tour.md) | Where in the source tree everything lives |

**Total: 28 chapters.** At 2 chapters/week, that's 14 weeks. At 3 chapters/week (the recommended pace if you have 1 hr/day), it's about 9–10 weeks.

## What you'll have at the end

- A folder of ~30 C files, each implementing one classic data structure or algorithm with tests. **This is a portfolio piece.** Push it to GitHub.
- The ability to walk into an interview and write any common DS from memory.
- The ability to read kernel headers (`include/linux/list.h`, `rbtree.h`, `hashtable.h`) and immediately understand them.

## Notes on language

We use **C**, not Python or C++. Why?

- C is what the kernel uses. Implementing in C exposes the **memory layout** of every structure — which matters for performance, cache, and concurrency.
- Interview rounds often allow C++. That's fine — once you can do it in C, C++ is a strict superset.
- Python hides too much for our purposes (e.g., it has no real "pointer" — but the kernel is pointers everywhere).

The first time you implement a hash table in C, with hand-rolled hashing, collision chains, and resize logic, **you become a different engineer**. Don't outsource that.

Now → [Big-O & complexity analysis](DSA-01-bigO.md).
