# DSA 14 — Sorting algorithms (read-to-learn version)

> Six sorts you must know, all traced step-by-step.

## Why so many sorts?

Each sort has a different trade-off:

| sort | best | average | worst | space | stable? | in place? |
|---|---|---|---|---|---|---|
| Bubble | O(n) | O(n²) | O(n²) | O(1) | yes | yes |
| Insertion | O(n) | O(n²) | O(n²) | O(1) | yes | yes |
| Selection | O(n²) | O(n²) | O(n²) | O(1) | no | yes |
| Merge | O(n log n) | O(n log n) | O(n log n) | O(n) | yes | no |
| Quick | O(n log n) | O(n log n) | O(n²) | O(log n) stack | no | yes |
| Heap | O(n log n) | O(n log n) | O(n log n) | O(1) | no | yes |

**Stable**: equal elements keep their original relative order. **In place**: needs only O(1)-O(log n) extra memory.

For this chapter, we trace **bubble, insertion, merge, quick** — the four to know cold for interviews.

## Bubble sort

**Idea:** repeatedly walk through the array; swap adjacent pairs that are out of order. Largest element "bubbles" to the end on each pass.

```c
void bubble_sort(int *a, size_t n) {
    for (size_t i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (size_t j = 0; j < n - 1 - i; j++) {
            if (a[j] > a[j+1]) {
                int t = a[j]; a[j] = a[j+1]; a[j+1] = t;
                swapped = 1;
            }
        }
        if (!swapped) break;          // already sorted
    }
}
```

### Trace on `[5, 2, 4, 1, 3]`

**Pass 1** (i=0):

| j | a[j] | a[j+1] | swap? | array after |
|---|---|---|---|---|
| 0 | 5 | 2 | yes | [2, **5**, 4, 1, 3] |
| 1 | 5 | 4 | yes | [2, 4, **5**, 1, 3] |
| 2 | 5 | 1 | yes | [2, 4, 1, **5**, 3] |
| 3 | 5 | 3 | yes | [2, 4, 1, 3, **5**] |

After pass 1: 5 is at the end. swapped=true.

**Pass 2** (i=1, inner runs n-1-i = 3 times):

| j | a[j] | a[j+1] | swap? | array after |
|---|---|---|---|---|
| 0 | 2 | 4 | no  | [2, 4, 1, 3, 5] |
| 1 | 4 | 1 | yes | [2, 1, **4**, 3, 5] |
| 2 | 4 | 3 | yes | [2, 1, 3, **4**, 5] |

After pass 2: 4 in place. swapped=true.

**Pass 3** (i=2, inner 2 iterations):

| j | a[j] | a[j+1] | swap? | array after |
|---|---|---|---|---|
| 0 | 2 | 1 | yes | [1, **2**, 3, 4, 5] |
| 1 | 2 | 3 | no | [1, 2, 3, 4, 5] |

After pass 3: looks sorted. swapped=true.

**Pass 4** (i=3, inner 1 iteration):

| j | a[j] | a[j+1] | swap? | array after |
|---|---|---|---|---|
| 0 | 1 | 2 | no | [1, 2, 3, 4, 5] |

swapped=false → **break.** Done.

**Result: `[1, 2, 3, 4, 5]`.** 4 passes, ~9 swaps.

**Complexity:** O(n²) average and worst. In-place. Stable. **Almost never use it** — but easy to teach.

## Insertion sort

**Idea:** like sorting a hand of cards. Pick up one card at a time, slide it left until it's in the right spot.

```c
void insertion_sort(int *a, size_t n) {
    for (size_t i = 1; i < n; i++) {
        int key = a[i];
        size_t j = i;
        while (j > 0 && a[j-1] > key) {
            a[j] = a[j-1];     // shift right
            j--;
        }
        a[j] = key;             // place key
    }
}
```

### Trace on `[5, 2, 4, 1, 3]`

**i=1, key=2.** Compare 2 with 5 → 2 < 5, shift 5 right.

```
Before:  [5, 2, 4, 1, 3]
Step:    j=1, a[j-1]=5 > 2 → shift: a[1]=5, j=0
Place:   a[0] = 2
After:   [2, 5, 4, 1, 3]
```

**i=2, key=4.** Compare 4 with 5 → shift. Compare 4 with 2 → stop.

```
Before:  [2, 5, 4, 1, 3]
Step:    j=2, a[j-1]=5 > 4 → shift: a[2]=5, j=1
Step:    j=1, a[j-1]=2 < 4 → stop
Place:   a[1] = 4
After:   [2, 4, 5, 1, 3]
```

**i=3, key=1.** Shift 5, shift 4, shift 2.

