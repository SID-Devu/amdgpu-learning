# Chapter 11 — Bridge to Part I

You made it. **Take a moment.** Two or three weeks ago you had not opened a terminal. Now you have:

- navigated Linux from the command line,
- written code in Python and C,
- used variables, conditions, loops, and functions,
- compiled C programs,
- read error messages and fixed bugs.

That is genuinely 80% of what most "intro to programming" courses teach in a semester. You did it in chapters.

You are no longer a beginner-beginner. You are now just a beginner. That's a real promotion.

## What changes in Part I

Part I is **C, deeply.** The same ideas you learned (variables, loops, functions) but with the *machine-level meaning* exposed.

In Python, when you wrote `x = 5`, the language hid:

- where `x` lives in memory,
- what shape `5` has in bytes,
- what happens if you ask for `x` after deleting it,
- how the operating system gave you that memory in the first place.

In Part I we lift the floorboards. We talk about:

- **Bytes, words, alignment**: what `int` *physically* is.
- **Pointers**: a number that is the address of another byte. The single most powerful and dangerous thing in C.
- **Memory layout**: stack vs heap, what `malloc` and `free` actually do.
- **Undefined behavior**: places where C says "you broke the rules; whatever happens is on you."

Then we apply it to write our own memory allocator, the kind you'd find in a kernel driver.

Part I is **the** part that separates "people who know C" from "people who can write C in a kernel." Don't rush it.

## Recommended pacing for Part I

| Chapter | Days | Goal |
|---|---|---|
| 1 — What is a program | 1 | Re-read with fresh eyes. Most of it should now feel familiar. |
| 2 — Toolchain | 2 | Get comfortable with `gcc`, `make`, header files. |
| 3 — Types, integer promotion, UB | 3 | This is dense. Take your time. |
| 4 — Pointers | 5+ | The famous one. Draw boxes on paper. |
| 5 — Structs, unions, alignment | 3 | Foundational for kernel data types. |
| 6 — Bit manipulation | 2 | The language of hardware registers. |
| 7 — Allocators | 4 | Hands-on: build one. |
| 8 — Preprocessor & build | 2 | Demystifies `#include`. |
| 9 — Function pointers | 2 | The mechanism behind `file_operations`. |
| 10 — Defensive C | 2 | Habits to keep. |

About **6 weeks at 1 hour a day**, plus the corresponding labs.

If a chapter feels rough: re-read; type the example three times; sleep; come back. The brain consolidates this stuff while you sleep.

## How to read Part I (and the rest of the book)

1. **Read the chapter once for shape.** Don't try to understand everything. Get the *map*.
2. **Type every code block** into your editor. Compile. Run.
3. **Modify the code.** Break it. See what changes. (Compilers can't catch bugs you didn't write.)
4. **Do the exercises.** Even the ones that seem easy.
5. **Check the corresponding lab in `labs/`** if there is one.
6. **Write 3 sentences in your notebook**: what's clear, what's still fuzzy, what you'll do tomorrow.

If after all that something is still fuzzy, **flag it and move on**. Often the *next* chapter clarifies the previous one. Don't get stuck for 2 weeks on one paragraph.

## What to expect emotionally

There will be moments — chapter 4 (pointers), chapter 7 (allocators), Part III (virtual memory), Part VII (GPU command submission) — where you will feel like you genuinely don't understand anything. **That is the experience.** Every kernel engineer has felt it. Some still do. They ship anyway.

When you hit that wall:

- Re-read the previous chapter.
- Sleep on it.
- Draw a diagram on paper.
- Come back.

Walls are growth. The ones who quit at the wall remain juniors forever. The ones who push through become senior.

## A sneak preview: the same FizzBuzz, kernel-style

Here's what kernel C looks like, just so you've seen it. Don't try to understand it — just notice that it's *the same shape* as your `fizzbuzz.c`:

```c
static int my_module_init(void)
{
    int i;

    pr_info("module loaded\n");

    for (i = 1; i <= 20; i++) {
        if (i % 15 == 0)
            pr_info("FizzBuzz\n");
        else if (i % 3 == 0)
            pr_info("Fizz\n");
        else if (i % 5 == 0)
            pr_info("Buzz\n");
        else
            pr_info("%d\n", i);
    }

    return 0;
}
```

Differences from your fizzbuzz.c:

- `static int my_module_init(void)` instead of `int main(void)` — a function the kernel calls when this module loads.
- `pr_info(...)` instead of `printf(...)` — the kernel doesn't have stdio; `pr_info` writes to the kernel log instead.
- No `#include <stdio.h>` — kernel doesn't have those headers; it has `<linux/...>` ones.

Otherwise: same `for`, `if`, `else if`. **You already know how to read this.** You just need to learn the new vocabulary, which is what Parts V and VI do.

## A reminder of the journey

You are following this map:

```
Part 0 (you are here, finishing) — Absolute zero
Part 1 — C deeply
Part 2 — C++ for systems
Part 3 — How the OS works
Part 4 — How threads and memory really work
Part 5 — The Linux kernel itself
Part 6 — Writing real Linux drivers
Part 7 — The GPU stack and the amdgpu driver
Part 8 — Hardware/software interaction (PCIe, DMA, profiling)
Part 9 — Getting hired and growing
```

That's a *years*-long climb in real terms — but you climb a few hundred feet a week and don't look down.

## A request

Before you turn the page to Part I:

1. Open your notebook.
2. Write the date.
3. Answer in your own words:
   - "A program is..."
   - "A driver is..."
   - "When my program crashes, the first thing I do is..."
   - "The hardest thing in Part 0 was..."
   - "I'm proud that I..."

That's your snapshot of Day 0 of your real engineering career. In 12 months, when you're sending kernel patches, you'll re-read it and smile.

OK. Let's go.

➡️ Onward to [Part I — C from Zero to Driver-Grade](../part1-c/01-what-is-a-program.md).
