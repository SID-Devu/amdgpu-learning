# Chapter 32 — Time, timers, workqueues, kthreads

Drivers need to wait for things, schedule deferred work, run periodic background tasks, and time-out hardware operations. This chapter covers the API.

## 32.1 Time bases

```c
ktime_t t = ktime_get();                    /* CLOCK_MONOTONIC, ns */
unsigned long j = jiffies;                  /* tick count */
unsigned long ns = ktime_get_real_ns();     /* CLOCK_REALTIME */
```

- **jiffies** — incremented at `HZ` (typically 250 or 1000 on Linux). Use for coarse waits (seconds).
- **ktime_t** — nanoseconds, monotonic. Use for everything else.
- **`ktime_get_boottime`** — survives suspend.

Convert:

```c
msecs_to_jiffies(500)
jiffies_to_msecs(j)
nsecs_to_jiffies(n)
```

Don't compare jiffies with `<`; use `time_before(a, b)` (handles wrap-around).

## 32.2 Sleeping (process context only)

```c
msleep(100);                /* milliseconds, may sleep more */
usleep_range(100, 200);     /* microseconds, sleeping */
schedule_timeout_interruptible(msecs_to_jiffies(500));
```

Tip: `usleep_range(min, max)` lets the kernel batch wakeups; better than `udelay` which busy-waits.

## 32.3 Busy-waiting (atomic context)

```c
udelay(50);     /* microseconds */
mdelay(2);      /* milliseconds — discouraged */
ndelay(100);    /* nanoseconds */
cpu_relax();    /* PAUSE / YIELD inside spin loops */
```

Use only when you must, in atomic context, and only briefly. Long busy waits eat CPU.

## 32.4 Timers — one-shot deferred work

```c
struct timer_list t;
timer_setup(&t, my_callback, 0);
mod_timer(&t, jiffies + msecs_to_jiffies(500));

/* … */
del_timer_sync(&t);
```

`my_callback(struct timer_list *t)` runs in **softirq** context (atomic). Cannot sleep. Use for short, rare deferred work — for anything substantive prefer workqueues.

## 32.5 High-resolution timers (`hrtimer`)

```c
struct hrtimer ht;
hrtimer_init(&ht, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
ht.function = my_hr_cb;
hrtimer_start(&ht, ms_to_ktime(1), HRTIMER_MODE_REL);
```

Nanosecond-resolution wakeups. `my_hr_cb` returns `HRTIMER_NORESTART` or `HRTIMER_RESTART`.

## 32.6 Workqueues — sleeping deferred work

```c
struct work_struct w;
INIT_WORK(&w, my_work_fn);

schedule_work(&w);

/* in IRQ */
schedule_work(&dev->w);
```

`my_work_fn` runs in a **kthread** (process context). It can:

- sleep,
- take mutexes,
- copy from/to user (well, almost — but there's no userspace context, so no `copy_to_user` to *the calling* user).

Variants:

- `schedule_work(w)` — system work queue.
- `schedule_delayed_work(dw, delay)` — like timer + work.
- `queue_work(wq, w)` — on your own workqueue.
- `alloc_workqueue("mywq", WQ_UNBOUND, 0)` — for ordered or per-cpu wq.
- `cancel_work_sync(w)` / `flush_work(w)` — wait for completion.

amdgpu has many workqueues for: hot-plug, AMD Vega CG, RAS, scheduler, etc.

## 32.7 Tasklets (deprecated)

`tasklet_init`, `tasklet_schedule` — softirq-context deferred work. No new code should use them; prefer threaded IRQs and workqueues. amdgpu still has some.

## 32.8 Kthreads

A kernel-only "thread":

```c
struct task_struct *t;

t = kthread_run(my_thread_fn, dev, "amdgpu-cleanup");
/* … */
kthread_stop(t);
```

Inside `my_thread_fn`:

```c
while (!kthread_should_stop()) {
    wait_event_interruptible(wq, condition || kthread_should_stop());
    /* do work */
}
```

`kthread_stop` sets `should_stop` and wakes; thread should exit.

amdgpu uses kthreads for: GPU scheduler workers (`drm_sched_main`), suspend helper, RAS, etc.

## 32.9 RCU's `call_rcu`

If you need to defer freeing until current RCU readers finish:

```c
struct foo {
    struct rcu_head rcu;
    /* … */
};

static void foo_release(struct rcu_head *r) {
    struct foo *f = container_of(r, struct foo, rcu);
    kfree(f);
}

call_rcu(&f->rcu, foo_release);
```

Or simpler `kfree_rcu(p, rcu)`.

## 32.10 The completion (review)

For "wait until something finishes once":

```c
struct completion done;
init_completion(&done);

/* IRQ */
complete(&done);

/* waiter */
wait_for_completion(&done);
wait_for_completion_timeout(&done, msecs_to_jiffies(1000));
```

amdgpu uses completions for power-management transitions and reset stages.

## 32.11 Time-outs in waits

Always bound your waits. Hardware can hang, fences can be lost. Use timeout variants:

```c
long ret = wait_event_interruptible_timeout(wq, cond, msecs_to_jiffies(500));
if (ret == 0)        /* timed out */;
else if (ret < 0)    /* signal */;
```

```c
unsigned long t = dma_fence_wait_timeout(f, true, msecs_to_jiffies(1000));
if (t == 0) /* TDR */;
```

If a fence times out, the GPU has hung — you trigger reset (Chapter 53). This is a real and ugly path that you will work on.

## 32.12 Exercises

1. Write a module that schedules a workqueue from a timer; the work prints jiffies. Verify it runs.
2. Create a kthread that prints "tick" every second, and stops on rmmod.
3. Use `wait_event_interruptible_timeout` waiting on a flag toggled from another kthread; observe behavior on timeout.
4. Use `hrtimer` for a 100µs periodic — check accuracy with `ktime_get`.
5. Read `kernel/workqueue.c::process_one_work`. See where the worker invokes your callback.

---

Next: [Chapter 33 — Locking in the kernel](33-kernel-locking.md).
