# DSA 0 — How to read this part (without running code)

> "I can't run the code. Can I still learn?"
> **Yes.** This part is rewritten so you can learn purely by reading.

## What changed

The earlier version of these chapters showed code and said "go try it." That's good if you can run a compiler. **It's useless if you can't.**

The rewritten chapters show, for every important algorithm:

1. **A picture** of the data structure in memory.
2. **The code** with each line explained in plain English.
3. **A worked trace** — a step-by-step table showing how variables change as the algorithm runs.
4. **A second worked example** — different input, same algorithm, again traced.
5. **What goes wrong** — drawings of buggy states.
6. **A recap** in 3–5 sentences.

If you read each chapter slowly with a notebook (paper or digital), **you will learn** even without running anything. Most senior engineers learned the basics this way — there were no online compilers in 1995.

## How to read each chapter

### Pass 1 — get the picture

- Read the **"Big idea"** section.
- Look at the **picture** of the data structure.
- Skip the code.
- Read the **recap** at the end.
- Now you know **what** the chapter is about, even if not **how**.

### Pass 2 — follow the trace

- Open your notebook.
- Read the code once. Don't worry if some lines confuse you.
- Read the worked trace **slowly**. At each step, **draw the diagram yourself**.
- Verify the variable values in the trace match what you drew.
- If something doesn't match, re-read the code line for the previous step.

### Pass 3 — explain it back

- Close the page.
- On a blank piece of paper, **draw the data structure** from memory.
- **Re-derive** the algorithm — don't copy it; reason it out.
- Read the chapter again to find the gaps.

After pass 3, you "own" the chapter. That's the goal.

## A note on diagrams

Almost everything is in **ASCII art**. It looks like this:

```
[10] -> [20] -> [30] -> NULL
```

Don't dismiss it. Every textbook diagram you've seen is a more polished version of this. The shape **is** the lesson.

For trees:

```
      10
     /  \
    5    15
   / \
  3   8
```

For arrays + indices:

```
index:   0    1    2    3    4
value: |10 | 20 | 30 | 40 | 50 |
```

For pointers:

```
p ──▶ [10 | next ──▶ [20 | next ──▶ NULL ]]
```

Read these as if they were pictures in a book. Your brain will fill in the rest.

## What if I really want to run something?

You don't have to. But if you do:

- **Online**: compiler-explorer.com (godbolt) lets you compile and step through C in a browser. Free.
- **Online**: replit.com gives you a real Linux box in a tab.
- **Online**: programiz.com/c/online-compiler — just paste C code, hit "Run."

These need only a browser. No installation.

But again — **you don't need them**. This rewritten part is designed to teach you without them.

## A note on the original chapters

The original chapters (DSA-01 to DSA-28) are still here. Some of the later ones (advanced topics) still expect you to run code. The **first 10 chapters** have been rewritten in the new style. Continue with the originals once you've mastered the foundations — by then you may want to run code yourself.

| Original style | Rewritten — read-to-learn |
|---|---|
| DSA-01 Big-O | ✅ rewritten |
| DSA-02 Arrays | ✅ rewritten |
| DSA-03 Strings | ✅ rewritten |
| DSA-04 Linked Lists | ✅ rewritten |
| DSA-05 Stacks & Queues | ✅ rewritten |
| DSA-06 Hash Tables | ✅ rewritten |
| DSA-07 Trees | ✅ rewritten |
| DSA-10 Heaps | ✅ rewritten |
| DSA-14 Sorting | ✅ rewritten |
| DSA-15 Searching | ✅ rewritten |
| DSA-08, 09, 11, 12, 13, 16-28 | original (still readable) |

Chapters not yet rewritten will be in the next round.

## Last thing — be patient

Learning a data structure from a diagram + trace **feels slower** than running code. It's not. Running code skips the part where you actually understand. Reading + tracing **is** the understanding.

You don't get faster by skipping steps. You get faster by mastering them.

→ Now read [DSA-01 — Big-O and complexity](DSA-01-bigO.md).
