# Chapter 60 — System-software interview question bank with answers

A curated list of questions taken from real loops. Each has a model answer that demonstrates senior-level thinking. Memorize the answers, but also be able to riff.

## 60.1 C language

**Q1. Why does this print 0?**

```c
unsigned u = 1;
int s = -1;
if (s < u) puts("a"); else puts("b");
```

**A**: Mixed signed/unsigned comparison promotes both to `unsigned int`. `(unsigned)-1` is `UINT_MAX`, greater than `1`. So we go to "b", not "a." This is integer-promotion + 2's-complement.

**Q2. What's the difference between `int *p` and `int * const p`?**

**A**: `int *p` is a pointer to int; both `p` and `*p` are mutable. `int * const p` is a const pointer to int; `*p` is mutable but `p` cannot be reassigned. Read declarations right-to-left.

**Q3. Why is this UB?**

```c
int a = INT_MAX;
a = a + 1;
```

**A**: Signed integer overflow. Compiler may assume it doesn't happen and remove subsequent overflow checks.

**Q4. What is strict aliasing?**

**A**: The compiler may assume two pointers of incompatible types don't point to the same memory; only `char*` and `unsigned char*` may alias anything. Use `memcpy` to bit-cast.

**Q5. Implement `container_of`.**

**A**:

```c
#define offsetof(T, m) ((size_t)&((T*)0)->m)
#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))
```

## 60.2 C++

**Q1. Rule of 5.**

**A**: If you define one of {dtor, copy ctor, copy assign, move ctor, move assign}, define (or `=delete`) all five. Rule of 0: prefer to need none, by composing types that manage themselves.

**Q2. What does `std::move(x)` do?**

**A**: It is `static_cast<T&&>(x)`. It does nothing at runtime. It changes the type so move overloads are picked.

**Q3. When would you reach for `shared_ptr` over `unique_ptr`?**

**A**: When ownership is genuinely shared across multiple owners with non-trivial lifetimes. Otherwise prefer `unique_ptr` — it's cheaper, single-ownership is clearer.

**Q4. Why does `std::vector<T>::push_back` move when `T`'s move is `noexcept`?**

**A**: Strong exception guarantee: if the move can't throw, growing the vector and moving elements is safe to roll back. If move can throw, fall back to copy.

**Q5. Difference between `std::function` and a templated callable parameter?**

**A**: `std::function` is type-erased — fixed type, indirect call, may allocate. Template is monomorphized — generic but produces a separate instantiation per callable.

## 60.3 Operating systems

**Q1. fork vs. exec.**

**A**: `fork` duplicates the process (COW). `exec` replaces the image. `fork`+`exec` is the standard way to start a new program with custom env/argv.

**Q2. What is virtual memory and why?**

**A**: Each process sees its own address space mapped via the MMU and page tables to physical RAM. Provides isolation, over-commit, and copy-on-write fork. We discussed in Chapter 19.

**Q3. What's the difference between user and kernel mode?**

**A**: A CPU bit (CPL=0 vs 3 on x86) decides privileged-instruction access. User code can only enter kernel via syscall/exception/interrupt. Kernel pages are not accessible to user code.

**Q4. What does `mmap` actually do?**

**A**: Adds a VMA to the process's mm; doesn't allocate physical pages. On first access, page-fault handler allocates and installs PTEs. Backing can be a file, anonymous, or device.

**Q5. CFS in one paragraph.**

**A**: Linux's default scheduler. Each task has a `vruntime`; scheduler picks the leftmost in a red-black tree keyed by `vruntime`. Tasks accumulate `vruntime` weighted by nice value.

## 60.4 Multithreading and concurrency

**Q1. Difference between `std::atomic` and `volatile`.**

**A**: `volatile` prevents compiler caching of the value but does not insert any CPU memory ordering. `std::atomic` provides both atomicity and ordering. Use `atomic` for thread synchronization; `volatile` for MMIO and signal handlers.

