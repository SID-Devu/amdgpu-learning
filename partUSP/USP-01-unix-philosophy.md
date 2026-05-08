# USP 1 — Unix philosophy & the system call mental model

> "Do one thing well." Every program in Unix communicates with the kernel through system calls. Master both ideas; everything else falls in.

## The Unix philosophy

The four classic rules:

1. **Make each program do one thing well.**
2. **Expect the output of every program to become the input to another.**
3. **Design and build software, even operating systems, to be tried early, ideally within weeks.**
4. **Use tools in preference to unskilled help to lighten a programming task.**

Practically:
- `cat`, `grep`, `sort`, `uniq`, `wc` each do one thing. Combine via pipes.
- Text streams as the universal interface.
- "It's done" = it works. Iterate.

This shapes every Linux/Unix system you'll ever touch.

## Everything is a file (descriptor)

Open a regular file → fd. Open a TCP connection → fd. Open a pipe → fd. Open a GPU device → fd (`/dev/dri/card0`).

A **file descriptor** (fd) is a small integer index into a per-process table. Every fd points to a kernel **file object** which knows how to do `read`, `write`, `ioctl`, `mmap`, `poll`, `close`.

The fd table is per-process, copied on `fork`, kept across `exec` (unless `O_CLOEXEC`).

```c
int fd = open("hello.txt", O_RDONLY);
char buf[100];
ssize_t n = read(fd, buf, sizeof(buf));
close(fd);
```

This is the **same shape** as opening `/dev/dri/card0` and `read`-ing from it — that's how userspace talks to the GPU driver. `ioctl` is the slightly more structured variant.

## System calls — the boundary

A **system call** is the only way a userspace program asks the kernel to do something. They:
- run privileged code in **kernel mode**,
- have a small numbered table (around 400 on x86-64),
- are accessed via a special instruction (`syscall` on x86-64, `svc` on ARM64),
- return a value or `-1` (errno set on failure).

You **never** call the syscall instruction directly in normal code. Libc wraps them. `read()` is a libc function that ultimately fires the `read` syscall.

## strace — your microscope

Watch any program's system calls:

```bash
strace -f -e trace=open,openat,read,write,close ./myprogram
strace -c ./myprogram                         # summary stats
strace -p PID                                 # attach to running process
strace -tt -T ./myprogram                     # with timestamps and durations
```

If you can read `strace` output, you can understand any program's interaction with the kernel. **Do this often.**

## errno — how syscalls report errors

```c
#include <errno.h>
ssize_t n = read(fd, buf, len);
if (n < 0) {
    perror("read");                  // prints "read: Bad file descriptor"
    fprintf(stderr, "errno=%d %s\n", errno, strerror(errno));
}
```

Common errno values:
- `ENOENT` — no such file.
- `EACCES` — permission denied.
- `EINVAL` — invalid argument.
- `EAGAIN` / `EWOULDBLOCK` — try again later (non-blocking I/O).
- `EINTR` — system call interrupted by signal.
- `EBADF` — bad fd.
- `EIO` — generic I/O error.
- `ENOMEM` — out of memory.

Always check syscall return values. Always `errno` after a `-1`.

`man errno` gives the full list with descriptions.

## Man pages — your authoritative reference

Sections you care about:
- **2** — system calls (`man 2 read`, `man 2 fork`).
- **3** — library functions (`man 3 printf`, `man 3 malloc`).
- **5** — file formats (`man 5 proc`, `man 5 services`).
- **7** — overviews (`man 7 signal`, `man 7 epoll`, `man 7 socket`).

`apropos signal` lists everything related. Use it.

If you don't have a habit of reading `man` pages: get one **today**. Linux engineers live in `man`.

## The standard fd numbers

```
0 = stdin
1 = stdout
2 = stderr
```

These are inherited by every process at start. `printf` writes to fd 1. `perror` writes to fd 2. Pipes and shells redirect them.

`./prog > out.txt 2>&1` — redirect stdout to file, then dup stderr onto stdout's destination.

## Process address space — preview

A typical Linux process layout (top to bottom):

```
0x7fff...  +-------------------+
           | kernel mapping    |  (read-only window for vDSO etc)
           +-------------------+
           | stack (grows down)|
           |        ↓          |
           +-------------------+
           |     ...gap...     |
           +-------------------+
           | mmap region       |  (shared libs, mmap files)
           |        ↑          |
           +-------------------+
           |    ...gap...      |
           +-------------------+
           | heap (brk grows up)|
           |        ↑          |
           +-------------------+
           | .bss (uninit data)|
           | .data (init data) |
           | .text (code)      |
0x400000   +-------------------+
```

Ranges are virtual. The MMU maps them to physical pages. We'll go deeper in OS chapters.

`cat /proc/$$/maps` shows your shell's layout right now.

## Shells, environment, arguments

A program receives:
- `argc`, `argv[]`: command line.
- `envp[]` (or `environ`): environment variables.
- A working directory.
- Open fds 0, 1, 2.
- A signal mask + handlers (default).
- Resource limits (`ulimit`).

```c
int main(int argc, char *argv[], char *envp[]) {
    for (int i = 0; i < argc; i++) printf("argv[%d]=%s\n", i, argv[i]);
    for (char **e = envp; *e; e++) printf("%s\n", *e);
    return 0;
}
```

## "Hello, system calls"

Strip away libc:

```c
#include <unistd.h>
int main(void) { write(1, "hi\n", 3); return 0; }
```

`strace ./hi` will show only one `write` syscall (plus startup). Compare to `printf` — buffered, so `strace` shows the same write but at exit / flush.

## A note on glibc / libc

`libc` (glibc on Linux) provides:
- syscall wrappers (`open`, `read`, `write`, etc.),
- C standard library (`stdio.h`, `string.h`),
- thread library (in glibc this is integrated; previously a separate `libpthread`),
- locale, internationalization,
- math, dynamic loader, etc.

`man 7 libc` is your starting point.

For minimalism / interview understanding, **musl libc** is much smaller, more readable. Worth reading some.

## Try it

1. `strace -c ls -la /usr/bin > /dev/null` — what's the most-frequent syscall?
2. Write a 3-line program that uses `write()` to print "hi" without `printf`. Compile, `strace` it.
3. `man 2 open` — read it. Identify what each `O_*` flag means. Try `O_CREAT | O_TRUNC | O_WRONLY` then write into it.
4. Find your shell's PID. `cat /proc/$$/maps` and identify the heap, stack, and shared libs.
5. Check three errno values you don't know yet. Memorize ENOENT, EAGAIN, EINTR.
6. Read `man 7 fanotify` (or anything in section 7). Notice how documentation explains overall subsystems.

## Read deeper

- *The Linux Programming Interface* (TLPI) by Michael Kerrisk — the bible for this material. Buy this book.
- *Advanced Programming in the Unix Environment* (APUE) by Stevens — the older bible; still excellent.
- `man 2 syscalls` — list of every Linux system call with introduction version.

Next → [Processes — fork, exec, wait, exit](USP-02-processes.md).
