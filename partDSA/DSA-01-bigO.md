# DSA 1 — Big-O and complexity (read-to-learn version)

> If you only learn one thing from this part: **how the time of an algorithm grows when the input grows**. That's Big-O.

## The big idea

Imagine you have a list of names. You want to find one name in it.

- If the list has **10 names**, it takes you maybe **5 seconds** of looking.
- If the list has **100 names**, it takes you maybe **50 seconds**.
- If the list has **1000 names**, it takes you about **500 seconds**.

Notice: **10× more names → 10× more time.** That relationship is what we call **linear**, written **O(n)**.

Now suppose the list is **sorted alphabetically** and you do "binary search" — look at the middle, then half, then half again. Then:

- 10 names → about **4 looks**.
- 100 names → about **7 looks**.
- 1000 names → about **10 looks**.
- 1,000,000 names → about **20 looks**.

The time grew **very slowly** even though the list got 100,000× larger. That's **logarithmic**, written **O(log n)**.

**That's all Big-O is.** A label saying *"how does time grow as input gets bigger?"*

## The 7 labels you must know

Below, `n` = "size of the input." Read each row as **"as n doubles, time grows by ___."**

| Label | Name | When n doubles, time grows by | Example |
|---|---|---|---|
| **O(1)** | constant | nothing | look up a value in a hash table |
| **O(log n)** | logarithmic | a tiny bit (one step) | binary search in sorted list |
| **O(n)** | linear | doubles | scan a list to find max |
| **O(n log n)** | "n log n" | a bit more than doubles | merge sort, quicksort |
| **O(n²)** | quadratic | quadruples | bubble sort, naive duplicate check |
| **O(2ⁿ)** | exponential | **doubles per +1 to n** | brute-force subsets |
| **O(n!)** | factorial | astronomical | brute-force permutations |

Memorize this table. It's the alphabet of complexity.

## How big is each, really? (numbers!)

Here's `n = 100`. How many "operations" does each curve take?

```
O(1)        →                      1 op
O(log₂ n)   →                      ~7 ops
O(n)        →                    100 ops
O(n log n)  →                    700 ops
O(n²)       →                 10,000 ops
O(2ⁿ)       →     1,000,000,000,...,000  (30 zeros — universe age)
O(n!)       →     too big to write
```

Now `n = 1000`:

```
O(1)        →                      1 op
O(log₂ n)   →                     ~10 ops
O(n)        →                  1,000 ops
O(n log n)  →                 10,000 ops
O(n²)       →              1,000,000 ops
O(2ⁿ)       →     so huge you can't compute it
```

A modern computer does about **1,000,000,000 operations per second**.

So:
- **O(n²) on n=10,000**: 100 million ops → about **0.1 seconds**. Fine.
- **O(n²) on n=100,000**: 10 billion ops → about **10 seconds**. Slow.
- **O(n²) on n=1,000,000**: a trillion ops → **17 minutes**. Too slow.

That's why an algorithm that "works on small input" can **die** on bigger input.

## Reading code to find Big-O

Here's a recipe. For each piece of code:

1. **One loop over n items** = `O(n)`.
2. **Loop over n inside another loop over n** = `O(n²)`.
3. **Loop where index halves each step** = `O(log n)`.
4. **No loops at all** = `O(1)`.
5. **Function calls** = whatever the called function's complexity is.

### Example 1 — O(1)

```c
int first_element(int *arr) {
    return arr[0];
}
```

**One step:** read element 0 and return. Time does not depend on `n`. **O(1).**

### Example 2 — O(n)

```c
int sum(int *arr, int n) {
    int s = 0;
    for (int i = 0; i < n; i++)
        s += arr[i];
    return s;
}
```

**The loop runs `n` times.** Each iteration does a constant amount of work (1 add). Total: `n` adds → **O(n).**

If `n = 1000`, the body runs 1000 times.
If `n = 1,000,000`, the body runs a million times.

### Example 3 — O(n²)

```c
int has_duplicate(int *arr, int n) {
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (arr[i] == arr[j])
                return 1;
    return 0;
}
```

**Two nested loops.** The outer runs `n` times. The inner runs (on average) `n/2` times. Total: about `n × n/2` = `n²/2` operations.

We **drop the constant** (the `/2`) for Big-O notation. Result: **O(n²).**

If `n = 1000`, the body runs about **500,000** times.
If `n = 10,000`, the body runs about **50,000,000** times. **100× slower** for 10× input.

