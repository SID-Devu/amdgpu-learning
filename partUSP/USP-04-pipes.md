# USP 4 — Pipes & FIFOs

> The simplest IPC. `cmd1 | cmd2` is two processes sharing a pipe.

## Anonymous pipes

```c
int fds[2];
pipe(fds);                // fds[0] = read end, fds[1] = write end
```

A pipe is a **kernel ring buffer** (typical 64 KB). One process writes; another reads. Reads block until data; writes block when full. `EOF` on read when all write ends closed.

Anonymous pipes are inherited across `fork`, but otherwise can only be shared between related processes (i.e., a process and its descendants). For unrelated processes, use FIFOs (next section).

## Pipe + fork = command pipeline

```c
int p[2]; pipe(p);
pid_t a = fork();
if (a == 0) {
    dup2(p[1], 1);          // stdout → pipe write
    close(p[0]); close(p[1]);
    execlp("ls", "ls", "-l", NULL);
    _exit(127);
}
pid_t b = fork();
if (b == 0) {
    dup2(p[0], 0);          // stdin → pipe read
    close(p[0]); close(p[1]);
    execlp("wc", "wc", "-l", NULL);
    _exit(127);
}
close(p[0]); close(p[1]);    // parent has no use for them
waitpid(a, NULL, 0);
waitpid(b, NULL, 0);
```

This is exactly what the shell does for `ls -l | wc -l`. **You must close unused ends in every process** — otherwise the reader never sees EOF and hangs forever.

## dup2 — replace a file descriptor

`dup2(oldfd, newfd)`: closes `newfd` and makes it refer to the same file as `oldfd`. Used to "wire" pipes/files to stdin/stdout/stderr before exec.

```c
dup2(fd, 1);    // make stdout refer to whatever fd was
```

## FIFOs (named pipes)

`mkfifo("/tmp/mypipe", 0644)` creates a special file in the filesystem. Any process can `open` it and read/write — no relationship needed.

Opening `O_RDONLY` blocks until someone opens `O_WRONLY`, and vice versa. Or use `O_NONBLOCK`.

```c
mkfifo("/tmp/p", 0644);
// process A:
int fd = open("/tmp/p", O_WRONLY); write(fd, "hi", 2);
// process B:
int fd = open("/tmp/p", O_RDONLY); read(fd, buf, 2);
```

Same kernel ring buffer behind the scenes; you just have a filesystem name to find it.

Modern alternatives: Unix domain sockets (more featureful), eventfd, message queues.

## SIGPIPE — when the reader is gone

If you `write` to a pipe whose **read end is closed**, the kernel sends you `SIGPIPE`. Default action: terminate. Useful (auto-die when downstream gone), often surprising.

To handle gracefully:

```c
signal(SIGPIPE, SIG_IGN);       // now write returns -1 with errno=EPIPE
ssize_t n = write(fd, buf, len);
if (n < 0 && errno == EPIPE) { /* downstream gone */ }
```

Servers usually `SIG_IGN` SIGPIPE.

## Atomic writes — PIPE_BUF

Writes ≤ `PIPE_BUF` bytes (4096 on Linux) are **atomic**: not interleaved with concurrent writers. Bigger writes may be split.

This matters when multiple processes write to the same pipe.

## Pipe sizes

Default 64 KB on Linux. `fcntl(fd, F_SETPIPE_SZ, 1<<20)` to grow.

For high-throughput pipelines, big pipes and `splice()` (zero-copy between fds) make a real difference.

## splice / tee / vmsplice

Linux extras for zero-copy pipe data movement:

- `splice(fd_in, NULL, fd_out, NULL, len, 0)` — moves bytes between an fd and a pipe (or pipe to fd) without copying through userspace.
- `tee()` — duplicates data between two pipes.
- `vmsplice()` — splice from userspace memory.

Used by tools that move large data between sockets/files efficiently.

## Pipes vs sockets

- Pipes: byte stream, half-duplex, simple, fast for parent-child.
- Unix domain sockets: byte or datagram, bidirectional, can pass file descriptors between processes (`SCM_RIGHTS`), can authenticate the peer's UID. The choice for non-trivial IPC.

## Common bugs

1. **Forgetting to close write ends** in readers: read never sees EOF.
2. **Forgetting to close read ends** in writers: SIGPIPE never fires; the system just buffers.
3. **Using a single pipe in both directions**: no, pipes are one-way. Make two, or use a socketpair.
4. **Signal-on-write surprise**: not ignoring SIGPIPE → mysterious crashes.
5. **Reads return 0 (EOF), not -1**: handling 0 as success buffer is a classic bug.

## socketpair — bidirectional cousin of pipe

```c
int sv[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
```

Like `pipe()`, but full-duplex. Use when you need to read **and** write between two processes. Plus you can pass fds via SCM_RIGHTS.

## Try it

1. Write a program that pipes `ls | wc -l` from C using `pipe`, `fork`, `dup2`, `exec`. Verify count matches `bash -c 'ls | wc -l'`.
2. Build a 3-stage pipeline: `cat file | grep error | sort -u`.
3. Make a FIFO. Have two unrelated terminals connect: writer in one, reader in another.
4. Trigger SIGPIPE deliberately. Then ignore it; observe `write` returning -1 with EPIPE.
5. Write a program where two processes communicate bidirectionally via a `socketpair`.
6. Bonus: implement `tee` (the command) using two open files and pipe data through both.

## Read deeper

- TLPI chapters 44, 45 (pipes, FIFOs), 57–58 (sockets, Unix domain sockets).
- `man 2 pipe`, `man 2 splice`, `man 7 pipe`.

Next → [Shared memory & mmap](USP-05-shm-mmap.md).
