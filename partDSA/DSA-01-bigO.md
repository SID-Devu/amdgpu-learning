# DSA 1 — Big-O and complexity analysis

Before any code, the language. **"How fast is this?"** is the question every engineer asks. Big-O is how we answer.

## The intuition

Imagine a function that processes `n` items.

- **`O(1)`** — takes the same time whether `n=1` or `n=1,000,000`. Example: looking up an item in a hash table.
- **`O(log n)`** — adding more items barely makes it slower. Example: binary search in a sorted array.
- **`O(n)`** — twice the items, twice the time. Example: scanning an array.
- **`O(n log n)`** — typical for "good" sorting (mergesort, quicksort).
- **`O(n^2)`** — twice the items, **four times** the time. Example: bubble sort.
- **`O(2^n)`** — adding one item **doubles** the time. Example: brute-force subset enumeration.
- **`O(n!)`** — combinatorial explosion. Example: trying all permutations.

A picture, with `n=100`:

| Complexity | Operations | "Feels like" |
|---|---|---|
| `O(1)` | 1 | instant |
| `O(log n)` | ~7 | instant |
| `O(n)` | 100 | fast |
| `O(n log n)` | ~700 | fast |
| `O(n^2)` | 10,000 | OK |
| `O(2^n)` | ~10^30 | heat death of universe |

If your code is `O(2^n)` and `n>40`, **it does not finish.** This is the most common reason "my code works on small input but hangs."

## Formal definition (just enough)

We say `f(n) = O(g(n))` if there exist constants `c, n0` such that for all `n ≥ n0`:

```
f(n) ≤ c · g(n)
```

Translation: "for big enough `n`, `f` is at worst a constant multiple of `g`."

Big-O hides:
- **constants** — `O(1000n)` and `O(n)` are the same.
- **lower-order terms** — `O(n^2 + 100n + 5)` is just `O(n^2)`.

That's the point: we want to compare *growth*, not absolute time.

## How to compute it (a recipe)

For a piece of code:

1. Find the loops. Each loop over `n` items contributes one factor of `n`.
2. Nested loops multiply.
3. Sequential blocks add — but the sum reduces to the largest term.
4. A function call counts as the function's own complexity.

### Examples

```c
// O(1)
int first(int *arr) { return arr[0]; }

// O(n)
int sum(int *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += arr[i];
    return s;
}

// O(n^2)
int has_duplicate(int *arr, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i+1; j < n; j++)
            if (arr[i] == arr[j]) return 1;
    return 0;
}

// O(n log n) — common pattern: outer loop is n, inner halves
void weird(int n) {
    for (int i = 0; i < n; i++)
        for (int j = 1; j < n; j *= 2)
            do_something();
}

// O(log n) — halving each step
int bsearch(int *a, int n, int x) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == x) return mid;
        if (a[mid] < x) lo = mid+1; else hi = mid-1;
    }
    return -1;
}
```

## Big-O, Big-Ω, Big-Θ

- **`O(g)`**: upper bound. "no worse than g."
- **`Ω(g)`**: lower bound. "no better than g."
- **`Θ(g)`**: tight. Both upper and lower.

In interviews and practice, **we mostly say "O" but mean "Θ"** — because we want the actual rate, not a loose upper bound. (Saying "bubble sort is `O(2^n)`" is technically true and entirely useless.)

## Best, average, worst case

For an algorithm:
- **Best case** — minimum time on the friendliest input.
- **Average case** — expected time on random input.
- **Worst case** — maximum time on the meanest input.

Examples:
- Quicksort: best `O(n log n)`, average `O(n log n)`, **worst `O(n^2)`** (already-sorted with bad pivot).
- Hash table lookup: average `O(1)`, **worst `O(n)`** (all collide).

When the worst case bites you, your latency p99 spikes, your customers churn, and your boss asks why. **Always know your worst case.**

## Amortized analysis

Some operations are usually fast but occasionally slow. The *average* cost over a long sequence can still be small.

Classic example: **dynamic array push**. We double the capacity when full.

- Most pushes: O(1).
- Every now and then: O(n) (the resize).
- Average over n pushes: still O(1) **amortized**.

This is the difference between "max single-op time" and "total time / total ops." In real-time / kernel context, the difference matters: you may not tolerate any single op being slow.

## Space complexity

Same Big-O notation, but for memory. Includes:

- the data structure itself,
- temporary buffers,
- recursion stack (each call frame is space).

A naive recursive Fibonacci is `O(2^n)` time, `O(n)` space (depth of recursion).

## Common mistakes (catch yourself)

1. **Counting `n` when there are two variables.** `for i in m: for j in n` is `O(m*n)`, not `O(n^2)`.
2. **Ignoring hidden costs.** A `printf` inside a hot loop is `O(n)` calls into libc — measurable.
3. **Thinking better Big-O is always faster.** A small `O(n^2)` algorithm beats a `O(n log n)` algorithm with huge constants for small `n`. Quicksort beats merge sort partly for this reason. Profile.
4. **Forgetting cache effects.** Two `O(n)` algorithms can differ 10× in real time if one is cache-friendly and one isn't. Big-O doesn't see cache.

## "When does it matter, when does it not"

| Situation | Big-O matters? |
|---|---|
| `n = 10`, never grows | Barely |
| `n = 1000`, grows | Yes |
| `n = millions`, on hot path | A lot |
| Real-time / latency-sensitive | Worst-case is everything |
| One-shot script | Often you don't care |

## Try it

1. What's the time complexity of:
   ```c
   for (int i = 1; i < n; i *= 2)
       for (int j = 0; j < i; j++)
           printf("hi\n");
   ```
   *(Hint: sum a geometric series.)*

2. Write an `O(n)` algorithm to find the missing number from `1..n+1` given an array of `n` distinct integers in that range. *(Hint: sum.)*

3. A function takes 1 second on `n=1000`. If it's `O(n^2)`, how long on `n=10000`? On `n=100000`?

4. Look up `kernel/sort.c` in the Linux source. What sort does it use? What's the complexity?

5. Read your own code from yesterday. Pick one function. Write its Big-O on a sticky note. (You'll do this for the rest of your career.)

## Read deeper

- *Introduction to Algorithms* (CLRS) — the bible. Heavy. Use as reference.
- *Algorithms* by Sedgewick — gentler intro, great pictures.
- The book *Algorithms Unlocked* — for absolute beginners.
- Online: https://bigocheatsheet.com/ — keep open while doing the rest of this Part.

Next → [Arrays — the foundation](DSA-02-arrays.md).
