# DSA 14 — Sorting algorithms

> "Sort it" is the second most-asked thing in interviews. Know all the standard sorts cold.

## The sorting menu

| Algorithm | Best | Avg | Worst | Space | Stable? | In-place? |
|---|---|---|---|---|---|---|
| Bubble sort | n | n² | n² | 1 | yes | yes |
| Insertion sort | n | n² | n² | 1 | yes | yes |
| Selection sort | n² | n² | n² | 1 | no | yes |
| Merge sort | n log n | n log n | n log n | n | yes | no |
| Quicksort | n log n | n log n | n² | log n (stack) | no | yes |
| Heapsort | n log n | n log n | n log n | 1 | no | yes |
| Counting sort | n + k | n + k | n + k | k | yes | no |
| Radix sort | n × d | n × d | n × d | n + base | yes | no |
| Bucket sort | n + k | n + k | n² | n + k | yes | no |
| Tim sort | n | n log n | n log n | n | yes | no |
| Intro sort | n log n | n log n | n log n | log n | no | yes |

**Stable** = equal elements stay in original order. Important when sorting records by one field while preserving prior order of another.

## The simple ones

### Bubble sort — don't use, but know it
```c
for (int i = 0; i < n - 1; i++)
    for (int j = 0; j < n - 1 - i; j++)
        if (a[j] > a[j+1]) { swap(&a[j], &a[j+1]); }
```

### Insertion sort — fast on small or nearly-sorted
```c
for (int i = 1; i < n; i++) {
    int x = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > x) { a[j+1] = a[j]; j--; }
    a[j+1] = x;
}
```
**Use case**: hybrid sorts (Tim sort, intro sort) switch to insertion sort for small subarrays. The reason: lower constants beat n log n when n < ~16.

### Selection sort — never use
Finds the minimum of remaining elements; swaps to position. Always O(n²), no real virtues.

## The real ones

### Merge sort — the classic divide & conquer
1. Split in half.
2. Recursively sort each half.
3. Merge.

```c
void merge(int *a, int l, int m, int r) {
    int n1 = m - l + 1, n2 = r - m;
    int *L = malloc(n1 * sizeof(int));
    int *R = malloc(n2 * sizeof(int));
    for (int i = 0; i < n1; i++) L[i] = a[l + i];
    for (int j = 0; j < n2; j++) R[j] = a[m + 1 + j];
    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) a[k++] = (L[i] <= R[j]) ? L[i++] : R[j++];
    while (i < n1) a[k++] = L[i++];
    while (j < n2) a[k++] = R[j++];
    free(L); free(R);
}

void merge_sort(int *a, int l, int r) {
    if (l < r) {
        int m = (l + r) / 2;
        merge_sort(a, l, m);
        merge_sort(a, m + 1, r);
        merge(a, l, m, r);
    }
}
```

