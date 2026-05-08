# DSA 2 — Arrays (read-to-learn version)

> RAM is one giant array. Once you understand arrays, you understand half of computer memory.

## The big idea

An **array** is a row of boxes lined up next to each other in memory. Each box:

- holds **one value**,
- is **the same size** as the others,
- has a **number** (its **index**, starting at 0).

Picture (an array of 5 ints):

```
index:    0      1      2      3      4
        +-----+-----+-----+-----+-----+
value:  | 10  | 20  | 30  | 40  | 50  |
        +-----+-----+-----+-----+-----+
addr:   1000  1004  1008  1012  1016
```

Each int is 4 bytes (`sizeof(int) == 4` on most systems). The array starts at address `1000`. So `arr[0]` is at 1000, `arr[1]` is at 1004, `arr[2]` at 1008, and so on.

**This is why arrays are fast:** to find the address of `arr[i]`, the computer just calculates `start + i × sizeof(item)`. One multiplication, one addition. Done. **O(1).**

## How operations behave (memorize)

| Operation | Time | Why |
|---|---|---|
| `arr[i]` | **O(1)** | direct address arithmetic |
| `arr[i] = x` | **O(1)** | write to that address |
| Search for `x` (unsorted) | **O(n)** | scan everything |
| Search for `x` (sorted, binary search) | **O(log n)** | halve range |
| Insert at end (room available) | **O(1)** | just write |
| Insert at front | **O(n)** | shift everyone right |
| Delete from end | **O(1)** | decrement size |
| Delete from front | **O(n)** | shift everyone left |

**Why insert/delete at front is O(n):** to insert at index 0, you must move every existing element one slot right. For 1 million elements, that's 1 million writes.

Picture: we want to insert `5` at index 0:

```
Before:
        +-----+-----+-----+-----+-----+
        | 10  | 20  | 30  | 40  | 50  |     n=5, no room
        +-----+-----+-----+-----+-----+

Step 1: shift right (start from end!)
        +-----+-----+-----+-----+-----+-----+
        | 10  | 20  | 30  | 40  | 50  |     |
        +-----+-----+-----+-----+-----+-----+
                                       ↑
                                       a[5] = a[4] = 50

        +-----+-----+-----+-----+-----+-----+
        | 10  | 20  | 30  | 40  |     | 50  |
        +-----+-----+-----+-----+-----+-----+
                                 ↑
                                 a[4] = a[3] = 40

        ... continues until a[1] = a[0] = 10 ...

Step 2: write
        +-----+-----+-----+-----+-----+-----+
        |  5  | 10  | 20  | 30  | 40  | 50  |     n=6
        +-----+-----+-----+-----+-----+-----+
```

That's 5 shifts + 1 write = 6 ops for n=5. For n=1,000,000 it'd be 1,000,001 ops. **O(n).**

## Static vs dynamic vs growable

### Static array
```c
int arr[5];
```
Size is **fixed** at compile time. Lives on the stack (auto). Free when the function returns.

### Dynamic array (`malloc`)
```c
int *arr = malloc(n * sizeof(int));
// ... use ...
free(arr);
```
Size known at **runtime**. Lives on the heap. **You must free it.**

### Growable array (vector / list)
What if you want it to grow as you add elements?

