# DSA 15 — Searching (read-to-learn version)

> Searching = finding a target. The data structure decides how fast.

## The big idea

Given a target value, where is it (or is it even there)?

- **Unsorted array** → no shortcuts. Look at every element. **O(n).**
- **Sorted array** → halve the search range each step. **O(log n).** This is **binary search.**
- **Hash table** → compute hash, jump to bucket. **O(1) average.**
- **BST** (balanced) → walk the tree. **O(log n).**

Linear search is trivial; hash and BST are covered in DSA-06 and DSA-07. This chapter focuses on **binary search** — the most asked interview pattern of all time.

## Linear search (the obvious one)

```c
int linear_search(int *a, int n, int x) {
    for (int i = 0; i < n; i++)
        if (a[i] == x) return i;
    return -1;
}
```

Walk left to right. Found: return index. Reach the end: return -1.

**Cost: O(n).** No assumption about the array's order. If the input is unsorted, this is the best you can do.

## Binary search (the magic one)

If the array is **sorted**, you can be much faster.

**Idea:** look at the middle. The target is either:
- the middle element (done), or
- in the **left** half (because target < middle), or
- in the **right** half (because target > middle).

Either way, you've eliminated half. Repeat.

```c
int binary_search(int *a, int n, int x) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;     // (1) avoids overflow vs (lo+hi)/2
        if (a[mid] == x) return mid;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid - 1;
    }
    return -1;
}
```

(1) `(lo + hi) / 2` can overflow if both are near INT_MAX. `lo + (hi - lo) / 2` is mathematically equal but avoids overflow.

### Trace 1: find 60 in `[10, 20, 30, 40, 50, 60, 70]`

```
index:    0    1    2    3    4    5    6
value:  [10, 20, 30, 40, 50, 60, 70]
```

Initial: lo=0, hi=6.

| Step | lo | hi | mid | a[mid] | comparison | new lo | new hi |
|---|---|---|---|---|---|---|---|
| 1 | 0 | 6 | 3 | 40 | 40 < 60 | **4** | 6 |
| 2 | 4 | 6 | 5 | 60 | match! return 5 |   |   |

**Found at index 5 in 2 steps.**

### Trace 2: find 35 in the same array (not present)

| Step | lo | hi | mid | a[mid] | comparison | new lo | new hi |
|---|---|---|---|---|---|---|---|
| 1 | 0 | 6 | 3 | 40 | 40 > 35 | 0 | **2** |
| 2 | 0 | 2 | 1 | 20 | 20 < 35 | **2** | 2 |
| 3 | 2 | 2 | 2 | 30 | 30 < 35 | **3** | 2 |
| 4 | 3 | 2 | (lo > hi) | exit loop |   |   |   |

Return -1. **Not found, in 3 steps.**

### Why log n?

The range starts at size 7. After step 1: 3 (right half). After step 2: 1. After step 3: 0 (lo > hi).

Each step **halves** the range. Number of halvings before reaching size 0: **log₂(n)**. For n=7, that's about 3.

For n=1,000,000: **20 steps**. For n=1,000,000,000: **30 steps.** Almost free.

## The off-by-one minefield (read carefully)

Binary search is famous for off-by-one bugs. **Understand the conventions** to avoid them.

### Convention 1: closed interval `[lo, hi]`

- Initialize: `lo = 0, hi = n - 1`.
- Loop: `while (lo <= hi)`.
- Update: `lo = mid + 1` or `hi = mid - 1`.

This is what we used above.

### Convention 2: half-open interval `[lo, hi)`

- Initialize: `lo = 0, hi = n`.
- Loop: `while (lo < hi)`.
- Update: `lo = mid + 1` or `hi = mid`.

Many real codebases (including the C++ STL `lower_bound`) use this style. Pick one. **Stick with it.**

## Variant: find the **first** occurrence (lower_bound)

What if the array has duplicates and you want the **leftmost** match?

```c
int lower_bound(int *a, int n, int x) {
    int lo = 0, hi = n;            // half-open
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid;     // keep going left even on equal
    }
    return lo;     // index of first element >= x
}
```

The trick: when `a[mid] == x`, we **don't return immediately**. We push `hi = mid` to keep searching left.

### Trace: lower_bound of 30 in `[10, 20, 30, 30, 30, 40, 50]`

```
index:    0    1    2    3    4    5    6
value:  [10, 20, 30, 30, 30, 40, 50]
```

| Step | lo | hi | mid | a[mid] | a[mid] < 30? | action |
|---|---|---|---|---|---|---|
| 1 | 0 | 7 | 3 | 30 | no | hi = 3 |
| 2 | 0 | 3 | 1 | 20 | yes | lo = 2 |
| 3 | 2 | 3 | 2 | 30 | no | hi = 2 |
| 4 | 2 | 2 | exit |   |   |   |

Return **lo = 2.** First occurrence of 30 is at index 2. ✓

