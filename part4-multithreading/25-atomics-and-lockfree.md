# Chapter 25 — Atomics, lock-free data structures

Atomics are the building block of lock-free programming, of every mutex implementation, and of every kernel concurrency primitive. Get them right and you can write data structures that scale across cores. Get them wrong and you produce subtle bugs that show up only on weak-memory CPUs.

## 25.1 The atomic operations

The C++/C atomic library exposes:

- **load**, **store**, **exchange** (atomic swap),
- **fetch_add / fetch_sub / fetch_and / fetch_or / fetch_xor**,
- **compare_exchange_weak / compare_exchange_strong** (CAS — compare-and-swap).

Each can take a memory order (`relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`).

CAS is the keystone:

```cpp
std::atomic<int> v{0};
int expected = 0;
if (v.compare_exchange_strong(expected, 1)) {
    /* succeeded: v was 0, now 1 */
} else {
    /* failed: expected was overwritten with the actual value of v */
}
```

`compare_exchange_weak` may spuriously fail (it's used in retry loops). `compare_exchange_strong` only fails if the value differs.

Almost any lock-free algorithm decomposes into "load expected; do work; CAS to commit; on fail, retry."

## 25.2 The lock-free counter

```cpp
std::atomic<int64_t> counter{0};
void inc() { counter.fetch_add(1, std::memory_order_relaxed); }
```

`fetch_add` compiles to `lock xadd` on x86 — a single locked instruction. Cost: ~10 cycles uncontended; spirals to thousands under contention from cache-line bouncing.

If you see a hot global counter, consider sharding (per-CPU/per-thread counters with a periodic rollup). That's how `vmstat`'s counters work in the kernel.

## 25.3 Lock-free stack (Treiber stack)

```cpp
template <class T>
class TreiberStack {
    struct Node { T v; Node *next; };
    std::atomic<Node*> head_{nullptr};
public:
    void push(T v) {
        Node *n = new Node{std::move(v), nullptr};
        Node *h = head_.load(std::memory_order_relaxed);
        do {
            n->next = h;
        } while (!head_.compare_exchange_weak(h, n,
                    std::memory_order_release,
                    std::memory_order_relaxed));
    }
    bool pop(T& out) {
        Node *h = head_.load(std::memory_order_acquire);
        while (h) {
            if (head_.compare_exchange_weak(h, h->next,
                    std::memory_order_acquire,
                    std::memory_order_acquire)) {
                out = std::move(h->v);
                delete h;       /* WARNING: ABA / use-after-free risk */
                return true;
            }
        }
        return false;
    }
};
```

Subtle: this works in single-threaded tests but is **broken** in the multithreaded case because of the **ABA problem**. Thread T1 reads `head=A`; T2 pops A and B, pushes A back. T1's CAS succeeds — it points head to old A's `next`, which has been freed.

Fixes:

- Hazard pointers.
- Epoch-based reclamation (used by Folly, RCU).
- Double-word CAS with a counter (limited to platforms with `cmpxchg16b`).

In the kernel, lock-free structures use **RCU** for reclamation: deleted nodes wait until all readers have passed a grace period.

> **Rule:** lock-free isn't free. Don't use it unless you've measured a lock to be the bottleneck, and you accept the complexity.

## 25.4 Single-producer single-consumer queue (revisited)

We saw it in Chapter 23. SPSC is the *one* lock-free pattern you can use confidently because it has no ABA: only the producer modifies head, only the consumer modifies tail.

This is exactly the model amdgpu uses for the **command ring**: CPU is producer; GPU is consumer.

## 25.5 The MCS / queue lock

A **mutex** that scales to many cores: each waiter spins on its own cache line; on release, the holder writes to the next waiter's line. No cache-line bouncing.

The kernel's `qspinlock` is a refined version. You don't write one — but understand the *idea*: scalable locks reduce contention by giving each waiter a distinct cache line.

## 25.6 Read-Copy-Update (RCU)

RCU lets multiple readers proceed concurrently with a writer, lock-free, with a small write-side cost. The trick:

1. **Readers** dereference the pointer, then assume the data is valid until they leave the read critical section.
2. **Writers** allocate a new version, install it via an atomic store, and *wait for a grace period* (until every CPU has gone through a quiescent state) before reclaiming the old.

```c
struct foo *p;

/* reader */
rcu_read_lock();
struct foo *f = rcu_dereference(p);
use(f);
rcu_read_unlock();

/* writer */
struct foo *new_f = make_new();
struct foo *old = rcu_replace_pointer(&p, new_f);
synchronize_rcu();
free(old);
```

Reads are *zero-cost* on a preemption-disable kernel: `rcu_read_lock()` is `preempt_disable()`, the grace-period machinery handles waiting elsewhere. Writes pay a synchronization cost.

Used pervasively in: VFS, networking, the route cache, every list with read-mostly access. The amdgpu IRQ vector list uses RCU.

Read `Documentation/RCU/whatisRCU.rst`. RCU questions show up in kernel interviews.

## 25.7 Hazard pointers (briefly)

A reader publishes a "hazard" pointer to a memory address it's reading; writers check the global hazard list before reclaiming. Used in concurrent libraries (Folly, ROCm runtime in places). RCU is generally simpler in the kernel.

## 25.8 Memory barriers in the kernel

Userland uses C++ atomics with memory orders. The kernel uses macros:

- `READ_ONCE(x)` / `WRITE_ONCE(x, v)` — atomic load/store on a plain word, prevents tearing and certain compiler reorderings.
- `smp_rmb()` / `smp_wmb()` / `smp_mb()` — runtime CPU read/write/full barriers (SMP-only; UP-no-op).
- `smp_load_acquire(p)` / `smp_store_release(p, v)` — analogous to C++ acquire/release.
- `barrier()` — compiler barrier only.

For atomics: `atomic_t`, `atomic64_t`. Operations: `atomic_inc`, `atomic_add`, `atomic_cmpxchg`, `atomic_dec_and_test`. Most are **fully ordered** by default (unlike C++); explicit relaxed variants exist as `atomic_*_relaxed`.

For reference counting, kernel uses `refcount_t` (with overflow check):

```c
refcount_t r;
refcount_inc(&r);
if (refcount_dec_and_test(&r)) free(obj);
```

## 25.9 The `seq_lock`

Optimistic concurrency. Reader checks a sequence number before and after; if it changed, retry. Writer increments the sequence (odd = in progress). No reader wait, no shared cacheline writes from readers.

```c
unsigned seq;
do {
    seq = read_seqbegin(&lock);
    /* read snapshot */
} while (read_seqretry(&lock, seq));
```

Used in `xtime` (gettimeofday) and a few other read-heavy paths.

## 25.10 Practical guidance

For our work:

1. **Default to a lock.** A mutex or spinlock is correct, simple, and 99% of the time fast enough.
2. **Use RCU** for read-mostly lookups (a list of registered objects, for example).
3. **Use SPSC ring buffers** for producer/consumer where you know the topology (CPU↔GPU command ring).
4. **Reach for full lock-free with CAS** only when the above three don't fit. Document carefully and add stress tests.
5. **Run with TSan / KCSAN** during development.

## 25.11 Exercises

1. Build the Treiber stack. Run with two threads doing 1M push/pop each. Check against ASan/TSan.
2. Demonstrate the ABA problem: pop A, pop B, free A, push A back. Show the bug.
3. Implement a tagged-pointer CAS (low bits as counter) to fix ABA. Run again under TSan.
4. Implement a per-CPU sharded counter; benchmark vs. a single atomic counter on 16 threads.
5. Read `kernel/rcu/tree.c::synchronize_rcu`; understand at a high level the grace-period detection.

---

Next: [Chapter 26 — The Linux kernel concurrency toolbox](26-kernel-concurrency.md).
