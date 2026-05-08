# DSA 10 — Heaps & priority queues (read-to-learn version)

> A heap is a tree pretending to be an array — and it gives you the **min** (or **max**) of millions of items in **constant time**.

## The big idea

A **priority queue** is a data structure that always lets you grab the **highest priority** item. Used everywhere:

- OS scheduler picks the highest-priority runnable task.
- Dijkstra's algorithm picks the closest unvisited node.
- Top-K problems: "find the 100 largest sales today."
- Linux's deadline scheduler (`include/linux/rbtree.h` in scheduling code).

A **heap** is the simplest, fastest way to implement a priority queue.

## The heap shape and rules

A **min-heap**:
1. Is a **complete** binary tree (every level full except possibly the last; last level filled left-to-right).
2. **Heap property**: every node's value is ≤ its children's values.

The smallest value is always at the **root**. Given any node, both its children are ≥ it.

Picture (values 1, 3, 6, 5, 9, 8, 7):

```
              1                    ← root (smallest)
            /   \
           3     6
          / \   / \
         5   9 8   7
```

Verify the property:
- 1 ≤ 3 ✓, 1 ≤ 6 ✓
- 3 ≤ 5 ✓, 3 ≤ 9 ✓
- 6 ≤ 8 ✓, 6 ≤ 7 ✓

A **max-heap** is the same idea, flipped: every node ≥ its children. Largest at root.

## The trick: store a tree as an array (no pointers!)

Because the tree is **complete**, we can pack it into an array level by level.

```
   tree:                              array:

              1                       index:  0  1  2  3  4  5  6
            /   \                     value: [1, 3, 6, 5, 9, 8, 7]
           3     6
          / \   / \
         5   9 8   7
```

The mapping:
- `arr[0]` = root.
- For node at index `i`:
  - **left child** is at `2*i + 1`
  - **right child** is at `2*i + 2`
  - **parent** is at `(i - 1) / 2`

### Verify on the example

| index | value | left child idx | left value | right child idx | right value | parent idx | parent value |
|---|---|---|---|---|---|---|---|
| 0 | 1 | 1 | 3 | 2 | 6 | — | — |
| 1 | 3 | 3 | 5 | 4 | 9 | 0 | 1 |
| 2 | 6 | 5 | 8 | 6 | 7 | 0 | 1 |
| 3 | 5 | (none) | | (none) | | 1 | 3 |
| 4 | 9 | (none) | | (none) | | 1 | 3 |
| 5 | 8 | (none) | | (none) | | 2 | 6 |
| 6 | 7 | (none) | | (none) | | 2 | 6 |

Every parent ≤ its children. The heap property holds.

**Why arrays?** No pointer overhead, contiguous memory (cache-friendly), no malloc per node. Tons of efficiency over a tree-with-pointers.

## Insert into a min-heap (sift up)

To add a new element:
1. Append at the end of the array.
2. **Sift up**: while the new element is smaller than its parent, swap them.

### Trace: insert 2 into our heap

Start:

```
              1
            /   \
           3     6
          / \   / \
         5   9 8   7

  arr = [1, 3, 6, 5, 9, 8, 7]
```

**Step 1: append**

```
  arr = [1, 3, 6, 5, 9, 8, 7, 2]
                              ↑
                              new at index 7
```

```
              1
            /   \
           3     6
          / \   / \
         5   9 8   7
        /
       2     ← inserted at next free slot
```

The heap property is broken — 2's parent is 5, but 2 < 5.

**Step 2: sift up**

Index 7's parent: `(7-1)/2 = 3`, value 5. 2 < 5 → **swap.**

```
  arr = [1, 3, 6, 2, 9, 8, 7, 5]
                  ↑
                  2 is now at index 3
```

```
              1
            /   \
           3     6
          / \   / \
         2   9 8   7
        /
       5
```

Still broken — 2's parent is 3 (index `(3-1)/2 = 1`). 2 < 3 → **swap.**

```
  arr = [1, 2, 6, 3, 9, 8, 7, 5]
            ↑
            2 is now at index 1
```

