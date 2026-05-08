# DSA 22 — Backtracking

> Try, recurse, undo. The systematic way to enumerate solutions of combinatorial problems.

## The pattern

```
backtrack(state):
    if state is a solution: record it
    for choice in candidates(state):
        apply(choice)
        backtrack(state)
        undo(choice)
```

Three ingredients:
- **state**: what describes "where we are."
- **choices**: legal options at this state.
- **prune**: skip branches that can't improve.

## N-Queens (the classic)

Place N queens on an N×N board so none attack.

```c
int N;
int col[N];   // col[r] = chosen column for row r

int safe(int r, int c) {
    for (int i = 0; i < r; i++)
        if (col[i] == c || abs(col[i] - c) == r - i) return 0;
    return 1;
}

void solve(int r) {
    if (r == N) { /* record solution */ return; }
    for (int c = 0; c < N; c++) {
        if (!safe(r, c)) continue;
        col[r] = c;
        solve(r + 1);
        // no explicit undo needed — col[r] is overwritten next iteration
    }
}
```

## Permutations

```c
void permute(int *a, int l, int n) {
    if (l == n) { /* record */ return; }
    for (int i = l; i < n; i++) {
        swap(&a[l], &a[i]);
        permute(a, l + 1, n);
        swap(&a[l], &a[i]);   // undo
    }
}
```

## Subsets (power set)

For each element, either include or exclude.

```c
void subsets(int *a, int n, int i, int *cur, int sz) {
    if (i == n) { /* record cur[0..sz-1] */ return; }
    subsets(a, n, i + 1, cur, sz);                // exclude
    cur[sz] = a[i];
    subsets(a, n, i + 1, cur, sz + 1);            // include
}
```

## Sudoku solver

```c
int board[9][9];

int valid(int r, int c, int v) {
    for (int i = 0; i < 9; i++)
        if (board[r][i] == v || board[i][c] == v) return 0;
    int br = (r / 3) * 3, bc = (c / 3) * 3;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[br+i][bc+j] == v) return 0;
    return 1;
}

int solve() {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (board[r][c] == 0) {
                for (int v = 1; v <= 9; v++)
                    if (valid(r, c, v)) {
                        board[r][c] = v;
                        if (solve()) return 1;
                        board[r][c] = 0;
                    }
                return 0;        // no value worked
            }
    return 1;                    // filled
}
```

## Word search (in a grid)

Given a grid of letters and a word, does it exist as a connected path?

DFS with mark-visit-unmark.

```c
int dfs(int r, int c, const char *w) {
    if (!*w) return 1;
    if (r<0||c<0||r>=R||c>=C) return 0;
    if (board[r][c] != *w) return 0;
    char saved = board[r][c]; board[r][c] = '#';
    int ok = dfs(r+1,c,w+1) || dfs(r-1,c,w+1) || dfs(r,c+1,w+1) || dfs(r,c-1,w+1);
    board[r][c] = saved;
    return ok;
}
```

The "mark + unmark" is the backtracking idiom.

## Pruning — the key to speed

Naive backtracking explores every branch — exponential. Pruning cuts branches early.

### Branch-and-bound
At each node, compute an **optimistic bound** on the best you could achieve down this branch. If it's worse than your current best, skip.

Example: knapsack with branch-and-bound.

### Constraint propagation
Each choice may force other deductions. (Dancing Links + Algorithm X for Sudoku is essentially this.)

## Common backtracking problems

- **N-Queens**.
- **Permutations / combinations**.
- **Subsets / power set**.
- **Sudoku**.
- **Word search**.
- **Generate parentheses** ("output all valid `(())()` strings of length 2n").
- **Combination sum**.
- **Letter combinations of a phone number**.
- **Restore IP addresses**.
- **Palindrome partitioning**.
- **Knight's tour**.

## Memoization for backtracking

Sometimes a "backtracking" problem has overlapping subproblems → it's actually DP. (Word break, palindrome partitioning min cuts, etc.)

If the same `(state)` is recomputed → memoize → DP.

## Iterative backtracking

You can use an explicit stack. Used when recursion is too deep for the call stack. Less common for interview problems.

## Common bugs

1. **Forgetting to undo** the change after recursion → state corruption.
2. **Mutating a shared "result" container** without copying when recording a solution.
3. **Not pruning** → time-out on bigger inputs.
4. **Wrong base case** — recording an incomplete solution.
5. **Swapping the recursion call order** when order matters (e.g., lexicographic output).

## Try it (must-do list)

1. N-Queens — count solutions for N=8 (should be 92).
2. Permutations of `[1,2,3]` — print all 6.
3. Subsets of `[1,2,3]` — print all 8.
4. Sudoku solver — solve a published puzzle.
5. Word search II (Leetcode 212) — combines trie and backtracking.
6. Combination sum (Leetcode 39) — without and with duplicates.
7. Generate parentheses — exactly `2*n` chars, balanced.

## Read deeper

- CLRS chapter 35 (Approximation Algorithms — many start as backtracking).
- *The Art of Computer Programming* vol. 4A (Knuth) — exhaustive coverage of dancing links + Algorithm X.
- "Backtracking algorithm" tutorials on competitive programming wikis.

Next → [Union-Find / DSU](DSA-23-union-find.md).
