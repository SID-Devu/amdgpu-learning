# Lab 02 — Lock-free SPSC ring buffer

Same shape as a GPU command ring: one producer (CPU), one consumer (GPU). We make ours CPU↔CPU to validate the algorithm.

## Build & run

```bash
make run        # correctness + throughput
make tsan       # under ThreadSanitizer
```

You should see `OK` and a Mops/s number; on a modern desktop expect 50-200 Mops/s. TSan should report zero races — confirming the acquire/release pairing is correct.

## Stretch

1. Add a multi-producer variant with CAS on `head_` (becomes MPSC).
2. Compare throughput vs. a `std::mutex`-protected `std::queue`.
3. Map the same data structure into shared memory between two processes via `shm_open` + `mmap`.
