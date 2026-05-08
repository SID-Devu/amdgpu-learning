# USP 6 — Message queues, semaphores, futexes

> Three IPC primitives: structured messages, counted blocking, and the lock primitive that makes everything fast.

## POSIX message queues

```c
#include <mqueue.h>
struct mq_attr a = { .mq_maxmsg = 10, .mq_msgsize = 256 };
mqd_t q = mq_open("/myq", O_CREAT | O_RDWR, 0600, &a);

char buf[256];
mq_send(q, buf, len, /*priority=*/5);
unsigned prio;
mq_receive(q, buf, sizeof(buf), &prio);

mq_close(q);
mq_unlink("/myq");
```

- Bounded queue of fixed-size messages.
- Messages have priority (highest delivered first).
- Lives in the kernel under `/dev/mqueue`.
- Senders block when full; receivers block when empty (configurable nonblocking).

When to use: structured event passing between unrelated processes, with priority delivery.

## System V message queues

`msgget`, `msgsnd`, `msgrcv`. Older API (1980s), key-based naming. Avoid; use POSIX or sockets.

## POSIX semaphores

A semaphore is a non-negative counter:
- `sem_wait` decrements (blocks if 0).
- `sem_post` increments (wakes a waiter if any).

### Named (between processes)

```c
sem_t *s = sem_open("/sem", O_CREAT, 0600, /*initial=*/1);
sem_wait(s);
// critical section
sem_post(s);
sem_close(s);
sem_unlink("/sem");
```

### Anonymous (in shared memory)

```c
sem_t *s = mmap(...); 
sem_init(s, /*pshared=*/1, /*initial=*/1);
```

### Counting semaphore vs binary

A semaphore initialized to 1 acts as a mutex (binary). Initialized to N, allows up to N concurrent holders (e.g., a "pool of 10 workers").

Used in: producer/consumer (1 sem for empty slots, 1 for filled), bounded-resource access, simple multi-process synchronization.

## System V semaphores

`semget`, `semop`. Older. Powerful but byzantine. Avoid in new code.

## eventfd — a counter you can poll

```c
int fd = eventfd(0, EFD_NONBLOCK);
uint64_t v = 1; write(fd, &v, 8);   // increment
read(fd, &v, 8);                    // gets and resets
```

Useful for waking an event-loop thread. Drop into `epoll`. Used by libraries like libevent, libuv internally.

## futex — the kernel primitive under userspace mutexes

`futex` ("fast userspace mutex") is the syscall that makes lock-free locks possible:
- Userspace tries an atomic `cmpxchg` to acquire a lock.
- If contended, calls `futex(FUTEX_WAIT, addr, val)` to sleep.
- Releaser calls `futex(FUTEX_WAKE, addr, n)` to wake waiters.

You almost never call futex directly. `pthread_mutex_t` uses futex under the hood. Some advanced users (lock-free libraries, JITs) do call it directly.

`man 2 futex` — read it once for awareness.

## When to use which IPC

| Scenario | Best |
|---|---|
| Structured messages with priority | POSIX message queue |
| Mutex/semaphore between processes | POSIX semaphore in shared memory |
| Wake a thread from epoll loop | `eventfd` |
| One process sends events to another over a connection | Unix socket |
| Bidirectional, possibly across machines | TCP socket |
| Bulk data | shared memory + lock + signaling |
| Simple "child done" | `wait()` / SIGCHLD |

## Locks across processes

Two ways to protect shared memory between processes:

### POSIX process-shared mutex

```c
pthread_mutex_t *m = mmap(...);
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutex_init(m, &attr);
```

Now `pthread_mutex_lock(m)` works between processes. Cheap (futex-backed).

**Robust** mutexes (`pthread_mutexattr_setrobust`) survive owner-process crashes — the next acquirer gets `EOWNERDEAD` and can recover.

### POSIX semaphore in shared memory

Simpler: `sem_init` with `pshared=1`. Use as binary mutex.

## Common bugs

1. **Forgetting to `unlink`** named queues/semaphores → resource leak (`ls /dev/mqueue`).
2. **Mutex not in shared memory** when used across processes → each process locks its own copy. Subtle.
3. **Initializer running twice** in shared memory → corruption. Initialize from one process only.
4. **Holding lock across long ops or syscalls that may block on disk/net** → contention.
5. **Stale lock state on crash** — use robust mutexes.

## Try it

1. POSIX message queue: parent sends 100 messages with random priorities; child reads and prints in order. Verify priority ordering.
2. Two processes share a counter via shared memory; protect with a process-shared `pthread_mutex_t`. Each does 1M increments. Verify final = 2M.
3. Use `eventfd` to integrate "I have work" notifications into an `epoll` loop.
4. Producer/consumer with semaphores: bounded ring buffer of size N; producer blocks when full, consumer when empty.
5. Crash a robust mutex holder; show the next acquirer gets EOWNERDEAD.

## Read deeper

- TLPI chapters 51–53 (POSIX IPC).
- `man 7 mq_overview`, `man 7 sem_overview`, `man 2 futex`.
- "Mutexes and Memory Barriers" — useful articles on futex internals.

Next → [Sockets — TCP & UDP](USP-07-sockets.md).