### Example 4 — O(log n)

```c
int binary_search(int *a, int n, int x) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (a[mid] == x) return mid;
        if (a[mid] < x) lo = mid + 1;
        else            hi = mid - 1;
    }
    return -1;
}
```

The search range starts at size `n` and **halves** each step (we cut to either left half or right half).

How many times can you halve `n` before reaching 1?

```
n = 1000 → 500 → 250 → 125 → 62 → 31 → 15 → 7 → 3 → 1     (10 halvings)
n = 1024 → 512 → 256 → 128 → 64 → 32 → 16 → 8 → 4 → 2 → 1 (10 halvings)
```

That count is `log₂ n`. **O(log n).**

For n = 1 million, that's only **20 halvings**. For n = 1 billion, **30 halvings**. *Almost free.*

## A trace: how `binary_search` actually runs

Let's trace `binary_search([10, 20, 30, 40, 50, 60, 70], 7, 60)`.

Array (indices on top, values below):
```
index:    0    1    2    3    4    5    6
value:  [10] [20] [30] [40] [50] [60] [70]
```

We want to find `x = 60`. Initially: `lo = 0`, `hi = 6`.

**Step 1**

| | value |
|---|---|
| lo | 0 |
| hi | 6 |
| mid = (0+6)/2 | **3** |
| a[mid] = a[3] | 40 |
| 40 < 60? | yes → search right half |
| lo = mid+1 | **4** |

After step 1: `lo = 4`, `hi = 6`. Search range: indices 4..6, values [50, 60, 70].

**Step 2**

| | value |
|---|---|
| lo | 4 |
| hi | 6 |
| mid = (4+6)/2 | **5** |
| a[mid] = a[5] | 60 |
| 60 == 60? | **YES — return 5** |

Done. We found 60 at index 5. **Two steps** to search 7 elements.

For 7 elements `log₂ 7 ≈ 2.8`. The actual count was 2. Matches!

## Best, average, worst case

The **same algorithm** can be fast or slow depending on input.

Take "find x in unsorted array, scan from front":

```c
for (int i = 0; i < n; i++)
    if (a[i] == x) return i;
return -1;
```

- **Best case**: `x == a[0]` — found in 1 step → **O(1).**
- **Worst case**: `x` not in array — scanned all `n` → **O(n).**
- **Average case** (assuming uniform): `x` somewhere in the middle → about `n/2` steps → still **O(n).**

When we say "Big-O" without qualification, we usually mean **worst case**. That's the safe, conservative bound.

## What Big-O ignores (and why)

Big-O **drops constants and lower-order terms**.

Suppose your algorithm runs `5n + 10` steps. We say it's `O(n)`, **not** `O(5n + 10)`.

Why? Because:
- The constant `5` depends on the machine and compiler. It changes.
- The `+10` is a one-time setup cost. As `n` grows, it becomes negligible.
- We want a label that says **how it scales**, not how fast it is on a specific machine.

The 5 might matter when comparing two `O(n)` algorithms in practice — but for the **theoretical class**, we ignore it.

## Space complexity (memory, not time)

Same idea, but for **memory used** instead of time.

```c
int sum(int *a, int n) {
    int s = 0;
    for (int i = 0; i < n; i++) s += a[i];
    return s;
}
```

Memory: just `s` and `i`. **O(1)** extra space.

```c
int *copy(int *a, int n) {
    int *out = malloc(n * sizeof(int));   // n ints
    for (int i = 0; i < n; i++) out[i] = a[i];
    return out;
}
```

Memory: a new array of `n` ints. **O(n)** extra space.

```c
void print_pairs(int *a, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            printf("%d %d\n", a[i], a[j]);
}
```

This **takes O(n²) time** but only **O(1) extra space** (no new memory; just prints).

Time and space are independent. Always think about both.

## Common confusions (resolve them now)

### "Two nested loops always means O(n²)"

Not always! What if the inner loop runs only `log n` times?

```c
for (int i = 0; i < n; i++)
    for (int j = 1; j < n; j *= 2)    // doubles → log n times
        do_something();
```

**Outer:** n times. **Inner:** log n times. Total: **O(n log n).**

### "If I add an O(n) and an O(n²), what is it?"

Take the **bigger** term. `O(n + n²) = O(n²)`. The smaller term doesn't matter for big n.

### "What about two functions, each O(n)?"

