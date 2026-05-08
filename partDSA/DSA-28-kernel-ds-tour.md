# DSA 28 — DSA in the Linux kernel — guided tour

> A walking tour of where every DSA you've learned actually lives in the kernel source. Print this and put it on your wall.

When you finish DSA, this chapter is your "bridge" to Part V. Each item below is a real header or file; clone the kernel and `vim` it yourself.

## Linked lists

- `include/linux/list.h` — doubly linked, circular, embedded `list_head`.
- `include/linux/list_bl.h` — bit-locked list (1-bit lock at head).
- `include/linux/llist.h` — lockless singly-linked list (CAS-based push, batch pop).
- `include/linux/hlist.h` — singly linked, used for hash tables (smaller than `list_head`).

Used absolutely everywhere. `git grep "struct list_head" | wc -l` will surprise you.

## Hash tables

- `include/linux/hashtable.h` — generic chained hash table (uses `hlist_node`).
- `include/linux/rhashtable.h` — resizable, RCU-safe, lock-friendly. Used by netfilter, IPC, etc.
- `lib/hash.c`, `lib/rhashtable.c`.

## Trees (RB-tree)

- `include/linux/rbtree.h` — generic red-black tree.
- `lib/rbtree.c` — implementation.
- `include/linux/rbtree_augmented.h` — augmenting RB-trees (e.g., interval tree).
- `include/linux/interval_tree.h`, `lib/interval_tree.c` — augmented RB for interval overlap queries.

Used in:
- `mm/vma.c` (VMAs in process address space),
- `kernel/sched/fair.c` (CFS runqueue),
- `drivers/gpu/drm/amd/amdgpu/amdgpu_vm.c` (GPU virtual address tracking),
- `drivers/iommu/` (IOMMU device tables),
- many more.

## Radix trees / XArray

- `include/linux/radix-tree.h`, `lib/radix-tree.c` — older radix tree (gradually deprecated).
- `include/linux/xarray.h`, `lib/xarray.c` — modern replacement: cleaner API, RCU-friendly.

Used in:
- page cache (`mapping->i_pages`) — keyed by file offset,
- IDR/IDA (id allocators),
- `pidmap`,
- block layer.

Read `Documentation/core-api/xarray.rst` — best entry point.

## ID allocators

- `include/linux/idr.h`, `lib/idr.c` — ID R(adix) — assign integer IDs to pointers, lookup pointer by ID.
- `lib/ida` (small case of idr) — bitmap-based IDA for "just give me a free integer."

Used in: PCI device numbers, file descriptors (no, those are arrays — but conceptually similar), `class_id`, anywhere a "small int handle" maps to a pointer.

## Bitmaps

- `include/linux/bitmap.h`, `lib/bitmap.c` — array-of-longs bit operations.
- `include/linux/bitops.h` — single-bit ops, with atomic variants.

Used in: CPU masks (`cpumask`), node masks (NUMA), interrupt masks, GPU VMID allocation, slab allocator state, page allocator state.

## Queues

- `include/linux/kfifo.h`, `kernel/kfifo.c` — generic FIFO. SPSC lock-free.
- `include/linux/circ_buf.h` — small circular buffer macros.
- DRM/scheduler queues — `drivers/gpu/drm/scheduler/`.

## Heaps / priority queues

- `lib/min_heap.c`, `include/linux/min_heap.h` — generic min-heap.
- Used by tracing for keeping bounded sets of "top events."

CFS scheduler uses RB-tree on `vruntime` instead of a heap.

## Per-CPU data

- `include/linux/percpu.h`, `mm/percpu.c`.
- `DEFINE_PER_CPU(type, var)`, `this_cpu_inc(var)`, `per_cpu(var, cpu)`.

Used pervasively: counters, allocators (slab uses per-CPU magazines), networking stats.

## Refcounting

- `include/linux/refcount.h`, `lib/refcount.c` — saturating counter, robust against overflow/underflow.
- `include/linux/kref.h` — wrap a refcount + a release callback.

Used in: file objects, `struct device`, `drm_file`, GEM objects, `drm_gem_object`, basically anything shared.

## Wait queues / completions

- `include/linux/wait.h`, `kernel/sched/wait.c` — wait queues for "sleep until condition."
- `include/linux/completion.h` — one-shot waits ("done"-style signals).

Used in: I/O wait, fence completion, GPU job submission flow.

## Locking primitives

- `include/linux/spinlock.h` — spinlocks.
- `include/linux/mutex.h` — sleeping mutex.
- `include/linux/rwsem.h` — reader/writer semaphore.
- `include/linux/rcupdate.h` — RCU.
- `include/linux/seqlock.h` — sequence locks.

