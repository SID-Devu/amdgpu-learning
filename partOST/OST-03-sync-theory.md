# OST 3 — Synchronization theory: Peterson, semaphores, monitors

> The academic foundation for the locks you've already used. Why they work; where they fail.

## Why we need sync

Two threads incrementing a shared counter:

```
load r, [counter]
add r, 1
store [counter], r
```

If interleaved arbitrarily, results are wrong. The textbook word: **race condition**. The fix: ensure the read-modify-write happens **atomically** or that **only one thread is in the critical section at a time**.

## Mutual exclusion — the goal

Three properties for a correct mutual exclusion algorithm:

1. **Mutual exclusion**: at most one thread in the critical section.
2. **Progress**: if no one is in CS and someone wants in, they enter (no spurious blockage).
3. **Bounded waiting**: a thread waiting won't wait forever.

## Peterson's algorithm (theoretical, software-only)

For two threads, with shared `flag[2]` and `turn`:

```
flag[i] = true;
turn = j;          // let the other go
while (flag[j] && turn == j) ;   // wait
// critical section
flag[i] = false;
```

Provably correct **on a sequentially consistent memory model**. Doesn't work on real CPUs without memory barriers — a beautiful illustration of why memory ordering matters.

It's mostly of historical / educational interest; real systems use hardware atomics.

## Hardware support

Modern CPUs offer atomic primitives:

- **Test-and-set (TAS)**: atomically read and set a bit. Set means "I have the lock."
- **Compare-and-swap (CAS)**: atomically replace value if it equals expected.
- **Load-linked/Store-conditional (LL/SC)** on RISC: load with reservation; store succeeds only if no other write happened.
- **Fetch-and-add**: atomic increment.

x86: `LOCK CMPXCHG`, `LOCK XADD`, `XCHG` (implicit lock).
ARM: LL/SC pair `LDAXR`/`STLXR`.

These are the building blocks of every synchronization primitive.

## Spinlock

```c
volatile int lock = 0;

void lock_acquire(void) {
    while (__atomic_test_and_set(&lock, __ATOMIC_ACQUIRE)) {
        cpu_relax();   // pause / yield hint
    }
}

void lock_release(void) {
    __atomic_clear(&lock, __ATOMIC_RELEASE);
}
```

Pros: very fast on uncontended; no syscall.
Cons: spinning wastes CPU on contention. Bad for long critical sections. **Good** in the kernel for **short** sections (especially in interrupt context where you can't sleep).

`cpu_relax()` issues `pause` on x86 — hints to the CPU that this is a spin-wait, reduces power and improves SMT sibling performance.

## Mutex (sleeping lock)

If contention is likely, **sleep** instead of spinning. Linux's `pthread_mutex_t`:

1. Try CAS to acquire (uncontended path → no syscall).
2. If contended, futex syscall to sleep.
3. On unlock, wake one waiter via futex.

Result: uncontended ≈ free; contended pays one syscall.

## Reader-writer lock

Many readers OR one writer. Useful when reads dominate. Implementation tricks:

- Counter for # readers + bit for writer.
- Avoid writer starvation (queue readers behind a waiting writer).
- Sequence locks (seqlock) — reads don't block writers; reader retries on conflict.

Linux: `rwlock_t`, `rw_semaphore`, `seqlock_t`.

## Semaphore (Dijkstra, 1965)

A counter:
- `wait()` (P): decrement; if becomes negative, block.
- `signal()` (V): increment; wake one waiter.

A binary semaphore (count {0, 1}) is essentially a mutex.

A counting semaphore is great for "limit N concurrent users":
```c
sem_init(&sem, 0, 4);   // 4 concurrent
sem_wait(&sem); /* do work */ sem_post(&sem);
```

## Monitors (Hoare/Hansen)

Higher-level abstraction: a class with implicit mutex around methods + condition variables for waiting on conditions.

```
monitor BoundedQueue {
    queue q;
    cond not_empty, not_full;

    void put(x) { while (full(q)) wait(not_full); q.push(x); signal(not_empty); }
    int take() { while (empty(q)) wait(not_empty); int x = q.pop(); signal(not_full); return x; }
}
```

Java's `synchronized` + `wait/notify` is essentially monitors. C/POSIX uses mutex + condvar to build them.

Two semantic flavors:
- **Hoare**: when a `signal` happens, the signaler immediately yields to the waiter (stack-order priority). Cleaner reasoning.
- **Mesa**: signal just makes a waiter Ready; signaler keeps running. Waiter must re-check the condition (hence the `while`!). This is what POSIX does.

The "while loop around `wait`" in your pthreads code is **specifically because** of Mesa semantics + spurious wakeups.

## Memory ordering — the modern wrinkle

Compiler and CPU may reorder reads/writes. On x86, mostly OK (TSO). On ARM/POWER, much weaker.

C11 / C++11 memory orderings:

- `relaxed`: atomic, no ordering with other accesses.
- `acquire`: subsequent reads see prior writes from the releaser.
- `release`: prior writes become visible to acquiring threads.
- `acq_rel`: both.
- `seq_cst`: total order.

A correctly written mutex uses `acquire` on lock and `release` on unlock — that's why prior writes by the unlocker are visible to the next locker.

We'll see real lock-free patterns in Part IV.

## Lock-free vs wait-free

- **Lock-free**: at least one thread always makes progress. No deadlock.
- **Wait-free**: every thread always makes progress within a bounded number of steps. No starvation.

Wait-free is much harder. Lock-free is what most "lock-free queue / map" papers actually achieve.

## Common synchronization primitives — summary

| Primitive | Use |
|---|---|
| Atomic ops | counters, flags, lock-free DS |
| Spinlock | short critical sections, especially in IRQ context |
| Mutex | normal critical sections |
| RW lock | read-heavy critical sections |
| Seqlock | rare writes, frequent reads |
| Semaphore | counting / signaling |
| Condition variable | wait-until-condition |
| Barrier | "all threads reach here before continuing" |
| RCU | read-mostly, deferred reclaim |
| Hazard pointers | lock-free pointer reclamation |
| Monitor | C-style: mutex + condvar + invariants |

## Common bugs

1. **Deadlock**: cyclic lock acquisition.
2. **Livelock**: threads constantly retry but no progress.
3. **Starvation**: some thread never gets the lock.
4. **Priority inversion** (RT): low-prio holds, high-prio blocked, mid-prio runs forever.
5. **Lost wakeup**: signaling without holding the right lock; receiver missed it.
6. **Memory ordering bug**: flag set after data without release barrier; reader sees flag, not data.

## Try it

1. Read OSTEP chapter "Locks" + "Lock-based Data Structures."
2. Implement Peterson's algorithm in C with **memory barriers**. Then strip the barriers; observe failure on ARM (or under TSan).
3. Implement a spinlock with `__atomic_test_and_set`. Compare speed to pthread_mutex on a benchmark.
4. Implement a counting semaphore using mutex + condvar.
5. Implement a producer/consumer via monitor (mutex + condvar). Write a class-style header.
6. Read `Documentation/locking/` in the kernel — `mutex-design.rst`, `rt-mutex.rst`, `spinlocks.rst`.

## Read deeper

- OSTEP chapters 28–34 (Locks, Semaphores, Condition Variables, Monitors).
- *The Art of Multiprocessor Programming* (Herlihy, Shavit) — academic but readable.
- Linus's mailing-list rant about `volatile` not being for synchronization (it's not!).

Next → [Classic synchronization problems](OST-04-classic-problems.md).
