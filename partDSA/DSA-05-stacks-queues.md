# DSA 5 — Stacks and queues (read-to-learn version)

> Two of the simplest data structures, used everywhere from function calls to GPU command rings.

## The big idea

A **stack** is a pile of plates. You can only:
- put a plate on **top** (push),
- take a plate from the **top** (pop).

The plate at the bottom is the one you put down first. Last in, first out — **LIFO**.

A **queue** is a line at a coffee shop. Customers:
- join at the **back** (enqueue),
- leave from the **front** (dequeue).

The first customer to join is the first to be served. First in, first out — **FIFO**.

## Stack: pictures and operations

```
Stack:

        | 30 |   ← top (just pushed)
        | 20 |
        | 10 |   ← bottom (pushed first)
        +----+

operations:
  push(x):  put x on top
  pop():    remove and return the top
  peek():   look at the top without removing
```

### Trace: push 10, 20, 30, then pop, pop

| Step | Operation | Stack (top on right) |
|---|---|---|
| 0 | (empty) | `[ ]` |
| 1 | push(10) | `[ 10 ]` |
| 2 | push(20) | `[ 10, 20 ]` |
| 3 | push(30) | `[ 10, 20, 30 ]` |
| 4 | pop() → returns 30 | `[ 10, 20 ]` |
| 5 | pop() → returns 20 | `[ 10 ]` |

**Last in (30) was first out.** That's LIFO.

## Stack as an array

The simplest implementation: an array plus a `size` counter telling you where the top is.

```c
typedef struct {
    int *data;
    size_t size;     // number of items currently in stack
    size_t cap;
} stack_t;
```

Picture:

```
   data: [ 10 | 20 | 30 |  ?  |  ?  |  ?  ]
            0    1    2    3     4     5
                       ↑
                       size = 3 (next push goes to index 3)
```

`data[size - 1]` is the top. Push: `data[size++] = x`. Pop: `return data[--size]`.

```c
void s_push(stack_t *s, int x) {
    if (s->size == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 8;
        s->data = realloc(s->data, s->cap * sizeof(int));
    }
    s->data[s->size++] = x;
}

int s_pop(stack_t *s) {
    return s->data[--s->size];
}

int s_peek(stack_t *s) {
    return s->data[s->size - 1];
}
```

Push and pop are both **O(1)** (amortized for push because of growing).

## Why stacks matter — the call stack

Every running program has a **call stack** — a literal stack of "where was I" markers.

When function A calls B, the CPU **pushes** A's return address onto the stack. When B returns, it **pops**. If B calls C, C pushes too.

Picture during nested calls A → B → C:

```
Stack of return addresses (and local variables for each call):

   |  C's locals  |   ← top (the function currently running)
   |  C's return  |
   |  B's locals  |
   |  B's return  |
   |  A's locals  |
   |  A's return  |   ← bottom (the original caller)
   +--------------+
```

When C returns, its frame pops. We're back in B. When B returns, it pops. We're back in A.

Recursion **piles up** stack frames — too deep, and you blow the stack (`Segmentation fault`).

## Classic problem: balanced brackets

Given a string like `({[]})`, decide if every opening bracket has a matching close in the right order.

**Idea:** push each opener; on close, pop and check it matches.

```c
int balanced(const char *s) {
    char st[1024];
    int top = 0;
    for (; *s; s++) {
        char c = *s;
        if (c == '(' || c == '[' || c == '{') {
            st[top++] = c;
        } else {
            if (top == 0) return 0;       // close without matching open
            char o = st[--top];
            if (c == ')' && o != '(') return 0;
            if (c == ']' && o != '[') return 0;
            if (c == '}' && o != '{') return 0;
        }
    }
    return top == 0;                       // all openers matched
}
```

### Trace on `"({[]})"`

`top` starts at 0. We track the stack as a list (top on right).

| char | type | action | stack |
|---|---|---|---|
| `(` | open | push | `[ ( ]` |
| `{` | open | push | `[ (, { ]` |
| `[` | open | push | `[ (, {, [ ]` |
| `]` | close | pop `[`, matches | `[ (, { ]` |
| `}` | close | pop `{`, matches | `[ ( ]` |
| `)` | close | pop `(`, matches | `[ ]` |

End of string. Stack is empty → return 1. **Balanced.** ✓

### Trace on `"([)]"`

| char | type | action | stack |
|---|---|---|---|
| `(` | open | push | `[ ( ]` |
| `[` | open | push | `[ (, [ ]` |
| `)` | close | pop `[`. close is `)`, open is `[`. Mismatch! | return 0 |