```
Before:  [2, 4, 5, 1, 3]
j=3:     a[2]=5 > 1 → shift: a[3]=5, j=2
j=2:     a[1]=4 > 1 → shift: a[2]=4, j=1
j=1:     a[0]=2 > 1 → shift: a[1]=2, j=0
j=0:     stop (j==0)
Place:   a[0] = 1
After:   [1, 2, 4, 5, 3]
```

**i=4, key=3.** Shift 5, shift 4. Then 2 ≤ 3 → stop.

```
Before:  [1, 2, 4, 5, 3]
j=4:     a[3]=5 > 3 → shift: a[4]=5, j=3
j=3:     a[2]=4 > 3 → shift: a[3]=4, j=2
j=2:     a[1]=2 < 3 → stop
Place:   a[2] = 3
After:   [1, 2, 3, 4, 5]
```

**Result: `[1, 2, 3, 4, 5]`.** Done.

**Complexity:** O(n²) average and worst, **O(n)** when input is nearly sorted (each key only moves a few slots). Used inside other sorts (e.g., for small subarrays in TimSort and IntroSort).

## Merge sort (divide & conquer)

**Idea:** split the array in half; sort each half (recursively); merge.

```
                  [5, 2, 4, 1, 3]
                  /              \
              [5, 2]            [4, 1, 3]
              /    \             /     \
           [5]    [2]         [4]   [1, 3]
                                       / \
                                    [1]  [3]

   then merge bottom-up:
              [5]+[2] = [2, 5]
              [1]+[3] = [1, 3]
              [4]+[1,3] = [1, 3, 4]
              [2,5]+[1,3,4] = [1, 2, 3, 4, 5]
```

### The merge step (the heart of merge sort)

Given two **sorted** subarrays, walk both with two pointers. At each step, take the smaller front element.

Merge `[2, 5]` and `[1, 3, 4]`:

| Step | left ptr | right ptr | left[i] | right[j] | take | result |
|---|---|---|---|---|---|---|
| 1 | 0 | 0 | 2 | 1 | right (1) | [1] |
| 2 | 0 | 1 | 2 | 3 | left (2) | [1, 2] |
| 3 | 1 | 1 | 5 | 3 | right (3) | [1, 2, 3] |
| 4 | 1 | 2 | 5 | 4 | right (4) | [1, 2, 3, 4] |
| 5 | 1 | exhausted | 5 | — | left (5) | [1, 2, 3, 4, 5] |

**Result: `[1, 2, 3, 4, 5]`.**

Code:

```c
void merge(int *a, int lo, int mid, int hi, int *tmp) {
    int i = lo, j = mid, k = lo;
    while (i < mid && j < hi)
        tmp[k++] = (a[i] <= a[j]) ? a[i++] : a[j++];
    while (i < mid) tmp[k++] = a[i++];
    while (j < hi)  tmp[k++] = a[j++];
    for (int x = lo; x < hi; x++) a[x] = tmp[x];
}

void merge_sort(int *a, int lo, int hi, int *tmp) {
    if (hi - lo < 2) return;            // 0 or 1 element — sorted
    int mid = (lo + hi) / 2;
    merge_sort(a, lo, mid, tmp);
    merge_sort(a, mid, hi, tmp);
    merge(a, lo, mid, hi, tmp);
}
```

**Complexity:** O(n log n) time (every level does O(n) merge work, log n levels). O(n) extra space (tmp array). **Stable.** Best for **linked lists** (no random access penalty).

## Quick sort (divide & conquer with a twist)

**Idea:** pick a **pivot**. Partition the array into "things < pivot" and "things ≥ pivot". Recurse on each half.

```c
int partition(int *a, int lo, int hi) {
    int pivot = a[hi - 1];
    int i = lo;
    for (int j = lo; j < hi - 1; j++) {
        if (a[j] < pivot) {
            int t = a[i]; a[i] = a[j]; a[j] = t;
            i++;
        }
    }
    int t = a[i]; a[i] = a[hi-1]; a[hi-1] = t;
    return i;
}

void quick_sort(int *a, int lo, int hi) {
    if (hi - lo < 2) return;
    int p = partition(a, lo, hi);
    quick_sort(a, lo, p);
    quick_sort(a, p + 1, hi);
}
```

### Trace on `[5, 2, 4, 1, 3]`, pivot = last (3)

Initial: `[5, 2, 4, 1, 3]`. lo=0, hi=5. Pivot=3.

| j | a[j] | < pivot? | i before | action | array |
|---|---|---|---|---|---|
| 0 | 5 | no | 0 | — | [5,2,4,1,3] |
| 1 | 2 | yes | 0 | swap a[0],a[1]; i=1 | [**2**,5,4,1,3] |
| 2 | 4 | no | 1 | — | [2,5,4,1,3] |
| 3 | 1 | yes | 1 | swap a[1],a[3]; i=2 | [2,**1**,4,5,3] |

End of loop. Swap pivot (a[4]=3) with a[i=2]=4:

