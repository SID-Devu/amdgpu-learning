# Chapter 23 — Threads, the C/C++ memory model

Multithreading is where most candidates fall down in an AMD/NVIDIA/Qualcomm interview. Memory ordering, atomics, and "what does volatile do?" come up nearly every time. This chapter and the next four make sure you can answer correctly.

## 23.1 What a thread is

A thread is an independent flow of execution that *shares the address space* with its peers. They each have their own:

- **registers** (including PC and SP),
- **stack**,
- **errno** (in glibc, via TLS).

They share:

- the **heap**,
- **globals**,
- **file descriptors**,
- **memory mappings**.

Two threads can read the same variable at the same time — that's safe. If at least one writes and at least one other reads or writes, you have a **data race**, and the result is **undefined behavior** unless you use synchronization (atomics, mutexes, etc.).

## 23.2 pthreads

```c
#include <pthread.h>
#include <stdio.h>

void *worker(void *arg) {
    printf("hi from %ld\n", (long)arg);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, (void *)1);
    pthread_create(&t2, NULL, worker, (void *)2);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
```

Compile: `gcc -pthread thread.c -o thread`.

API:

- `pthread_create` — start.
- `pthread_join` — wait for end + collect result.
- `pthread_detach` — give up join right; thread cleans itself up.
- `pthread_self` — get current thread id.

In modern C++ you'd write `std::thread t(worker, 1); t.join();` — the underlying call is the same.

## 23.3 The data race

```c
int counter = 0;

void *inc(void *_) {
    for (int i = 0; i < 1000000; i++) counter++;   /* RACE */
    return NULL;
}
```

Run with two threads. Expected `2000000`. Actual: anywhere from ~1.0M to 2.0M. Why? `counter++` is three machine ops:

```
load   eax, [counter]
add    eax, 1
store  [counter], eax
```

Two threads interleave: T1 loads 5, T2 loads 5, both add to 6, both store 6 — one increment lost.

The fix: atomicity (Chapter 25) or a mutex (Chapter 24).

But the C standard goes further: a *data race* is **undefined behavior** even if "you got lucky." The compiler is allowed to assume your code has no races, and may transform racy code arbitrarily.

## 23.4 The C/C++ memory model

C11 and C++11 standardized a memory model: **what is observable in a program's executions across multiple threads.**

Key rule: **a program with a data race has UB.** Period.

To not have a race: every access to shared mutable data must be done via:

- a **mutex** held by all participants,
- an **atomic** with appropriate ordering,
- a **volatile** for hardware (special case; not a substitute for atomics).

The atomics offer **memory orderings**:

| Order | Cost | Guarantees |
|---|---|---|
| `relaxed` | cheapest | only atomicity, no ordering |
| `acquire` | mid | reads after this, see writes that happened before the matching release |
| `release` | mid | writes before this become visible to a subsequent acquire |
| `acq_rel` | mid | both, on a RMW |
| `seq_cst` | most expensive | sequential consistency, total order |

C11/`<stdatomic.h>`:

```c
#include <stdatomic.h>
atomic_int counter = 0;

atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
```

C++11/`<atomic>`:

```cpp
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);
counter.fetch_add(1);   /* default seq_cst */
```

For a counter you only read at the end, `relaxed` is fine. For a flag that signals "data is ready to read," you need `release` on store and `acquire` on load.

## 23.5 The acquire-release pattern

The single most important pattern.

Producer:

```cpp
data = make_data();
ready.store(true, std::memory_order_release);
```

Consumer:

```cpp
while (!ready.load(std::memory_order_acquire)) { }
use(data);
```

Release publishes prior writes; acquire ensures subsequent reads see them. Without the release/acquire, the compiler or CPU could reorder writes and `use(data)` could see uninitialized.

This pattern underlies every lock-free queue, every double-buffering scheme, every ring buffer.

## 23.6 Sequential consistency

`seq_cst` (the default) gives a single total order of all atomic ops across all threads. Easy to reason about. Expensive on weak-memory CPUs (ARM, POWER) because it requires full barriers. On x86, it's almost free.

For perf-sensitive code, use `acquire`/`release`/`relaxed` explicitly. For correctness-first code, use the default.

## 23.7 Hardware memory ordering

CPUs reorder loads/stores (within their own constraints). x86 is "TSO" (Total Store Order): loads can pass earlier stores from the same CPU only via the **store buffer**, and only with respect to *other CPUs*. Stores aren't reordered with stores; loads aren't reordered with loads.

ARM and POWER are weakly ordered: nearly any reorder is allowed unless a barrier is inserted.

`std::memory_order_*` is a **portable abstraction** over what each hardware needs. Use it; don't insert manual barriers in portable code unless you know exactly what you're doing.

The kernel uses its own families: `smp_mb()`, `smp_rmb()`, `smp_wmb()`, `barrier()`, `READ_ONCE/WRITE_ONCE`, `smp_load_acquire`/`smp_store_release`. We discuss those in Part V.

## 23.8 What `volatile` is and isn't

- `volatile` prevents the compiler from optimizing away or coalescing reads/writes.
- `volatile` does **not** insert any CPU barrier.
- `volatile` does **not** make a read-modify-write atomic.
- `volatile` is correct for: MMIO, signal handlers, `setjmp/longjmp` locals.

So `volatile int counter; counter++;` is still racy. Use `_Atomic int` (C) or `std::atomic<int>` (C++).

## 23.9 Memory model in practice — a tiny lock-free queue

A single-producer single-consumer ring buffer:

```cpp
template <class T, size_t N>
class spsc {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
public:
    bool push(const T& v) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (((h + 1) & (N - 1)) == t) return false;   /* full */
        data_[h] = v;
        head_.store((h + 1) & (N - 1), std::memory_order_release);
        return true;
    }
    bool pop(T& out) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (h == t) return false;                     /* empty */
        out = data_[t];
        tail_.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }
private:
    T                       data_[N];
    std::atomic<size_t>     head_{0};   /* producer writes */
    std::atomic<size_t>     tail_{0};   /* consumer writes */
};
```

Producer publishes new `head_` with release after writing data; consumer reads with acquire to see the data. No mutex; no kernel call. The simplest non-trivial lock-free structure.

## 23.10 Thread-local storage

`thread_local` (C11/C++11) gives each thread its own copy of a variable.

```cpp
thread_local int my_id = 0;
```

Used for per-thread allocators, per-thread error state. Cheap on x86 (FS/GS segment register) but not free.

In the kernel, the equivalent is **per-CPU variables** (`DEFINE_PER_CPU`, `this_cpu_read`, `this_cpu_write`).

## 23.11 False sharing

Two threads write to two variables that happen to be on the same cache line. Each write invalidates the other's cache. Performance dies.

Fix: align hot per-thread/per-CPU data to a cache line:

```cpp
struct alignas(64) Slot { std::atomic<int> v; };
```

Kernel: `____cacheline_aligned`.

## 23.12 Exercises

1. Implement the racing counter from §23.3. Confirm corruption with `-O2`.
2. Replace `int counter` with `std::atomic<int>`; confirm correctness with `relaxed` ops.
3. Implement the SPSC queue. Pump 1e8 ints through it on two threads. Measure throughput.
4. Add a producer-consumer with a mutex; benchmark. Compare to lock-free.
5. Read `Documentation/memory-barriers.txt` from the kernel. It is long, dense, and the authoritative source on memory ordering for kernel work.

---

Next: [Chapter 24 — Mutexes, condition variables, semaphores](24-locks-and-condvars.md).