**Q2. Acquire-release semantics.**

**A**: A `release` store makes prior writes in the same thread visible to a thread doing an `acquire` load on the same atomic. The standard pairing for hand-off: producer writes data then `release`-stores a flag; consumer `acquire`-loads the flag, then reads data.

**Q3. Implement a SPSC ring buffer.**

**A**: head and tail are atomics. Producer writes data, then release-stores new head. Consumer acquire-loads head; if differs from tail, reads data, advances tail with relaxed store. Power-of-2 size for cheap modulo.

**Q4. ABA problem.**

**A**: A thread reads a pointer A, plans to CAS, but in between another thread pops A, pushes A again with different state. CAS succeeds, but state has changed. Mitigations: tagged pointers, hazard pointers, RCU.

**Q5. Spinlock vs mutex.**

**A**: Spinlock busy-waits; cheap if uncontended and held briefly; required in atomic contexts. Mutex sleeps; better for longer or contention-heavy critical sections. In the kernel, spinlock is the default for short, IRQ-safe sections; mutex for sleepable.

**Q6. Deadlock — how to prevent?**

**A**: Establish a global lock order; always acquire in the same order. Tools: lockdep. Use `std::scoped_lock` / `mutex_trylock` retry.

**Q7. RCU in 30 seconds.**

**A**: Readers traverse without locking. Writers publish via atomic store, then wait for a grace period (every reader has passed a quiescent state) before reclaiming the old. Trades write-side latency for zero read-side cost. Used for read-mostly data.

## 60.5 Memory model and architecture

**Q1. Why is `cache_line_size` 64 bytes?**

**A**: Industry consensus tradeoff between coherence overhead and bandwidth utilization. Larger lines mean more false sharing; smaller lines mean more tag overhead. 64 hits a sweet spot.

**Q2. What is false sharing?**

**A**: Two threads write different variables on the same cache line. The line bounces between cores' caches via coherence, killing performance. Fix: align hot per-thread data to a cache line.

**Q3. TSO vs weakly ordered (x86 vs ARM).**

**A**: x86 TSO: stores ordered, loads can pass earlier stores to other addresses. ARM weak: nearly anything can be reordered; explicit barriers required. Portable code uses C++ `<atomic>` orders.

**Q4. What is the store buffer?**

**A**: A small per-core FIFO holding pending writes that haven't yet reached the cache. Lets the core continue executing without waiting. On x86, the store buffer can reorder a younger load past an older store to a different address.

## 60.6 PCIe and DMA

**Q1. Posted vs. non-posted PCIe writes.**

**A**: Memory writes are posted: no completion expected. Reads and config writes are non-posted: require completion. A read is the way to "fence" prior posted writes.

**Q2. Coherent vs. streaming DMA.**

**A**: Coherent (`dma_alloc_coherent`): kernel returns a buffer with consistent CPU/device view, no flushing required, often uncached. Streaming (`dma_map_single`): existing buffer mapped for one transfer; CPU caches flushed/invalidated at map/unmap.

**Q3. Why might DMA fail with old hardware?**

**A**: Device's DMA mask doesn't cover the buffer's physical address. Kernel falls back to bounce buffers (SWIOTLB) — slow.

**Q4. What does the IOMMU do?**

**A**: Translates device addresses (IOVA) to physical RAM via IO page tables managed by the kernel. Provides protection (devices can only DMA to mapped regions) and abstraction (devices see virtual addresses).

## 60.7 GPU concepts

**Q1. What's a wavefront?**

**A**: AMD's SIMD execution unit — 32 (RDNA) or 64 (CDNA) lanes executing the same instruction in lock-step. Like a CUDA "warp."

**Q2. What's a fence in DRM?**

**A**: A `dma_fence` — an opaque signal-once primitive representing "this GPU work has completed." Drivers signal from IRQ; userspace and other drivers wait or attach callbacks.

**Q3. What's TTM?**

