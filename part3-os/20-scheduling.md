# Chapter 20 — Scheduling: CFS, deadline, real-time

The kernel scheduler decides which task runs on which CPU and for how long. Drivers don't *implement* scheduling, but driver code that runs in process context (your `read`, `write`, `ioctl`) must understand:

- when it can sleep,
- when it must not,
- how long it might block,
- preemption.

## 20.1 Scheduling classes

Linux has multiple scheduling classes, sorted by priority (highest first):

1. **Stop** — kernel-internal; pre-empts everything.
2. **Deadline (`SCHED_DEADLINE`)** — real-time; tasks declare runtime/deadline/period; EDF + CBS.
3. **Real-time (`SCHED_FIFO`, `SCHED_RR`)** — fixed-priority; preempts CFS.
4. **CFS (`SCHED_NORMAL`/`SCHED_BATCH`/`SCHED_IDLE`)** — fair scheduler; default.
5. **Idle** — runs only when nothing else can.

For driver work, almost everything you create is `SCHED_NORMAL`. Some IRQ threads become `SCHED_FIFO`. GPU schedulers use kthreads at default priority.

## 20.2 The CFS algorithm

CFS = Completely Fair Scheduler. Tracks per-task **vruntime** (virtual runtime: real runtime weighted by nice value). The runqueue is a red-black tree keyed by `vruntime`. Pick the leftmost. Run it for a slice; resume.

Nice values from -20 (highest priority) to +19 (lowest) scale the rate at which `vruntime` grows. A nice -10 task accumulates `vruntime` ~10x slower than nice 0, so it runs 10x more often.

You won't need to modify CFS. You need to know: **fair, dynamic, no starvation, no fixed priority among `SCHED_NORMAL` tasks**.

## 20.3 Preemption

Linux is *preemptible*: when a higher-priority task wakes, the running task can be kicked off. Configurable:

- `CONFIG_PREEMPT_NONE` — never preempt in kernel mode (servers).
- `CONFIG_PREEMPT_VOLUNTARY` — preempt at explicit `cond_resched()` points.
- `CONFIG_PREEMPT` — preempt almost anywhere.
- `CONFIG_PREEMPT_RT` — fully preemptible kernel for RT use.

Most desktop distros use `CONFIG_PREEMPT`. Server distros use `CONFIG_PREEMPT_NONE`. As a driver author, **assume `CONFIG_PREEMPT_RT` is possible** — your code must use locks correctly even when interrupted at the worst moments.

## 20.4 Blocking, sleeping, scheduling

Inside a syscall, your driver code can:

- **Run** (CPU-bound).
- **Sleep** waiting for an event (preferred).
- **Spin** waiting for hardware (only for very short waits).
- **Yield** with `cond_resched()` for long CPU-bound loops.

To sleep on an event, kernel pattern:

```c
DECLARE_WAITQUEUE(wait, current);

add_wait_queue(&dev->wq, &wait);
for (;;) {
    set_current_state(TASK_INTERRUPTIBLE);
    if (cond) break;
    if (signal_pending(current)) { ret = -ERESTARTSYS; break; }
    schedule();
}
__set_current_state(TASK_RUNNING);
remove_wait_queue(&dev->wq, &wait);
```

Most code uses the `wait_event_interruptible*` macros that wrap that pattern.

`schedule()` is the call that voluntarily gives up the CPU.

## 20.5 Atomic context

Some kernel code runs **atomically** — it cannot sleep. If you sleep there, the system will deadlock or bug. The contexts:

- **Hard IRQ handler** (top half).
- **Softirq / tasklet**.
- **Holding a spinlock**.
- **`preempt_disable()`** region.
- **RCU read-side critical section** (cannot block).

In atomic context:

- No `kmalloc(GFP_KERNEL)` (use `GFP_ATOMIC`).
- No mutexes (use spinlocks).
- No `copy_from_user`/`copy_to_user` (they may page-fault and sleep).
- No `msleep`/`schedule` — only `udelay`/`mdelay` (busy-wait).
- No userspace access of any kind.

Driver writers learn this distinction by debugging the first time they put `mutex_lock` in an interrupt handler and crash.

## 20.6 Affinity, CPU pinning, isolation

```c
sched_setaffinity(pid, sizeof(mask), &mask);
```

Pins a thread to specific CPUs. For latency-critical work (GPU command submission threads), pinning helps cache locality. Boot parameter `isolcpus=2-7` removes CPUs from CFS, letting userspace pin RT work to them.

amdgpu's scheduler kthreads are not pinned by default but can be tuned by the kernel scheduler.

## 20.7 IRQ threads and kthreads

The kernel can route hardware interrupts to a dedicated **kthread** (rather than running them in hard IRQ context). This lets the handler sleep, take mutexes, and be preempted. Achieved with `request_threaded_irq()`. We will use this pattern in Chapter 22.

A **kthread** is a kernel-only "thread" — `kthread_create`, `kthread_run`, `kthread_stop`. amdgpu has *many* kthreads: command-submission worker, suspend/resume helper, RAS handler.

## 20.8 Latency budgets

For realtime audio: < 1 ms per buffer.
For graphics frame: 16.6 ms (60 Hz).
For GPU command submission overhead: < 100 µs from ioctl entry to job queued.

Driver code in the user-syscall path must keep latency in mind. Holding a mutex too long blocks all callers. Doing work in hard IRQ delays everyone.

## 20.9 The `current` macro and `task_struct`

`current` returns a `struct task_struct *` for the currently-running task. From it you can read:

```c
current->pid, current->comm, current->mm,
current->files, current->signal,
current->cred->uid, current->prio
```

But: many fields require locks; not everything in `task_struct` is safe to read from any context. Stick to the obvious fields.

## 20.10 Exercises

1. Read `Documentation/scheduler/sched-design-CFS.rst`.
2. Run `chrt -f 50 ./your_program` to launch with `SCHED_FIFO` priority 50. (Requires CAP_SYS_NICE.) Observe with `top -p $(pidof your_program)`.
3. Write a kernel module that creates a kthread that prints "tick" every second.
4. In your kthread, deliberately call `mutex_lock` then `msleep` to verify it works in a sleeping context. Then put the same code in a tasklet — observe the BUG.
5. Read `kernel/sched/fair.c::pick_next_task_fair` (a few hundred lines of dense code). Don't try to understand all of it. Recognize the rb-tree traversal.

---

Next: [Chapter 21 — The I/O stack — VFS, page cache, block layer](21-io-stack.md).
