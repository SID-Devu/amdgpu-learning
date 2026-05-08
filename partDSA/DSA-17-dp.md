# DSA 17 — Dynamic programming

> Recursion + memoization, formalized. The interviewer's favorite topic.

## When DP applies

A problem is amenable to DP when:

1. **Overlapping subproblems** — the same subproblem is solved many times by naive recursion.
2. **Optimal substructure** — the optimal answer uses optimal answers of subproblems.

Together: don't redo work. Save subproblem answers.

## Two approaches

### Top-down (memoization)

Just write the recursion, then cache results.

```c
int memo[100];   // -1 means uncomputed

int fib(int n) {
    if (n < 2) return n;
    if (memo[n] != -1) return memo[n];
    return memo[n] = fib(n-1) + fib(n-2);
}
```

Easy to write — write the natural recursion, add caching. **The standard answer in interviews.**

### Bottom-up (tabulation)

Iterative; build up from base cases.

```c
int fib(int n) {
    int dp[n+1];
    dp[0] = 0; dp[1] = 1;
    for (int i = 2; i <= n; i++) dp[i] = dp[i-1] + dp[i-2];
    return dp[n];
}
```

Often faster and uses less memory (you can sometimes throw away old rows). Common after writing the recursion to "see" the pattern.

## DP recipe

1. **Define the state**: what determines a subproblem? (e.g., "first i items, capacity j" for knapsack).
2. **Define the recurrence**: how to compute the answer from smaller states.
3. **Identify base cases**.
4. **Choose top-down or bottom-up**.
5. **Optimize space** (often you only need previous row).

## Classic problems

### Fibonacci
Above. O(n) time, O(1) space (only need last two).

### Climbing stairs
"You can take 1 or 2 steps. How many ways to reach step n?"

`ways[n] = ways[n-1] + ways[n-2]` — Fibonacci again.

### House robber
Array of values. Pick a subset with max sum, no two adjacent.

`dp[i] = max(dp[i-1], dp[i-2] + a[i])`.
O(n), O(1) space.

### Coin change (minimum coins)
Given coin denominations, find min coins to make amount V.

```
dp[0] = 0
dp[v] = min over coin c of (dp[v-c] + 1)
```
O(V × #coins).

### Coin change (number of ways)
```
ways[0] = 1
for each coin c:
    for v from c to V:
        ways[v] += ways[v-c]
```
Outer loop on coins to avoid double-counting orderings.

### 0/1 Knapsack
n items with weight `w[i]` and value `v[i]`. Capacity W. Max total value with each item used 0 or 1 times.

```
dp[i][j] = max(dp[i-1][j],                       // skip
               dp[i-1][j - w[i]] + v[i])          // take
```
O(nW). Can be reduced to O(W) space by iterating j backwards.

### Longest common subsequence (LCS)
Strings `a` and `b`. Length of longest common subsequence.

```
dp[i][j] = dp[i-1][j-1] + 1                if a[i-1] == b[j-1]
         = max(dp[i-1][j], dp[i][j-1])     otherwise
```
O(n*m).

### Edit distance (Levenshtein)
Min insert/delete/replace to convert `a` into `b`.

```
dp[i][j] = min(dp[i-1][j] + 1,                            // delete
               dp[i][j-1] + 1,                            // insert
               dp[i-1][j-1] + (a[i-1] != b[j-1] ? 1 : 0)) // replace
```
O(n*m). Used in spellcheck, diff tools, bioinformatics.

### Longest increasing subsequence (LIS)
Length of longest strictly increasing subsequence in an array.

DP: O(n²): `dp[i] = 1 + max dp[j] for j<i where a[j] < a[i]`.
With binary search: **O(n log n)** — classic interview optimization.

### Matrix chain multiplication
Given dimensions of n matrices, find the cheapest order.

`dp[i][j] = min over k of (dp[i][k] + dp[k+1][j] + p_{i-1} * p_k * p_j)`.
O(n³). Order matters: `(AB)C` may be cheaper than `A(BC)`.

### Word break
Given a string and a dictionary, can the string be split into dictionary words?

`dp[i]` = can prefix of length i be split? `dp[i] = OR over j (dp[j] AND s[j..i] in dict)`.
O(n²).

### Subset sum / partition
Subset sum: is there a subset summing to T? `dp[i][j]` = "can we get sum j using first i items?"
O(n × T).

## State space caveats

- DP runs in **state count × per-state work**.
- If your state space is exponential in n (e.g., subsets), DP is no faster than brute force. Bitmask DP (subsets in `2^n` states with bit-tricks) is good only when n ≤ 20.
- The art of DP is **finding the right state**.

## Bitmask DP (when n is small)

State = a subset, encoded as bits. E.g., Traveling Salesman Problem with `dp[mask][last]` = min cost path that visited exactly `mask` set of cities ending at `last`. O(2^n × n²). Works for n ≤ 20.

## DP on trees

Compute `f(node)` from `f(children)` via post-order traversal.

Example: max independent set on a tree. For each node:
- `dp[v][0]` = max set NOT including v = sum over children of max(dp[c][0], dp[c][1])
- `dp[v][1]` = max set including v = 1 + sum over children of dp[c][0]

## DP on intervals

State indexed by `(left, right)`. Matrix chain mult, palindrome partitioning, optimal BST.

## DP on digits ("digit DP")
For problems like "count integers in [L, R] with property P." State: position, tight bound flag, partial state.

Advanced; you'll see it in competitive programming.

## Memoization gotchas

- Bound your cache keys (e.g., 2D map for 2-arg functions).
- Beware **non-deterministic recursion** — if a function depends on state outside its args, memoization breaks.
- Recursion depth + memo can be an awkward fit; bottom-up may be cleaner.

## Common DP patterns to recognize

- "Min/max ways" → typically DP.
- "Count ways" → typically DP.
- "Longest / shortest something with property" → typically DP.
- "Yes/No achievable" → typically DP (boolean).
- "Choose with constraints, optimize sum" → knapsack-family.

## Common bugs

1. **Wrong order of loops** in bottom-up (e.g., 0/1 knapsack iteration in wrong direction → unbounded knapsack).
2. **Forgetting base case** in memoization → infinite loop (or wrong answer if the cache says `0`).
3. **Mixing 0-indexed and 1-indexed** state and array.
4. **Using `int` when sum can overflow** — switch to `long long`.
5. **Optimization too early** (writing 1D array before 2D works) — write the clear version first; optimize space after.

## Try it (top 10 must-do DPs)

1. Climbing stairs.
2. House robber.
3. Coin change (min coins) and coin change (number of ways).
4. 0/1 knapsack.
5. LCS — and LIS in O(n log n).
6. Edit distance.
7. Word break.
8. Maximum subarray (Kadane) — O(n) DP.
9. Partition equal subset sum.
10. Longest palindromic substring.

If you can solve all 10 from memory in C, you're DP-fluent.

## Read deeper

- CLRS chapter 15 (Dynamic Programming).
- "DP from scratch" tutorials on Codeforces and AtCoder.
- For real-world DP: query optimization in databases (Selinger's algorithm), Smith-Waterman in bioinformatics.

Next → [Greedy algorithms](DSA-18-greedy.md).
