# Lab 01 — Arena allocator

A bump allocator. Foundation for understanding the kernel's `mempool`, `dma_pool`, and the GPU command-buffer suballocators.

## Build & run

```bash
make run
```

You should see `arena tests passed` and AddressSanitizer reporting no errors.

## Stretch

1. Add a `arena_save` / `arena_restore` for nested scopes.
2. Make a "growing" arena that allocates a new chunk when the current is full.
3. Compare to `malloc` performance for 1M small allocations.
