# Lab 03 — Thread pool with mutex + condvar

The classical pattern. Used in every userspace runtime worker: ROCm dispatcher, Mesa shader-compile pool, etc.

## Build & run

```bash
make run
```

## Stretch

1. Add task priority (use a priority queue).
2. Add per-thread queue with work stealing.
3. Replace `std::function` with a fixed-size SBO-friendly callable; benchmark.
