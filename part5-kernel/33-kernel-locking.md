# Chapter 33 — Locking in the kernel

We've already met spinlocks, mutexes, and RCU in Part IV. This chapter is the **driver-author's checklist**: which lock to pick, which traps to avoid, and how to satisfy lockdep on your first patch.

## 33.1 Decision tree

```
Need a lock around state shared between contexts?
│
├─ Held in IRQ handler (hardirq)?            → spin_lock_irqsave
├─ Held in softirq/tasklet only?             → spin_lock_bh
├─ Held only in process context, short?      → spinlock_t
├─ Held only in process context, may sleep?  → mutex
├─ Mostly read, few writes?                  → rwsem (for read-many) or RCU (read-mostly)
└─ Single producer, single consumer?         → atomic + memory barriers, no lock
```

## 33.2 Order of locks

Always document. Place a comment at the top of your driver:

```c
/*
 * Lock ordering:
 *  dev->lock     -> bo->lock     -> fence->lock
 *  pm_lock       -> dev->lock
 */
```

Take in that order. Never the reverse.

`lockdep` will validate this empirically, but documentation is the contract for readers.

## 33.3 Common lock idioms

### Per-device mutex protecting state

```c
struct mydrv {
    struct mutex     lock;       /* protects fields below */
    struct list_head bos;
    int              n_bos;
    bool             initialized;
};
```

A single mutex is the right starting point. Optimize later.

### Spin in IRQ; mutex elsewhere

If the same data is touched from IRQ and process context:

```c
spinlock_t  lock;       /* IRQ-safe */
```

In process context: `spin_lock_irqsave(&lock, flags); … spin_unlock_irqrestore(&lock, flags);`.

In IRQ: `spin_lock(&lock); … spin_unlock(&lock);` (already in IRQ context).

### Reader/writer with seqlock

For read-heavy timing-like data:

```c
seqlock_t sl = __SEQLOCK_UNLOCKED(sl);

unsigned seq;
do {
    seq = read_seqbegin(&sl);
    /* read snapshot */
} while (read_seqretry(&sl, seq));
```

Writer: `write_seqlock`/`write_sequnlock`.

### RCU for registered objects

```c
list_for_each_entry_rcu(p, &irq_sources, node) {
    if (p->matches(...)) p->handle(...);
}
```

Writer: `list_add_rcu`, `synchronize_rcu`, `kfree`.

## 33.4 Sleeping vs. atomic context — the bug

The most common kernel bug:

```c
spin_lock(&dev->lock);
buf = kmalloc(64, GFP_KERNEL);   /* BUG: may sleep */
```

Replace with `GFP_ATOMIC`, or move the alloc outside the spinlock.

`CONFIG_DEBUG_ATOMIC_SLEEP` catches this at runtime. **Always** boot dev kernels with this on.

## 33.5 IRQ-safe variants

When the same lock is taken from IRQ:

- `spin_lock_irqsave(lock, flags)` — saves IF flag, disables IRQ on this CPU.
- `spin_lock_irq(lock)` — disables IRQ; only safe if you know IRQs are currently enabled.
- `spin_lock_bh(lock)` — disables softirq.

Mismatched pairs (lock with IRQ-save, unlock without restore) are a bug. lockdep notices.

## 33.6 Read-side cost

- **Spinlock read-only**: still a CAS + memory write. Cache line is owned exclusively when held.
- **rwlock**: read paths still write the rwlock state; cache-line bouncing under reader contention.
- **rw_semaphore**: better for sleepers; still an atomic op.
- **RCU**: zero atomic ops on the read side (preempt_disable is free on PREEMPT_NONE). Best for read-heavy.

For a global structure read by every userspace IO and modified rarely (e.g. registered IRQ source list), RCU is the right answer.

## 33.7 Lock-free counters (per-CPU)

For statistics counters that get incremented hot and read rarely:

```c
DEFINE_PER_CPU(u64, my_stat);

this_cpu_inc(my_stat);

u64 sum = 0;
int cpu;
for_each_possible_cpu(cpu) sum += *per_cpu_ptr(&my_stat, cpu);
```

Each CPU writes only its own; aggregation walks once. Extremely scalable.

## 33.8 lockdep features

- **Lock order tracking**: complains about A→B then B→A.
- **Soft/hard IRQ tracking**: a lock taken from IRQ must always be `irq_save`'d everywhere.
- **Recursive lock detection** for non-recursive locks.
- **Unlock-without-lock and lock-with-no-init checks.**

Read its splats carefully — they tell you exactly which call sites disagreed.

## 33.9 Deadlock avoidance recipes

- Lock annotation comments.
- `mutex_trylock` for "best-effort" paths; if you fail, drop everything and retry.
- `__must_check` and clean unwind.
- Avoid calling foreign code (callbacks, user code) under a lock.

## 33.10 Exercises

1. Build the chardev skeleton (Chapter 29) and add a per-device mutex protecting an internal counter. Increment from `write`, read from `read`.
2. Add a kthread that increments the same counter; ensure the lock is correct.
3. Make a deliberate lock-order inversion across two mutexes between two contexts; observe lockdep splat.
4. Replace the mutex with a per-CPU counter; verify functional equivalence.
5. Read `Documentation/locking/lockdep-design.rst`. Understand what each splat term means.

---

Next: [Chapter 34 — The driver model: kobject, sysfs, devices, drivers, buses](34-driver-model.md).
