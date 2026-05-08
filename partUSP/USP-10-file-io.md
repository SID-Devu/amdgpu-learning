# USP 10 — File I/O deep

> read, write, lseek, mmap, sendfile, O_DIRECT, fsync. The full menu of how bytes get to and from disk.

## The two layers

- **Standard I/O** (`stdio.h`): `fopen`, `fread`, `printf`, `fgets`. Buffered in libc.
- **Low-level I/O** (`<unistd.h>`): `open`, `read`, `write`, `lseek`, `close`. Direct syscalls.

Stdio is convenient. Low-level I/O is the truth.

`fopen` returns a `FILE *`. `open` returns an `int` (the fd). `fileno(fp)` gets the fd from a FILE.

## open flags

```c
int fd = open(path, O_RDONLY);
int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
int fd = open(path, O_RDWR  | O_CREAT | O_EXCL,  0600);   // fails if exists
int fd = open(path, O_RDWR  | O_DIRECT);                   // bypass page cache
int fd = open(path, O_RDWR  | O_NONBLOCK);                 // for fifos, ttys
int fd = open(path, O_RDWR  | O_APPEND);                   // atomic append
int fd = open(path, O_RDWR  | O_CLOEXEC);                  // fd not inherited by exec
```

Always pass mode (the third arg) when O_CREAT is used, or you get junk perms.

`O_CLOEXEC` is **best practice on every open** unless you specifically need the fd inherited.

## read, write — partial completes

```c
ssize_t n = read(fd, buf, count);
```

- `n > 0` — bytes read.
- `n == 0` — EOF (regular file) or "no data this time" (some special fds).
- `n < 0` — error; check `errno`.

`read` may return **less than asked**. Loop:
```c
size_t got = 0;
while (got < count) {
    ssize_t r = read(fd, buf + got, count - got);
    if (r < 0) { if (errno == EINTR) continue; return -1; }
    if (r == 0) break;
    got += r;
}
```

Same for `write`. Always loop.

## lseek — random access

```c
off_t pos = lseek(fd, offset, SEEK_SET);   // SEEK_CUR, SEEK_END
```

For sparse files: `lseek` past EOF and write — kernel allocates only the touched blocks. Sparse files conserve space (e.g., 1 GB log file with most of it zero takes 4 KB on disk).

## pread / pwrite — atomic positioned I/O

```c
ssize_t n = pread(fd, buf, count, offset);
```

No `lseek` race. Multiple threads can simultaneously `pread` with different offsets. Best for parallel I/O.

## readv / writev — vectored I/O

```c
struct iovec iov[3];
iov[0] = (struct iovec){.iov_base = "GET ", .iov_len = 4};
iov[1] = (struct iovec){.iov_base = path,   .iov_len = strlen(path)};
iov[2] = (struct iovec){.iov_base = "\r\n", .iov_len = 2};
writev(fd, iov, 3);
```

Single syscall, multiple buffers, all-or-nothing for consistency. Used in HTTP servers, network protocols.

## sendfile — zero-copy

```c
ssize_t n = sendfile(outfd, infd, &off, count);
```

Kernel-side copy from one fd to another without going through userspace. Used by web servers serving static files: `sendfile(socket, file, ...)`.

For pipe ↔ fd, use `splice`. For two pipes, `tee`.

## O_DIRECT — bypass the page cache

```c
int fd = open(path, O_RDWR | O_DIRECT);
```

Reads/writes go directly between user buffers and the device, skipping kernel page cache.

Constraints (architecture-dependent):
- Buffer must be page-aligned.
- Buffer size must be a multiple of block size (often 512 or 4096).
- Offset must be aligned.

Used by databases that manage their own cache (Oracle, PostgreSQL with direct_io, MySQL InnoDB).

## fsync, fdatasync, sync

`write` does not mean "on disk." It means "in kernel page cache, will be flushed eventually."

