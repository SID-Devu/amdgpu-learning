# Part USP — Userspace systems programming

> **Stage 5 of the [LEARNING PATH](../LEARNING-PATH.md). 4–6 weeks at 1 hr/day.**

You've done C and DSA. Time to **master Unix as a user**. Every concept here has a kernel counterpart you'll learn later — but doing it from userspace first is **non-negotiable**.

## Why this part exists

Most books jump from "C basics" to "kernel modules." That's a leap. Between C and the kernel, there's a whole layer:

- How a program becomes a process.
- How processes communicate.
- How threads share memory.
- How files actually get to disk.
- How sockets push bytes across the network.
- How `/proc` exposes the kernel back to userspace.

A junior engineer who's never written `fork()`, `pipe()`, `epoll_wait()`, or `mmap()` has no business poking at GPU kernel code. **Master userspace first.**

When you reach the kernel, every concept will feel familiar — "oh, the kernel `cdev_alloc` is just like `open()`-but-from-the-other-side."

## Table of contents

| # | Chapter | What you'll be able to do |
|---|---|---|
| 1 | [Unix philosophy & system call mental model](USP-01-unix-philosophy.md) | Read a man page; trace any program's system calls |
| 2 | [Processes — fork, exec, wait, exit](USP-02-processes.md) | Write a tiny shell |
| 3 | [Signals — interrupts to processes](USP-03-signals.md) | Handle Ctrl-C, deferred quits, parent notification |
| 4 | [Pipes & FIFOs — the simplest IPC](USP-04-pipes.md) | Chain commands; build pipelines |
| 5 | [Shared memory & mmap](USP-05-shm-mmap.md) | Zero-copy IPC; map a file as a buffer |
| 6 | [Message queues, semaphores, futexes](USP-06-mq-sem.md) | Synchronize processes |
| 7 | [Sockets — TCP & UDP](USP-07-sockets.md) | Write a chat server, an echo server |
| 8 | [I/O multiplexing — select, poll, epoll, io_uring](USP-08-epoll.md) | Handle 10,000 connections in one thread |
| 9 | [pthreads deep — locks, condvars, barriers](USP-09-pthreads.md) | Write robust multi-threaded servers |
| 10 | [File I/O deep — fd, blocking, O_DIRECT, sendfile](USP-10-file-io.md) | Read/write large files efficiently |
| 11 | [/proc, /sys, /dev — the magic filesystems](USP-11-procfs.md) | Inspect any running system |
| 12 | [Userspace performance & debug tools](USP-12-perf-tools.md) | Use strace, ltrace, perf, valgrind, gdb fluently |

## What you'll build

By the end:

- a tiny **shell** that does `fg`, `bg`, pipes, signals;
- a **TCP echo server** scaling to 10,000 connections via `epoll`;
- a **multi-process** producer/consumer with shared memory;
- a **file copy** tool using `sendfile` for zero-copy speed;
- familiarity with `strace`, `perf`, `valgrind`, `gdb` such that you can debug any C program.

These projects + your DSA repo = **the portfolio** that gets you the FTE phone screen.

## Mental model

In Unix:

> **Everything is a file descriptor**, **every kernel service is a system call**, **every system call is a number** (`/usr/include/asm-generic/unistd.h`).

A process has:
- a memory address space,
- a table of open file descriptors,
- credentials (uid, gid),
- signal handlers,
- threads sharing all of the above.

Once you internalize this, the kernel side becomes obvious — the kernel is **the implementer of the system calls**, and a driver is **a backend for some file's read/write/ioctl**.

## How to study this part

Each chapter:
1. Read it. Type all examples.
2. Run them. Modify them.
3. Use `strace -f` to see the system calls actually fired.
4. Read the relevant `man` pages — `man 2 fork`, `man 2 epoll_wait`, etc.
5. Do the exercises.

Go in order. Don't skip — chapters depend on each other.

→ [USP-01 — Unix philosophy & system call mental model](USP-01-unix-philosophy.md).
