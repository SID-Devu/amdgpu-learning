# USP 5 — Shared memory & mmap

> The fastest IPC: zero-copy because there's literally one copy, shared. And `mmap` is the same trick on a file.

## What `mmap` actually does

`mmap` asks the kernel to make a region of memory in your process appear as a window onto something — usually a file or a chunk of anonymous memory. After `mmap`, you read/write **memory** and the kernel handles "what to do with that data."

```c
#include <sys/mman.h>

void *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
if (p == MAP_FAILED) perror("mmap");
// now p[0..len) is a window onto fd from offset 0
munmap(p, len);
```

Key knobs:
- `PROT_READ`, `PROT_WRITE`, `PROT_EXEC` — protections.
- `MAP_SHARED` — writes flush to the file (or shared with other processes).
- `MAP_PRIVATE` — copy-on-write; writes never reach the file.
- `MAP_ANONYMOUS` — no file; just memory (for `MAP_SHARED` between forked children, or for a custom allocator).
- `MAP_FIXED` — pin to a specific address (dangerous; use `MAP_FIXED_NOREPLACE` instead).

## File-backed mmap

```c
int fd = open("data.bin", O_RDWR);
struct stat st; fstat(fd, &st);
char *p = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
p[0] = 'X';                       // writes the file
msync(p, st.st_size, MS_SYNC);    // force flush
munmap(p, st.st_size); close(fd);
```

Beautiful when you want to treat a big file as a giant array. Used by:
- the dynamic loader (mapping `.so` files into memory),
- databases (LMDB, SQLite mmap mode),
- text editors (mmap your file, edit, save),
- bulk data loaders (read 100 GB log without touching `read()`).

## Anonymous mmap — for the heap or for shm

```c
char *p = mmap(NULL, 1<<20, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
```

This is **how `malloc`'s big allocations work**. glibc uses `mmap` for >128 KB requests. Smaller go through `brk`/`sbrk` for the heap.

Two processes can share anonymous memory if you `mmap` with `MAP_SHARED | MAP_ANONYMOUS` **before** `fork`.

## POSIX shared memory — for unrelated processes

```c
#include <sys/mman.h>
#include <fcntl.h>

int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0600);
ftruncate(fd, 4096);
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
// process B opens the same name and mmaps it.
```

Memory is now shared between any process that opens `/myshm`. Backed by `tmpfs` at `/dev/shm`. Lives until `shm_unlink("/myshm")`.

## Synchronization in shared memory

Plain reads/writes by multiple processes race. You need locks:
- POSIX **process-shared mutexes**: `pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED)`. Place mutex inside the shared region.
- Atomic operations (`stdatomic.h`) — `atomic_int` placed in shared memory works fine.
- Semaphores (next chapter).
- Futexes (lower-level building block).

Example: shared atomic counter between two processes.

```c
#include <stdatomic.h>
atomic_int *counter = mmap(...);
atomic_fetch_add(counter, 1);
```

## Performance tricks

### MAP_HUGETLB / Transparent Huge Pages
2 MB or 1 GB pages instead of 4 KB. Big TLB win for huge maps. Some workloads (databases, GPU pinning) need it.

### MAP_POPULATE
Pre-fault all pages immediately, avoiding the page-fault storm at first access. Latency-sensitive workloads.

### madvise
Tell the kernel about your access pattern:
- `MADV_SEQUENTIAL` — readahead more aggressively.
- `MADV_RANDOM` — read just the page.
- `MADV_DONTNEED` — drop the pages, will refault if accessed.
- `MADV_HUGEPAGE` — try to use huge pages.
- `MADV_DONTFORK` — don't copy this region in `fork`.

```c
madvise(p, len, MADV_SEQUENTIAL);
```

## msync — controlling the flush

For `MAP_SHARED`, writes go to the page cache; they hit disk on the next writeback. `msync(p, len, MS_SYNC)` forces immediate flush. `MS_ASYNC` schedules.

## Cost of mmap

mmap itself is cheap; the cost is the **page faults** when you first touch each page. For a 1 GB map, that's 256K faults of 4 KB each. Use `MAP_POPULATE` or huge pages for bulk loads.

## When NOT to use mmap

- Many small reads from many small files — `read()` is simpler and has less syscall overhead.
- Files that grow during use — mmap's size is fixed at the time of the call. (For grow scenarios, remap.)
- Reading network sockets — you can't mmap a socket; use `splice` for zero-copy.

## Real-world examples

- **GPU**: BO mappings for CPU visibility (write-combining mappings via `mmap` of `/dev/dri/cardN`). The mmap on a DRM file routes through `drm_gem_mmap` and ends up in your driver's `vm_ops->fault` callback. We'll see this in Part VII.
- **Databases**: SQLite with `mmap_size` pragma, LMDB, MongoDB historically.
- **Browsers**: `MAP_PRIVATE` of fonts and large data files.

## Common bugs

1. **Forgetting `ftruncate`**: a freshly created shm has size 0; mmap will SEGV on access.
2. **Wrong PROT**: `PROT_READ` mmap then writing → SIGSEGV.
3. **Mmap past EOF**: with `MAP_SHARED`, accessing past file end → SIGBUS.
4. **`MAP_PRIVATE` confusion**: writes appear local, lost on `munmap`.
5. **Forgot to munmap**: mmap leak; `pmap PID` shows the leak grow.
6. **Concurrent access without sync**: races. Always synchronize.

## Try it

1. mmap a file; toggle a byte; flush; verify with `xxd file`.
2. Two processes share a counter via `shm_open` + `mmap` + `atomic_int`. Increment 1M times each; verify final equals 2M.
3. Implement a "tail -f" via mmap: print bytes as they appear in a growing file (you'll need to re-mmap on size changes).
4. Use `madvise(MADV_SEQUENTIAL)` then read a 1 GB file. Compare to without; compare to `read()` only. Time them.
5. Bonus: write a parent that creates an `MAP_SHARED | MAP_ANONYMOUS` region, forks 4 children, each fills 1/4 of the buffer. Parent waits and prints checksum.

## Read deeper

- TLPI chapters 49 (memory mappings), 53 (POSIX shared memory).
- `man 2 mmap`, `man 2 madvise`, `man 7 shm_overview`.
- "Linux Page Cache" — search lwn.net.

Next → [Message queues, semaphores, futexes](USP-06-mq-sem.md).
