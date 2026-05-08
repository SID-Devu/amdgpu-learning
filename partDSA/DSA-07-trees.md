# DSA 7 — Binary trees & BSTs

> Trees are recursion made concrete. They organize data hierarchically and support fast ordered operations.

## Binary tree

Each node has at most **two children** — left and right.

```c
typedef struct tnode {
    int val;
    struct tnode *left, *right;
} tnode_t;
```

A general tree (any number of children) is usually represented as binary — "first-child / next-sibling." Or as `node + dynamic array of children`.

## Tree traversals

Three orderings on a binary tree (recursive definition):

```c
void inorder(tnode_t *r) {
    if (!r) return;
    inorder(r->left);
    printf("%d ", r->val);
    inorder(r->right);
}

void preorder(tnode_t *r) {
    if (!r) return;
    printf("%d ", r->val);
    preorder(r->left);
    preorder(r->right);
}

void postorder(tnode_t *r) {
    if (!r) return;
    postorder(r->left);
    postorder(r->right);
    printf("%d ", r->val);
}
```

- **Pre-order** — process root first. Used to copy a tree, serialize.
- **In-order** — left, root, right. On a BST, prints in sorted order.
- **Post-order** — children first. Used to free a tree (children freed before parent).

There's also **level-order** (BFS) — uses a queue, see DSA-11.

### Iterative in-order (using a stack)
Used when recursion depth is too deep:
```c
void inorder_it(tnode_t *r) {
    tnode_t *stack[1024]; int top = 0;
    tnode_t *cur = r;
    while (cur || top) {
        while (cur) { stack[top++] = cur; cur = cur->left; }
        cur = stack[--top];
        printf("%d ", cur->val);
        cur = cur->right;
    }
}
```

## Properties of trees

- **Height** of a node = longest path to a leaf.
- **Depth** of a node = path length from root.
- **Balanced** — heights of subtrees differ by at most 1 (or some constant). Balanced trees stay O(log n) deep.
- **Complete binary tree** — every level full except possibly the last (filled left to right). Heap layout.
- **Full binary tree** — every node has 0 or 2 children.

Number of nodes for a balanced tree of height h: 2^(h+1) − 1. So `n` nodes ⇒ height ≈ log₂(n). That's why operations on balanced trees are O(log n).

## Binary search tree (BST)

Invariant: for every node, all keys in the **left subtree are less**, all in the **right subtree are greater**.

```c
tnode_t *bst_insert(tnode_t *r, int v) {
    if (!r) {
        tnode_t *n = malloc(sizeof(*n));
        n->val = v; n->left = n->right = NULL;
        return n;
    }
    if (v < r->val) r->left = bst_insert(r->left, v);
    else if (v > r->val) r->right = bst_insert(r->right, v);
    return r;
}

int bst_find(tnode_t *r, int v) {
    while (r) {
        if (v == r->val) return 1;
        r = (v < r->val) ? r->left : r->right;
    }
    return 0;
}
```

### BST operation costs
| Operation | Average | Worst |
|---|---|---|
| Insert | O(log n) | O(n) |
| Find | O(log n) | O(n) |
| Delete | O(log n) | O(n) |

The worst case happens when you insert sorted data into a plain BST: it degenerates into a linked list of height n.

This is why we need **self-balancing trees** (next chapter): AVL, Red-Black. They guarantee O(log n) worst case.

## Delete from a BST (the trickiest part)

Three cases:
1. **No children** — just remove.
2. **One child** — replace node with that child.
3. **Two children** — replace with **in-order successor** (smallest in right subtree), then recursively remove that node.

```c
tnode_t *bst_min(tnode_t *r) { while (r->left) r = r->left; return r; }

tnode_t *bst_delete(tnode_t *r, int v) {
    if (!r) return NULL;
    if (v < r->val) r->left = bst_delete(r->left, v);
    else if (v > r->val) r->right = bst_delete(r->right, v);
    else {
        if (!r->left)  { tnode_t *t = r->right; free(r); return t; }
        if (!r->right) { tnode_t *t = r->left; free(r); return t; }
        tnode_t *succ = bst_min(r->right);
        r->val = succ->val;
        r->right = bst_delete(r->right, succ->val);
    }
    return r;
}
```

This is asked in interviews. Practice it.

## Tree problems you should be able to solve

- **Height of a tree** — recursive: `1 + max(h(left), h(right))`.
- **Is a BST valid?** — pass down min/max bounds.
- **Lowest common ancestor (LCA)** — for BST: the first node `v` whose value lies between `a` and `b`.
- **Mirror a tree** — swap left and right at every node.
- **Diameter** — longest path between any two nodes.
- **Serialize/deserialize a tree** — store via pre-order with NULL markers.
- **Path sum** — does any root-to-leaf path sum to k?
- **Convert sorted array to balanced BST** — recurse with mid as root.

For each of these, write the recursive solution. **Most tree problems are 5 lines of recursion.** This is why mastering recursion is mandatory.

## When BST loses to hash table

| | BST | Hash table |
|---|---|---|
| Lookup | O(log n) | O(1) average |
| Sorted iteration | O(n), in order | O(n + buckets), arbitrary order |
| Range queries | O(k + log n) | not supported |
| Worst-case latency | bounded (if balanced) | hash flood / resize spikes |
| Memory | 2 ptrs per node + value | bucket array + entries |

**Use a tree** when you need ordered iteration or range queries; **use a hash** for raw fast lookup.

## Common tree bugs

1. **Forgetting to free children before parent** in `free_tree` — lose pointer to children.
2. **Forgetting to update parent's pointer** when replacing during deletion.
3. **Stack overflow** on deep recursion — use iterative versions for very deep trees.
4. **Not handling duplicates** — your BST may break invariants if you allow duplicates without explicit policy.
5. **Modifying keys after insertion** — invariants now wrong.

## Try it

1. Implement a BST with `insert`, `find`, `delete`, `min`, `max`, `inorder_print`, and `free_tree`.
2. Validate-BST: write `int is_bst(tnode_t *r)` that returns 1 iff the tree is a valid BST.
3. Compute the diameter of a binary tree in a single recursion.
4. Convert a sorted array `[1,2,3,4,5,6,7]` to a height-balanced BST.
5. Lowest Common Ancestor in a generic binary tree (not BST) — recursion that returns `node` if found in subtree, NULL otherwise.
6. Serialize a binary tree to a string and deserialize back.

## Read deeper

- CLRS chapter 12 (Binary Search Trees) — solid theory.
- *Algorithms* by Sedgewick — outstanding diagrams.
- For the curious: **threaded BSTs** (no separate stack; uses null pointers for traversal).

Next → [Self-balancing trees: AVL, Red-Black](DSA-08-balanced-trees.md).