**A**: Translation Table Manager. A DRM-internal memory manager that handles VRAM/GTT pools, eviction, migration, and cross-driver sharing. amdgpu uses it.

**Q4. What's the lifecycle of an `amdgpu_cs_ioctl`?**

**A**: Parse chunks → reserve BOs (ww_mutex) → validate IBs (whitelist) → build `drm_sched_job` → push to scheduler entity → eventually scheduler runs `amdgpu_job_run` → emits PM4 packets to the ring → doorbell → GPU executes → fence packet writes seqno → IRQ → `amdgpu_fence_process` → `dma_fence_signal` → userspace's wait wakes.

**Q5. What's a doorbell?**

**A**: A small MMIO/PCIe page the GPU uses as a "wake up" notification channel. CPU writes a value (typically the new wptr); GPU's CP/MES sees it and runs new work. With KFD, doorbells are mapped per-process to userspace for low-overhead submission.

**Q6. Per-process VM — why?**

**A**: Each process has its own GPU virtual address space, isolated by the GPU's MMU + per-process VMID. Lets two processes share GPU VAs without conflict and protects against cross-process DMA.

## 60.8 Debugging

**Q1. Tell me about the hardest bug you debugged.**

**A**: Pick one real bug. Tell it as a story:

- Symptom (intermittent crash under suspend on certain laptops).
- Triage (dmesg showed a NULL deref after PSP firmware reload).
- Hypothesis (race between PSP completion and IH handler).
- Tools (ftrace, kdump, lockdep).
- Root cause (callback could race with init; needed seqcount).
- Fix (added barrier + null check; tested with stress suspend loop).
- Lessons (init paths must guard against early IRQs).

If you don't have one yet, build one through the labs.

**Q2. Walk me through what you do when you see a kernel oops.**

**A**: Capture full dmesg; find the offending function; resolve symbol via `addr2line` against vmlinux; identify the source line; reason about what state is missing. If reproducible, attach gdb / use ftrace. If a regression, `git bisect`.

**Q3. How do you find a memory leak in a long-running driver?**

**A**: Build with kmemleak; run the workload; let it scan; read the report. Cross-check with `slabinfo` for our cache growing. Add more `kmemleak_no_leak` annotations or fix the leak.

## 60.9 System design

**Q1. Design an event-driven I/O subsystem for a device that does both reads and writes asynchronously.**

**A**: Per-fd state: ring buffer for completions, two wait queues (read/write), IRQ handler enqueues. Userspace via `read`/`poll`. Async via eventfd for io_uring integration. Locking: per-fd spinlock (IRQ-safe). Fast path: tracked metric counters via `this_cpu_inc`. Safety: bounded queues, drop-or-block policy.

**Q2. Design a GPU command-submission API that minimizes ioctl overhead.**

**A**: User-mode queues: doorbell pages mapped per-process; user writes commands and rings the doorbell directly. Kernel only involved for setup/teardown/error. Sync via shared memory fences. Fairness via firmware scheduler (MES). This is essentially KFD.

**Q3. How would you guarantee a 1080p 60Hz display with no tearing on a system under heavy GPU load?**

**A**: Use atomic KMS with `DRM_MODE_PAGE_FLIP_EVENT` synced to vblank. Ensure scanout BO is pinned in VRAM (or fast GTT). Use front/back buffer chain. Set high-priority DRM scheduler for the compositor's submissions. Monitor throttling; ensure DPM stays at high power state during animation.

## 60.10 Exercises

1. Pick five questions above; record yourself answering them aloud. Cringe at the first take; iterate.
2. Find five more real questions from glassdoor / blind for the team you're targeting; add answers.
3. Pair up with a friend and do mock interviews; both sides.
4. For every C/C++ snippet here, type it out and predict; run; explain discrepancies.
5. Write a one-page "the bug I'm proudest of fixing." This becomes part of your STAR-method answer to the inevitable question.

---

Next: [Chapter 61 — Projects that get you hired](61-portfolio-projects.md).
