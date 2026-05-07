# Chapter 26 — The Linux kernel concurrency toolbox

The kernel has its own concurrency primitives, optimized for kernel constraints (no general malloc in IRQ context, preemption rules, hardirq vs. softirq vs. process). This chapter is the cheat-sheet of which to use when.

## 26.1 Spinlock

```c
spinlock_t lock;
spin_lock_init(&lock);
spin_lock(&lock);
/* … critical section … */
spin_unlock(&lock);
```

- Spins (busy-waits) — never sleeps.
- Disables **preemption** while held.
- Variants:
  - `spin_lock_irq` — disable IRQs (use only in known-non-IRQ context).
  - `spin_lock_irqsave(&l, flags)` / `spin_unlock_irqrestore(&l, flags)` — save and disable IRQs, restore on unlock. Use whenever the same lock might be taken from IRQ context.
  - `spin_lock_bh` / `spin_unlock_bh` — disable softirqs.

Critical-section rules:

- **No sleeping**, ever. No `kmalloc(GFP_KERNEL)`, no `mutex_lock`, no `copy_to_user`.
- **Hold for microseconds.**
- **Don't acquire two unrelated spinlocks** without a documented order.

Use spinlock when: small critical sections; hot paths; required from IRQ.

## 26.2 Mutex

```c
struct mutex m;
mutex_init(&m);
mutex_lock(&m);
/* … critical section … */
mutex_unlock(&m);
```

- May sleep — process context only.
- Cannot be used from hardirq, softirq, or with interrupts disabled.
- Variants: `mutex_trylock`, `mutex_lock_interruptible` (returns `-EINTR` if signaled).

Use mutex when: critical section may sleep, do I/O, allocate `GFP_KERNEL`, or is long.

## 26.3 Semaphore (rare in modern kernel)

`struct semaphore`. Counting. `down`, `up`. Mostly replaced by `struct mutex` for binary use. New code rarely uses.

## 26.4 Completion

A one-shot "wait until done" primitive. Cleaner than condvars for "wait for IRQ to fire" or "wait for thread to finish" patterns.

```c
struct completion done;
init_completion(&done);

/* waiter */
wait_for_completion(&done);

/* completer (often in IRQ) */
complete(&done);
```

`complete_all(&done)` wakes all waiters and the next `wait_for_completion` returns immediately (until reinit).

amdgpu uses completions for one-shot reset and suspend/resume sync points.

## 26.5 Wait queues

Generalized "wait until predicate." More flexible than completion.

```c
DECLARE_WAIT_QUEUE_HEAD(wq);
int condition = 0;

/* waker */
condition = 1;
wake_up(&wq);

/* waiter */
wait_event_interruptible(wq, condition);
```

Use waitqueues for arbitrary wait-on-state patterns.

## 26.6 Reader-writer locks

- `rwlock_t` — spinning rwlock; prefer never; cache-thrashes on contention.
- `struct rw_semaphore` — sleeping rwsem; useful for read-heavy data.

For most cases prefer **RCU** instead.

## 26.7 RCU primitives

```c
struct foo *p = some_global;

/* readers */
rcu_read_lock();
struct foo *q = rcu_dereference(p);
use(q);
rcu_read_unlock();

/* writers */
struct foo *new_p = kmalloc(sizeof(*new_p), GFP_KERNEL);
*new_p = ...;
rcu_assign_pointer(p, new_p);
synchronize_rcu();
kfree(old_p);
```

Lists & hash:

- `list_for_each_entry_rcu(pos, head, member)` — read traversal.
- `list_add_rcu`, `list_del_rcu` — write side.

RCU works because the kernel knows the readers go through *quiescent states* (return to user, schedule, idle) at which they cannot hold an old pointer.

## 26.8 Atomic ops in the kernel

```c
atomic_t a = ATOMIC_INIT(0);
atomic_inc(&a);
atomic_dec(&a);
atomic_read(&a);
atomic_set(&a, 5);
int old = atomic_xchg(&a, 1);
int prev = atomic_cmpxchg(&a, 0, 1);
```

64-bit: `atomic64_t`.

Bit ops: `set_bit`, `clear_bit`, `test_and_set_bit`, `change_bit`. Atomic by default.

Refcounts: `refcount_t` (for safety, refuses to overflow), `kref` (RAII-ish ref structure with put callback).

## 26.9 Per-CPU variables

```c
DEFINE_PER_CPU(int, my_counter);
get_cpu_var(my_counter)++;
put_cpu_var(my_counter);
```

Or:

```c
this_cpu_inc(my_counter);     /* preempt-safe shorthand */
```

Used to scale: each CPU writes its own copy; aggregate later. amdgpu uses per-CPU caches in some allocator paths.

## 26.10 Memory barriers

| Barrier | Effect |
|---|---|
| `barrier()` | compiler only |
| `smp_rmb()` | runtime read barrier (SMP) |
| `smp_wmb()` | runtime write barrier |
| `smp_mb()`  | full barrier |
| `smp_load_acquire(p)` | acquire-load |
| `smp_store_release(p, v)` | release-store |
| `mb()`, `rmb()`, `wmb()` | unconditional (for MMIO) |

Pair acquires with releases. Most subsystems prefer `smp_load_acquire`/`smp_store_release` over raw barriers.

## 26.11 `lockdep`

Run-time detector for:

- Lock-order inversion.
- Recursive lock attempts.
- Sleeping with spinlock.
- Misuse of irq-safe vs irq-unsafe locks.

Enable with `CONFIG_PROVE_LOCKING=y`. Whenever you boot a development kernel, **enable lockdep**. Many subtle deadlocks become detectable on the first run.

## 26.12 KCSAN

Kernel Concurrency Sanitizer. Like TSan for the kernel. `CONFIG_KCSAN=y`. It finds data races on shared memory.

## 26.13 Practical patterns

- **Per-device state protected by one mutex.** Default starting point.
- **Fast path read with RCU; slow update with mutex.** For lookups (registered objects, etc.).
- **IRQ → workqueue with spinlock between.** For interrupt-driven event delivery.
- **Atomic counter for `n_users`; mutex for invariants.** A common combo.
- **`refcount_t`, `kref`, `kref_get`, `kref_put`** for object lifetime.

## 26.14 Exercises

1. Write a tiny kernel module with a `spinlock_t` protecting a global counter. Increment from a kthread; ensure no race (atomics would be cleaner — practice both).
2. Add a workqueue handler that processes items from a queue protected by a mutex.
3. Set up an `rcu_read_lock` reader and an `rcu_assign_pointer + synchronize_rcu + kfree` writer over a small list. Watch with `lockdep` to confirm no warnings.
4. Boot your dev kernel with `CONFIG_PROVE_LOCKING=y` and provoke a lock-order inversion deliberately. Read the lockdep splat in dmesg.
5. Read `Documentation/locking/locktypes.rst`. It is the official summary and matches this chapter closely.

---

Next: [Chapter 27 — GPU concurrency: fences, queues, doorbells](27-gpu-concurrency.md).
