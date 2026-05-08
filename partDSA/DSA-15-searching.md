# DSA 15 — Searching: linear, binary, ternary, exponential

> Binary search is the most subtle 5-line algorithm in CS. Get it right; you'll use it daily.

## Linear search — O(n)

```c
int linear_search(int *a, int n, int x) {
    for (int i = 0; i < n; i++) if (a[i] == x) return i;
    return -1;
}
```

Use when:
- the array is unsorted, **or**
- n is small (cache + simplicity beat asymptotics).

## Binary search — O(log n) on sorted data

```c
int binary_search(int *a, int n, int x) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;   // overflow-safe!
        if (a[mid] == x) return mid;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid - 1;
    }
    return -1;
}
```

### Pitfalls
- `mid = (lo + hi) / 2` can **overflow** for big int arrays. Use `lo + (hi - lo) / 2`.
- Off-by-one: do you use `<` or `<=`? Half-open or closed interval? Pick one and stick with it.
- Boundaries when `x` is duplicated.

### Variants you must know

#### Lower bound — first index with `a[i] >= x`
```c
int lower_bound(int *a, int n, int x) {
    int lo = 0, hi = n;        // half-open [lo, hi)
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid;
    }
    return lo;
}
```

#### Upper bound — first index with `a[i] > x`
Same as above with `<=` instead of `<`.

These two together give you "count of x" = `upper - lower`. Used everywhere.

#### "Find smallest index i such that pred(i) is true" — the generic binary search

This is the **most useful pattern**. Many interview problems reduce to: "find the smallest x where some condition first becomes true."

```c
int first_true(int lo, int hi, int (*pred)(int)) {
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (pred(mid)) hi = mid;
        else           lo = mid + 1;
    }
    return lo;   // lo == hi; first index where pred is true (or hi if never)
}
```

Examples:
- Sqrt: smallest x where `x*x >= target`.
- Allocate-pages problem: smallest #pages where everyone fits.
- Capacity to ship in D days.
- "Median of two sorted arrays" (the famous LC hard).

### Recursive binary search
Same logic but uses the call stack. Equivalent — prefer iterative for small stacks.

## Ternary search — for unimodal functions

If `f(x)` first increases then decreases (unimodal) on `[lo, hi]`, ternary search finds the maximum:

```c
double ternary_search(double lo, double hi) {
    while (hi - lo > 1e-9) {
        double m1 = lo + (hi - lo) / 3;
        double m2 = hi - (hi - lo) / 3;
        if (f(m1) < f(m2)) lo = m1;
        else               hi = m2;
    }
    return (lo + hi) / 2;
}
```

Used in: numerical optimization, finding optimal angle/timing, game-theory problems.

## Exponential / galloping search — unbounded array

You don't know `n`, but you can probe `a[i]` and check if it's still valid.

1. Start at index 1; double until you find `a[i] >= x` or run out of array.
2. Binary search in `[i/2, i]`.

Used in: Tim sort's merge phase, querying a remote sorted dataset.

## Interpolation search — guessing where it should be

If your data is uniformly distributed, **interpolate**:
```
mid = lo + ((x - a[lo]) * (hi - lo)) / (a[hi] - a[lo])
```
- Average **O(log log n)** on uniform data.
- Worst **O(n)** on adversarial data.

Use only when you trust the distribution.

## Searching in a 2D matrix

### Sorted in rows and columns (like a Young tableau)
Start at top-right (or bottom-left). Move down if smaller; left if greater. O(m + n).

### Each row sorted, rows sorted by first element (a flattened sort)
Treat as 1D array via index math: `mid_row = mid / cols, mid_col = mid % cols`. Standard binary search. O(log(m*n)).

## Search across rotated sorted array

You've sorted `[1..7]`, then rotated by 3: `[5, 6, 7, 1, 2, 3, 4]`. Find `x` in O(log n)?

Idea: at each `mid`, one half is still sorted. Check whether `x` is in the sorted half; recurse on the right side. Classic interview question.

## Search in a stream (online)

You don't have the array; just see numbers one by one. "Find the median" → median-of-stream (two heaps, DSA-10).

## Bloom filter — "definitely not, maybe yes"

Probabilistic. Insert: hash element with k hashes, set k bits. Query: check k bits — all set means "maybe present"; any unset means "definitely not."

- O(1) insert and query.
- Tiny space (~10 bits per element for 1% false positive rate).
- **Cannot delete** (in basic version).

Used: caching layers, bytecode dedup, web crawl, BigTable. We'll do this in DSA-26.

## In the kernel

- `bsearch()` exists (`lib/bsearch.c`). Used to look up symbol info, etc.
- A lot of kernel "search" is hash-based or RB-tree-based.
- `find_first_zero_bit` (in bitmaps) is a very fast linear search using x86 `bsf`/`tzcnt`.

## Common bugs

1. **Off-by-one in bounds**: `<= hi` vs `< hi`.
2. **`(lo + hi) / 2` overflow** on huge arrays.
3. **Wrong update for "not found"**: returning `lo` when you meant `-1`, or vice versa.
4. **Re-checking same mid** in a loop with no progress (infinite loop).
5. **Floating-point precision** in ternary/interpolation search — pick a tolerance and be consistent.

## Try it

1. Implement binary search, lower_bound, upper_bound. Verify on duplicates: `[1,2,2,2,3,4]`, `lower_bound(2)=1`, `upper_bound(2)=4`.
2. Implement integer sqrt: smallest `r` with `r*r > n`, then `r-1`. O(log n).
3. Solve "Search in rotated sorted array" — Leetcode 33. O(log n).
4. Solve "Capacity to ship packages within D days" — Leetcode 1011. The pattern: binary search on the answer.
5. Implement exponential/galloping search.
6. Bonus: read `lib/bsearch.c` and compare to your implementation.

## Read deeper

- "Why all your binary searches are broken" (Joshua Bloch's classic article on integer overflow).
- *Programming Pearls* by Jon Bentley — chapter "Writing Correct Programs" is entirely about binary search.
- Knuth TAOCP vol. 3, §6.2 — exhaustive coverage.

Next → [Recursion deep](DSA-16-recursion.md).
