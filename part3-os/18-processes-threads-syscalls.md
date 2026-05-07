# Chapter 18 — Processes, threads, address spaces, syscalls

## 18.1 The Linux process

Every process is a `struct task_struct` (about 11000 bytes). It contains everything the kernel knows about a runnable thing: PID, state, priority, files table, address space, scheduling stats, signal info, ptrace info.

The PID namespace tree is rooted at PID 1 (init/systemd). PID 0 is the idle task.

Get a feel:

```bash
cat /proc/$$/status        # current shell's status
cat /proc/$$/maps          # virtual address layout
cat /proc/$$/fd            # open file descriptors
ps -eLo pid,tid,stat,comm  # threads (TIDs)
```

## 18.2 Processes vs. threads on Linux

In Linux, both are `struct task_struct`. The difference is what the threads share via `clone(2)` flags:

| Flag | Shared |
|---|---|
| `CLONE_VM` | Address space |
| `CLONE_FS` | Filesystem info (cwd) |
| `CLONE_FILES` | File descriptors |
| `CLONE_SIGHAND` | Signal handlers |
| `CLONE_THREAD` | Same thread group (TGID = PID) |

Two `task_struct`s sharing all of those = "two threads in one process." Two sharing none = two independent processes. Containers tweak this with namespaces.

`getpid()` returns the **TGID** (process id). `gettid()` returns the **PID** (thread id).

## 18.3 Address space (`mm_struct`)

Each process has a `struct mm_struct` describing its virtual memory. Threads of one process share the same `mm_struct`.

`mm_struct` contains:

- a tree of `vm_area_struct` (VMAs) — contiguous mapped regions,
- a pointer to the page tables (`pgd`),
- per-process counters (RSS, etc.).

A `VMA` describes one mapping: `[start, end)`, permissions (RWX), backing file (if any), flags (anonymous, shared, private). `mmap` creates a VMA. `mprotect` changes its perms.

`/proc/self/maps`:

```
55c5a8000000-55c5a8001000 r-xp 00000000 fd:00 12345 /usr/bin/cat
55c5a8001000-55c5a8002000 r--p 00001000 fd:00 12345 /usr/bin/cat
7f0000000000-7f0000021000 r--p 00000000 fd:00 67890 /lib/x86_64-linux-gnu/ld.so
7ffd1c000000-7ffd1c021000 rw-p 00000000 00:00 0     [stack]
```

In the kernel, you'll often look up which VMA contains a given user address — `find_vma(mm, addr)`.

For GPU drivers, `mmap` is the userspace handle into device memory. `mmap("/dev/dri/card0", offset)` asks the driver to install a VMA that, on page fault, populates from a GEM buffer (Chapter 45).

## 18.4 fork, exec, wait

```c
pid_t pid = fork();
if (pid == 0) {
    /* child */
    execlp("ls", "ls", "-l", NULL);
    _exit(127);
} else if (pid > 0) {
    /* parent */
    int status;
    waitpid(pid, &status, 0);
}
```

`fork` duplicates the process with copy-on-write page tables. `execve` replaces the image. `wait`/`waitpid` reaps a child (without reaping, the child becomes a zombie).

For threads, `pthread_create` calls `clone()` directly with all the share flags.

## 18.5 The syscall mechanism on x86_64

User code:

```asm
mov  rax, 1            ; SYS_write
mov  rdi, 1            ; fd = stdout
mov  rsi, msg          ; buf
mov  rdx, msg_len      ; len
syscall
```

`syscall` reads `IA32_LSTAR` MSR (set at boot to `entry_SYSCALL_64`), saves user RIP/RSP/flags, switches to kernel stack via `swapgs`+`gs:`, and jumps. The kernel:

1. Validates the syscall number.
2. Dispatches to `sys_write` via the syscall table.
3. The handler validates user pointers (with `copy_from_user`/`copy_to_user`).
4. Returns; `sysretq` restores user state.

The full table is in `arch/x86/entry/syscalls/syscall_64.tbl`. The number for `write` is 1.

Calling convention is similar to the System V AMD64 user-space ABI but with `r10` instead of `rcx` (because `syscall` clobbers `rcx`).

## 18.6 ioctl: the catch-all syscall

```c
int ioctl(int fd, unsigned long request, void *argp);
```

`ioctl` is a per-driver custom syscall. The driver defines a set of "request" numbers (encoded with `_IOR`/`_IOW`/`_IOWR` macros in `<asm-generic/ioctl.h>`) and a struct for each. We will see *thousands* of GPU ioctls (DRM defines a base set, amdgpu adds vendor-specific ones).

Example (a DRM ioctl):

```c
#define DRM_IOCTL_AMDGPU_CS \
    DRM_IOWR(0x46, struct drm_amdgpu_cs_in)

struct drm_amdgpu_cs_in {
    uint32_t ctx_id;
    uint32_t bo_list_handle;
    uint32_t num_chunks;
    uint32_t flags;
    uint64_t chunks;       /* user pointer to array */
};
```

We'll write an entire `ioctl` in Chapter 36 and decode amdgpu ones in Chapter 49.

## 18.7 Signals

Async per-process notifications. Kernel writes to a queue; the next syscall return checks it; if a handler is registered, kernel synthesizes a frame on the user stack and invokes it.

You will rarely write signal-handling driver code. But you must know that signals can interrupt syscalls — `read` may return `-EINTR`. Userspace runtimes (HSA, HIP) deal with this on every blocking call.

Within a driver, on a wait, you should respect signals:

```c
ret = wait_event_interruptible(queue, condition);
if (ret == -ERESTARTSYS) return -ERESTARTSYS;
```

`wait_event_interruptible` (vs. `wait_event`) allows a signal to break the wait. Most user-driven driver waits should be interruptible.

## 18.8 epoll/poll/select and async I/O

Userspace `poll(2)`/`epoll(7)` waits on multiple fds. The kernel side: every `struct file` whose fops define `.poll` (or `.poll_oneshot`) participates. Drivers implement `.poll` to expose readiness.

Modern: `io_uring` is a fd-and-rings interface for asynchronous I/O. As a driver author you might expose your queue through io_uring eventually, but DRM hasn't gone there yet — DRM uses fences for async.

## 18.9 Resource limits

`/proc/<pid>/limits`. `setrlimit/getrlimit`. Important for GPU work: `RLIMIT_MEMLOCK` and `RLIMIT_AS` because GPU drivers pin memory and create huge VMAs.

## 18.10 Exercises

1. Write a C program that `fork`s 5 children, each prints its PID, then exits. Parent waits for all.
2. Write a multi-threaded program (3 threads) that increments a shared counter 1e6 times each. Observe the race; we will fix it in Part IV.
3. Write a tiny syscall using inline assembly:
   ```c
   long my_write(int fd, const void *buf, size_t n) {
       long ret;
       asm volatile("syscall"
                    : "=a"(ret)
                    : "0"(1), "D"(fd), "S"(buf), "d"(n)
                    : "rcx", "r11", "memory");
       return ret;
   }
   ```
   Use it to print "hello" without libc.
4. `strace -e ioctl -f -o trace.txt glxgears`; identify amdgpu/i915/nouveau ioctls.
5. Cat `/proc/<pid>/maps` for several processes; understand the layout.

---

Next: [Chapter 19 — Virtual memory, paging, TLBs, MMUs](19-virtual-memory.md).
