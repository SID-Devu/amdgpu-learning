# OST 1 — Process model: the formal view

> The OS abstracts the CPU as "many processes time-sharing one or more CPUs." Here's the formal version.

## The illusion

A modern computer has, say, 16 CPUs. Yet we run hundreds of programs **simultaneously**. The OS provides this illusion by **time-sharing** (multiplexing) and **multiplexing across cores**.

Two enabling abstractions:

- **Process**: an executing program with private memory, fds, etc.
- **Thread**: a separately schedulable execution within a process.

## Process control block (PCB)

The kernel keeps a per-process **control block** with:

- Process ID, parent PID.
- Process state (Running, Ready, Blocked, Zombie, ...).
- Saved CPU registers (when not running).
- Memory map (page tables / VMAs).
- File descriptor table.
- Signal handlers + masks.
- Credentials (uid, gid).
- Accounting info (CPU time, peak memory).

Linux's PCB: `struct task_struct` in `include/linux/sched.h`. It's **massive** (3000+ lines for the struct + bits scattered). The OS books call it the "PCB"; in Linux it's `task_struct`.

## Process state diagram

```
       New
        |
        v
   +----------+   schedule       +-----------+
   |  Ready   | <------------->  | Running   |
   +----------+   preempt        +-----------+
       ^                              |
       |  wake                         v block / wait
       |                          +-----------+
       +------------------------- |  Blocked  |
                                  +-----------+
                                       |
                                  exit |
                                       v
                                   Zombie -> reaped -> Dead
```

A process is **always in exactly one state**. The kernel's scheduler moves it.

In Linux: `R` (running), `S` (sleep, interruptible), `D` (sleep, uninterruptible), `T` (stopped), `Z` (zombie).

## Context switch

The OS pauses one process and runs another. Cost:

- Save old registers to PCB (all general regs, FP regs, page table base).
- Restore new registers.
- TLB flush (or partial — modern CPUs use ASIDs/PCIDs to avoid).
- Cache pollution as new process's data displaces old.

Cost: a few microseconds typically. **Not zero.** Excessive context switching is a real perf problem.

## Process creation models

Different OSes use different primitives:

- **fork + exec** (Unix). Two-step, flexible.
- **CreateProcess** (Windows). Single-step, gives a fully constructed new process from scratch.
- **spawn (POSIX)**. Compromise; slowly replacing fork in many embedded systems.

`fork` is convenient but can be **expensive** for large processes (copy-on-write helps, but page table copying is real). On embedded with no MMU, fork is even harder.

`vfork` (mostly historical) — share parent's address space until exec. Dangerous, sometimes used for performance.

## Threads — finer granularity

Two main models:

- **1:1** — every userspace thread is a kernel thread (Linux NPTL, Windows). Best.
- **N:1** — many user threads on one kernel thread (old "green threads"). Limited.
- **M:N** — pooling user threads onto kernel threads (Go's goroutines, Erlang processes). Best for huge concurrency.

Linux's NPTL pthreads is 1:1. The kernel scheduler sees every thread.

Go's runtime is M:N: millions of goroutines on a few kernel threads. Userspace scheduler atop kernel threads. Also true of Rust's `tokio`, Java's project Loom, etc.

## Process vs thread vs fiber

- **Process**: separate address space, OS-scheduled.
- **Thread**: shared address space, OS-scheduled.
- **Fiber / coroutine**: shared address space, **userspace**-scheduled (no kernel involvement). Cheap (~stack size), bigger pool. Used in async runtimes.

A modern app may have N processes × M threads × K fibers.

## Birth and death

- `fork`: copy yourself.
- `exec`: replace your code.
- `exit`: I'm done.
- `wait`: collect a child's exit status.

These four are the entire process model in Unix.

## Resource isolation: namespaces (preview)

In Linux, even processes share the same kernel. To **isolate**, use namespaces:

- **PID namespace**: each container sees its own PID 1.
- **Mount namespace**: each container sees its own filesystem.
- **Network namespace**: each container sees its own NICs.
- **User namespace**: map UIDs.
- **IPC namespace**: separate POSIX/SysV IPC.
- **UTS namespace**: separate hostname.
- **Cgroup namespace**: hide cgroup hierarchy.

Containers (Docker, podman) = process + namespaces + cgroups (resource caps).

## CPU scheduling preview

Once a process is **Ready**, the scheduler picks the next one to run. Goals:

- **Fairness** — every process gets some CPU.
- **Throughput** — total work done per unit time.
- **Latency** — interactive responsiveness.
- **Priority** — important tasks first.
- **Affinity** — keep tasks on cache-warm CPUs.
- **Power** — don't wake idle cores unnecessarily.

These goals **conflict.** Real schedulers compromise.

Next chapter dives in.

## Linux specifics

- Every thread has a `task_struct`. Linux schedules at thread granularity, not process.
- A process is just "a group of tasks sharing memory, fds, etc." It's `task->mm` shared.
- Cgroup v2 controls CPU, memory, IO, devices.
- The `clone()` syscall is what `fork()`, `pthread_create()`, and `unshare()` are all built on.

Read `man 2 clone` for a glimpse of how flexible Linux's process model is.

## Try it

1. Read `man 2 clone`. Look at the `CLONE_*` flags. Identify which combination = fork, which = pthread_create.
2. Read `/proc/PID/status` and `/proc/PID/stat`. Identify "state" character.
3. Check context-switch cost: write a benchmark using two threads pingpong'ing via futex; measure latency.
4. Read 2 chapters of OSTEP: "The Process" and "Process API."
5. Run a CPU-bound program. Watch in `top` how its state stays R. Run a sleep'ing program. Watch S/D states.

## Read deeper

- OSTEP chapters 4–6.
- *Modern Operating Systems* (Tanenbaum) chapter 2.
- `Documentation/scheduler/` in the kernel.

Next → [CPU scheduling theory](OST-02-scheduling-theory.md).
