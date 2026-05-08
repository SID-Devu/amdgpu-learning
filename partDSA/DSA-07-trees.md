# DSA 7 — Binary trees & BSTs (read-to-learn version)

> A tree is "a list, but it forks." Once it forks, you can search log(n) nodes instead of n.

## The big idea

A **binary tree** is a structure where each "node" can have up to **two** children: a left child and a right child.

```
            10           ← root (top of the tree)
           /  \
          5    15        ← children of 10
         / \     \
        3   8    20      ← children of children
                / \
               17  25    ← deeper still
```

Why is this useful? Because if the tree is **balanced** (left and right halves roughly equal), and you sort it cleverly, you can find any value in **log n** steps — same as binary search, but for a structure you can grow and shrink.

## Vocabulary (memorize)

```
            10
           /  \
          5    15
         / \     \
        3   8    20
                / \
               17  25
```

- **Root**: the top node (10).
- **Leaf**: a node with no children (3, 8, 17, 25).
- **Internal node**: a node with at least one child (5, 15, 20, 10).
- **Parent / child**: 5 is parent of 3; 3 is child of 5.
- **Sibling**: nodes with the same parent (3 and 8 are siblings).
- **Subtree**: a node + all its descendants. (Subtree rooted at 5 = {5, 3, 8}.)
- **Depth of a node**: number of edges from root to that node. (10 has depth 0; 5 has depth 1; 3 has depth 2.)
- **Height of a tree**: longest path from root to any leaf. (Above tree: height = 3.)

## A binary tree node in C

```c
typedef struct tnode {
    int val;
    struct tnode *left;
    struct tnode *right;
} tnode_t;
```

A node has a value and **two** pointers (left subtree, right subtree). Either pointer can be `NULL` (no child on that side).

## Binary Search Tree (BST) — the rule

A **BST** adds one rule: for **every** node,

- All values in the **left** subtree are **less** than the node's value.
- All values in the **right** subtree are **greater** than the node's value.

The example above is a BST. Verify:

- Root 10: left subtree {5, 3, 8} all < 10 ✓; right subtree {15, 20, 17, 25} all > 10 ✓.
- Node 5: left {3} < 5 ✓; right {8} > 5 ✓.
- Node 15: no left; right {20, 17, 25} all > 15 ✓.
- Node 20: left {17} < 20 ✓; right {25} > 20 ✓.

The rule applies **recursively** at every node.

## How search works in a BST

To find a value:
- Start at root.
- If the value matches → done.
- If smaller → go left.
- If bigger → go right.
- If you fall off the tree (NULL) → not found.

```c
tnode_t *bst_find(tnode_t *root, int v) {
    while (root) {
        if (v == root->val) return root;
        if (v < root->val)  root = root->left;
        else                 root = root->right;
    }
    return NULL;
}
```

Each step **halves** the work (in a balanced tree). Total: **O(log n).**

### Trace: find 17 in the example

```
            10
           /  \
          5    15
         / \     \
        3   8    20
                / \
               17  25
```

| Step | At node | val | comparison | go where |
|---|---|---|---|---|
| 1 | 10 | 10 | 17 > 10 | right |
| 2 | 15 | 15 | 17 > 15 | right |
| 3 | 20 | 20 | 17 < 20 | left |
| 4 | 17 | 17 | match! | **return 17** |

**4 steps** to find 17. The tree has 7 nodes; log₂(7) ≈ 2.8 — close.

### Trace: find 99 (not present)

| Step | At node | val | comparison | go where |
|---|---|---|---|---|
| 1 | 10 | 10 | 99 > 10 | right |
| 2 | 15 | 15 | 99 > 15 | right |
| 3 | 20 | 20 | 99 > 20 | right |
| 4 | NULL |   |   | **return NULL** |

Not found. Took 3 steps to fall off the tree.

## Insert into a BST

To insert: walk down like a search; when you hit a NULL, attach the new node there.

```c
tnode_t *new_node(int v) {
    tnode_t *n = malloc(sizeof(*n));
    n->val = v; n->left = n->right = NULL;
    return n;
}

tnode_t *bst_insert(tnode_t *root, int v) {
    if (!root) return new_node(v);
    if (v < root->val) root->left  = bst_insert(root->left,  v);
    else                root->right = bst_insert(root->right, v);
    return root;
}
```

