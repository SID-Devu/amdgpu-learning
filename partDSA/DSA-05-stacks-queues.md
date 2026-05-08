# DSA 5 — Stacks, queues, and ring buffers

> Function calls are a stack. The kernel's IH (interrupt-handler) ring is a queue. The kernel's `kfifo` is a ring buffer. These three structures are everywhere.

## Stack — LIFO

Last-in, first-out. Operations:

| Op | Time |
|---|---|
| `push(x)` | O(1) |
| `pop()` | O(1) |
| `peek()` | O(1) |
| `empty()` | O(1) |

### Array-backed stack
```c
typedef struct { int *data; size_t size, cap; } stack_t;

void s_push(stack_t *s, int x) {
    if (s->size == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->data = realloc(s->data, s->cap * sizeof(int));
    }
    s->data[s->size++] = x;
}

int s_pop(stack_t *s) { return s->data[--s->size]; }   // assumes non-empty
int s_peek(stack_t *s) { return s->data[s->size - 1]; }
int s_empty(stack_t *s) { return s->size == 0; }
```

Linked-list stack is also fine; array version is faster (cache).

### Real-world uses
- Function call frames (the **call stack**).
- Undo/redo stack in editors.
- Parser state (matching brackets, expression evaluation).
- DFS traversal.
- Backtracking.

### Classic problem: balanced brackets
Given `"({[]})"` → valid. `"({[}"`  → not.

```c
int balanced(const char *s) {
    char st[1024];
    int top = 0;
    while (*s) {
        char c = *s++;
        if (c == '(' || c == '[' || c == '{') st[top++] = c;
        else {
            if (top == 0) return 0;
            char o = st[--top];
            if (c == ')' && o != '(') return 0;
            if (c == ']' && o != '[') return 0;
            if (c == '}' && o != '{') return 0;
        }
    }
    return top == 0;
}
```

This is one of the most common interview warm-ups. Can you write it in 5 minutes?

### Postfix expression evaluator
Stack of operands; each operator pops two, pushes the result. Why? Because postfix never needs precedence rules. Calculators historically used this (HP RPN).

## Queue — FIFO

First-in, first-out.

| Op | Time |
|---|---|
| `enqueue(x)` (push back) | O(1) |
| `dequeue()` (pop front) | O(1) |
| `front()` | O(1) |

### Linked-list queue
```c
typedef struct qnode { int val; struct qnode *next; } qnode_t;
typedef struct { qnode_t *head, *tail; size_t size; } queue_t;

void q_enq(queue_t *q, int v) {
    qnode_t *n = malloc(sizeof(*n));
    n->val = v; n->next = NULL;
    if (q->tail) q->tail->next = n;
    else q->head = n;
    q->tail = n;
    q->size++;
}

int q_deq(queue_t *q, int *out) {
    if (!q->head) return -1;
    qnode_t *n = q->head;
    *out = n->val;
    q->head = n->next;
    if (!q->head) q->tail = NULL;
    free(n);
    q->size--;
    return 0;
}
```

### Array-backed circular queue (ring buffer)

Faster, no allocations after init:

```c
typedef struct {
    int *buf;
    size_t cap;
    size_t head, tail, size;
} ring_t;

int r_enq(ring_t *r, int v) {
    if (r->size == r->cap) return -1;
    r->buf[r->tail] = v;
    r->tail = (r->tail + 1) % r->cap;
    r->size++;
    return 0;
}

int r_deq(ring_t *r, int *out) {
    if (r->size == 0) return -1;
    *out = r->buf[r->head];
    r->head = (r->head + 1) % r->cap;
    r->size--;
    return 0;
}
```

If `cap` is a power of two, replace `% r->cap` with `& (r->cap - 1)` — one cycle cheaper. The kernel does this everywhere.

### Real-world uses
- Job scheduling (FIFO order).
- BFS traversal (next chapter).
- Producer/consumer pipelines.
- The **GPU command buffer ring** (e.g., `amdgpu_ring`) — packets in, GPU eats them in order.
- The **interrupt handler ring** in `amdgpu_irq.c` — hardware writes events; CPU drains.