**Not balanced.** ✓

## Queue: pictures and operations

```
   front                                back
   ↓                                       ↓
   [ A | B | C | D | E ]
   
   dequeue():  remove A from front
   enqueue(F): add F to back
```

After dequeue + enqueue:

```
   front                                back
   ↓                                       ↓
   [ B | C | D | E | F ]
```

## The naive queue (and why it's slow)

If you implement a queue as a normal array with `enqueue = append, dequeue = remove front`, **dequeue is O(n)** because you have to shift every remaining element left.

```
Before dequeue:  [ A | B | C | D | E ]
                   ↑
                   front
After dequeue:   [ B | C | D | E |   ]      shifted everyone left

That shift takes n-1 writes. O(n) per dequeue. Bad.
```

## Circular queue (ring buffer) — fast queue

The fix: don't shift. Use a **head** index for the front, a **tail** index for the back. **Wrap around** when you reach the end of the array.

```c
typedef struct {
    int *buf;
    size_t cap;
    size_t head, tail, size;
} ring_t;
```

Picture (cap = 5):

```
   index:    0    1    2    3    4
           +----+----+----+----+----+
   buf:    | A  | B  | C  |    |    |
           +----+----+----+----+----+
            ↑              ↑
            head=0         tail=3
            size = 3
```

Enqueue D: write to `buf[tail]`, then `tail = (tail+1) % cap`.

```
   buf:    | A  | B  | C  | D  |    |
                                ↑
                                tail=4
            head=0
            size = 4
```

Enqueue E: tail wraps from 4 to (4+1)%5 = 0... wait no, **tail goes to 5%5 = 0** after writing index 4.

Actually let me redo this carefully.

```
state:
            head=0, tail=4, size=4   buf = [A, B, C, D, _]

enqueue(E):
            buf[4] = E                  buf = [A, B, C, D, E]
            tail = (4+1) % 5 = 0        head=0, tail=0, size=5  ← FULL
```

Now the ring is full. `head == tail` and `size == cap`.

Dequeue: read `buf[head]`, then `head = (head+1) % cap`.

```
dequeue() → A
            head = (0+1) % 5 = 1        buf = [_, B, C, D, E]   head=1, tail=0, size=4
dequeue() → B
            head = 2                    buf = [_, _, C, D, E]   head=2, tail=0, size=3
```

Notice: front is at `head=2`. Back is at `tail=0`. The "front-to-back" range **wraps around** the array. That's the magic of a ring buffer.

Enqueue F:

```
            buf[0] = F                  buf = [F, _, C, D, E]
            tail = 1                    head=2, tail=1, size=4
```

The data F goes back at index 0 because that slot is free.

### The full code

```c
int r_enq(ring_t *r, int v) {
    if (r->size == r->cap) return -1;        // full
    r->buf[r->tail] = v;
    r->tail = (r->tail + 1) % r->cap;
    r->size++;
    return 0;
}

int r_deq(ring_t *r, int *out) {
    if (r->size == 0) return -1;             // empty
    *out = r->buf[r->head];
    r->head = (r->head + 1) % r->cap;
    r->size--;
    return 0;
}
```

Both **O(1).** No shifting. The only overhead: a modulo.

If `cap` is a **power of two**, you can replace `% cap` with `& (cap - 1)` — same result, one cheaper instruction. The kernel does this everywhere.

## Where you see queues in real systems

- **Print job queue** — first job submitted prints first.
- **Network packet buffers** — packets in, packets out, FIFO.
- **GPU command rings** (`amdgpu_ring`) — CPU writes commands, GPU reads them. **Literally a ring buffer.**
- **Interrupt-handler ring** in `amdgpu_irq.c` — hardware writes events, CPU drains.
- **Producer/consumer pipelines** — one thread fills the queue, another drains it.
- **Linux's `kfifo`** — kernel ring-buffer macros. Read `include/linux/kfifo.h`.

## SPSC ring buffer — fast queue between two threads

If exactly **one** thread enqueues and **one** thread dequeues, you can write a queue **without locks** using only atomic head/tail indices. This is called **single-producer single-consumer** (SPSC).

```c
// producer thread:
size_t t = atomic_load(&tail);
if ((t + 1) % cap == atomic_load(&head)) return FULL;     // full
buf[t] = item;
atomic_store(&tail, (t + 1) % cap);                       // publish

// consumer thread:
size_t h = atomic_load(&head);
if (h == atomic_load(&tail)) return EMPTY;
*out = buf[h];
atomic_store(&head, (h + 1) % cap);
```