```
              1
            /   \
           2     6
          / \   / \
         3   9 8   7
        /
       5
```

2's parent is 1 (index 0). 2 > 1 → **stop.** Heap property restored.

| Step | swap? | between | after |
|---|---|---|---|
| 0 | append | — | `[1, 3, 6, 5, 9, 8, 7, 2]` |
| 1 | yes | indices 7 ↔ 3 | `[1, 3, 6, 2, 9, 8, 7, 5]` |
| 2 | yes | indices 3 ↔ 1 | `[1, 2, 6, 3, 9, 8, 7, 5]` |
| 3 | no (2 > 1) | — | done |

**Two swaps. O(log n) — at most height of the tree.**

## Remove the min (extract-min, sift down)

To remove the root:
1. Save root's value (this is the answer).
2. Move the **last element** of the array into the root.
3. Decrement size.
4. **Sift down**: swap with the smaller child while needed.

### Trace: extract-min from `[1, 2, 6, 3, 9, 8, 7, 5]`

Start:

```
              1
            /   \
           2     6
          / \   / \
         3   9 8   7
        /
       5
```

**Step 1: save 1.** That's the answer.

**Step 2: move last (index 7, value 5) to root.**

```
  arr = [5, 2, 6, 3, 9, 8, 7]      (size 7 now)
```

```
              5
            /   \
           2     6
          / \   / \
         3   9 8   7
```

Heap property broken — 5 > 2 (its left child).

**Step 3: sift down.** Compare 5 with its children (2, 6); pick the smaller (2). 5 > 2 → swap.

```
  arr = [2, 5, 6, 3, 9, 8, 7]
```

```
              2
            /   \
           5     6
          / \   / \
         3   9 8   7
```

Now 5 is at index 1. Children: 3 (index 3), 9 (index 4). Smaller is 3. 5 > 3 → swap.

```
  arr = [2, 3, 6, 5, 9, 8, 7]
```

```
              2
            /   \
           3     6
          / \   / \
         5   9 8   7
```

5 is at index 3. Children: indices 7, 8 — but the array only has 7 elements (indices 0–6). 5 has **no children** → stop.

| Step | action | array | tree |
|---|---|---|---|
| 0 | save root | `[1,2,6,3,9,8,7,5]` | (above) |
| 1 | last → root | `[5,2,6,3,9,8,7]` | top has 5 |
| 2 | swap with smaller child (2) | `[2,5,6,3,9,8,7]` | 5 down |
| 3 | swap with smaller child (3) | `[2,3,6,5,9,8,7]` | 5 down again |
| 4 | no children — stop |   |   |

**Returned: 1.** **Two sift-down swaps.** **O(log n).**

The tree's height is at most ~log₂(n). Sifting up or down can move at most that many levels. Both insert and extract-min are **O(log n).**

## The full code (min-heap, array-backed)

```c
typedef struct {
    int *a;
    size_t size, cap;
} heap_t;

static void heap_swap(heap_t *h, size_t i, size_t j) {
    int t = h->a[i]; h->a[i] = h->a[j]; h->a[j] = t;
}

void heap_push(heap_t *h, int v) {
    if (h->size == h->cap) {
        h->cap = h->cap ? h->cap * 2 : 16;
        h->a = realloc(h->a, h->cap * sizeof(int));
    }
    size_t i = h->size++;
    h->a[i] = v;
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (h->a[parent] <= h->a[i]) break;     // heap restored
        heap_swap(h, parent, i);
        i = parent;
    }
}

int heap_pop(heap_t *h) {
    int top = h->a[0];
    h->size--;
    h->a[0] = h->a[h->size];
    size_t i = 0;
    for (;;) {
        size_t l = 2*i + 1, r = 2*i + 2, smallest = i;
        if (l < h->size && h->a[l] < h->a[smallest]) smallest = l;
        if (r < h->size && h->a[r] < h->a[smallest]) smallest = r;
        if (smallest == i) break;                // heap restored
        heap_swap(h, i, smallest);
        i = smallest;
    }
    return top;
}
```

Read this carefully against the traces above. Each line maps directly to a step in the trace.

