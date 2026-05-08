# USP 3 — Signals: interrupts to processes

> A signal is the kernel saying "drop what you're doing — this happened." Powerful, ancient, full of pitfalls.

## What signals are

A small fixed set of "interrupt numbers" the kernel can deliver to a process. Each has a name and a default action.

`man 7 signal` lists them. Common ones:

| Signal | # | Default | Triggered by |
|---|---|---|---|
| SIGHUP | 1 | term | terminal hangup; "reload config" by convention |
| SIGINT | 2 | term | Ctrl-C |
| SIGQUIT | 3 | core dump | Ctrl-\\ |
| SIGILL | 4 | core | illegal instruction |
| SIGTRAP | 5 | core | debugger |
| SIGABRT | 6 | core | `abort()` |
| SIGBUS | 7 | core | bad memory access |
| SIGFPE | 8 | core | floating-point exception |
| SIGKILL | 9 | term | **uncatchable** |
| SIGUSR1, SIGUSR2 | 10, 12 | term | user-defined |
| SIGSEGV | 11 | core | segfault |
| SIGPIPE | 13 | term | wrote to pipe with no reader |
| SIGALRM | 14 | term | `alarm()` timer |
| SIGTERM | 15 | term | polite "please exit" |
| SIGCHLD | 17 | ignore | child status changed |
| SIGCONT | 18 | continue | resume after stop |
| SIGSTOP | 19 | stop | **uncatchable** |
| SIGTSTP | 20 | stop | Ctrl-Z |

`SIGKILL` and `SIGSTOP` cannot be caught, blocked, or ignored. The kernel just delivers them.

## Default actions

- **term** — exit.
- **core** — exit + write core dump.
- **stop** — pause.
- **continue** — resume.
- **ignore** — discard.

You can override defaults (except KILL/STOP).

## Sending signals

```c
kill(pid, SIGTERM);          // send to one process
kill(-pgid, SIGINT);          // send to a process group
raise(SIGABRT);               // send to self
pthread_kill(thread, SIGUSR1);// send to a specific thread (in your process)
```

From shell: `kill -TERM 1234`, `kill -9 1234`.

## Catching signals — sigaction (the right way)

```c
#include <signal.h>
#include <stdio.h>

void on_int(int sig) {
    write(2, "got SIGINT\n", 11);     // async-signal-safe!
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_int;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;          // restart interrupted syscalls
    sigaction(SIGINT, &sa, NULL);

    for (;;) pause();
    return 0;
}
```

**Don't use `signal()`** — its semantics differ between Unixes. Use `sigaction`.

`SA_RESTART` makes most syscalls auto-retry instead of failing with `EINTR`. Useful default.

## Async-signal-safe — the rule

Inside a signal handler, you may **only** call functions on the async-signal-safe list:
- `write`, `_exit`, `kill`, `signal`, `sigaction`, `sigprocmask`, ...
- **Not** `printf`, `malloc`, `free`, `fprintf`, `fopen`, `fputs`, anything that takes locks or uses heap.

Why: a signal can interrupt your program **anywhere**. If it interrupts `malloc` and you call `malloc` in the handler → recursive call into a half-mutated allocator → corruption.

Common pattern: handler **only sets a flag**; main loop checks the flag.

```c
volatile sig_atomic_t got_int = 0;
void on_int(int sig) { got_int = 1; }

int main(void) {
    /* ... install handler ... */
    while (!got_int) {
        /* normal work */
    }
    /* clean shutdown */
}
```

`sig_atomic_t` is a type that can be read/written atomically wrt signal delivery.

## Self-pipe trick — bridging signals to a normal event loop

Make a pipe; in the handler, `write(pipefd, "x", 1)`. Add the read end to your `select`/`poll`/`epoll` set. Now signals become events in your event loop. Used in many real servers (libev, libuv).

## signalfd — modern alternative

```c
sigset_t mask; sigemptyset(&mask); sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, NULL);
int fd = signalfd(-1, &mask, 0);
struct signalfd_siginfo si;
read(fd, &si, sizeof(si));
```