**Why this works:**
- Only the producer writes `tail`.
- Only the consumer writes `head`.
- The producer reads `head` to check fullness; the consumer reads `tail` to check emptiness.
- No two threads write the same variable → no need for a mutex.

The actual code needs **memory barriers** (acquire/release) for correctness on weak-memory-ordering CPUs (ARM). We'll cover this in Part IV (multithreading). For now, just absorb the **shape**.

This pattern powers `kfifo`, GPU rings, audio buffers, video frame queues — anywhere one fast producer feeds one fast consumer.

## Stack vs queue — when to use what

| Need | Use |
|---|---|
| Undo / redo | stack (last action first to undo) |
| Function call tracking | stack |
| Match brackets / parsing | stack |
| Depth-first search (DFS) | stack |
| Print jobs / task queue | queue |
| Breadth-first search (BFS) | queue |
| Producer-consumer | queue (often ring buffer) |

If you need **fast random access** in either, you don't want stack/queue — you want a different structure.

## Common bugs

### Bug 1: pop from empty stack

```c
int s_pop(stack_t *s) {
    return s->data[--s->size];      // if size==0, this becomes data[-1] → bad
}
```

Always check: `if (s->size == 0) return -1;` (or use a separate "is empty" check).

### Bug 2: ring buffer fullness off-by-one

The classic mistake: `head == tail` is ambiguous — does it mean **empty** or **full**?

Two solutions:
1. Keep a separate `size` counter (what we did above).
2. Always leave one slot empty. Then `(tail+1)%cap == head` means full, `head == tail` means empty.

Both work. Pick one and stick to it across enqueue/dequeue.

### Bug 3: mixing up head and tail

In a queue, **dequeue from head**, **enqueue at tail**. Mix them up → you've got a stack!

## Recap

1. **Stack** = LIFO. Push and pop on the same end.
2. **Queue** = FIFO. Enqueue at back, dequeue at front.
3. Array-backed stack: simple, fast.
4. Naive array queue: O(n) dequeue (shifts).
5. **Ring buffer** = fast O(1) queue. Used everywhere from kernel `kfifo` to GPU rings.
6. **SPSC ring buffer** = lock-free queue between exactly one producer and one consumer.

## Self-check (in your head)

1. Push 1, 2, 3, 4 onto an empty stack. Pop twice. What's on the stack?

2. Enqueue 1, 2, 3, 4 into an empty queue. Dequeue twice. What's the front element now?

3. In a ring buffer with `cap=5`, after these ops: enq A, enq B, enq C, deq, enq D, enq E, enq F — what's `head`, `tail`, `size`?

4. The string `"((()"` — does `balanced` return 0 or 1? Trace the stack to verify.

5. Why is dequeue **O(1)** for a ring buffer but **O(n)** for a naive array queue?

---

**Answers:**

1. After pushes: `[1, 2, 3, 4]`. After two pops (4 then 3 leave): **`[1, 2]`**.

2. After enqueues: `[1, 2, 3, 4]` (front=1). After two dequeues (1 then 2 leave): **front element is now 3**, queue is `[3, 4]`.

3. Trace:
   - Start: head=0, tail=0, size=0
   - enq A: buf[0]=A, tail=1, size=1
   - enq B: buf[1]=B, tail=2, size=2
   - enq C: buf[2]=C, tail=3, size=3
   - deq: A out, head=1, size=2
   - enq D: buf[3]=D, tail=4, size=3
   - enq E: buf[4]=E, tail=0 (wrap), size=4
   - enq F: buf[0]=F, tail=1, size=5
   
   **Final: head=1, tail=1, size=5** (full). Buffer = [F, B, C, D, E].

4. Trace:
   - `(` push → `[(]`
   - `(` push → `[(, (]`
   - `(` push → `[(, (, (]`
   - `)` pop → `[(, (]`
   - end of string. Stack is **not empty** (size=2) → return 0.
   
   **Not balanced.** ✓

5. Ring buffer: dequeue just **reads** index `head` and bumps `head` by one. No data moves. Naive array queue: dequeue must **shift** all remaining elements left. Shift is O(n).

If you got 4/5 you understand stacks and queues. 5/5, move on.

Next → [DSA-06 — Hash tables](DSA-06-hash-tables.md).