- O(n log n) always.
- **Stable.** Used when stability matters.
- O(n) extra space — annoying.
- Excellent for **external sorting** (data on disk that doesn't fit in RAM): merge sorted runs. Used by `sort(1)`, MapReduce, Spark.

### Quicksort — the fastest in practice (usually)
1. Pick a **pivot**.
2. **Partition**: rearrange so left ≤ pivot ≤ right.
3. Recurse on each side.

```c
int partition(int *a, int l, int r) {
    int pivot = a[r];
    int i = l;
    for (int j = l; j < r; j++) {
        if (a[j] <= pivot) { swap(&a[i], &a[j]); i++; }
    }
    swap(&a[i], &a[r]);
    return i;
}

void quick_sort(int *a, int l, int r) {
    if (l < r) {
        int p = partition(a, l, r);
        quick_sort(a, l, p - 1);
        quick_sort(a, p + 1, r);
    }
}
```

- Average **O(n log n)**, **worst O(n²)** when pivot is bad.
- Constants are very small → in benchmarks, quicksort is usually fastest.
- Not stable.
- In-place (with stack space O(log n)).

**Pivot selection matters.** Random pivot, median-of-three, or "ninther" defeat the worst case in practice. Worst-case adversaries still exist (the "killer adversary"), which is why C++'s std::sort uses **introsort**: quicksort, but if recursion depth gets too high, fall back to heapsort.

### Heapsort
Build a max-heap; repeatedly pop the max into the array's tail.
- O(n log n) worst case (no degradation).
- In-place.
- **Not stable.**
- Cache-unfriendly (heap accesses jump around).

Used as **fallback** in introsort. Used in OS schedulers (when worst case must be bounded).

### Tim sort — the production sort
Hybrid of merge sort and insertion sort. Identifies "runs" of already-sorted data; merges them efficiently. Adapts to the data.

- O(n) on already-sorted data; O(n log n) worst.
- Stable.
- Used in: Python's `list.sort`, Java 7+'s `Arrays.sort` for objects, Android, V8's `Array.prototype.sort`.

**The lesson**: in production, never write your own. Use the language's `sort`. It's almost always Tim sort or intro sort, and they're highly optimized.

## Non-comparison sorts (faster than n log n!)

If you can avoid comparisons (e.g., keys are small integers), you can sort in O(n).

### Counting sort
```c
void counting_sort(int *a, int n, int K) {  // values in 0..K-1
    int count[K]; memset(count, 0, sizeof(count));
    for (int i = 0; i < n; i++) count[a[i]]++;
    for (int i = 0, k = 0; i < K; i++)
        while (count[i]--) a[k++] = i;
}
```
O(n + K). Useful when K is small. Stable variant uses prefix sums.

### Radix sort
Sort by least-significant digit first (LSD), or most-significant (MSD). Each pass is a stable counting sort by one digit.
- O(n × d) where d = number of digits.
- Used for fixed-width integer/string keys (network sorting, GPU sort kernels).

### Bucket sort
Spread items into buckets based on a hash; sort each bucket; concatenate.
- O(n) when input is uniform; O(n²) worst.

## When the input matters more than the algorithm

- **Already sorted?** Insertion sort or Tim sort: O(n).
- **Many duplicates?** 3-way partition (Dutch flag) version of quicksort.
- **Small K**? Counting sort.
- **Big data, doesn't fit in RAM?** External merge sort.
- **GPU**? Radix sort (parallel friendly), bitonic sort.

## In the kernel

- `lib/sort.c` — `sort()` function, currently uses **heapsort** (worst-case O(n log n), no extra memory).
- `lib/list_sort.c` — `list_sort()` sorts a `struct list_head` linked list. Uses bottom-up merge sort (no recursion, no extra memory).

## Common interview tricks

1. **"Sort an almost-sorted array (each element ≤ k away from its sorted position)"** → use a min-heap of size k+1. O(n log k).
2. **"Sort an array of 0s, 1s, and 2s"** → Dutch national flag, O(n), in-place.
3. **"Top K elements"** → see DSA-10 (heap of size K).
4. **"Quickselect"** — find Kth largest in O(n) average without sorting. Like quicksort but recurse on only one side.
5. **"Sort linked list"** → merge sort (no random access; quicksort needs that).

## Common bugs

1. **Index off-by-one** in merge or partition.
2. **Stack overflow** on quicksort with poor pivot on already-sorted input.
3. **Not handling duplicates** in partition (causes O(n²)).
4. **Comparator inconsistency** — must be a strict weak order. `cmp` must be deterministic and transitive.

## Try it

1. Implement merge sort and quicksort. Sort 1M random ints; time both.
2. Implement insertion sort. Time on n=10. Compare to merge sort. Why is insertion faster?
3. Implement Dutch national flag (sort 0s, 1s, 2s in one pass).
4. Implement quickselect; find the median in O(n) average.
5. Read `lib/sort.c` in the Linux kernel. Why heapsort and not quicksort?
6. Implement counting sort and radix sort. Time vs comparison sort on uniform random ints.

## Read deeper

- CLRS chapters 6–8 (Heapsort, Quicksort, Sorting in Linear Time).
- "TimSort: A Description and Implementation Of A Hybrid Sorting Algorithm" — original paper.
- For huge data: Vitter, "External Memory Algorithms and Data Structures."

Next → [Searching](DSA-15-searching.md).
