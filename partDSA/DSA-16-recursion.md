# DSA 16 — Recursion deep

> If you can think recursively, half of DSA falls in your lap. Trees, DP, divide-and-conquer, backtracking — all recursion in a costume.

## What recursion is

A function that **calls itself** on a smaller subproblem. Two ingredients:

1. **Base case** — when to stop.
2. **Recursive case** — reduce the problem.

The classic:
```c
int factorial(int n) {
    if (n <= 1) return 1;            // base
    return n * factorial(n - 1);      // recurse
}
```

To trust recursion, **assume the recursive call works**. Then ask: does combining its result give the right answer for `n`?

This is the "**recursive leap of faith**" — trust the smaller call. If that smaller call is correct, your code is correct (by induction).

## How recursion uses the stack

Each call gets a **stack frame**: parameters, locals, return address. They stack up.

```
factorial(3) -> 3 * factorial(2)
                       2 * factorial(1)
                              1 (base)
```

Stack peaks at depth `n`. A C program typically has 8 MB stack; a deep recursion (millions deep) can blow the stack — `Segmentation fault`. Convert to iterative or use an explicit stack.

## Tail recursion — the only "free" recursion

Tail call: recursion is the **last thing** the function does, so there's nothing to remember after it returns.

```c
int fact_tail(int n, int acc) {
    if (n <= 1) return acc;
    return fact_tail(n - 1, acc * n);   // tail call
}
```

A smart compiler converts this to a loop (no stack growth). C does not guarantee tail-call optimization (though `-O2` often performs it). Don't rely on it; write the loop yourself if depth matters.

## Common recursion families

### 1. Linear recursion

`f(n)` calls itself once. Depth = n, total work proportional to recursion depth.

Examples: factorial, sum of array, "is this string a palindrome."

### 2. Binary recursion

`f(n)` calls itself twice.

Naive Fibonacci:
```c
int fib(int n) {
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}
```
This is **O(2^n)** and useless beyond n≈40. Memoization fixes it; see DSA-17.

### 3. Tree recursion

Each call branches into k subproblems. Total calls = exponential.

Examples: subset enumeration ("for each element, include or exclude").

### 4. Mutual recursion

Two functions call each other. `is_even(n)` calls `is_odd(n-1)` and vice versa.

### 5. Tail recursion

As above.

### 6. Divide & conquer

`f(n)` solves multiple subproblems of size n/k and combines. See DSA-19.

### 7. Backtracking

Try all possibilities; undo on failure. See DSA-22.

## Master theorem (computing recursion complexity)

For recurrences of the form `T(n) = a · T(n/b) + f(n)`:

- If `f(n) = O(n^(c))` and `c < log_b(a)` → `T(n) = Θ(n^(log_b a))`.
- If `f(n) = Θ(n^(log_b a))` → `T(n) = Θ(n^(log_b a) log n)`.
- If `f(n) = Ω(n^(c))` and `c > log_b(a)` (with some regularity) → `T(n) = Θ(f(n))`.

Useful examples:
- Merge sort: `T(n) = 2·T(n/2) + n` → **Θ(n log n)**.
- Binary search: `T(n) = T(n/2) + 1` → **Θ(log n)**.
- Strassen's matrix mult: `T(n) = 7·T(n/2) + n²` → **Θ(n^(log₂7)) ≈ Θ(n^2.81)**.

## When recursion is wrong

- **Deep call chain on bounded stack** — stack overflow.
- **Repeated subproblems** — exponential blowup. Solution: memoization (DP).
- **Each call costs more than the work it does** — converting to a tight loop wins.
- **Recursive in interrupt context** (kernel) — kernel stack is 8–16 KB. Stay shallow.

## Converting recursion to iteration

### Pattern: linear recursion → loop
Trivial. Replace with a `for` or `while`.

### Pattern: tree recursion → explicit stack
Push the "next subproblem" onto a stack; loop until empty.

```c
// DFS using explicit stack instead of recursion
void dfs_iter(node *root) {
    node *stk[N]; int top = 0;
    if (root) stk[top++] = root;
    while (top) {
        node *n = stk[--top];
        // visit n
        if (n->right) stk[top++] = n->right;
        if (n->left)  stk[top++] = n->left;   // pushed last → popped first → preorder
    }
}
```

Same algorithm; bounded stack you control.

## Worked examples

### Power function (efficient)
```c
double power(double x, int n) {
    if (n == 0) return 1;
    if (n < 0) return 1 / power(x, -n);
    if (n % 2 == 0) { double h = power(x, n/2); return h * h; }
    return x * power(x, n - 1);
}
```
O(log n) calls. The doubling trick is fundamental.

### GCD (Euclidean algorithm)
```c
int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
```
Tail recursive. O(log min(a,b)) — and the proof is beautiful (Lamé's theorem connects this to Fibonacci).

### Reverse a linked list (DSA-04, recursive form)
```c
node *rev(node *head) {
    if (!head || !head->next) return head;
    node *new_head = rev(head->next);
    head->next->next = head;
    head->next = NULL;
    return new_head;
}
```

### Tower of Hanoi
```c
void hanoi(int n, char from, char via, char to) {
    if (n == 0) return;
    hanoi(n - 1, from, to, via);
    printf("Move %d: %c -> %c\n", n, from, to);
    hanoi(n - 1, via, from, to);
}
```
O(2^n). The classic example.

## Common bugs

1. **Forgotten base case** → infinite recursion → stack overflow.
2. **Base case on wrong condition** → wrong answer.
3. **Modifying a global state and forgetting to restore** in backtracking.
4. **Off-by-one in subproblem size** (`n/2` vs `n/2 + 1` for odd n) — careful with integer division.
5. **Using recursion when iteration is clearly better** — premature elegance.

## Try it (do all of these)

1. Sum of array recursively. Then iteratively. Compare clarity.
2. Reverse a string recursively.
3. Print all subsets of a set (binary recursion). Trace by hand for `{1,2,3}`.
4. Implement integer power with `O(log n)` calls.
5. Compute GCD using recursion. Compare to iterative version.
6. Compute Fibonacci recursively (no memoization). Time `fib(40)`. Now memoize. Time `fib(40)` again. Notice the difference.

## Read deeper

- "How to Design Programs" (Felleisen et al.) — outstanding intro to recursive thinking.
- *Structure and Interpretation of Computer Programs* (SICP) chapters 1.1, 1.2.
- "Programming Praxis" exercises if you want to drill recursion.

Next → [Dynamic programming](DSA-17-dp.md).
