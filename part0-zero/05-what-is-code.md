# Chapter 5 — What is code?

You've heard "code" your whole life. Let's nail down what it actually means.

## Code is just text in a file

When someone "writes code," they open a text editor (like `nano` or VS Code from chapter 4), type some sentences in a special language the computer understands, and save the file.

That's it. Code is text. It's not magic, it's not a black box, it's literally a `.txt`-like file with a different extension.

Examples:

| File | Language |
|---|---|
| `hello.py` | Python |
| `hello.c` | C |
| `hello.cpp` | C++ |
| `hello.sh` | Bash (shell script) |
| `hello.html` | HTML (a web page, not really "code" but text) |
| `hello.rs` | Rust |
| `hello.java` | Java |

The extension doesn't make a file work. It's just a hint to humans (and editors) about what's inside.

## What does "running" a program mean?

You **wrote** text. To **run** it you need a program that reads your text and *does* what the text says.

There are two main flavors of "run":

### Flavor 1: an interpreter reads your file and acts on it line by line

This is how Python and Bash work.

```bash
python3 hello.py
```

This launches the program **`python3`**, which opens `hello.py`, reads the first line, does what it says, then the second line, then the third, etc.

The interpreter is itself a program (someone wrote `python3` in C, mostly). It exists on your computer because it was installed.

### Flavor 2: a compiler turns your file into a new program, which you then run

This is how C and C++ work.

```bash
gcc hello.c -o hello       # compile: hello.c → hello (a binary)
./hello                    # run the binary
```

Step 1 produces a brand new file (`hello`) made of machine code — the language the CPU itself speaks. Step 2 runs that file.

Why two flavors?

| Interpreter | Compiler |
|---|---|
| Easy. You change your code, run it. | More setup. You change, compile, then run. |
| Slow. The interpreter does work on every line every time. | Fast. The compiler did the hard work once; running is direct CPU instructions. |
| Friendlier error messages. | Less friendly errors, but better hardware control. |
| Python, Bash, JavaScript, Ruby. | C, C++, Rust, Go. |

**The Linux kernel and amdgpu are written in C.** That's what we're heading towards. But for chapters 6–8 we use Python because it's gentler.

## A program is a list of instructions

Whatever the language, a program is fundamentally a list of instructions:

```
1. take this number
2. add 1 to it
3. print it
4. if it's less than 10, go back to step 2
```

That's actual logic. We'll dress it up in Python syntax in the next chapter:

```python
n = 0
while n < 10:
    n = n + 1
    print(n)
```

Same thing. The fancy parts are just how the language wants you to write it.

## What happens when you run a program (the kid version)

You type:

```bash
./hello
```

What happens, in slow motion:

1. The shell finds the file `hello` (you'll learn about `PATH` later).
2. The shell asks the **operating system** (Linux) to run it.
3. Linux makes a new **process** — a sandbox with its own memory and a place to track the program.
4. Linux loads the program's instructions from disk into the process's memory.
5. Linux starts the CPU executing the instructions.
6. The instructions run. They might do calculations, print to the screen, read files.
7. When the program finishes, Linux cleans up the process. The shell prompt comes back.

You don't have to remember every step. Just internalize: **a program is *loaded into memory* and *the CPU runs it*.**

## "But what is a programming language *really*?"

A programming language is a set of rules ("syntax") for how you write code, and a meaning ("semantics") for what each piece of code does.

For example, in Python, this is valid:

```python
print("hello")
```

In C, the same thing looks like:

```c
#include <stdio.h>
int main(void) {
    printf("hello\n");
    return 0;
}
```

C requires more ceremony because it gives you more control over the machine. Python hides the ceremony but takes the control away. Different trade-offs.

## "Source code" vs "binary"

- **Source code**: the human-readable text you wrote. `hello.c` is source code.
- **Binary** (or **executable**): the machine-code file produced by the compiler. `hello` (no extension on Linux) is a binary.

You only ship source code to other developers. You only ship binaries to users (or both).

You can peek inside a binary:

```bash
file ./hello
# ELF 64-bit LSB pie executable, x86-64, dynamically linked, ...

ls -l ./hello
# -rwxr-xr-x 1 sudhdevu sudhdevu 16640 May 7 14:30 hello
```

`x` in the permissions means "executable." That's why `./hello` works — the file is marked as runnable.

## A driver is just a program too

Important grounding: the Linux kernel is a (huge) C program. A driver like amdgpu is a smaller C program *inside* it.

When you eventually write `myring.c` in lab 05, it will be compiled into `myring.ko` — a binary, just a special one that gets loaded **into the kernel** instead of run as a user process. We'll explain that in detail in Part V.

For now: don't be scared of the word "driver." A driver is just code. You can read it, write it, change it. There is no priesthood.

## Try it (10 minutes)

We'll do the *very* first hands-on:

```bash
cd ~/ldd-textbook
mkdir hello-py
cd hello-py
nano hello.py
```

In `nano`, type exactly:

```python
print("Hello, world!")
```

Save (`Ctrl+O`, Enter), quit (`Ctrl+X`).

Then run it:

```bash
python3 hello.py
```

You should see:

```
Hello, world!
```

Congratulations. You have written and run a program.

If `python3` is not installed:

```bash
sudo apt install python3
```

If it doesn't print "Hello, world!" but instead complains, **read the error message carefully**. 99% of the time it's a typo in your file.

## Self-check

- What's the difference between an interpreter and a compiler? Name an example of each.
- Why does running a C program take 2 commands but running a Python program takes 1?
- What is a "binary"? Where does it come from?
- Is the amdgpu driver fundamentally different from `hello.py`? (Spoiler: no, just bigger and runs in a different mode.)

You wrote code. Tomorrow we make it actually do useful things.

➡️ Next: [Chapter 6 — Your first program (in Python)](06-first-program-python.md)