This is **recursive**. The recursion goes: "insert into the left subtree" or "into the right subtree." It bottoms out when we hit NULL → make a new node.

### Trace: insert 12 into the example tree

```
Initial:
            10
           /  \
          5    15
         / \     \
        3   8    20
                / \
               17  25
```

| Step | At node | comparison | direction |
|---|---|---|---|
| 1 | 10 | 12 > 10 | right |
| 2 | 15 | 12 < 15 | left |
| 3 | NULL | (empty) | attach new node 12 here |

Result:

```
            10
           /  \
          5    15
         / \   / \
        3   8 12  20
                 / \
                17  25
```

12 became the **left child of 15**, which had no left child before.

## In-order traversal (visits values in sorted order!)

The magic of BST: walk it in a specific order, and you visit values **sorted ascending**.

The order is: **left, self, right.**

```c
void inorder(tnode_t *root) {
    if (!root) return;
    inorder(root->left);
    printf("%d ", root->val);
    inorder(root->right);
}
```

### Trace on the example tree

```
            10
           /  \
          5    15
         / \     \
        3   8    20
                / \
               17  25
```

We start at 10 and call `inorder(10)`:

```
inorder(10):
  inorder(5):                              ← go left from 10
    inorder(3):
      inorder(NULL)        ← left of 3
      print 3              ▶ output: 3
      inorder(NULL)        ← right of 3
    print 5                ▶ output: 3 5
    inorder(8):
      inorder(NULL)
      print 8              ▶ output: 3 5 8
      inorder(NULL)
  print 10                 ▶ output: 3 5 8 10
  inorder(15):                             ← go right from 10
    inorder(NULL)          ← left of 15
    print 15               ▶ output: 3 5 8 10 15
    inorder(20):
      inorder(17):
        inorder(NULL)
        print 17           ▶ output: 3 5 8 10 15 17
        inorder(NULL)
      print 20             ▶ output: 3 5 8 10 15 17 20
      inorder(25):
        inorder(NULL)
        print 25           ▶ output: 3 5 8 10 15 17 20 25
        inorder(NULL)
```

**Final output: 3 5 8 10 15 17 20 25** — sorted! That's the BST property at work.

## Three traversal orders (memorize)

| Name | Order | Use |
|---|---|---|
| **In-order** | left, self, right | sorted output of BST |
| **Pre-order** | self, left, right | clone a tree; serialize |
| **Post-order** | left, right, self | free a tree (children before parent) |

Picture each on the example tree:

```
in-order:    3 5 8 10 15 17 20 25     (sorted)
pre-order:   10 5 3 8 15 20 17 25
post-order:  3 8 5 17 25 20 15 10     (children of each node visited before it)
```

For pre-order, every "self" comes **before** its children. For post-order, every "self" comes **after** all its descendants.

## Why "balance" matters: the worst case

Suppose you insert sorted: 1, 2, 3, 4, 5.

| step | what you insert | tree |
|---|---|---|
| 1 | 1 | `[1]` |
| 2 | 2 | `[1] → right [2]` |
| 3 | 3 | every insert goes right |
| 4 | 4 | every insert goes right |
| 5 | 5 | every insert goes right |

Result: it's just a linked list pretending to be a tree.

```
   1
    \
     2
      \
       3
        \
         4
          \
           5
```

Look-ups now take **n** steps, not log n. The tree's "balance" is broken.

**Self-balancing BSTs** (AVL trees, Red-Black trees) automatically rotate after insert/delete to keep heights logarithmic. The Linux kernel uses **Red-Black trees** in `include/linux/rbtree.h` for many things — process tree, deadline scheduler, file extents, GPU virtual address space (`amdgpu_vm_bo_va`).

We cover red-black rotations in DSA-08. For now, just remember: **a regular BST can degrade to a list. Balanced BSTs prevent that.**

## Counting the height

```c
int height(tnode_t *root) {
    if (!root) return 0;
    int l = height(root->left);
    int r = height(root->right);
    return 1 + (l > r ? l : r);
}
```

The recursion: height of a tree = 1 + max(height of left subtree, height of right subtree). Empty tree has height 0.

### Trace: height of our example tree

We compute bottom-up.