```c
fsync(fd);       // flush data + metadata
fdatasync(fd);   // flush data only (skip metadata if not strictly needed)
sync();          // flush everything system-wide
```

Crashes between `write` and `fsync` lose data. Databases call `fsync` at commit boundaries.

## File locking

### Advisory (most common)
```c
struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
fcntl(fd, F_SETLKW, &fl);     // block until acquired (whole file)
// ...
fl.l_type = F_UNLCK;
fcntl(fd, F_SETLK, &fl);
```

Other processes that **also** call `fcntl` will respect it; processes that don't, won't (advisory).

### `flock(fd, LOCK_EX | LOCK_NB)` — simpler, BSD-style.
### Open file description locks (`F_OFD_SETLK`) — modern, properly per-fd.

Used in: PID files, lock files for daemons, multi-process databases.

## Inotify — file change notifications

```c
int ifd = inotify_init1(IN_CLOEXEC);
int wd = inotify_add_watch(ifd, "/etc/hostname", IN_MODIFY);
char buf[4096];
read(ifd, buf, sizeof(buf));   // returns inotify_event records
```

Watch files/dirs for changes. Used by `tail -f`, file syncers, build tools (`watchman`).

## stat — file metadata

```c
struct stat st;
stat(path, &st);
printf("size=%ld inode=%lu uid=%d mode=%o\n", st.st_size, st.st_ino, st.st_uid, st.st_mode);
```

Tells you size, perms, owner, mtime, atime, inode, device. Useful constantly.

`fstat(fd, &st)` for an open fd.

## /tmp considerations

`/tmp` is often `tmpfs` (RAM-backed) → fast but bounded by RAM. For large temp files, use `/var/tmp` (disk-backed). For application caches, `~/.cache`.

## Async I/O alternatives

- **POSIX AIO** (`aio_read`/`aio_write`): existed for years; rarely used; mostly disappointing on Linux.
- **io_uring**: modern, fast, supports everything.

For most apps: synchronous + threads, or epoll + non-blocking sockets.

## Page cache and writeback

When you `write`, the kernel marks pages **dirty** and they sit in the page cache. The kernel writeback thread writes them to disk later (based on dirty ratio, time). On crash → unwritten data is lost. Hence `fsync`.

`echo 3 > /proc/sys/vm/drop_caches` drops clean cached pages. Useful for benchmarks (don't run in prod).

## Common bugs

1. **Forgetting to loop on partial read/write.**
2. **Using `printf` after fork without flush** — output may appear twice (parent had buffered).
3. **Forgetting `O_CLOEXEC`** — fd leaks into exec'd subprocesses.
4. **Missing `fsync`** in databases — data loss on crash.
5. **Using `gets` (or any unbounded function)** — buffer overflow.
6. **Forgetting close** — fd leak; eventually `EMFILE`.

## Try it

1. Write a function `read_file(path)` returning the whole file as a malloc'd buffer. Loop on `read` properly.
2. Implement a copy command using `read`/`write`. Then implement using `mmap`. Then using `sendfile`. Compare speeds on a 1 GB file.
3. Use `lseek` past EOF to make a 1 GB sparse file. `du -sh` shows ~0; `ls -l` shows 1 GB.
4. Implement `tail -f` using inotify.
5. Open with `O_DIRECT` and write a page-aligned 4 KB block. Read it back. Compare timing to `O_RDWR`.
6. Bonus: implement a simple write-ahead log: append, fsync, then return success. Replay on startup.

## Read deeper

- TLPI chapters 4–5, 13, 14, 15 — file I/O, buffered I/O, file metadata, file locking.
- `man 2 open`, `man 2 read`, `man 2 sendfile`, `man 2 fsync`, `man 7 inotify`.
- "Files are hard" — Dan Luu's blog post on the surprises in file durability.

Next → [/proc, /sys, /dev — the magic filesystems](USP-11-procfs.md).
