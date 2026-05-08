# DSA 2 — Arrays: the foundation

> RAM is a giant array. Every other data structure is built on this idea.

## What an array is, really

An array is a **contiguous block of memory** holding `n` elements of the same size. Given a base pointer `arr` and an index `i`:

```
address of arr[i]  =  arr + i * sizeof(element)
```

The CPU does this multiplication and addition in one instruction. That's why **`arr[i]` is `O(1)`**: there's no search — it's just arithmetic.

Pictorially, `int arr[5] = {10,20,30,40,50};` looks like:

```
addr:    1000   1004   1008   1012   1016
value:    10     20     30     40     50
index:     0      1      2      3      4
```

## Array operations and their costs

| Operation | Time | Why |
|---|---|---|
| Access `arr[i]` | O(1) | Direct address arithmetic |
| Update `arr[i] = x` | O(1) | One memory write |
| Search unsorted | O(n) | Must scan |
| Search sorted (binary) | O(log n) | Halve each step |
| Insert at end (if room) | O(1) | Just write |
| Insert at front | O(n) | Shift everything right |
| Delete from front | O(n) | Shift everything left |
| Delete from end | O(1) | Just decrement size |

## Static vs dynamic arrays

### Static
```c
int arr[100];  // size fixed at compile time
```
- On the stack (typically). Fast to allocate, freed automatically.
- Size must be known.

### Dynamic
```c
int *arr = malloc(n * sizeof(int));
// ... use ...
free(arr);
```
- Size known only at runtime.
- Lives on the heap.
- You're responsible for `free`.

### Resizable (the "vector" / `std::vector` / Python list)

Real programs need arrays that grow. The trick: **double the capacity when full.**

```c
typedef struct {
    int *data;
    size_t size;     // current number of elements
    size_t cap;      // allocated capacity
} vec_t;

void vec_init(vec_t *v) { v->data = NULL; v->size = 0; v->cap = 0; }

int vec_push(vec_t *v, int x) {
    if (v->size == v->cap) {
        size_t newcap = v->cap == 0 ? 4 : v->cap * 2;
        int *nd = realloc(v->data, newcap * sizeof(int));
        if (!nd) return -1;
        v->data = nd;
        v->cap = newcap;
    }
    v->data[v->size++] = x;
    return 0;
}

int vec_pop(vec_t *v, int *out) {
    if (v->size == 0) return -1;
    *out = v->data[--v->size];
    return 0;
}

void vec_free(vec_t *v) { free(v->data); v->data = NULL; v->size = v->cap = 0; }
```

Why double? Because doubling makes `vec_push` **O(1) amortized** — the cost of all resizes summed is geometric: `1 + 2 + 4 + ... + n ≈ 2n`. Linear capacity growth would be `O(n)` per push.

**Practical tip:** when you know the final size, `realloc` once up front. Saves time and prevents fragmentation.

## Multi-dimensional arrays

Two ways to represent a matrix.

### Row-major (C default)
```c
int m[3][4];          // 3 rows, 4 cols
m[i][j];              // row i, col j
// Address: base + (i*4 + j) * sizeof(int)
```
Memory layout: `[row0_col0, row0_col1, row0_col2, row0_col3, row1_col0, ...]`

This means **iterating row-by-row is cache-friendly**:

```c
for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
        m[i][j] = ...;   // sequential memory access — fast
```

Iterating column-by-column **thrashes the cache** for big matrices. Easy 5–10× slowdown. Remember this.

### Flat (do it yourself)
```c
int *m = malloc(rows * cols * sizeof(int));
m[i * cols + j];     // row i, col j
```
This gives you flexibility for non-rectangular shapes (jagged arrays).

## Common array patterns / algorithms

### 1. Reverse in place
```c
void reverse(int *a, int n) {
    for (int i = 0, j = n - 1; i < j; i++, j--) {
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}
```

### 2. Find max / min / sum
```c
int max_arr(int *a, int n) {
    int m = a[0];
    for (int i = 1; i < n; i++) if (a[i] > m) m = a[i];
    return m;
}
```

### 3. Two-sum (unsorted) — O(n) with hash table; O(n²) brute
We'll do this with hash tables in chapter 6.

### 4. Maximum subarray sum (Kadane's algorithm) — O(n)
```c
int max_subarray(int *a, int n) {
    int best = a[0], cur = a[0];
    for (int i = 1; i < n; i++) {
        cur = (a[i] > cur + a[i]) ? a[i] : cur + a[i];
        if (cur > best) best = cur;
    }
    return best;
}
```
This is one of the most-asked interview problems. Memorize it.

### 5. Move zeros to end (in place) — O(n)
```c
void move_zeros(int *a, int n) {
    int j = 0;
    for (int i = 0; i < n; i++) if (a[i] != 0) a[j++] = a[i];
    while (j < n) a[j++] = 0;
}
```
Pattern: **two pointers** (read with `i`, write with `j`). You'll see this everywhere.

### 6. Rotate array by k

```c
// reverse(a, 0, n-1); reverse(a, 0, k-1); reverse(a, k, n-1)
// O(n) time, O(1) space — clever
```

## Arrays in the Linux kernel

- `int reg[NUM_REGS]` — register banks of GPU IPs.
- `unsigned long bitmap[]` — bitmaps via `include/linux/bitmap.h`.
- `kvmalloc_array(n, size, flags)` — for "I want an array, allocate it however you can" (slab if small, vmalloc if big).
- `DECLARE_FLEX_ARRAY` — the modern way to embed a variable-length tail.

A common idiom:

```c
struct foo {
    int n_items;
    struct item items[];   // flexible array member
};

struct foo *f = kmalloc(sizeof(*f) + n * sizeof(struct item), GFP_KERNEL);
f->n_items = n;
```

Single allocation, contiguous, cache-friendly.

## Common bugs

1. **Off-by-one** — `for (i = 0; i <= n; i++)` is one too far. `< n`, not `<= n`.
2. **Out-of-bounds** — C does **no bounds checking**. `arr[1000]` on `int arr[10]` is undefined behavior. Run with valgrind or AddressSanitizer (`-fsanitize=address`) regularly.
3. **Forgetting `free`** — every `malloc` needs exactly one `free`. Use AddressSanitizer to catch leaks.
4. **Returning a stack array from a function** — UB. Pointer dangles. Allocate or pass in a buffer.
5. **`sizeof(arr)` after passing to a function** — inside the function `arr` is just a pointer; `sizeof` gives pointer size (8), not array size. Always pass length explicitly.

## Try it (do all of these in C, no copy-paste)

1. Implement `vec_t` from the chapter and add `vec_get`, `vec_set`, `vec_remove(idx)`. Test with 1M pushes; print the number of resizes.
2. Write a function `int *unique(int *a, int n, int *out_n)` that returns a new array with duplicates removed (preserve order). O(n²) is fine; O(n) needs hash table (later).
3. Implement Kadane's algorithm. Test with `{-2,1,-3,4,-1,2,1,-5,4}` → expect 6.
4. Rotate an array right by `k` using three reverses. Handle `k > n` correctly (use `k %= n`).
5. Bonus: implement an O(n) "majority element" finder (Boyer-Moore voting algorithm). Look it up if needed.

## Read deeper

- CLRS chapter 2 (insertion sort) — uses arrays as the canonical input.
- `include/linux/array_size.h`, `include/linux/overflow.h` — kernel array helpers; how to multiply `n*size` without overflow.

Next → [Strings (in C, properly)](DSA-03-strings.md).
