# DSA 27 — Concurrent data structures

> Lock-free queues, RCU, hazard pointers — the bridge from "DSA" to "kernel concurrency." This chapter previews ideas you'll learn deeply in Part IV.

## Why concurrent DS are special

Two threads accessing the same DS without synchronization can:
- Tear writes (one thread sees a half-updated value).
- Skip updates (lost updates in counters).
- Read freed memory (use-after-free).
- Deadlock.

**Solutions**:
1. **Lock-based** — surround critical sections with mutexes / spinlocks.
2. **Lock-free / wait-free** — use atomic ops + careful memory ordering.
3. **RCU (Read-Copy-Update)** — readers never block; writers reclaim later.

Lock-free is fast and tricky. The Linux kernel uses **all three** depending on the situation.

## Atomic operations (preview)

Operations executed indivisibly. Hardware support:

- `atomic_load`, `atomic_store`
- `atomic_fetch_add` (read + add + write atomically)
- `atomic_compare_exchange` (the **CAS** — compare-and-swap):
  ```
  if (*p == expected) { *p = new; return true; } else return false;
  ```

CAS is the universal primitive for lock-free programming.

Memory orderings (C11 / kernel `smp_*`):
- **relaxed**: atomic but no ordering with other accesses.
- **acquire**: subsequent reads see prior writes from the releasing thread.
- **release**: prior writes become visible to acquiring thread.
- **seq_cst**: total order across all threads.

Full treatment in Part IV.

## Lock-based concurrent queue

Simplest. One mutex; all operations take it.

```c
typedef struct {
    queue_t q;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
} mq_t;

void mq_enq(mq_t *m, int v) {
    pthread_mutex_lock(&m->lock);
    q_enq(&m->q, v);
    pthread_cond_signal(&m->not_empty);
    pthread_mutex_unlock(&m->lock);
}

int mq_deq(mq_t *m, int *out) {
    pthread_mutex_lock(&m->lock);
    while (m->q.size == 0) pthread_cond_wait(&m->not_empty, &m->lock);
    int rc = q_deq(&m->q, out);
    pthread_mutex_unlock(&m->lock);
    return rc;
}
```

Bottlenecks at the mutex. Fine for moderate contention.

## Single-producer, single-consumer (SPSC) lock-free queue

Two atomic indices, single producer thread, single consumer thread. Already saw the sketch in DSA-05.

```c
size_t head, tail;     // atomic
T buf[CAP];

// Producer
size_t t = atomic_load_relaxed(&tail);
if ((t + 1) % CAP == atomic_load_acquire(&head)) return FULL;
buf[t] = item;
atomic_store_release(&tail, (t + 1) % CAP);

// Consumer
size_t h = atomic_load_relaxed(&head);
if (h == atomic_load_acquire(&tail)) return EMPTY;
item = buf[h];
atomic_store_release(&head, (h + 1) % CAP);
```

**Why release/acquire?** The producer must finish writing `buf[t]` **before** the consumer sees the updated `tail`. The release on `tail` and acquire on `tail` form the synchronization edge.

**Lock-free** because no thread blocks; if interrupted at any line, others can still progress.
**Wait-free** because every operation finishes in O(1) steps regardless of contention.

The kernel's `kfifo` is essentially this.

## MPMC queue (multiple producers, multiple consumers)

Much harder. Either:
- One queue + a lock — simple, contended.
- Lock-free MPMC — Michael-Scott queue (CAS-based linked list); ABA-prone (need hazard pointers / counters).

Boost / Folly / TBB have battle-tested implementations. **Don't write your own** unless for learning. Subtle bugs and they only show up under heavy load on certain CPUs.

## ABA problem

CAS reads value `A`, gets preempted; another thread changes value to `B` then back to `A`; CAS succeeds, but the world has changed. Causes lost updates in lock-free linked lists.

**Mitigations**:
- **Tagged pointers** (low/high bits as a counter).
- **Hazard pointers** (tell the world "I'm using this pointer; don't free it").
- **RCU** (defer reclamation; when no readers reference, then reclaim).

## RCU (Read-Copy-Update) — Linux's superpower

Designed by Paul McKenney for the Linux kernel. Used **everywhere** (dcache, networking, cgroups, SLAB, IDR, KFD).

