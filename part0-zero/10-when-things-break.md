# Chapter 10 — When things break (the most important chapter)

Programs don't work the first time. Or the second. Or the tenth. **That's not a bug — that's the job.**

A senior engineer is not someone whose code "just works." A senior engineer is someone who *fixes* code faster than a junior. The faster you can debug, the better an engineer you are.

This chapter is the one most beginners skip. **Don't skip it.** Reading it carefully now is worth 10 chapters of code.

## The mindset

Three rules.

### Rule 1: The computer is not lying to you

When the computer says "Permission denied," it is not picking on you. There genuinely is no permission. When it says `NameError: 'foo' is not defined`, the variable `foo` genuinely is not defined.

**Always assume the error message is true.** Read it.

99% of beginner debugging goes like this:

1. Get an error.
2. Don't read it.
3. Re-run the same thing hoping it works.
4. Same error.
5. Frustration.
6. Eventually read the error.
7. "Oh, I had a typo on line 5."

Skip steps 2-6. Read the error first.

### Rule 2: Read the error from the top

When you get a stack of error output, **the cause is usually at the top, not the bottom**. (Some tools are reversed, but Python and `gcc` show the cause first.)

Example:

```
Traceback (most recent call last):
  File "fizzbuzz.py", line 4, in <module>
    print(n)
NameError: name 'n' is not defined
```

The cause: `n` is not defined. The location: `fizzbuzz.py`, line 4. The fix: define `n` (probably the loop variable was misspelled).

C example:

```
hello.c: In function 'main':
hello.c:5:5: error: expected ';' before '}' token
    5 |     printf("hi\n")
      |     ^
      |     ;
    6 | }
```

Cause: missing `;`. Location: line 5. The compiler even shows where to put it (`^` arrow under the spot, suggested `;` below).

### Rule 3: Make one change at a time

When code is broken, the temptation is to change 3 things at once. Don't. Change **one** thing. Run. See what happens. Then the next change.

If you change 3 things and it works, you don't know which fix worked. Worse, you might have introduced 2 new bugs that cancel out.

Single-change debugging feels slow and is fast.

## The 90% checklist

When something doesn't work, walk this list. The fix is almost always in here.

1. **Did you save the file?** Many beginners edit, run, get the old behavior, panic. Save (`Ctrl+S` or `:w` in vim).
2. **Are you running the right file?** `python3 fizz.py` when the file is `fizzbuzz.py` produces "no such file."
3. **Spelling.** `pirntf` vs `printf`. `lenght` vs `length`. Run `grep` over your code if in doubt.
4. **Indentation (Python)** or **braces (C)**. A single missed brace can cause weird errors many lines later.
5. **Did you include the right header (C)?** If you use `printf`, you need `#include <stdio.h>`.
6. **Is the file in the directory you think?** `pwd`, `ls`. Easy to be in `~/Documents` when your file is in `~/ldd-textbook`.
7. **Did the compile actually succeed?** If `gcc` printed errors, the binary didn't update. Running `./hello` is running yesterday's `hello`.
8. **Are arguments in the right order?** `cp src dst`, not `cp dst src`. `gcc -o hello hello.c` works; `gcc hello.c -o hello` works; `gcc hello.c hello -o` does not.

Fix any of these one at a time, re-run, see if the symptom is gone or different.

## Reading C compiler errors

C compiler errors are scary at first. They follow a pattern:

```
file:line:column: severity: message
```

Common ones:

### `error: expected ';' before ...`

You forgot a `;`. The line and column tell you where.

### `error: 'foo' undeclared`

You used `foo` without declaring it. Either `int foo;` it, or check spelling.

### `warning: implicit declaration of function 'printf'`

You called a function the compiler doesn't know about. Probably missing `#include <stdio.h>`.

### `warning: control reaches end of non-void function`

Your function returns `int` but doesn't `return` anything. Add `return 0;` (or whatever).

### `error: too few arguments to function 'add'`

You declared `add(int a, int b)` and called `add(5)`. Pass both.

### `Segmentation fault (core dumped)`

The program **ran but crashed**. You accessed memory you weren't supposed to (often: a NULL or uninitialized pointer). This is *the* C beginner experience. We dedicate Part I chapter 4 to it. For now, narrow down with print statements.

### `undefined reference to 'foo'`

The compiler compiled OK but the **linker** can't find `foo`. Either you forgot to define `foo`, or you forgot to pass the `.c` file or library to `gcc`.

### `warning: unused variable 'x'`

You declared `x` but never used it. Delete the declaration, or use it.

**Always compile with `-Wall -Wextra`** so you see warnings. Treat warnings as errors. They are.

## Reading Python errors

Python errors are friendlier. They almost always say:

- the kind (`NameError`, `TypeError`, `ValueError`, `IndexError`, `SyntaxError`, ...).
- the file and line.
- the underlying cause.

Pattern: read the **last** line (the kind + message). Look at the line in your file. Fix.

The most common Python errors:

| Error | Means | Fix |
|---|---|---|
| `SyntaxError` | invalid grammar | look at the line, count quotes, parens, colons |
| `IndentationError` | bad spaces | use 4 spaces; don't mix tabs |
| `NameError` | variable not defined | spell it right, define it before use |
| `TypeError` | wrong kind of value | `int + str`? convert one side |
| `ValueError` | right type, wrong content | `int("abc")` — check input |
| `IndexError` | index out of range | `len(list)` to know how big it is |
| `KeyError` | missing dict key | check the key exists first |
| `ZeroDivisionError` | divided by 0 | guard with `if x != 0:` |
| `AttributeError` | object doesn't have that thing | check spelling, type |

