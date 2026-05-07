# Chapter 24 — Mutexes, condition variables, semaphores

You will use these every day. We cover what they are, when each fits, and the dozen common bugs.

## 24.1 Mutex (mutual exclusion)

A mutex protects a *critical section* — a code region only one thread executes at a time.

```cpp
#include <mutex>
std::mutex m;
int counter = 0;

void inc() {
    std::lock_guard<std::mutex> g(m);
    counter++;
}
```

`lock_guard` is RAII: it locks on construction, unlocks on destruction. Always use RAII. Manual `m.lock()/unlock()` will leak the lock on early return or exception.

Underneath: `pthread_mutex_t` on POSIX, `std::mutex` wraps it. Linux uses **futexes** — fast userspace mutexes. Uncontended `lock`/`unlock` is a single CAS, no syscall. Contended path enters the kernel via `futex_wait`.

## 24.2 Recursive mutexes

`std::recursive_mutex` allows the same thread to lock multiple times (and must unlock the same number). Useful when callbacks may recurse into your locked code. Kernel rule: **avoid recursion-friendly designs.** A recursive mutex usually indicates an unclear ownership model.

## 24.3 Reader/writer locks

`std::shared_mutex` allows multiple readers OR one writer.

```cpp
std::shared_mutex sm;
{
    std::shared_lock<std::shared_mutex> g(sm);  /* read */
    /* … */
}
{
    std::unique_lock<std::shared_mutex> g(sm);  /* write */
    /* … */
}
```

Useful when reads vastly outnumber writes (cache lookups). Cost: a shared lock is more expensive than a mutex; on contention they often perform worse than expected.

In the kernel: `rwlock_t` (spinning) and `rw_semaphore` (sleeping). Even more relevant: **RCU** (Chapter 26).

## 24.4 Spin lock

A spin lock busy-waits instead of sleeping. Useful when:

- the critical section is very short,
- sleeping is forbidden (kernel atomic context).

In userspace, prefer mutex (which spins briefly then sleeps). In kernel, **`spinlock_t`** is the workhorse for short critical sections in IRQ context or per-CPU paths.

## 24.5 Deadlock

Two threads each hold a lock the other needs. Both wait forever.

Classic example:

```cpp
T1: lock(A); lock(B); …
T2: lock(B); lock(A); …
```

Prevention rules:

1. **Lock ordering.** Always take locks in a consistent global order. Document it.
2. **Don't call user code while holding a lock.** They might call back into you and try to lock something.
3. **Limit lock scope.** Hold locks for the shortest time possible.
4. **Use `std::scoped_lock(a, b)`** (C++17) for multi-lock cases — it deadlock-avoids.

The kernel's `lockdep` runtime validator finds lock-ordering violations automatically. Run kernels with `CONFIG_PROVE_LOCKING=y` while developing.

## 24.6 Condition variables

A condition variable lets a thread wait for a *predicate* to become true. Always paired with a mutex.

```cpp
std::mutex m;
std::condition_variable cv;
std::queue<int> q;

void producer() {
    {
        std::lock_guard<std::mutex> g(m);
        q.push(42);
    }
    cv.notify_one();
}

void consumer() {
    std::unique_lock<std::mutex> g(m);
    cv.wait(g, []{ return !q.empty(); });
    int v = q.front(); q.pop();
}
```

Rules:

1. **Always wait with a predicate** (`cv.wait(g, pred)`). Prevents spurious wakeups.
2. **Hold the mutex when modifying the predicate** and when calling `wait`.
3. **Notify after releasing or while holding the mutex** — both are correct, but the latter avoids the lost-wakeup race. Common practice: hold while modifying state, release, then notify.

Spurious wakeups: `wait` may return without anyone calling `notify_*`. The predicate-form loop handles this.

`notify_one` wakes one waiter; `notify_all` wakes all (then they each re-check predicate).

## 24.7 The producer/consumer queue

```cpp
template <class T>
class BlockingQueue {
public:
    void push(T v) {
        {
            std::lock_guard<std::mutex> g(m_);
            q_.push(std::move(v));
        }
        cv_.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> g(m_);
        cv_.wait(g, [&]{ return !q_.empty(); });
        T v = std::move(q_.front()); q_.pop();
        return v;
    }
private:
    std::mutex               m_;
    std::condition_variable  cv_;
    std::queue<T>            q_;
};
```

Reused everywhere. Many GPU runtime worker threads use this exact pattern.

## 24.8 Semaphore

A counting semaphore is a counter you can `acquire` (decrement, wait if 0) and `release` (increment, wake one). C++20 introduced `std::counting_semaphore`. Useful for resource pools and bounded queues.

```cpp
std::counting_semaphore<8> slots(8);
slots.acquire();   /* one fewer slot */
do_thing();
slots.release();
```

In the kernel: `struct semaphore` (deprecated for new code; prefer mutex or completion).

## 24.9 Once and lazy init

```cpp
std::once_flag flag;
std::call_once(flag, [&]{ heavy_init(); });
```

Guarantees `heavy_init` runs exactly once, even with concurrent callers. Replaces the double-checked-locking pattern, which is hard to get right.

Kernel: `static_branch_*`, `static_key`, simple atomic flags.

## 24.10 Barrier and latch (C++20)

`std::latch` — counter you decrement; threads `wait()` until counter hits 0. One-shot.

`std::barrier` — same, but resets each phase. Useful for parallel-loop synchronization.

In CUDA/HIP, `__syncthreads()` is a hardware-level analog at the warp/wave level. Do not confuse.

## 24.11 Common bugs

| Bug | Symptom | Fix |
|---|---|---|
| Forgot to lock | Race; wrong values | Add lock / atomic |
| Held lock during signal/notify | Receiver wakes immediately, can't acquire | Notify after unlock (sometimes) |
| Predicate not re-checked | Spurious wakeup → wrong state | Use `cv.wait(g, pred)` form |
| Lock order inconsistent | Deadlock | Globally order locks; lockdep |
| Recursive lock with std::mutex | Self-deadlock | Use recursive_mutex or restructure |
| Holding lock too long | Latency spikes | Shrink critical section |
| Sleeping with spinlock | Kernel BUG | Drop lock first |

## 24.12 Exercises

1. Implement `BlockingQueue<T>` from above. Test with multiple producers and consumers.
2. Implement a thread pool: N workers, a `BlockingQueue<std::function<void()>>` of tasks.
3. Implement a reader/writer simulation; benchmark `std::shared_mutex` vs `std::mutex`.
4. Construct a deadlock with two threads/two locks. Then fix with `std::scoped_lock`.
5. Read `kernel/locking/mutex.c`; identify the slow-path enter into the kernel.

---

Next: [Chapter 25 — Atomics, lock-free data structures](25-atomics-and-lockfree.md).