## Heap operations summary

| Operation | Time |
|---|---|
| Find min (peek root) | **O(1)** |
| Insert | **O(log n)** |
| Extract-min | **O(log n)** |
| Build heap from n elements | **O(n)** (clever, see below) |
| Heap sort (sort using heap) | **O(n log n)** |

## Build heap in O(n) — Floyd's bottom-up

If you have `n` items in a random array and want to make it a heap, you could call `heap_push` n times — that's O(n log n). But there's a faster way.

**Idea:** start from the **bottom** of the tree. Leaves are already valid heaps (size 1). Sift down each non-leaf node.

```c
void build_heap(int *a, size_t n) {
    if (n < 2) return;
    for (size_t i = n / 2 - 1; ; i--) {       // last non-leaf to root
        sift_down(a, n, i);
        if (i == 0) break;
    }
}
```

The mathematical reason this is O(n) (not O(n log n)): nodes at depth d are O(n / 2^d), and each sift-down moves O(log n - d) levels. The sum across all depths telescopes to O(n).

You don't need to memorize the proof. Just remember: **build_heap is O(n).** Heap sort then extract-min n times = O(n + n log n) = **O(n log n).**

## Heap sort (sort an array in place)

```c
void heap_sort(int *a, size_t n) {
    build_heap(a, n);                  // step 1: O(n)
    for (size_t end = n - 1; end > 0; end--) {
        // (a) swap root (smallest) to end
        int t = a[0]; a[0] = a[end]; a[end] = t;
        // (b) heapify the smaller heap (size = end)
        sift_down(a, end, 0);
    }
}
```

Wait — that sorts **descending**? Yes, with a min-heap. Use a **max-heap** for ascending order. (Min/max are symmetric — flip every comparison.)

Heap sort is **O(n log n)** worst case, **O(1)** extra space (in-place). Compare to merge sort (O(n log n) but O(n) extra space). Heap sort is the in-place sort of choice when memory is tight.

## Top-K problem (interview classic)

"Given a stream of N numbers, find the K largest."

Naive: sort the whole stream — O(N log N). For huge N this is wasteful — we only need K values.

**Better:** keep a **min-heap of size K**. For each new value:
- If heap has fewer than K items, push.
- Else if new value > heap root, pop and push the new value.

End: the heap holds the K largest.

Why min-heap for "K largest"? Because we only need to **evict the smallest of the current K** when a bigger one arrives.

**Time:** O(N log K). For N=10⁹, K=100, this is ~10¹⁰ ops vs ~3×10¹⁰ for full sort. Big win.

### Trace: K=3, stream = [4, 1, 7, 2, 9, 3]

Start: empty heap (size ≤ 3).

| value | action | heap (sorted view) |
|---|---|---|
| 4 | size<3, push | [4] |
| 1 | size<3, push | [1, 4] |
| 7 | size<3, push | [1, 4, 7] |
| 2 | 2 < heap_min (1)? no. **2 > 1 → pop 1, push 2** | [2, 4, 7] |
| 9 | 9 > 2 → pop 2, push 9 | [4, 7, 9] |
| 3 | 3 > 4? no → skip | [4, 7, 9] |

End: top-3 largest = **{4, 7, 9}.** ✓

## Where heaps appear in real systems

- **Linux scheduler (CFS)**: uses a Red-Black tree (similar idea: extract-min is O(log n)) to find the next runnable task.
- **Linux kernel timers**: a heap (`hrtimer`) ordered by expiration time. Next-to-fire is at the root.
- **Dijkstra's algorithm** (next chapter): repeatedly extract the closest unvisited node from a heap.
- **Merge K sorted arrays**: use a heap of K iterators to always pick the smallest next value. (Used in databases, MapReduce, log file merging.)
- **`std::priority_queue` in C++**, `heapq` in Python — built-in heaps you'll use in production code.

## Common bugs

### Bug 1: forgetting that the array is 0-indexed

If you copied a textbook that uses 1-indexed arrays:
- left = `2*i`, right = `2*i + 1`, parent = `i / 2`.

