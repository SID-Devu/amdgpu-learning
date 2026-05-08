# OST 2 — CPU scheduling theory

> Many algorithms exist. Each prioritizes different goals. Linux's CFS is one branch on this tree.

## The scheduling problem

Given:
- a set of processes (or threads) waiting to run,
- one or more CPUs,
- some constraints (priorities, deadlines, fairness),

decide **which one runs next**, and **for how long**.

## Metrics

Different schedulers optimize different metrics:

- **Throughput**: jobs completed per unit time.
- **Turnaround time**: completion time − arrival time.
- **Response time**: first-run time − arrival time.
- **Waiting time**: time spent in ready queue.
- **Utilization**: % of CPU busy.
- **Fairness**: how evenly time is shared.

A scheduler can't be best on all metrics. It picks a workload assumption.

## Classic algorithms

### FCFS (First-Come-First-Served)
Just a FIFO queue. Simple. **Convoy effect**: a long job blocks short jobs behind it. Bad for response time.

### SJF (Shortest Job First)
Run the shortest available job. Optimal for average waiting time. Requires knowing burst length (oracle), so impractical for general scheduling. Used as a model for analysis.

### SRTF (Shortest Remaining Time First) — preemptive SJF
If a new job has shorter remaining time than the current one, switch. Optimal in theory; impractical (oracle problem).

### Round-Robin (RR)
Each ready job gets a fixed **time slice** (e.g., 10 ms). Then preempt and rotate. Fair; good response time. Tuning quantum is critical: too small → too many context switches; too big → poor latency.

### Priority scheduling
Each job has a priority; highest priority runs. Possible **starvation** of low-priority jobs. Mitigation: **aging** (raise priority over time).

### MLFQ (Multilevel Feedback Queue)
Multiple queues with different priorities. New jobs start at top; if they use full quantum, demoted. If they yield (I/O bound), kept high. Approximates SJF without oracle.

Used in Solaris, classic Unix BSD/SVR4. Good general-purpose.

### Lottery scheduling
Each process holds "tickets." Scheduler picks a random ticket; that process runs. Probabilistic fairness, easy to implement, no aging needed.

Not used in real OSes (probabilistic fairness is hard to debug), but academically beautiful.

### Stride scheduling
Deterministic version of lottery. Each process accumulates a "pass" with stride proportional to weight; lowest pass runs.

### Fair-share / Proportional share
Allocate CPU **per user / per group**, not per process. Cgroups in Linux do this.

### CFS (Completely Fair Scheduler)
Linux's default. Each task tracks **virtual runtime** (`vruntime`); the scheduler always runs the task with **least vruntime**. Effectively keeps everyone "equally behind." Implemented as an RB-tree on vruntime.

Weighted by `nice` value (priority): low-nice tasks accumulate vruntime slower. Result: fair sharing scaled by priority.

`Documentation/scheduler/sched-design-CFS.rst`.

### EEVDF (Earliest Eligible Virtual Deadline First)
Newer Linux scheduler (replacing CFS in 6.6+). Improves on CFS for low-latency interactive workloads. More principled approach to wake-up balance.

### Deadline scheduling (EDF, RM)
For real-time tasks with deadlines:
- **EDF (Earliest Deadline First)**: highest priority = nearest deadline.
- **RMS (Rate Monotonic)**: shorter period → higher priority. Static priorities.

Used in real-time kernels and Linux's `SCHED_DEADLINE`.

## Linux's scheduling classes

```
   SCHED_DEADLINE   (highest)  — deadline scheduler, real-time hard
   SCHED_FIFO                  — real-time FIFO, run until yield
   SCHED_RR                    — real-time RR, time-sliced
   SCHED_NORMAL    (CFS)       — default, fair share
   SCHED_BATCH                 — slightly less interactive bias
   SCHED_IDLE      (lowest)    — only when nothing else
```

A task has a class. Within `SCHED_NORMAL`, CFS (or EEVDF). Real-time always preempts normal.

## Multi-CPU scheduling

Per-CPU run queues. **Load balancing** moves tasks between CPUs to keep all busy. Trade-off: cache locality (don't move) vs balance (do move).

- **Affinity** (`sched_setaffinity`): pin a task to specific CPUs. Used for performance-critical code.
- **NUMA**: prefer local memory. Linux's NUMA balancer migrates pages to where threads run.

GPU work submission and isr handlers benefit from affinity. We'll see this in driver chapters.

## Preemption — when does a task lose CPU?

- **Cooperative**: only when it yields. Old systems, JS engines, some real-time. Bad for misbehaving tasks.
- **Preemptive**: timer interrupt fires; scheduler may switch. Standard now.

Linux is preemptive in user-mode and (in `PREEMPT=y` kernels) in kernel-mode too. Real-time variants (PREEMPT_RT) make almost everything preemptible.

## Wake-up & sleep

Tasks block on:
- I/O (read from disk, network).
- Mutex/condvar.
- Sleep (`nanosleep`).
- Wait for child.

When the event happens, the kernel **wakes** the task: changes state to Ready, possibly migrates CPU, possibly preempts current task.

Wake-up balancing decisions are subtle and are a frequent source of "scheduler tweaks" in patches.

## Real-time scheduling

Hard real-time: missing a deadline is a failure (medical, automotive, GPU compositors arguably).

Linux ≠ hard RT, but with `PREEMPT_RT` patches and `SCHED_DEADLINE`, can be soft RT.

Key concept: **priority inversion** — a low-prio task holding a lock blocks a high-prio task. Solution: **priority inheritance** (the holder's priority is bumped to the highest waiter). Linux's RT mutexes implement this.

## Schedulers in the wild

- Linux: CFS / EEVDF + SCHED_DEADLINE + RT classes.
- Windows: priority-based with quanta and dynamic boosts.
- macOS / iOS: Multi-Level Feedback Queue (Mach derivative).
- AWS Firecracker: Linux scheduler, but constrained.
- AWS Lambda: cgroup-isolated containers.

## What's hard

Scheduling is full of edge cases:

- Wake-up storms (1000 tasks ready at once).
- Cache + NUMA (move = cache miss explosion).
- Power management (waking a sleeping CPU costs energy).
- Priority inversion + livelock.
- Thermal throttling (the CPU runs slower; scheduler unaware).

Read the changelogs of `kernel/sched/`. Almost every release tweaks something.

## Try it

1. Read OSTEP chapters on Scheduling (introduction + MLFQ + CFS).
2. Run `chrt` to set a process to SCHED_FIFO. Watch how it preempts CFS tasks. **Don't run anything broken with high RT priority — it can hang the whole system.**
3. Pin a thread with `sched_setaffinity` to CPU 0; observe in `htop`.
4. Read `Documentation/scheduler/sched-design-CFS.rst`.
5. Pick a long-form `lkml` thread on a recent scheduler patch. Skim it.

## Read deeper

- OSTEP chapters 7–10 (Scheduling).
- Linux: `kernel/sched/` and `Documentation/scheduler/`.
- "EEVDF: A Latency-Aware Scheduler for Linux" — recent overview articles.

Next → [Synchronization theory](OST-03-sync-theory.md).