If we'd used the basic binary_search, it might return any of indices 2, 3, 4. lower_bound guarantees the leftmost.

`upper_bound` is the symmetric — find the first element **strictly greater than** x.

## Other search variants

### Search for "what would be the insertion point"

`lower_bound(a, n, x)` returns where you'd **insert** x to keep the array sorted. Useful for "insert in sorted list."

### Search in a rotated sorted array

Classic interview: `[4, 5, 6, 7, 0, 1, 2]` is sorted, then rotated. Search for 0.

The trick: at each step, **one half is still sorted**. Test if target is in the sorted half; if yes, recurse there; otherwise the other half.

```c
int search_rotated(int *a, int n, int x) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == x) return mid;
        if (a[lo] <= a[mid]) {
            // left half is sorted
            if (x >= a[lo] && x < a[mid]) hi = mid - 1;
            else                            lo = mid + 1;
        } else {
            // right half is sorted
            if (x > a[mid] && x <= a[hi]) lo = mid + 1;
            else                           hi = mid - 1;
        }
    }
    return -1;
}
```

Still O(log n). The "which half is sorted" trick is the key insight.

## Ternary search (only on unimodal functions)

Binary search needs a **monotonic** array (sorted). For some functions that increase then decrease (one peak), you can ternary-search to find the peak — split the range into thirds. Used in optimization, not in everyday data structures.

## Search in a 2D matrix

If a 2D matrix is sorted row by row (and the first element of each row > last of previous), treat it as a 1D array of length r×c with index i mapping to `(i / c, i % c)`. Apply binary search.

If it's only row-sorted and column-sorted (each row sorted left to right; each column sorted top to bottom), use the **stair-case search**:

- Start at top-right corner.
- If target == cell → done.
- If target < cell → go left.
- If target > cell → go down.

O(r + c). Beautiful.

## When binary search isn't applicable

- **Unsorted array** → can't halve meaningfully. Use linear search or build a hash table.
- **Linked list** → `a[mid]` is O(n). Binary search on linked list is O(n log n) — worse than linear scan O(n)! Don't.
- **Approximate matches** → binary search finds exact. For "closest match," extend lower_bound and check both sides.

## In Linux

- `lib/sort.c` and `lib/bsearch.c` — kernel's binary-search-style helpers.
- DRM and amdgpu use `bsearch` for finding GPU page entries, register descriptions, etc.
- The radix tree (`include/linux/radix-tree.h`) is a more advanced search structure used in the page cache — closer to a trie + sparse array.

## Common bugs

### Bug 1: forgetting the `+1` and `-1`

```c
while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (a[mid] < x) lo = mid;       // BUG: should be mid+1
    else            hi = mid;
}
```

If `a[mid] < x` and you set `lo = mid` instead of `mid+1`, the next iteration has `lo == mid` again — **infinite loop**.

### Bug 2: assuming sorted when it isn't

```c
binary_search(unsorted_array, n, x);    // returns garbage
```

Binary search only works on sorted input. Always verify (or sort first).

### Bug 3: integer overflow on huge arrays

```c
int mid = (lo + hi) / 2;     // overflows if lo, hi near INT_MAX
```

Use `lo + (hi - lo) / 2`.

### Bug 4: closed vs half-open interval mixing

The biggest source of off-by-one bugs. Decide your convention up front and stick to it for **all** binary searches in your codebase.

## Recap

1. **Linear search**: O(n). Works on any input.
2. **Binary search**: O(log n). Requires **sorted** input.
3. Halving the range each step is the magic — log₂(1B) ≈ 30 steps.
4. **lower_bound** finds the leftmost occurrence (or insertion point).
5. **Rotated sorted array** still allows O(log n) search via "one half is sorted" trick.
6. Don't use binary search on linked lists.

## Self-check (in your head)

1. Binary search on `[1, 3, 5, 7, 9]` for x=5. How many steps?

2. Binary search on a sorted array of 1 million ints. Maximum steps?

3. `lower_bound([1, 2, 4, 4, 4, 6], 4)` → returns what?

4. `lower_bound([1, 2, 4, 4, 4, 6], 5)` → returns what?

5. Why is `(lo + hi) / 2` dangerous on huge arrays?

---

**Answers:**

1. lo=0, hi=4. mid=2 → a[2]=5 → match! **1 step.**

2. log₂(1,000,000) ≈ **20 steps.**

3. **2** — the leftmost index where 4 appears.

4. **5** — the index where 5 *would* be inserted to keep sorted (right after the last 4, before 6).

5. Integer overflow: if `lo` and `hi` are near INT_MAX, `lo + hi` exceeds INT_MAX → wraps to a negative number → `mid` is negative → array access at negative index → undefined behavior. Use `lo + (hi - lo) / 2`.

If 4/5 you understand searching. 5/5, move on.

Next → DSA-16 (recursion) — original style still readable. Or jump to [DSA-26 — system-design DSA](DSA-26-system-design-ds.md) for LRU cache.