```
height(3) = 1 + max(0, 0) = 1
height(8) = 1 + max(0, 0) = 1
height(5) = 1 + max(1, 1) = 2

height(17) = 1 + max(0, 0) = 1
height(25) = 1 + max(0, 0) = 1
height(20) = 1 + max(1, 1) = 2
height(15) = 1 + max(0, 2) = 3

height(10) = 1 + max(2, 3) = 4
```

Tree height = 4. (Counting nodes; some books count edges.)

## Delete from a BST (the tricky one)

Three cases:

1. **Node has no children** → just remove it.
2. **Node has one child** → replace it with that child.
3. **Node has two children** → find the **in-order successor** (smallest value in the right subtree), copy its value into this node, then delete the successor (which has at most 1 child).

### Picture case 3

Delete 15 from the example:

```
Before:
            10
           /  \
          5    15           ← delete this
         / \     \
        3   8    20
                / \
               17  25
```

Find smallest value in 15's right subtree → walk left from 20: hit 17 (17 has no left). **17 is the in-order successor.**

Copy 17 into 15's position. Then delete the original 17 (which had no children).

```
After:
            10
           /  \
          5    17           ← was 15, now holds 17
         / \     \
        3   8    20
                / \
               (gone) 25
```

(17 had no left child and we removed it as the lower copy. Now 20 has only a right child 25.)

## Why two pointers per node makes a tree more powerful than a list

Each step in a list: only ONE choice (next).
Each step in a BST: TWO choices (left or right). And each choice **prunes** half the remaining tree.

That's why log n vs n. Same memory, structured better.

## Common bugs

### Bug 1: forgetting to handle NULL

```c
int height(tnode_t *r) {
    int l = height(r->left);   // crashes if r is NULL
    ...
}
```

Always check `if (!root) return ...` at the top of any tree function.

### Bug 2: not maintaining the BST property

```c
void bad_insert(tnode_t *r, int v) {
    if (v < r->val) bad_insert(r->left, v);     // works
    else            bad_insert(r->right, v);    // works
    // BUT: forgot to actually create the node when r->left is NULL!
}
```

Always check for NULL and create the node there.

### Bug 3: stack overflow on deep trees

A recursive tree function uses the call stack — one frame per node depth. For an extremely deep (unbalanced) tree, this can crash with stack overflow.

Solution: balance the tree (DSA-08), or rewrite recursion as iteration with an explicit stack.

### Bug 4: assuming the tree stays sorted after a value change

```c
n->val = some_new_value;
```

If you mutate a node's value, the BST property might break (the value may belong elsewhere). **Always remove + reinsert** instead.

## Recap

1. A binary tree has up to two children per node.
2. A **BST** maintains: left < node < right (recursively).
3. Search and insert in a balanced BST: **O(log n).** Worst case (unbalanced): O(n).
4. **In-order** traversal yields **sorted** output.
5. Self-balancing BSTs (Red-Black, AVL) prevent worst-case degradation. Linux uses RB-trees heavily.
6. Three traversal orders: pre, in, post — pick by what you need.

## Self-check (in your head)

1. Insert 7, 3, 11, 1, 5, 9 into an empty BST in that order. Draw the tree.

2. In-order traversal of your tree above — what does it print?

3. In a perfectly balanced BST with 1023 nodes, how many steps to find any value?

4. What's the height of a tree with just one node?

5. Why does *deleting a node with two children* require finding the in-order successor — why can't you just remove it?

---

**Answers:**

1.
   ```
            7
           / \
          3   11
         / \  /
        1  5 9
   ```
   - Insert 7 → root.
   - Insert 3 → 3 < 7, left.
   - Insert 11 → 11 > 7, right.
   - Insert 1 → 1 < 7 left, 1 < 3 left.
   - Insert 5 → 5 < 7 left, 5 > 3 right.
   - Insert 9 → 9 > 7 right, 9 < 11 left.

2. **1 3 5 7 9 11.** (Sorted! That's BST + in-order.)

3. log₂(1023) ≈ **10 steps.**

4. **1** (the single node is itself the only level).

5. Because removing the node leaves a "hole" with two children. You can't just glue them together — one is < and one is >. The in-order successor (smallest in the right subtree) is the **next-larger value**; replacing the deleted node with it keeps the BST property intact.

If 4/5 you understand BSTs. 5/5, move on.

Next → [DSA-10 — Heaps & priority queues](DSA-10-heaps.md). (We're skipping DSA-08, 09 — balanced trees and B-trees — for the read-to-learn rewrite; the originals are still readable.)