## Print debugging

The classic move when stuck: add `print` statements to see what the program is doing.

```python
def sum_evens_up_to(N):
    total = 0
    for i in range(1, N + 1):
        print(f"DEBUG: i={i}, total={total}")
        if i % 2 == 0:
            total = total + i
    return total
```

Now run it. The `DEBUG:` output shows you what's happening. When you see the bug, fix it, then **delete the print**.

In C:

```c
printf("DEBUG: i=%d total=%d\n", i, total);
```

Same idea.

There are fancier tools (`gdb`, `pdb`, `pyspy`, `strace`, `ftrace`, ...). Learn `print` first. It works everywhere, including inside drivers (where it's called `printk`).

## The 30-minute rule

If you've been stuck on the same problem for **30 minutes** without progress, do this:

1. **Stand up.** Walk for 5 minutes. No phone.
2. **Re-state the problem in a notebook.** What were you trying to do? What's actually happening? What's the gap?
3. **Reduce.** Can you make a smaller test case that fails? A 5-line program that shows the same bug?
4. **Search.** Copy the *exact* error message into Google. Add the language name. (`gcc "expected ';' before"`. `python3 "TypeError" "NoneType"`.)
5. **Read someone else's question.** Stack Overflow has the answer to most beginner questions. Read the *accepted* answer carefully.
6. **Ask.** If still stuck after 60 minutes, ask in:
   - your team's Slack/Teams (don't sit silently),
   - `#linux-newbie` IRC,
   - a coworker.

There is no medal for struggling alone for 6 hours. Senior engineers ask each other for help all the time. **Asking is a skill, not a weakness.**

## How to ask a good question

A bad question:

> hi guys my code dont work plz help

You will get no response.

A good question:

> Trying to compile this 8-line C program (paste). It compiles fine but at runtime I get `Segmentation fault`. I'm running it on Ubuntu 22.04, gcc 11.4. I've already tried changing the array size from 100 to 10. Same crash. What am I missing?

The good question:

- shows the exact code,
- shows the exact error,
- shows the environment,
- shows what you've already tried.

Spending 10 minutes writing a good question often *answers itself* — you discover the bug while writing.

## When the program "just hangs"

If a program seems to be doing nothing forever:

- Press `Ctrl+C` to interrupt.
- It's almost always an infinite loop. Trace through: what's the loop condition? What changes it? Maybe nothing.

## When the kernel "just hangs" (foreshadowing)

In Part V you'll be writing kernel code, where the consequences are bigger. A bad kernel module can hang or crash the whole machine. That's why we always test in a VM. We'll get there.

For now: any "loop forever" you make in Python or userspace C just runs until you `Ctrl+C` it. No harm done.

## A concrete debugging example

Here's a real session. You wrote `factorial.py`:

```python
def factorial(n):
    if n == 0:
        return 1
    return n + factorial(n - 1)        # bug

print(factorial(5))
```

Expected: `120`. Actual: `15`. Why?

1. **Is the answer wrong, or did the program crash?** It runs, but answer is wrong. So it's a logic bug, not a syntax/runtime bug.
2. **What do I expect for n=5?** 5×4×3×2×1 = 120.
3. **What does the code compute?** 5 + 4 + 3 + 2 + 1 + 1 = 16... wait, it gives 15. Hmm. Oh: 5 + 4 + 3 + 2 + 1 + factorial(0) = 5+4+3+2+1+1 = 16. Why does it print 15?
4. **Add prints.**

```python
def factorial(n):
    print(f"factorial({n}) called")
    if n == 0:
        return 1
    r = n + factorial(n - 1)
    print(f"factorial({n}) returning {r}")
    return r

print(factorial(5))
```

Run. Look at the output. You'll see the chain of calls and what each returned. The bug becomes obvious: `+` should be `*`.

Fix:

```python
return n * factorial(n - 1)
```

Run again: `120`. Done.

This is the entire game. Form a hypothesis, add prints, look, refine.

## Try it (30 minutes)

I've given you 5 broken Python programs. Your job: find and fix each.

(Type each one as-is, run, read the error, fix.)

### bug1.py

```python
print("hello world)
```

### bug2.py

```python
x = 10
y = 20
z = x + Y
print(z)
```

### bug3.py

```python
for i in range(5)
    print(i)
```

### bug4.py

```python
n = int(input("number: "))
if n = 0:
    print("zero")
else:
    print("nonzero")
```

### bug5.py

```python
def square(n):
    n * n

print(square(5))
```

(Hint for bug5: it prints `None`. Why?)

Solutions are intentionally not given here. Stare at the error and fix. If you get all 5 in 30 minutes, you've cleared the bar.

## Self-check

- Why does "make one change at a time" matter?
- What's the 30-minute rule?
- What's a good way to phrase a question when you're stuck?
- If a program prints `None`, what's the most likely cause?
- You see `Segmentation fault (core dumped)` running a C program. Did the compile succeed? (Yes — segfault is a *runtime* crash.)

Debugging is the skill. You will use it every day for the rest of your career. Be friends with errors.

➡️ Next: [Chapter 11 — Bridge to Part I](11-bridge-to-part1.md)