## Deque — double-ended queue

Push and pop at both ends. O(1) all four. Implemented with a doubly linked list, or a circular array. C++ has `std::deque`; in C you typically roll your own with a linked list or a circular array indexed by both head and tail.

Used for: sliding-window maximum, work-stealing schedulers (push to front, steal from back).

## Priority queue (preview)

Like a queue, but `dequeue` always returns the highest- (or lowest-) priority element. Implemented with a heap (DSA-10).

Used for: Dijkstra's algorithm, scheduling tasks by deadline, top-K problems.

## Ring buffers in detail (the real superpower)

A **ring buffer** is a fixed-size queue that wraps around. Crucial because:

- **Bounded memory** — never grows.
- **Lock-free single-producer, single-consumer (SPSC)** is achievable using only atomic head/tail indices and proper memory ordering. No mutex; works at interrupt level. (Lock-free patterns covered later in Part IV.)
- **Cache-friendly** — sequential indices.

### Lock-free SPSC ring (sketch — full version in Part IV)
```c
// Producer (only):
size_t t = atomic_load_relaxed(&tail);
if ((t + 1 - atomic_load_acquire(&head)) % cap == 0) return FULL;
buf[t] = item;
atomic_store_release(&tail, (t + 1) % cap);

// Consumer (only):
size_t h = atomic_load_relaxed(&head);
if (h == atomic_load_acquire(&tail)) return EMPTY;
*out = buf[h];
atomic_store_release(&head, (h + 1) % cap);
```

Why "release" on stores and "acquire" on loads? See multithreading chapters in Part IV — but the gist: ensures the write to `buf[t]` is visible **before** `tail` is updated, so the consumer doesn't read a stale slot. We'll explain in detail.

## kfifo — the kernel's ring buffer

`include/linux/kfifo.h`. Macro-based, generic over element type. SPSC lock-free. Used in keyboard buffers, USB, audio, interrupt-to-bottom-half, and many GPU paths.

```c
DECLARE_KFIFO(myfifo, int, 1024);   // 1024-element fifo of ints
INIT_KFIFO(myfifo);

kfifo_put(&myfifo, 42);
int v;
kfifo_get(&myfifo, &v);
```

**Read this header**. It's a beautiful piece of code.

## Common bugs

1. **Off-by-one in ring buffer fullness check.** Two common conventions: keep one slot empty (so head==tail means empty, full means tail+1 == head), or use a separate `size` counter. Pick one and be consistent.
2. **Producer reads consumer's index without a memory barrier.** On x86 you may get away with it; on ARM you will not.
3. **Stack overflow from deep recursion** — your call stack is finite (typically 8 MB user, 16 KB kernel). Convert deep recursions to explicit stacks.
4. **Forgetting to free dequeued nodes** in a linked queue.
5. **Race between enq and deq in a multithreaded context** without protection.

## Try it

1. Implement a stack with array backing. Implement balanced-brackets using it.
2. Implement a postfix calculator. `"3 4 + 2 *"` → `14`. Use your stack.
3. Implement a circular queue with power-of-two capacity (use bitmask, not modulo).
4. Implement a SPSC ring buffer **in single-threaded mode first** (no atomics), test, then add atomics for two threads. Measure messages/sec.
5. Read `include/linux/kfifo.h`. Identify which functions are SPSC-safe and which require external locking.
6. Bonus (sliding-window max): given an array and `k`, return the max in every window of size k. O(n) using a deque.

## Read deeper

- *The Linux Programming Interface* (TLPI) — Chapter on pipes (which are kernel ring buffers).
- `kernel/printk/printk_safe.c` — uses a per-CPU ring to defer logs from atomic context.
- LWN: "An introduction to lockless ring buffers" (search lwn.net).

Next → [Hash tables](DSA-06-hash-tables.md).