C is **0-indexed**, so:
- left = `2*i + 1`, right = `2*i + 2`, parent = `(i - 1) / 2`.

**Pick one and stick to it.**

### Bug 2: bounds check on children during sift-down

```c
size_t l = 2*i + 1;
if (a[l] < a[i])    // BUG: didn't check that l < size
```

If `l >= size`, you read past the end. Always: `if (l < size && a[l] < a[i])`.

### Bug 3: assumption that heap is sorted

A heap is **partially** ordered. You can't read off the array as "sorted." Only the **root** is the min. The 2nd smallest could be left or right of the root. To get sorted output, extract-min repeatedly (heap sort).

### Bug 4: comparing in the wrong direction (min vs max)

Min-heap: parent ≤ children (sift up if smaller, sift down to smaller child).
Max-heap: parent ≥ children (sift up if larger, sift down to larger child).

Mix them up → "heap" doesn't satisfy any property and operations don't restore it.

## Recap

1. A heap is a complete binary tree stored as an **array**. `arr[i]` parent at `(i-1)/2`; children at `2i+1`, `2i+2`.
2. **Min-heap**: parent ≤ children. Min always at root.
3. Insert and extract-min: **O(log n)**. Build a heap from n items: **O(n)**.
4. Heap sort: **O(n log n)** in-place.
5. The top-K problem: keep a min-heap of size K → O(N log K).
6. Used in scheduling, Dijkstra, timers, merge-K, OS task selection.

## Self-check (in your head)

1. Given the array `[10, 4, 7, 2, 1, 8]`, is it a min-heap? Why or why not?

2. Insert 0 into the min-heap `[1, 3, 6, 5, 9, 8, 7]`. Trace the sift-up. What's the final array?

3. Extract-min from `[1, 3, 6, 5, 9, 8, 7]`. What value comes out, and what's the array after?

4. In a min-heap of size 1023, what's the maximum number of swaps for one insert?

5. To find the **5 smallest** out of 1 billion numbers, do you use a min-heap or a max-heap of size 5?

---

**Answers:**

1. Check parent–child:
   - index 0 (value 10) — left = index 1 (value 4). 10 ≤ 4? **No.** Already broken.
   
   **Not a min-heap.**

2. Append 0 at index 7:
   ```
   [1, 3, 6, 5, 9, 8, 7, 0]
   ```
   Sift up from 7: parent (7-1)/2 = 3, value 5. 0 < 5 → swap.
   ```
   [1, 3, 6, 0, 9, 8, 7, 5]
   ```
   Now at index 3. Parent (3-1)/2 = 1, value 3. 0 < 3 → swap.
   ```
   [1, 0, 6, 3, 9, 8, 7, 5]
   ```
   Now at index 1. Parent 0, value 1. 0 < 1 → swap.
   ```
   [0, 1, 6, 3, 9, 8, 7, 5]
   ```
   Now at index 0 — root. Stop.
   
   **Final: `[0, 1, 6, 3, 9, 8, 7, 5]`.**

3. Returns **1** (root). Move last (7) to root: `[7, 3, 6, 5, 9, 8]` (size 6 now). Sift down from 0: children 3, 6. Smaller is 3 (left). 7 > 3 → swap → `[3, 7, 6, 5, 9, 8]`. Now at 1. Children: 5, 9. Smaller is 5. 7 > 5 → swap → `[3, 5, 6, 7, 9, 8]`. Now at 3. Children: 8, 9 — but only 8 (index 7) exists. 7 < 8 → no swap.
   
   **Final: `[3, 5, 6, 7, 9, 8]`.**

4. log₂(1023) ≈ **10 swaps.**

5. **Max-heap of size 5.** You're tracking "the 5 smallest seen so far"; the **largest** of those is the candidate for eviction (when a smaller one arrives). Max-heap's root is that largest. Symmetric to top-K largest with min-heap.

If 4/5 you understand heaps. 5/5, move on.

Next → [DSA-14 — Sorting algorithms](DSA-14-sorting.md). (Skipping DSA-11/12/13 for the read-to-learn rewrite; originals still readable.)
