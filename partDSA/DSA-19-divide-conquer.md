# DSA 19 — Divide & conquer

> "Break it into halves, solve each, combine." Merge sort, quicksort, FFT, parallel reductions.

## The recipe

1. **Divide** the input into smaller subproblems.
2. **Conquer** each subproblem recursively.
3. **Combine** their solutions to form the answer.

The recurrence is `T(n) = a · T(n/b) + f(n)`. Apply the master theorem (DSA-16) to compute complexity.

## Classic D&C algorithms

### Merge sort
Two halves, sort each, merge. `T(n) = 2T(n/2) + n` → O(n log n). Stable. See DSA-14.

### Quicksort
Partition around a pivot, recurse on each side. `T(n) = T(k) + T(n-k-1) + n`. Expected O(n log n).

### Binary search
Halve the search range each step. `T(n) = T(n/2) + 1` → O(log n).

### Maximum subarray (D&C version)
For an interval, the max sub-array is either:
- in the left half, or
- in the right half, or
- crossing the middle.

Solve each, take the max. `T(n) = 2T(n/2) + n` → O(n log n).

(Kadane's O(n) DP solution is faster, but this D&C version is the canonical example.)

### Closest pair of points (in 2D)
Sort by x; recursively find closest pair in each half; check pairs crossing the dividing line within `min` distance. O(n log n) — a clever O(n) "strip" merge step.

### Strassen's matrix multiplication
Multiply two n×n matrices in O(n^log₂ 7) ≈ O(n^2.81) instead of O(n³).

7 recursive multiplications instead of 8, plus extra additions. Used in some BLAS libraries for very large matrices. For typical sizes, the constant overhead means classical n³ wins.

### Karatsuba multiplication
Multiply two n-digit numbers in O(n^log₂ 3) ≈ O(n^1.58). The trick: 3 recursive multiplications instead of 4. Used in big-int libraries (GMP).

### FFT (Fast Fourier Transform)
Polynomial multiplication / signal transform in O(n log n). Cooley-Tukey D&C: split into even and odd indices, recurse, combine using complex roots of unity.

Foundational; you don't need to write FFT from scratch unless you want to. Know it's D&C, O(n log n).

## Patterns for D&C

### Balanced split
Halve at each step; depth log n.

### Unbalanced split
Quicksort with bad pivot: depth could be n. Worst case bites you.

### Multiple smaller problems
Strassen splits into 7 smaller; FFT into 2 even + 2 odd.

## When D&C wins over DP

- When subproblems are **independent** (no overlap). DP shines on overlapping subproblems.
- When the combine step is cheap relative to the work.
- When you can **parallelize** halves on different cores/threads.

## Parallel D&C

Each recursive call can run on its own thread.

```c
void parallel_sort(int *a, int n) {
    if (n < THRESHOLD) { insertion_sort(a, n); return; }
    int m = n / 2;
    #pragma omp task shared(a)
    parallel_sort(a, m);
    #pragma omp task shared(a)
    parallel_sort(a + m, n - m);
    #pragma omp taskwait
    merge(a, m, a + m, n - m);
}
```

Threads have overhead — the `THRESHOLD` switches to serial for small subproblems. This is exactly how `std::sort` parallel modes are implemented.

## Cilk-style work-stealing
Each worker thread has a deque. Tasks pushed onto own deque (LIFO). Idle workers steal from others' deques (FIFO). Maximum cache reuse + automatic load balancing. Used in Intel TBB, Rust's Rayon, Go's runtime.

## D&C in the real world

- **Compilation**: parallel build (each TU is independent).
- **Map-reduce**: divide data, map, reduce — D&C at scale.
- **GPU radix sort**: divide into tiles, sort each tile, merge.
- **Image processing**: divide image into tiles, process in parallel, stitch.
- **Database query optimization**: parallel hash join, sort-merge join.

## Common bugs

1. Bad **base case** (too small, infinite recursion; too large, stack blowup).
2. **Combine step bugs** — you sorted halves, but your merge step has off-by-one.
3. **Sharing state** between recursive calls (one pollutes the other).
4. **Stack overflow** on deep recursion of huge inputs.
5. **Threading**: false sharing (two threads write adjacent ints in the same cache line; Part IV).

## Try it

1. Implement merge sort and quicksort. Verify O(n log n) by timing.
2. Implement closest-pair-of-points; test on 1000 random points.
3. Implement Karatsuba multiplication for arbitrary-precision integers (you'll write a tiny big-int).
4. Implement parallel quicksort with OpenMP. Time vs single-thread.
5. Bonus: FFT in C. Implement complex multiplication, test by computing convolution.

## Read deeper

- CLRS chapter 4 (Divide and Conquer).
- *Algorithms* by Sedgewick — gentler treatment with diagrams.
- "Cilk Plus" / "Habanero" papers on work-stealing.

Next → [Bit manipulation tricks](DSA-20-bits.md).