Will be deeply covered in Part V/IV.

## DMA-fences (GPU specifically)

- `include/linux/dma-fence.h`, `drivers/dma-buf/dma-fence.c` — the queue + waiter pattern for GPU completion. A reference-counted, signaled-once object with callbacks. Built on top of `wait_queue_head_t`.

GPU job ordering and inter-driver synchronization rely on this.

## DMA buffer sharing

- `include/linux/dma-buf.h`, `drivers/dma-buf/dma-buf.c` — a graph of buffer objects shared across devices, with attach/map/detach lifecycle. Used by GPU, V4L2, USB, etc.

## Workqueues / kthreads

- `include/linux/workqueue.h`, `kernel/workqueue.c`.
- `kthread_create()`, `kthread_run()`.

Used to defer work outside atomic context (interrupts).

## Slab / page allocator

- `mm/slab.c`, `mm/slub.c`, `mm/slob.c` — three slab implementations (SLUB is default).
- `mm/page_alloc.c` — buddy page allocator.

Internally uses: per-CPU caches (magazines), partial-slab lists, free-page bitmaps, NUMA-aware free lists, RCU for freeing slabs.

## Page cache

- `mm/filemap.c`, `mm/readahead.c`, `mm/swap.c`.

Uses XArray to map file offset → struct page; LRU lists for active/inactive pages; mark-and-sweep for dirty pages.

## TCP/networking

- `net/ipv4/tcp.c` — TCP state machine.
- `net/core/dev.c` — packet path.
- `net/core/skbuff.c` — sk_buff (the skb), the kernel's network packet abstraction.

Heavy use of: linked lists (skb queues), hash tables (connection tracking), per-CPU stats, RCU for routes.

## VFS (virtual filesystem)

- `fs/dcache.c` — dentry cache (recently-resolved paths). Hash table + LRU.
- `fs/inode.c` — inode cache.
- `fs/file_table.c` — open file table.

## Block layer

- `block/blk-mq.c` — multi-queue block layer (per-CPU request queues, dispatch into device).

Uses: bio chains (linked lists), per-CPU request pools, hash tables for in-flight tracking.

## Tracing & debugging

- `kernel/trace/ring_buffer.c` — per-CPU lock-free ring buffer for ftrace events. **Must read** if you want to understand high-perf logging.
- `kernel/trace/trace.c` — tracing core.

## Where the kernel **doesn't** use textbook DSA

- **No B-trees in core kernel.** Filesystems have them (`fs/btrfs/ctree.c`); core kernel doesn't.
- **No suffix arrays / tries** in core kernel. Some places use radix tree.
- **No skip lists.** RB-tree usually wins.
- **No regular hash tables in interrupt context** unless they're specifically lock-free / RCU-safe.

## Reading order to study real kernel DSA

If you have time, read in this order (small files first):

1. `include/linux/list.h` — short, sweet, masterful.
2. `include/linux/llist.h`.
3. `include/linux/hlist.h`.
4. `include/linux/hashtable.h`.
5. `include/linux/rbtree.h` and `lib/rbtree.c`.
6. `include/linux/interval_tree.h` and `lib/interval_tree.c`.
7. `Documentation/core-api/xarray.rst`, then `include/linux/xarray.h`.
8. `kernel/trace/ring_buffer.c` — masterclass.
9. `lib/refcount.c`, `lib/kref.c`.
10. Documentation/RCU/whatisRCU.rst.

This is **exactly the path that turns a DSA student into a kernel reader.**

## Try it

1. Pick three structures from this chapter. For each, find at least one driver in `drivers/gpu/drm/amd/amdgpu/` that uses it. Note the file:line.
2. Read `include/linux/list.h` end to end. Decode `list_for_each_entry`.
3. Read `include/linux/rbtree.h` and identify the macro that recovers the containing struct from an `rb_node`.
4. Read `include/linux/interval_tree.h` and explain (in your notebook) what `__subtree_last` is for.
5. `git grep "kfifo_put"` in the kernel. Pick three usages; read the surrounding code.
6. Trace one allocation through SLUB: `kmalloc` → `__kmalloc` → `kmem_cache_alloc` → ... — until you hit a lock or a per-CPU slot.

## Closing

This finishes Part DSA. **You now have a base in:**
- The standard interview DSA toolkit.
- The data structures that actually run the kernel.

Next stage: **Stage 5 of the [LEARNING PATH](../LEARNING-PATH.md)** — userspace systems programming.

→ [Part USP — Userspace systems programming](../partUSP/README.md).