If they run **one after the other**: `O(n + n) = O(2n) = O(n)`.
If they run **nested** (one inside the other): `O(n × n) = O(n²)`.

### "Recursion — how do I see the Big-O?"

Each recursive call is a step. If `f(n)` calls `f(n-1)` once, total calls = n → **O(n)**.
If `f(n)` calls `f(n-1)` twice, total calls = 2ⁿ → **O(2ⁿ)** (the famous bad Fibonacci).
If `f(n)` calls `f(n/2)` once, total calls = log n → **O(log n)** (binary search).
If `f(n)` calls `f(n/2)` twice, work doubles each level — total = O(n) (merge of merge sort).

We'll trace these in the recursion chapter.

## A picture summary

Here's how the curves look. Imagine the X-axis is `n`, Y-axis is "time."

```
   time
   ↑
   │                                      ●  O(n²)
   │                                  ●
   │                              ●
   │                          ●
   │                      ●               ●  O(n log n)
   │                  ●                ●
   │              ●               ●         ●  O(n)
   │          ●              ●         ●
   │      ●            ●         ●           ●  O(log n)
   │  ●           ●         ●        ●
   │  ●      ●         ●        ●
   └────────────────────────────────────────→ n
   1   2   4   8  16  32  64 128
```

O(n²) explodes upward. O(log n) is nearly flat. O(n) is a straight line. O(n log n) is slightly above linear.

## Common mistakes — what NOT to do

### Mistake 1: optimizing the wrong thing

If your code is O(n²) on n = 1,000,000, **no constant-factor tweak** (e.g., turn `*` into `<<`) will save you. You need a different **algorithm class** — go from O(n²) to O(n log n) or O(n).

### Mistake 2: forgetting the worst case

Quicksort is "O(n log n) average" — but **O(n²) worst** when given already-sorted input with bad pivot. Production code uses introsort (quicksort + fallback) precisely because of this.

### Mistake 3: thinking Big-O = fast

Big-O hides constants. Two O(n) algorithms can differ 100× in real speed. For small n, an O(n²) with tiny constants can beat an O(n log n) with big ones. **Don't worship Big-O — use it as a guide.**

### Mistake 4: counting wrong loops

```c
for (int i = 0; i < n; i++) {
    for (int j = 0; j < 10; j++)        // 10 is constant!
        do_x();
}
```

This is **O(n)**, not O(n²). The inner loop runs 10 times **regardless of n**, so it's a constant factor of the outer.

## Recap (read this every day for a week)

1. **Big-O** describes how an algorithm's work grows with input size.
2. The 7 classes you must know: **O(1), O(log n), O(n), O(n log n), O(n²), O(2ⁿ), O(n!).**
3. **Drop constants and lower terms.** Focus on growth.
4. To find Big-O: count loops over n. Nested = multiply. Sequential = take max.
5. **Worst case** is what matters.
6. **Both** time and space have Big-O.

## Self-check (no code-running needed — answer in your head)

1. You have a function with this code:
   ```c
   for (int i = 0; i < n; i++) printf("hi\n");
   for (int i = 0; i < n; i++) printf("bye\n");
   ```
   What's the Big-O? *(Answer below.)*

2. Same question for:
   ```c
   for (int i = 0; i < n; i++)
       for (int j = 0; j < n; j++)
           printf("%d %d\n", i, j);
   ```

3. If an algorithm is O(n) and processes 100 items in 0.01 seconds, how long for 10,000 items?

4. If an algorithm is O(n²) and processes 100 items in 0.01 seconds, how long for 10,000 items?

5. Which is faster for n = 1,000,000: an O(n²) algorithm or an O(n log n) algorithm?

---

**Answers:**

1. **O(n).** Two sequential O(n) loops = O(n+n) = O(2n) = O(n).
2. **O(n²).** Nested loops over n.
3. **About 1 second.** Linear: 100× input → 100× time.
4. **About 100 seconds.** Quadratic: 100× input → 10,000× time.
5. **O(n log n).** For n = 1,000,000: O(n²) = 10¹² ops, O(n log n) ≈ 2×10⁷ ops. About **50,000× faster**.

If you got 4/5, you understand Big-O. If you got 5/5, move on to the next chapter.

## Read deeper (if curious — not required)

- *Algorithms* by Sedgewick — has beautiful diagrams.
- "Big-O cheat sheet": https://bigocheatsheet.com/ — print and pin to your wall.

Next → [DSA-02 — Arrays: the foundation](DSA-02-arrays.md).