Now signals come as readable data on a file descriptor — easy to integrate with `epoll`. Linux-specific.

## Blocking signals — sigprocmask

```c
sigset_t set, oldset;
sigemptyset(&set); sigaddset(&set, SIGINT);
sigprocmask(SIG_BLOCK, &set, &oldset);
// SIGINT pending but not delivered
sigprocmask(SIG_SETMASK, &oldset, NULL);
```

Use to protect critical sections that must not be interrupted. Or to mask before `pselect`/`ppoll` so you can atomically unmask while waiting.

## SIGCHLD — children dying

When a child dies:
- The parent gets `SIGCHLD` (default action: ignore).
- The child becomes a zombie until reaped.

To auto-reap with `SIGCHLD`:

```c
void reap(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) ;
}
struct sigaction sa = { .sa_handler = reap, .sa_flags = SA_RESTART | SA_NOCLDSTOP };
sigemptyset(&sa.sa_mask);
sigaction(SIGCHLD, &sa, NULL);
```

Important: signals are not queued. If 5 children die at the same time, you get **one** SIGCHLD. Always loop with `WNOHANG` until no more.

## Real-time signals (SIGRTMIN..SIGRTMAX)

- Are queued.
- Can carry a small integer payload (`sigqueue(pid, sig, value)`).
- Used in real-time apps (POSIX timers, async I/O).

Most apps use the standard signals.

## Common signals you'll write handlers for

- **SIGINT/SIGTERM** — graceful shutdown.
- **SIGHUP** — reload config (servers).
- **SIGUSR1/SIGUSR2** — toggle log level, dump stats.
- **SIGCHLD** — reap children (or `SA_NOCLDWAIT` to auto-discard).
- **SIGPIPE** — usually `SIG_IGN` so writes return EPIPE and you handle it.

```c
signal(SIGPIPE, SIG_IGN);   // okay here; for ignore.
```

## Signals and threads

Each thread has its own signal mask. A signal directed at the **process** is delivered to **any** thread that doesn't block it. Use `pthread_sigmask` to control per-thread masks. Best practice: **block signals in worker threads; have one dedicated signal-handling thread** that calls `sigwaitinfo`.

## Signals from the kernel (faults)

`SIGSEGV` is delivered when the kernel detects you accessed unmapped memory. The siginfo has `si_addr` — the bad address.

You can install a handler and try to recover (e.g., for JITs that handle SEGV from speculative reads). Tricky. Also used by garbage collectors and protected-memory tricks (mark a region read-only and SEGV → handle the write).

## Common bugs

1. **Calling unsafe functions in handlers** — undefined behavior. Always remember the list.
2. **Forgetting `volatile sig_atomic_t`** for flag — compiler may optimize away your re-checks.
3. **`signal()` instead of `sigaction()`** — older style, surprising semantics on signal storm.
4. **Not handling EINTR** when not using `SA_RESTART` — your `read` returns -1 errno=EINTR; handle it.
5. **Lost SIGCHLD** because you only call `wait` once.
6. **Race between sigprocmask and pause()** — use `sigsuspend` or `pselect` to atomically unmask + wait.

## Try it

1. Write a program that traps `SIGINT` and instead of exiting, prints "press Ctrl-C twice to quit."
2. Use `alarm(5)` and a `SIGALRM` handler to break out of a `read()` if no data arrives in 5 seconds.
3. Use `signalfd` and `epoll` to handle SIGTERM as part of an event loop.
4. Make a parent that forks 5 children. Auto-reap with SIGCHLD and `WNOHANG`. Print PIDs as they exit.
5. Simulate a server that reloads config on SIGHUP. Use a flag in the handler; in main loop, when flag set, reload and clear.
6. Bonus: `gdb` your program; trigger SIGSEGV by writing to NULL. Observe how gdb stops on signal.

## Read deeper

- TLPI chapters 20–22 — signals, signal handlers, advanced signals.
- `man 7 signal`, `man 7 signal-safety` (lists async-signal-safe functions).
- *Linux Programming Interface* author's website has signal examples.

Next → [Pipes & FIFOs](USP-04-pipes.md).
