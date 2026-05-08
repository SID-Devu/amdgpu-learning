# USP 2 — Processes: fork, exec, wait, exit

> The four system calls that make Unix what it is.

## Process — what is it really?

A **process** is a running program plus its environment:
- a unique PID (process ID),
- its own virtual address space (memory),
- a file descriptor table,
- credentials (uid/gid),
- signal handlers,
- a parent process,
- maybe children.

`getpid()` gives your PID. `getppid()` gives your parent's. `ps -ef` lists all.

## fork() — make a copy of yourself

```c
#include <unistd.h>
pid_t pid = fork();
if (pid == 0) {
    // we're the CHILD. fresh PID.
    printf("child: pid=%d ppid=%d\n", getpid(), getppid());
} else if (pid > 0) {
    // we're the PARENT. pid is the child's PID.
    printf("parent: child=%d\n", pid);
} else {
    perror("fork");
}
```

After `fork`:
- A new process exists.
- Both processes resume **at the same line** (the line after `fork()`).
- They have **identical** memory, fds, working directory, etc. — but separate copies (copy-on-write under the hood).
- `fork()` returns 0 in the child, the child's PID in the parent.

This is the most fundamental Unix idiom. Read it three times.

## exec() — replace yourself with another program

`exec` family is **6 functions** (`execl`, `execlp`, `execv`, `execvp`, `execve`, `execvpe`) — same syscall, different convenience.

```c
execl("/bin/ls", "ls", "-la", NULL);
// only reached if exec FAILED
perror("execl");
```

`exec` does not return on success. The current process **becomes** `/bin/ls`. PID stays the same; memory/text/data are replaced.

Variants:
- `execl` — args as varargs, full path required.
- `execlp` — searches PATH.
- `execv` — args as `char *argv[]`.
- `execve` — full control: env passed explicitly.

## fork + exec — the standard combo

To run a new program **without losing yourself**, fork then exec:

```c
pid_t pid = fork();
if (pid == 0) {
    execl("/bin/ls", "ls", "-la", NULL);
    perror("execl"); _exit(127);
}
int status;
waitpid(pid, &status, 0);
```

This is exactly how a shell runs commands. The child becomes `ls`; the parent waits.

## wait() / waitpid() — reap children

When a process exits, its kernel state lingers as a **zombie** until the parent calls `wait`. `wait` returns the exit status.

```c
int status;
pid_t child = waitpid(pid, &status, 0);
if (WIFEXITED(status)) printf("exited: %d\n", WEXITSTATUS(status));
if (WIFSIGNALED(status)) printf("killed by signal %d\n", WTERMSIG(status));
```

If you don't reap, zombies accumulate (visible in `ps`). Big bug source in long-running programs.

`waitpid(-1, ...)` waits for any child. `waitpid(pid, ..., WNOHANG)` returns immediately if no child has exited (poll style).

## exit() — proper termination

```c
exit(0);     // calls atexit handlers, flushes stdio, closes fds, then _exit
_exit(0);    // raw: skip atexit, do not flush. Use in child after fork() failure.
return 0;    // from main: same as exit(0)
```

After `fork()` in the child, **before** `exec`, use `_exit` on errors — `exit` would flush parent's stdio buffers (which were inherited as a copy) twice.

## Orphans, zombies, init

- **Zombie**: dead but not yet waited for.
- **Orphan**: parent died first; orphans are re-parented to PID 1 (init / systemd), which reaps them automatically.

Long-running daemons traditionally `fork` and let the parent exit so the child becomes an orphan adopted by init — "daemonization."

## A tiny shell

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    char line[1024];
    while (printf("$ "), fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;        // trim newline
        if (strcmp(line, "exit") == 0) break;
        if (line[0] == 0) continue;

        char *argv[64]; int n = 0;
        char *t = strtok(line, " ");
        while (t && n < 63) { argv[n++] = t; t = strtok(NULL, " "); }
        argv[n] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp(argv[0], argv);
            perror("execvp"); _exit(127);
        } else if (pid > 0) {
            int status; waitpid(pid, &status, 0);
        }
    }
    return 0;
}
```

That's a (very minimal) shell. Add pipes, redirects, signals, environment substitution, and you have bash.

## Process groups & sessions (preview)

- **Process group**: a set of processes that can be signaled together (e.g., `Ctrl-C` kills the whole foreground pipeline).
- **Session**: a set of process groups, typically one terminal.

`setsid()`, `setpgid()`. Critical for shell job control and daemonization. We'll cover these only briefly — TLPI has a great chapter.

## getpid, getppid, getuid, getgid

Identity functions. Cheap. Often used in logging.

`getuid()` returns real user id; `geteuid()` effective (after setuid bit).

## Resource limits

```c
#include <sys/resource.h>
struct rlimit r;
getrlimit(RLIMIT_NOFILE, &r);
printf("max fds: soft=%lu hard=%lu\n", r.rlim_cur, r.rlim_max);
```

`ulimit -n` in the shell. Used to cap memory, CPU, fd count, core dumps. Essential when running services.

## Daemonization (briefly)

Modern systemd-managed services don't daemonize themselves — systemd manages their lifecycle. But classic daemon recipe:

1. `fork()`; parent exits.
2. `setsid()` to detach from terminal.
3. `fork()` again (so we can never reacquire a terminal).
4. `chdir("/")` so we don't keep a busy mount.
5. `umask(0)`.
6. Close fds 0, 1, 2; reopen to `/dev/null`.

Modern: just write a normal foreground program and let systemd run it.

## Common bugs

1. **Not checking fork's return value** — race conditions when fork fails.
2. **Forgetting to reap children** — zombie accumulation.
3. **Calling exit() in the child** before exec — flushing buffered I/O twice.
4. **Misordering**: parent reads from a pipe before child has written — depends on schedule, races.
5. **Inherited fds**: parent's fds are inherited; sometimes you want to close them with `O_CLOEXEC` so exec'd program doesn't get them.
6. **PATH security**: `execlp("ls", ...)` searches PATH. Untrusted PATH → arbitrary code.

## Try it

1. Write the tiny shell above. Add `cd` (must be built-in — why?). Add `&` for background.
2. Spawn 5 children. Use `waitpid` to reap them in order; print each exit status.
3. Demonstrate a zombie. Fork a child; in parent, sleep 30s without `wait`. In another terminal, `ps -ef | grep defunct`.
4. Demonstrate orphan. Parent exits before child. Inspect parent (ppid) inside the child after parent has died.
5. Read `man 2 fork`. What's NOT inherited by the child? (Hint: pending alarms, file locks under some conditions, async I/O.)
6. Bonus: implement `&` background and `wait` to reap them when they finish, without using SIGCHLD yet (we do signals next).

## Read deeper

- TLPI chapters 24, 25, 26, 27 — process creation, exec, exit, monitoring.
- `man 2 fork`, `man 2 execve`, `man 2 wait`, `man 2 _exit`.
- The original "fork" paper (1965, Project MAC) is a curiosity worth a look.

Next → [Signals](USP-03-signals.md).