```
Before swap:  [2, 1, 4, 5, 3]
After swap:   [2, 1, 3, 5, 4]
                       ↑
                       pivot 3 is now at index 2
```

**Return p=2.** Now recurse on `[2, 1]` (left) and `[5, 4]` (right).

Left subarray `[2, 1]` (lo=0, hi=2):
- pivot = a[1] = 1
- j=0: a[0]=2 < 1? no
- swap a[i=0] with a[hi-1=1] → `[1, 2]`. p=0.
- Recurse on `[1]` (size 1, return) and `[2]` (size 1, return).

Right subarray `[5, 4]` (lo=3, hi=5):
- pivot = a[4] = 4
- j=3: a[3]=5 < 4? no
- swap a[i=3] with a[4] → `[..., 4, 5]`. p=3.
- Recurse on `[4]` (size 1) and `[5]` (size 1).

Final array: `[1, 2, 3, 4, 5]`. ✓

**Complexity:** O(n log n) average. **O(n²)** worst case (already-sorted input with naive pivot — every partition is uneven). **In-place.** Not stable.

In production: pick a **random** or **median-of-three** pivot to avoid O(n²) on bad input. Or use **introsort**: quicksort that falls back to heap sort if recursion depth gets too big. C++'s `std::sort` is introsort.

## When to use which sort

```
Small array (< ~20 elements)?         insertion sort.
Need stable, O(n log n) guaranteed?    merge sort.
Need O(1) memory?                      heap sort.
Just want it fast in practice?         quicksort (introsort).
Linked list?                           merge sort.
Already (mostly) sorted?               insertion or TimSort.
Tiny range of integers?                counting/radix sort (next chapter).
```

## In Linux

The kernel's `sort()` (in `lib/sort.c`) is a **heap sort** variant. It's chosen because:
- O(n log n) **worst** case (no surprise stalls in critical paths).
- O(1) extra memory (no recursion stack to allocate).

User-space `qsort()` (libc) is usually introsort.

## Common bugs

### Bug 1: pivoting on the same value forever

```c
// in partition: if (a[j] <= pivot) i++;     // uses <=, not <
```

If many equal values, this still works. But: with `<` and pivot = first element, you may get O(n²) on already-sorted input. Use median-of-three or randomize.

### Bug 2: writing past the merge buffer

```c
int tmp[10];
merge_sort(a, 0, 1000000, tmp);     // BOOM, tmp is too small
```

`tmp` must be at least as big as `a`.

### Bug 3: off-by-one on hi/lo

Most recursive sorts use **half-open** intervals `[lo, hi)`. Mixing with `[lo, hi]` (inclusive) is the #1 bug source. Pick one convention and use it consistently.

### Bug 4: thinking quicksort is always fastest

Quicksort is fast on **random** input. On adversarial input (sorted, reverse-sorted), it can degrade to O(n²). That's why all production code uses introsort (quicksort + fallback).

## Recap

1. Bubble & insertion: O(n²), simple, in place. Insertion is great for **small** or **nearly-sorted** input.
2. Merge: O(n log n), stable, **needs O(n) extra memory.**
3. Quick: O(n log n) average, O(n²) worst, in-place, not stable. Pick pivot smartly.
4. Heap: O(n log n) always, in place, not stable. **Linux kernel's `sort()` uses this.**
5. Production sorts (`qsort`, `std::sort`) use **introsort** — quicksort + heap sort fallback.

## Self-check (in your head)

1. Sort `[3, 1, 2]` with bubble sort. How many swaps in pass 1?

2. Insertion sort on `[1, 2, 3, 4, 5]` (already sorted). How many comparisons total?

3. Why is merge sort preferred for sorting **linked lists**?

4. Why does quicksort on already-sorted input perform badly with a "first element as pivot" strategy?

5. Heap sort uses what data structure under the hood?

---

**Answers:**

1. Pass 1: j=0: 3>1 swap → [1,3,2]. j=1: 3>2 swap → [1,2,3]. **2 swaps.** Then pass 2 finds no swaps and bails out.

2. Each iteration of the outer loop (i=1..4) does **one comparison** (a[i-1] ≤ a[i] → no shift, place key in same spot). **4 comparisons total**. O(n) on already-sorted input — fast!

3. Linked lists have **no random access** (`a[i]` is O(i)), so random-pivot partitioning (quicksort) is awkward. Merge sort only needs **sequential access** — split list into halves, merge by walking both with two pointers. Natural fit.

4. With "first element as pivot" on a sorted array: partition puts everything in the right half (everything is ≥ pivot). Each recursion shrinks by 1 instead of halving. Total: n + (n-1) + ... + 1 = O(n²) recursive calls.

5. A **binary heap** (typically max-heap for ascending sort).

If 4/5 you understand sorting. 5/5, move on.

Next → [DSA-15 — Searching](DSA-15-searching.md).
