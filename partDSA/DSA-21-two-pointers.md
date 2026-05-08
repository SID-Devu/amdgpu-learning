# DSA 21 — Two pointers, sliding window, fast/slow

> Three patterns that turn O(n²) brute force into O(n) elegance. Learn them once.

## Two pointers — opposite ends

Two indices, one at each end, moving inward.

### Two-sum in a sorted array
```c
int two_sum_sorted(int *a, int n, int target, int *i_out, int *j_out) {
    int i = 0, j = n - 1;
    while (i < j) {
        int s = a[i] + a[j];
        if (s == target) { *i_out = i; *j_out = j; return 0; }
        if (s < target) i++;
        else j--;
    }
    return -1;
}
```
O(n).

### Reverse string in place
Already seen. Two pointers from each end.

### Trapping rain water (clever 2-ptr O(n) version)
Maintain `left_max`, `right_max`; advance the smaller side. The trapped water at each step is `min(left_max, right_max) - height[i]`.

### Container with most water (Leetcode 11)
Two heights, area = `min(h[i], h[j]) * (j - i)`. Move the shorter side inward; track max.

## Two pointers — same direction (read/write)

A "fast" pointer that scans, a "slow" pointer that builds the result.

### Remove duplicates from sorted array (in place)
```c
int dedup(int *a, int n) {
    if (n == 0) return 0;
    int j = 1;
    for (int i = 1; i < n; i++)
        if (a[i] != a[i-1]) a[j++] = a[i];
    return j;
}
```

### Move zeros to end (DSA-02 again)

### Partition by predicate (in place)
```c
int partition_p(int *a, int n, int (*pred)(int)) {
    int j = 0;
    for (int i = 0; i < n; i++) if (pred(a[i])) {
        int t = a[i]; a[i] = a[j]; a[j] = t;
        j++;
    }
    return j;   // first index of "false" group
}
```

## Sliding window

Maintain a window `[l, r]` over a sequence. Expand `r`; when window violates a constraint, shrink from `l`. Each element enters and leaves at most once → O(n).

### Longest substring with at most K distinct characters
```c
int longest_k_distinct(const char *s, int K) {
    int count[256] = {0}, distinct = 0;
    int best = 0, l = 0;
    for (int r = 0; s[r]; r++) {
        if (count[(unsigned char)s[r]]++ == 0) distinct++;
        while (distinct > K) {
            if (--count[(unsigned char)s[l++]] == 0) distinct--;
        }
        if (r - l + 1 > best) best = r - l + 1;
    }
    return best;
}
```

### Minimum window substring
"Smallest window in s containing all chars of t." Same pattern, plus a counter of "needed counts."

### Maximum sum of subarray of size k
Window of size exactly k. Maintain running sum; on each step subtract leaving element, add entering one.

### Longest substring without repeating characters
Sliding window with a "last-seen" map.

The pattern: **two indices (`l`, `r`) that both only ever move forward**. Total work O(n).

## Fast/slow pointers (Floyd's tortoise & hare)

### Cycle detection in linked list
DSA-04. Slow steps 1, fast steps 2.

### Find the start of the cycle
Once they meet, reset slow to head; advance both at speed 1; they meet at the cycle entry. Beautiful but tricky to derive — comes up in interviews.

### Find duplicate number (Leetcode 287)
Array of `n+1` integers in `[1..n]`, exactly one duplicated. Treat as linked list (`a[i]` = next index). Floyd's algorithm finds the duplicate in O(n) time, O(1) space.

### Find middle of linked list (one pass)
DSA-04.

## Three pointers

### 3Sum
"Triples summing to 0." Sort. For each `i`, two-pointer on `[i+1, n-1]`. O(n²).

### Sort colors (Dutch national flag)
Three pointers `lo`, `mid`, `hi`. `lo` for next 0, `hi` for next 2, `mid` scans.
```c
void sort012(int *a, int n) {
    int lo = 0, mid = 0, hi = n - 1;
    while (mid <= hi) {
        if (a[mid] == 0) { swap(&a[lo++], &a[mid++]); }
        else if (a[mid] == 1) mid++;
        else { swap(&a[mid], &a[hi--]); }
    }
}
```
One pass, O(n), in place.

## Patterns to recognize

- "Subarray / substring with property" → sliding window.
- "Pair / triple summing to X" in sorted → two pointers.
- "Cycle in linked list / sequence" → fast/slow.
- "In-place rearrangement preserving order" → read/write pointers.
- "Find Kth from end" → fast pointer K ahead, then both move.

## Common bugs

1. **Wrong shrink condition** in sliding window — over-shrinking gives wrong answer.
2. **Updating window state in wrong order** — check the entering/leaving update sequence.
3. **Off-by-one in window size** (`r - l` vs `r - l + 1`).
4. **Updating `l` past `r`** without bounds check.
5. **Forgetting that two pointers requires sorted input** for sum-style problems.

## Try it (top patterns)

1. Two-sum sorted (DSA above) — implement.
2. Longest substring without repeating chars — sliding window.
3. Minimum window substring — sliding window with counts.
4. Trapping rain water — 2-pointer O(n).
5. Sort colors — Dutch national flag.
6. Linked list cycle — Floyd's. Then find cycle start.
7. Bonus: 3Sum, 4Sum — generalize.

## Read deeper

- "Two-pointer techniques" tutorials on Leetcode and HackerRank.
- *Cracking the Coding Interview* — chapters on linked lists and arrays.

Next → [Backtracking](DSA-22-backtracking.md).
