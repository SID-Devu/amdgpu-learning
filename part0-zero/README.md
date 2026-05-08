# Part 0 — Absolute Zero

> **Read this if you have never written a line of code in your life.**
>
> This part assumes **nothing**. Not even that you know how to open a terminal. We will go from "what is a computer" to "I just wrote my first C program," step by step, slowly, with no jargon.

The rest of the book (Parts I–IX) is hard. It has to be — your job is hard. But you cannot start there. You have to walk before you run, and run before you fly. **Part 0 is the walking.**

## Who this part is for

- People who got a degree but never really wrote code.
- People who feel "everyone else seems to already know this stuff."
- People who tried programming books before and gave up because chapter 1 was already too much.

That's most fresh graduates. You are not behind. You are just starting. Welcome.

## How long it takes

About **2–3 weeks** at 1 hour a day. Less if you already know some of this; more if you take your time. **Take your time.** There is no prize for finishing fast.

## How to read this part

1. Read one chapter a day. Don't binge.
2. **Type every command** by hand. Do not copy-paste. Your fingers learn what your eyes don't.
3. If you don't understand a sentence, **read it twice more**, then move on. It will make sense by chapter 5.
4. After each chapter, do the "Try it" boxes. They take 5 minutes each.
5. **You are allowed to be stuck.** Being stuck is normal. Chapter 10 is about exactly this.

## Table of Contents

| # | Chapter | What you'll learn |
|---|---|---|
| 0 | [Start here](00-start-here.md) | What this part is, what mindset to bring |
| 1 | [What is a computer?](01-what-is-a-computer.md) | CPU, RAM, disk — in plain English |
| 2 | [Using Linux: the terminal](02-using-linux.md) | Open a terminal. Type commands. Don't be afraid |
| 3 | [Files and paths](03-files-and-paths.md) | What `/`, `~`, `.`, and `..` mean |
| 4 | [Text editors](04-text-editors.md) | How to actually create and edit a file |
| 5 | [What is code?](05-what-is-code.md) | Source code, running a program, the big picture |
| 6 | [Your first program (in Python)](06-first-program-python.md) | `print`, variables, arithmetic — gently |
| 7 | [Decisions and repeats](07-decisions-and-repeats.md) | `if`, `while`, `for`, in Python |
| 8 | [Functions](08-functions.md) | Naming a piece of code so you can reuse it |
| 9 | [From Python to C](09-from-python-to-c.md) | The same ideas in C — your real working language |
| 10 | [When things break](10-when-things-break.md) | The debugging mindset (the most important chapter) |
| 11 | [Bridge to Part I](11-bridge-to-part1.md) | You are ready. Here's the map. |

### Foundations side-track (read on demand)

These are *remedial* topics. If you ever feel "everyone seems to assume I know this," read the relevant one and come back.

| # | Chapter | Read when |
|---|---|---|
| A1 | [Numbers — binary, hex, decimal](foundations/A1-numbers.md) | You see `0xCAFE` or `1 << 12` and don't know what they mean |
| A2 | [Bits, bytes, words](foundations/A2-bits-and-bytes.md) | You're unsure what a byte is, or KB vs KiB |
| A3 | [Boolean logic — AND/OR/NOT/XOR](foundations/A3-boolean-logic.md) | Bit operations confuse you |
| A4 | [Shell power tour](foundations/A4-shell-power-tour.md) | You want pipes, grep, scripts beyond chapter 2 |
| A5 | [Character encoding — ASCII / UTF-8](foundations/A5-character-encoding.md) | Text shows up garbled or you hit a CRLF mystery |

See [`foundations/README.md`](foundations/README.md) for the index.

## Why we use Python first

You will end up writing **C** for your job (and that's what Part I onwards teaches). But C is unforgiving — small mistakes cause weird crashes that confuse beginners.

**Python** is friendly. It tells you exactly what went wrong in plain English. Using it for chapters 6–8 lets you focus on *the ideas* (variables, loops, functions) without fighting the language.

In chapter 9, we re-do every Python program in C, side by side, so you see the same idea in both. Then we never come back to Python.

## What you need

- A computer with **Linux installed** (Ubuntu 22.04 or later is easiest).
  - If you only have Windows: install **WSL2** (one Google search away). It gives you a real Linux inside Windows.
  - If you have Mac: most things in this book work, but get a Linux VM eventually.
- An internet connection.
- 1 hour a day for 2–3 weeks.
- A notebook (paper). Yes, paper. You will draw boxes and arrows.

That's it. Let's start.

➡️ Next: [Chapter 0 — Start here](00-start-here.md)
