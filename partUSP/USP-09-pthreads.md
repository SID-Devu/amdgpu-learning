# USP 9 — pthreads deep

> Threads share everything inside one process. Powerful, dangerous, essential.

## Threads vs processes

| | Process | Thread |
|---|---|---|
| Address space | private | shared with peer threads |
| File descriptors | private (after fork) | shared |
| `getpid` | own | same as process |
| Crashes | isolated (mostly) | crash one = crash all |
| Communication | IPC (slow) | shared memory (fast) |
| Creation cost | high | medium |

A process is always **one process** with one or more threads. Linux schedules threads (in the kernel they're called "tasks").

## Creating a thread

```c
#include <pthread.h>

void *worker(void *arg) {
    int id = (int)(long)arg;
    printf("hi from thread %d\n", id);
    return (void*)(long)(id * 2);
}

int main(void) {
    pthread_t t[4];
    for (int i = 0; i < 4; i++) pthread_create(&t[i], NULL, worker, (void*)(long)i);
    for (int i = 0; i < 4; i++) {
        void *ret;
        pthread_join(t[i], &ret);
        printf("thread %d returned %ld\n", i, (long)ret);
    }
}
```

`pthread_join` blocks until the thread finishes. Alternative: `pthread_detach` to "fire and forget" (no return value collected; thread cleans itself).

## Mutexes

```c
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_lock(&m);
// critical section
pthread_mutex_unlock(&m);
```

If multiple threads contend, all but one block. Backed by futex on Linux — uncontended lock is essentially zero-cost (an atomic CAS in userspace).

### Variants
- `pthread_mutex_trylock` — non-blocking; returns EBUSY if locked.
- `pthread_mutex_timedlock` — wait up to a timeout.
- `pthread_rwlock_t` — multiple readers OR one writer.
- Recursive mutex: same thread can lock multiple times. Avoid; usually a sign of bad design.
- Robust mutex: survives owner crash (returns EOWNERDEAD).

## Condition variables

For "wait until some condition is true."

```c
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m  = PTHREAD_MUTEX_INITIALIZER;
int ready = 0;

// waiter
pthread_mutex_lock(&m);
while (!ready) pthread_cond_wait(&cv, &m);
// safe to access protected data
pthread_mutex_unlock(&m);

// signaler
pthread_mutex_lock(&m);
ready = 1;
pthread_cond_signal(&cv);   // or pthread_cond_broadcast
pthread_mutex_unlock(&m);
```

**Always use `while (!cond)`**, never `if`. Spurious wakeups are real (rare, but allowed).

`pthread_cond_wait` atomically: drops the mutex + sleeps + re-acquires the mutex on wake. That atomicity is the whole point.

## Semaphores

`sem_t` (POSIX). Counting semaphore between threads. Useful for bounded resources.

## Read-write locks

```c
pthread_rwlock_t rw = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_rdlock(&rw);   // many readers OK
// or
pthread_rwlock_wrlock(&rw);   // exclusive
pthread_rwlock_unlock(&rw);
```

Win when reads vastly outnumber writes. Penalize writers slightly.

## Thread-local storage

```c
__thread int my_local;        // GCC extension; standard on Linux
// C11:
_Thread_local int my_local;
```

Each thread has its own copy. Used for: per-thread arenas in custom allocators, errno (yes, errno is `__thread`), thread-local random generators.

## pthread_once — initialize once

```c
pthread_once_t once = PTHREAD_ONCE_INIT;
void init(void) { /* ... */ }

pthread_once(&once, init);
```

Thread-safe initialization. Runs `init` exactly once across all threads.

## Cancellation

```c
pthread_cancel(thread);
```

Tries to cancel a thread. Cancel points (e.g., `read`, `write`, `pthread_cond_wait`) check and exit. Asynchronous mode is dangerous; avoid.

Better practice: shared "stop" flag the thread checks at safe points.

## Thread attributes

```c
pthread_attr_t a;
pthread_attr_init(&a);
pthread_attr_setstacksize(&a, 1 << 20);     // 1 MB stack
pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
pthread_create(&t, &a, fn, arg);
pthread_attr_destroy(&a);
```

Important: by default, glibc gives 8 MB stacks; many threads = lots of address space (not RAM until used). Reduce if creating thousands of threads.

## Producer / consumer (canonical example)

```c
// shared
queue_t q; pthread_mutex_t m; pthread_cond_t not_empty, not_full;

// producer
pthread_mutex_lock(&m);
while (full(&q)) pthread_cond_wait(&not_full, &m);
push(&q, item);
pthread_cond_signal(&not_empty);
pthread_mutex_unlock(&m);

// consumer
pthread_mutex_lock(&m);
while (empty(&q)) pthread_cond_wait(&not_empty, &m);
item = pop(&q);
pthread_cond_signal(&not_full);
pthread_mutex_unlock(&m);
```

Memorize this pattern. It's the foundation of every worker-pool server.

## Atomic operations (C11)

```c
#include <stdatomic.h>
atomic_int counter = 0;
atomic_fetch_add(&counter, 1);     // atomic ++
atomic_load(&counter);
atomic_store(&counter, 0);
atomic_compare_exchange_strong(&counter, &expected, desired);
```

Memory orderings: `relaxed`, `acquire`, `release`, `acq_rel`, `seq_cst`. Default `seq_cst` (sequentially consistent).

Atomics avoid mutexes for small ops (counters, flags). For real lock-free DS, see Part IV.

## Deadlocks

Two threads, two locks, opposite order:

```
Thread A: lock(X); lock(Y);
Thread B: lock(Y); lock(X);
```

Both stuck. Avoid:
- **Always acquire locks in a consistent global order.**
- Use `pthread_mutex_trylock` and back off on conflict.
- Run with **lockdep**-style tools (helgrind, TSan) to detect.

## Sanitizers — your best friends

```bash
gcc -fsanitize=thread -g -O1 prog.c
```

**ThreadSanitizer (TSan)** catches data races at runtime. Slower (5-10x), worth it during dev. Use it on every multi-threaded program you write.

**Helgrind / DRD** (valgrind tools) — alternative race detectors.

## Per-thread vs shared

- **Shared by default**: heap, globals, file descriptors, signal handlers (process-wide).
- **Per-thread**: stack, errno, thread-local storage.

## Common bugs

1. **Forgot to lock** — race condition, hard to reproduce.
2. **Locked but wrong lock** — protecting different objects with different locks; access from wrong context.
3. **Held lock too long** — through I/O or syscalls.
4. **Lost wakeup** — using `if` instead of `while` around `cond_wait`.
5. **Deadlock** — inconsistent lock order.
6. **Leaked thread** — never joined, never detached → resource leak.
7. **Detached thread referencing freed data** — synchronization needed for handoff.

## Try it

1. Producer/consumer with N producers, M consumers, bounded queue. Use mutex + condvar. Verify count.
2. Implement a thread pool: N worker threads pulling tasks from a queue. Submit 10000 small tasks; measure throughput.
3. Replace mutex with read-write lock for a "mostly read" map. Measure improvement.
4. Use `_Thread_local` to give each thread its own random generator. Compare to a single shared one + mutex.
5. Build a buggy program with a deliberate race; confirm TSan detects it.
6. Bonus: re-implement a SPSC queue using `atomic_int` indices and `memory_order_acquire/release`. Stress test for 60s.

## Read deeper

- TLPI chapters 29–33 (threads, sync, thread-local).
- *C++ Concurrency in Action* (Anthony Williams) — even though it's C++, the model and pitfalls translate.
- *Programming with POSIX Threads* by Butenhof — the classic.

Next → [File I/O deep](USP-10-file-io.md).