The trick: keep two numbers — `size` (how many elements you have) and `cap` (how many slots you've allocated). When `size == cap`, **double** `cap` and copy.

```c
typedef struct {
    int    *data;   // pointer to the array
    size_t  size;   // current count
    size_t  cap;    // capacity (allocated slots)
} vec_t;
```

The growth strategy:

```
Start:     cap=4, size=0
Push:      cap=4, size=1   [a]
Push:      cap=4, size=2   [a, b]
Push:      cap=4, size=3   [a, b, c]
Push:      cap=4, size=4   [a, b, c, d]   ← FULL!
Push e:    DOUBLE cap. realloc to cap=8.   ← copy 4 elements
           cap=8, size=5   [a, b, c, d, e]
Push:      cap=8, size=6
Push:      cap=8, size=7
Push:      cap=8, size=8   ← FULL!
Push:      DOUBLE cap=16, size=9            ← copy 8 elements
```

**Why doubling?** It's the magic ingredient that makes `push` **O(1) on average** (amortized).

Why amortized? Most pushes are O(1) (just write into an empty slot). Once in a while you copy n items (O(n)) — but that only happens every `n` pushes. Spread the cost: each push, on average, pays only O(1).

Sum the cost of pushing n items: 1 + 2 + 4 + 8 + ... + n ≈ 2n. **Linear total → O(1) per push, amortized.**

### The full vector code (read line by line)

```c
void vec_init(vec_t *v) {
    v->data = NULL;
    v->size = 0;
    v->cap  = 0;
}
```
Just zero everything out. We'll allocate later.

```c
int vec_push(vec_t *v, int x) {
    if (v->size == v->cap) {                 // (1) full?
        size_t newcap = v->cap == 0 ? 4      // first push: start at 4
                                    : v->cap * 2;
        int *nd = realloc(v->data, newcap * sizeof(int));
        if (!nd) return -1;                  // realloc failed
        v->data = nd;
        v->cap  = newcap;
    }
    v->data[v->size++] = x;                  // (2) write & bump size
    return 0;
}
```

Line (1): if size == cap, we're full → grow.
Line (2): `v->data[v->size++] = x` does two things: store `x` at slot `size`, then increment size. Equivalent to `v->data[v->size] = x; v->size = v->size + 1;`.

### Trace: pushing 5 elements

Starting: `vec_init` gives `data=NULL, size=0, cap=0`.

| Push | size before | cap before | full? | new cap | data after | size after |
|---|---|---|---|---|---|---|
| 10 | 0 | 0 | yes (0==0) | **4** | [10, _, _, _] | 1 |
| 20 | 1 | 4 | no | 4 | [10, 20, _, _] | 2 |
| 30 | 2 | 4 | no | 4 | [10, 20, 30, _] | 3 |
| 40 | 3 | 4 | no | 4 | [10, 20, 30, 40] | 4 |
| 50 | 4 | 4 | yes | **8** | [10, 20, 30, 40, 50, _, _, _] | 5 |

Pushes 1, 5 trigger reallocations (and copies). The other three are pure writes. Average over 5 pushes: still O(1).

## A multi-dimensional array (matrix)

A 2D array stored in memory:

```c
int m[3][4];
```

Memory layout — **row by row** (row-major):

```
                       column index
                  0    1    2    3
                +----+----+----+----+
   row 0        |0,0 |0,1 |0,2 |0,3 |    addresses: 1000, 1004, 1008, 1012
                +----+----+----+----+
   row 1        |1,0 |1,1 |1,2 |1,3 |    addresses: 1016, 1020, 1024, 1028
                +----+----+----+----+
   row 2        |2,0 |2,1 |2,2 |2,3 |    addresses: 1032, 1036, 1040, 1044
                +----+----+----+----+
```

`m[i][j]` is at `base + (i × cols + j) × sizeof(int)`. For `m[1][2]`: `1000 + (1×4 + 2) × 4 = 1000 + 24 = 1024`. ✓

### Why row-by-row vs column-by-column matters

The CPU loads memory in **chunks** called **cache lines** (typically 64 bytes — that's 16 ints). When you access `m[0][0]`, the CPU also pulls in `m[0][1]`, `m[0][2]`, ..., `m[0][15]` for free. They live in fast cache.

```c
// FAST — sequential reads (cache-friendly)
for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
        m[i][j] = ...;
```

Memory access pattern: 1000, 1004, 1008, 1012, 1016, 1020, ...

```c
// SLOW — column-first (cache-thrashing on big matrices)
for (int j = 0; j < 4; j++)
    for (int i = 0; i < 3; i++)
        m[i][j] = ...;
```

Memory access pattern: 1000, 1016, 1032, 1004, 1020, ... — jumping by row size each time.

For a 1000×1000 matrix of ints, column-first can be **10× slower** because cache lines are wasted. **Always loop in row-major order in C.**

## Worked example: reverse an array in place

Given `[10, 20, 30, 40, 50]`, we want `[50, 40, 30, 20, 10]`.

The two-pointer trick: one pointer starts at the left, one at the right. They swap and walk toward each other.

```c
void reverse(int *a, int n) {
    for (int i = 0, j = n - 1; i < j; i++, j--) {
        int t = a[i];   // save left
        a[i] = a[j];    // left = right
        a[j] = t;       // right = saved
    }
}
```

### Trace on `[10, 20, 30, 40, 50]` (n=5)

| Step | i | j | i<j? | swap a[i]↔a[j] | array after |
|---|---|---|---|---|---|
| Before | 0 | 4 | yes | 10 ↔ 50 | [50, 20, 30, 40, 10] |
| Iter 2 | 1 | 3 | yes | 20 ↔ 40 | [50, 40, 30, 20, 10] |
| Iter 3 | 2 | 2 | **no** (2 < 2 false) | stop |   |

Done. Array is reversed. **n/2 swaps total → O(n) time, O(1) extra space.**

### Why this works (the "invariant")

After each iteration, the **outermost** unprocessed pair has been swapped. Inner part is still raw. We stop when the two pointers meet (or cross). At that point, every element has been swapped to its mirror position.

Picture as we go:

```
After 0 swaps:  [10][20][30][40][50]    pointers at 0 and 4
                 ↑              ↑

After 1 swap:   [50][20][30][40][10]    pointers at 1 and 3
                     ↑      ↑

After 2 swaps:  [50][40][30][20][10]    pointers at 2 and 2 — STOP
                         ↑
```

`30` (the middle) doesn't move. That's fine — it's already at its mirror position.

## Worked example: maximum subarray (Kadane's algorithm)

Find the contiguous subarray with the largest sum. **Classic interview question.**

Input: `[-2, 1, -3, 4, -1, 2, 1, -5, 4]`. Answer: `6` (from subarray `[4, -1, 2, 1]`).

The trick: at each index, ask: *"is it better to extend the previous subarray, or start fresh here?"*

```c
int max_subarray(int *a, int n) {
    int best = a[0];     // best so far overall
    int cur  = a[0];     // best ending at current index
    for (int i = 1; i < n; i++) {
        cur = (a[i] > cur + a[i]) ? a[i] : cur + a[i];
        if (cur > best) best = cur;
    }
    return best;
}
```

### Trace on `[-2, 1, -3, 4, -1, 2, 1, -5, 4]`

Start: `best = -2, cur = -2`.

| i | a[i] | cur + a[i] | new cur (max of two) | best |
|---|---|---|---|---|
| 1 | 1 | -2+1 = -1 | max(1, -1) = **1** | max(-2, 1) = **1** |
| 2 | -3 | 1+(-3) = -2 | max(-3, -2) = **-2** | max(1, -2) = 1 |
| 3 | 4 | -2+4 = 2 | max(4, 2) = **4** | max(1, 4) = **4** |
| 4 | -1 | 4+(-1) = 3 | max(-1, 3) = **3** | max(4, 3) = 4 |
| 5 | 2 | 3+2 = 5 | max(2, 5) = **5** | max(4, 5) = **5** |
| 6 | 1 | 5+1 = 6 | max(1, 6) = **6** | max(5, 6) = **6** |
| 7 | -5 | 6+(-5) = 1 | max(-5, 1) = **1** | max(6, 1) = 6 |
| 8 | 4 | 1+4 = 5 | max(4, 5) = **5** | max(6, 5) = 6 |

**Final: best = 6.** ✓

This is **O(n) time, O(1) space.** A naive O(n²) brute force tries every (i, j) pair. Kadane's elegance is doing it in one pass.

## Common bugs (drawings included)

### Bug 1: off-by-one

```c
for (int i = 0; i <= n; i++)        // ← should be i < n
    a[i] = ...;
```

When `i == n`, you write past the end:

```
        +-----+-----+-----+-----+-----+
        |  ?  |  ?  |  ?  |  ?  |  ?  |  ← legitimate slots 0..n-1
        +-----+-----+-----+-----+-----+
                                       ↑
                                       a[n] — OUT OF BOUNDS!
```

C **does not check bounds.** You'll silently corrupt nearby memory. Maybe a crash later. Maybe a security hole.

### Bug 2: dangling pointer

```c
int *bad(void) {
    int local[10];
    return local;     // local dies when function returns!
}
```

`local` is on the stack of `bad`. After return, the stack frame is gone. Returned pointer points to dead memory. Touch it → undefined behavior.

Fix: allocate on the heap (`malloc`) or pass a buffer in.

### Bug 3: forgetting to free

```c
void leak(void) {
    int *a = malloc(1000);
    // ... use a ...
    // forgot free(a)
}
```

Every call leaks 1000 bytes. After enough calls, `malloc` fails (out of memory) or the system kills your process.

### Bug 4: `sizeof(arr)` after passing to a function

```c
void foo(int arr[]) {
    printf("%zu\n", sizeof(arr));    // prints 8 (a pointer), not the array size!
}

int main(void) {
    int x[100];
    foo(x);
}
```

Inside `foo`, `arr` is **a pointer**, not an array. `sizeof(pointer) == 8` on 64-bit systems. **Always pass the size as a separate argument.**

## Recap

1. An array is a contiguous block of memory; element address is `base + i × sizeof(item)`.
2. Indexing is **O(1)**. Search is **O(n)** (or O(log n) if sorted + binary search).
3. Insert/delete at front is **O(n)** (shifts everyone).
4. A growable array doubles capacity → **O(1) amortized** push.
5. 2D arrays are row-major in C; **iterate row-by-row** for cache speed.
6. Common bugs: off-by-one, dangling pointers, leaks, sizeof on parameter pointer.

## Self-check (no code-running needed)

1. An array of 1,000,000 ints. How long does `arr[500000]` take? *Constant or proportional to n?*

2. You insert at the **front** of a 1,000,000-element array a million times. How many total operations?

3. A growable array starts at cap=4 and doubles. After pushing 100 items, how many reallocations did you do? *(Hint: 4, 8, 16, 32, 64, 128 — count the steps.)*

4. In a 1000×1000 matrix, you sum all elements. Two loops. Which order is faster: row-then-col, or col-then-row?

5. Trace this short reverse on `[1, 2, 3]`:
   ```
   i=0, j=2 → swap a[0] ↔ a[2] → ?
   i=1, j=1 → loop ends → ?
   ```
   What is the final array?

---

**Answers:**

1. **Constant.** `arr[i]` is just one address calculation. Doesn't depend on the array size.

2. About **10¹²** (a trillion). Each insert at front is O(n), and you do it n times: n × n = n². For n=1,000,000 → 10¹² operations.

3. **5 reallocations.** cap goes 4 → 8 → 16 → 32 → 64 → 128. (At each double, you copy the old contents.)

4. **Row-then-col.** Cache-friendly. (See the matrix section above.)

5. After step 1: `[3, 2, 1]`. Step 2 doesn't run (i==j → loop condition false). **Final: [3, 2, 1].** ✓

If you got 4/5, you understand arrays. If you got 5/5, move on.

## Read deeper (optional)

- *The C Programming Language* (K&R), chapter on arrays.
- "What every programmer should know about memory" (Drepper), section on cache.

Next → [DSA-03 — Strings](DSA-03-strings.md).
