# OST 4 — Classic synchronization problems

> Dining philosophers. Readers/writers. Producer/consumer. The textbook problems exist because their solutions teach you patterns you'll reuse for life.

## Producer / Consumer (bounded buffer)

The most-used pattern in real systems. Producer adds; consumer removes; bounded queue.

### Solution with semaphores
```
sem empty = N    // empty slots
sem full  = 0    // filled slots
mutex  m

producer:
    sem_wait(empty);
    sem_wait(m);
    push(x);
    sem_post(m);
    sem_post(full);

consumer:
    sem_wait(full);
    sem_wait(m);
    x = pop();
    sem_post(m);
    sem_post(empty);
```

Two semaphores ensure correct count; mutex protects the queue itself.

### Solution with mutex + condvar
Already shown in [USP-09](../partUSP/USP-09-pthreads.md). Use `while (full)` and `while (empty)` — Mesa semantics.

### Lock-free version
Specialized: SPSC ring buffer. Multi-producer multi-consumer is much harder.

## Readers / Writers

Multiple readers OK simultaneously. Writers exclusive.

### Naive (writer starvation)
```
sem_wait(rw_mutex) on first reader
sem_post(rw_mutex) on last reader
```
Writers wait while any reader exists. If readers always come, writer never gets in → starvation.

### Writer-preference
Bias toward writers; readers wait if any writer is queued.

### Fair version (no starvation)
"Bus-stop" / Hoare's solution: queue-based; whoever's been waiting longest goes next. Linux's `rw_semaphore` is approximately fair.

### Seqlock
Optimized for "rare writes, frequent reads." Reader reads, validates by sequence counter, retries if needed. No reader blocking.

Trade-offs: seqlock → readers can be in violent retry loops if writers churn. Picks "scalability" over "writer fairness."

## Dining philosophers

Five philosophers, five forks, each needs both neighboring forks to eat. Classic deadlock setup.

### Failure mode (deadlock)
All grab left fork simultaneously → all wait for right fork → forever.

### Solutions
1. **Asymmetric**: one philosopher picks right first; others left first. Breaks the symmetry.
2. **Resource hierarchy**: total order on resources; always acquire in increasing order. (Generalizes to n forks.)
3. **Arbitrator**: a "headwaiter" (mutex) lets at most n-1 philosophers be picking up forks at once.
4. **Tannenbaum's algorithm**: state-machine — only eat when both neighbors aren't eating.

The lesson: **lock ordering** prevents deadlock. The kernel has the same rule.

## Sleeping barber

A barber sleeps; customers arrive, wake him; only N waiting chairs — overflow leaves.

Classic example of "synchronizing with a service provider." Solution: 3 semaphores (customers, barber_ready, mutex_for_chair_count).

Pattern shows up in real-life: Apache prefork, web server worker pools, kernel work-queue model.

## Cigarette smokers (a teaching trap)

Three smokers each have one ingredient (paper, matches, tobacco); an agent randomly produces two ingredients and the smoker with the third must form a cigarette.

The puzzle: solve **without conditional waiting / without changing the agent**. Forces students to learn `pthread_cond_t` properly.

It's a teaching device. In real systems you just refactor.

## H2O problem

Two H atoms and one O atom must combine. When 2 Hs and 1 O meet, they make a water molecule and proceed.

Solved with semaphores or barriers. Generalizes to "form groups before proceeding."

## Once initialization

Multiple threads might be the first to call `init`; only one should run it.

```
once_t once = ONCE_INIT;
pthread_once(&once, init);
```

Internally: a lock + a flag. First thread runs `init`; others block until done; subsequent calls are flag-only fast path.

C++11: `std::call_once`, `std::once_flag`.

## Token-passing (the producer/consumer's cousin)

A token rotates around N processes. Holder may do work; passes the token to the next. Used in distributed mutual exclusion.

## Barrier

All N threads stop at a point until all arrive; then all proceed.

```c
pthread_barrier_t b;
pthread_barrier_init(&b, NULL, N);
// each thread:
pthread_barrier_wait(&b);
```

Used in: parallel computations (each phase synchronized), GPU dispatches across thread groups.

## Reader-Writer with priority

Writers high-prio: starve readers. Readers high-prio: starve writers. Fair: alternate.

Linux's choice for `rwsem` is "writer-priority by default with reader fairness tweaks." See `kernel/locking/rwsem.c`.

## Banker's algorithm (deadlock avoidance)

A theoretical algorithm for granting resources only if the system stays in a "safe state" — one where some sequence of process completions exists.

Practical relevance: low. Real systems use **prevention** (lock ordering) or **detection + recovery** (rare).

## What this teaches

If you can solve dining philosophers and producer/consumer cleanly in C, **you can write robust threaded code anywhere**. These problems aren't toys — they're the kernel's daily workload in miniature.

## Try it

1. Producer/consumer (bounded buffer) — N producers, M consumers, capacity K. Verify count with stress test.
2. Readers/writers — both naive and fair versions. Show writer starvation in naive.
3. Dining philosophers — three solutions: ordered locking, arbiter, asymmetric. Confirm no deadlock.
4. H2O problem — combine 2H + 1O via semaphores.
5. Implement a `barrier` from scratch using mutex + condvar.
6. Bonus: dining philosophers with 1000 philosophers; measure throughput with each strategy.

## Read deeper

- *The Little Book of Semaphores* (Allen Downey) — free PDF, dozens of these problems.
- OSTEP chapter on "Common Concurrency Problems."
- `Documentation/locking/lockdep-design.rst` (Linux's deadlock detector).

Next → [Deadlock theory](OST-05-deadlock.md).
