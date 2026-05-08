# DSA 10 — Heaps and priority queues

> A heap gives you the next-most-important thing in O(log n). Schedulers, Dijkstra, top-K — all priority queues.

## Heap properties

A **binary heap** is a complete binary tree where, in a **min-heap**, every parent is **≤ its children** (max-heap: ≥). Stored implicitly as an array — no pointers.

For a node at index `i`:
- left child = `2i + 1`
- right child = `2i + 2`
- parent = `(i − 1) / 2`

```
index:  0   1   2   3   4   5   6
value:  3   5   8   9   7   12  10
```

Tree:
```
             3
           /   \
          5     8
         / \   / \
        9   7 12 10
```

Invariant: every parent ≤ both children. (Note: heaps are **not** sorted; they only enforce parent-child ordering.)

## Operations

### Insert (sift up)
```c
void heap_push(int *h, int *n, int x) {
    int i = (*n)++;
    h[i] = x;
    while (i > 0 && h[(i-1)/2] > h[i]) {
        int t = h[i]; h[i] = h[(i-1)/2]; h[(i-1)/2] = t;
        i = (i-1)/2;
    }
}
```
O(log n).

### Pop min (sift down)
```c
int heap_pop(int *h, int *n) {
    int min = h[0];
    h[0] = h[--(*n)];
    int i = 0;
    while (1) {
        int l = 2*i+1, r = 2*i+2, smallest = i;
        if (l < *n && h[l] < h[smallest]) smallest = l;
        if (r < *n && h[r] < h[smallest]) smallest = r;
        if (smallest == i) break;
        int t = h[i]; h[i] = h[smallest]; h[smallest] = t;
        i = smallest;
    }
    return min;
}
```
O(log n).

### Peek
`h[0]` — O(1).

### Heapify (build from array)
Naive: insert each → O(n log n).
Better: start at last non-leaf, sift down each → **O(n)**. Surprisingly tight bound; comes from the fact most nodes are near leaves.

```c
void heapify(int *h, int n) {
    for (int i = n/2 - 1; i >= 0; i--) {
        // sift down at i
    }
}
```

## Real applications

### Top-K
"Find the K largest elements" — use a min-heap of size K. For each new element, if it's bigger than the heap's min, pop and push. End up with K largest. O(n log k).

```c
int kth_largest(int *a, int n, int k) {
    int h[k]; int hn = 0;
    for (int i = 0; i < n; i++) {
        if (hn < k) heap_push(h, &hn, a[i]);
        else if (a[i] > h[0]) { heap_pop(h, &hn); heap_push(h, &hn, a[i]); }
    }
    return h[0];
}
```

### Dijkstra's shortest path
Priority queue keyed on distance — see DSA-12.

### Heap sort
Build a heap (O(n)), repeatedly pop the max into the array tail. O(n log n) **in-place**, no extra memory. Worst case O(n log n) — better worst case than quicksort.

### Schedulers
A scheduler's "next runnable task" is the lowest-priority-number task. Heap is the obvious choice. (Linux's CFS actually uses a red-black tree keyed on virtual runtime — same effect.)

### Median maintenance
Two heaps: max-heap of lower half, min-heap of upper half. Median = top of bigger heap (or average of both tops). O(log n) per insert.

### Event-driven simulation
"Next event by timestamp." Priority queue of events.

## Decrease-key (the awkward op)

For Dijkstra, you sometimes want to **lower** a key already in the heap. Standard binary heap doesn't have an O(log n) decrease-key without:
- a hash map from value → index, or
- a more complex structure (Fibonacci heap, pairing heap).

In practice: insert the new lower-priority entry, and skip stale entries when popping. Often fast enough.

## Variants you should know about

- **Binomial heap** — supports merge in O(log n).
- **Fibonacci heap** — amortized O(1) decrease-key. Rarely used in practice (huge constants).
- **Pairing heap** — much simpler than Fibonacci, fast in practice. Used in some real systems.
- **Leftist heap, skew heap** — mergeable.

For interviews, **binary heap is enough**. For real systems, pick one with merge support if you need it.

## In the Linux kernel

- The CFS scheduler uses a **red-black tree** keyed on `vruntime`, **not** a heap. Why? Because "remove arbitrary entry" (when a task is canceled) needs O(log n), and RB-tree gives that uniformly.
- `kernel/time/timer.c` for software timers uses a **timer wheel** rather than a heap — bucketed, O(1) average insert.
- DRM scheduler uses priority queues per ring.
- For "delayed work" via `delayed_workqueue`, internally there are timers.

So the lesson: heaps are great, but the kernel often picks alternatives that handle deletion or insertion at constant cost in their domain.

## Common bugs

1. **Wrong child index formula** (`2i+1` vs `2i`). Test with examples on paper.
2. **Forgetting to decrement size before sifting** during pop.
3. **Mixing min and max heap** — pick one and stick with it. To turn a min-heap into a max-heap, negate.
4. **Heapify in wrong order** — must go bottom-up.

## Try it

1. Implement min-heap with `push`, `pop`, `peek`, `heapify`, `size`.
2. Heap-sort an array. Verify in-place.
3. Top-K problem: given a stream of numbers, maintain the K largest. Test with K=10, n=1M.
4. Median maintenance with two heaps. After each push, print the running median.
5. Implement a simple event-driven simulator: events have `(timestamp, callback)`, the simulator pops the earliest and "fires" it. Add a few events that schedule new events and verify ordering.
6. Read `kernel/sched/fair.c` — find where the runqueue is searched for the next task. (Hint: `pick_next_task_fair`.)

## Read deeper

- CLRS chapter 6 (Heapsort) and chapter 19 (Fibonacci heaps).
- "A Practical Comparison of N-ary Heaps" — fanouts other than 2 sometimes win for cache.
- For the curious: Lock-free priority queues — Hunt & Sundell, et al.

Next → [Graphs](DSA-11-graphs.md).