### Idea
1. Readers proceed **without any locks**, just memory barriers.
2. Writers do **copy-on-update**: clone the structure, modify the clone, atomically swap the pointer.
3. Old copy is reclaimed only after a **grace period** during which all preexisting readers have finished.

Reads scale linearly with cores. Writes pay the cost of waiting (or `call_rcu` for deferred).

### Userspace API (urcu library)
```c
rcu_read_lock();
struct foo *f = rcu_dereference(g_foo);
use(f);
rcu_read_unlock();
```

```c
struct foo *new = malloc(sizeof(*new));
*new = *old; modify(new);
rcu_assign_pointer(g_foo, new);
synchronize_rcu();             // wait for old readers
free(old);
```

### Linux kernel API
- `rcu_read_lock()` / `rcu_read_unlock()` — readers.
- `rcu_dereference(p)` — read pointer with barrier.
- `rcu_assign_pointer(p, v)` — write pointer with barrier.
- `synchronize_rcu()` — block until all readers done. **Sleeps**; can't use in atomic context.
- `call_rcu(head, fn)` — register a callback to run after the grace period; doesn't sleep.

We'll cover RCU thoroughly in Part IV chapter 4.

## Hazard pointers

Each thread publishes which pointers it's currently using. Reclaimers check the published list before freeing.

Lower overhead than RCU for some workloads. Used in some lock-free libraries (Folly, TBB).

## Per-CPU data

Avoid contention by giving each CPU its own copy. Update local; read by aggregating across CPUs.

Examples:
- Statistics counters (one per CPU; sum at read).
- Memory pools (each CPU has private slab; spillover to global).
- Network packet stats (each CPU has its own; tools sum).

The kernel API: `DEFINE_PER_CPU`, `per_cpu_ptr`, `__this_cpu_inc`. We'll use these later.

## Sequence locks (seqlock)

Optimized for "rare writers, frequent readers."

- Writer increments a sequence counter on enter and exit (odd = in progress, even = done).
- Reader reads counter, reads data, reads counter again. If counter changed or was odd → retry.

Used in: jiffies (kernel time), struct stat updates, GPS time. Reads scale to many cores; writers serialize.

## Lock-free hash table

`rhashtable` in the Linux kernel: lock-free reads via RCU, per-bucket spinlocks for writes, automatic resize. Used in TIPC, NFT, and increasingly elsewhere.

## What this chapter is for

Knowing **that** these structures exist, **why** they exist, and **roughly how** they work. You'll implement these in Part IV (the multithreading deep-dive). For DSA purposes, recognize:

- "Producer-consumer between two threads" → SPSC ring buffer.
- "Many readers, few writers, on every CPU" → RCU.
- "Counter incremented from many CPUs" → per-CPU counter, summed on read.
- "Random access to data that may resize" → rhashtable / RCU-protected hash.

## Common bugs (preview)

1. **Forgetting memory barriers** — works on x86, fails on ARM.
2. **Use-after-free** in lock-free DS — fix with hazard pointers / RCU.
3. **ABA** — fix with tagged pointers.
4. **Reading a partial pointer** on 32-bit (a 64-bit pointer is two writes) — use atomic load.
5. **Treating `volatile` as enough for synchronization** — it is **not**.

## Try it

1. Implement an MPMC bounded queue with a single mutex + condvar (the easy way). Stress-test with 4 producers, 4 consumers.
2. Implement a SPSC lock-free ring buffer. Use C11 atomics. Stress test with 1 producer, 1 consumer for 60 seconds; verify nothing lost.
3. Read `Documentation/RCU/whatisRCU.rst` (Linux). Try to summarize in 100 words.
4. Use the urcu library to convert a singly-linked list to RCU-protected. Insertions safe; deletions wait for grace period.
5. Read `lib/llist.c` (lock-less linked list). Identify the trick (single-CAS push, batch pop).

## Read deeper

- "Is Parallel Programming Hard, and If So, What Can You Do About It?" by Paul McKenney — free PDF, the bible.
- *The Art of Multiprocessor Programming* by Herlihy & Shavit — the academic foundation.
- `Documentation/RCU/` in the Linux source.
- "C++ Concurrency in Action" by Anthony Williams.

Next → [DSA in the Linux kernel — guided tour](DSA-28-kernel-ds-tour.md).
