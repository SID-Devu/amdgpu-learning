# OST 5 — Deadlock: prevention, avoidance, detection

> When threads or processes hold resources and wait for resources others hold. Forever. The textbook hazard.

## The four conditions for deadlock (Coffman, 1971)

A deadlock requires **all four**:

1. **Mutual exclusion** — at least one resource is non-shareable.
2. **Hold and wait** — a thread holding one resource waits for another.
3. **No preemption** — resources can't be forcibly taken from holders.
4. **Circular wait** — chain of threads waiting on each other.

Eliminate any one → no deadlock.

## Prevention strategies

### 1. Eliminate hold-and-wait
Acquire all resources up front. Drawbacks: poor utilization, may not know set in advance.

### 2. Allow preemption
Release a held lock if needed. `pthread_mutex_trylock` + back-off. Used in some lockless / wait-die schemes.

### 3. Impose a total order on resources (the standard fix)

If every thread acquires locks in the **same global order**, circular wait is impossible.

```c
// Always acquire A then B, never the other order.
pthread_mutex_lock(&A);
pthread_mutex_lock(&B);
// ...
pthread_mutex_unlock(&B);
pthread_mutex_unlock(&A);
```

Linux kernel does this religiously. **Lockdep** (kernel) verifies this at runtime.

For complex code, define **lock classes** and document order. E.g., "always acquire `mm->mmap_lock` before `vma->vm_lock`."

## Avoidance — the Banker's algorithm

Decide whether to grant a resource based on whether granting keeps the system in a **safe state** (i.e., some serial completion order exists).

Theoretical; rarely used in practice. Modern OSes prefer prevention.

## Detection + recovery

Allow deadlocks; periodically check the resource-allocation graph. If found, kill or roll back a process.

Used in:
- Databases (deadlock detection in InnoDB, Postgres, Oracle — kill the youngest victim, retry).
- Filesystems with complex locking.
- Linux **lockdep** is a static-style detector — runs in development kernels.

## Livelock

Like deadlock, but threads are **active** — retrying, backing off, retrying — making no progress.

Common pattern: two threads each `trylock; if fail, release and retry`. They keep failing in lockstep.

Mitigate with exponential backoff or randomization.

## Priority inversion (and inheritance)

Low-prio holds lock; high-prio blocks on it; mid-prio (no lock dependency) preempts low → high stuck. Famous example: NASA's Mars Pathfinder reset loop.

Fix: **priority inheritance** — when high-prio waits on a lock, the holder's effective priority is bumped. Linux's RT mutexes (`rt_mutex`) implement this for `SCHED_DEADLINE`/`SCHED_FIFO`.

## Lockdep — Linux's deadlock detector

`CONFIG_PROVE_LOCKING=y` activates lockdep. Every lock acquisition is recorded; lockdep tracks the **partial order** observed at runtime.

If you ever take **A→B** in one place and **B→A** in another, lockdep prints a "possible recursive locking" warning, even before a deadlock occurs. Saves countless hours.

```
=============================================
[ INFO: possible recursive locking detected ]
[...]
```

Always run kernel development kernels with lockdep on.

## Common deadlock-shaped bugs

1. **Re-entrant locking** — same thread tries to acquire a non-recursive mutex it already holds. Hangs.
2. **Calling a sleeping function with a spinlock held** — kernel BUG, system can stall.
3. **Allocating memory with `GFP_KERNEL` while holding a lock that the writeback path needs** — circular dependency on memory pressure.
4. **Two processes locking each other's PI-mutexes** — only avoided with priority inheritance.

## Strategies in real systems

| System | Approach |
|---|---|
| Databases (InnoDB, Postgres) | Detection + abort youngest |
| Linux kernel | Prevention via lock ordering + lockdep |
| Java | Wait-with-timeout, plus Java's `tryLock` |
| Go | Cooperative — use channels, avoid shared-state locks where possible |
| Erlang/Akka | No shared state; actors only |

## Algorithms tangentially related

- **Wait-die** / **wound-wait** schemes (in DBMS): age-based decisions to avoid deadlock.
- **Two-phase locking (2PL)**: acquire all locks, then release. Common in DB transactions; with strict ordering, prevents deadlock.

## Common bugs

1. **Unannotated lock order** — different files take locks differently.
2. **Lock cycles via callback** — calling user-supplied function while holding lock; user's callback re-enters your lock.
3. **Implicit lock dependency** — `printk` may take internal locks; calling from inside spinlock context can deadlock if not careful.
4. **Leaked lock** — early return with lock held.

## Try it

1. Construct a deadlock between two threads with two mutexes. Detect it with `pthread_mutex_timedlock` returning ETIMEDOUT.
2. Run with TSan; observe the "lock order inversion" report.
3. Add a third mutex; create a 3-cycle deadlock. Detect.
4. Fix all three by enforcing a global order; rerun.
5. Read `Documentation/locking/lockdep-design.rst`. Try to summarize how lockdep records "lock-acquired-after" pairs.
6. Bonus: build a tiny "lock checker" tool that records pairs `(L1, L2)` from your test program and reports cycles.

## Read deeper

- OSTEP chapter on "Common Concurrency Problems" (deadlock + non-deadlock bugs).
- "Eraser: A Dynamic Data Race Detector for Multi-Threaded Programs" (the paper behind Helgrind/TSan).
- `Documentation/locking/` in Linux.

Next → [Memory management theory](OST-06-memory-theory.md).
