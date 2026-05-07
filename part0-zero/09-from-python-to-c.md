# Chapter 9 — From Python to C

You now know what variables, decisions, loops, and functions are. **Those ideas exist in C too**, just dressed differently.

This chapter is the **bridge**. We rebuild every Python program from chapters 6–8 in C, side by side. After this you will be able to read Part I of the book.

## Get the C compiler

```bash
sudo apt install build-essential
gcc --version
```

You should see something like `gcc (Ubuntu …) 11.x` or newer.

`gcc` is the GNU C compiler. It turns your `.c` source code into a runnable binary.

## Hello, C

`~/ldd-textbook/hello-c/hello.c`:

```c
#include <stdio.h>

int main(void)
{
    printf("Hello, world!\n");
    return 0;
}
```

Build and run:

```bash
cd ~/ldd-textbook/hello-c
gcc hello.c -o hello
./hello
```

Output:

```
Hello, world!
```

That's it. You wrote your first C program. Let's break it down line by line.

| Line | What it means | Python equivalent |
|---|---|---|
| `#include <stdio.h>` | "I want to use the standard input/output library." | `import sys` (sort of) |
| `int main(void)` | "Define a function called `main` that returns an `int` and takes no arguments." | `def main():` |
| `{ ... }` | The function body — like Python's indentation, but explicit. | indented block |
| `printf("Hello, world!\n");` | Print a string. `\n` is newline. | `print("Hello, world!")` |
| `return 0;` | Exit with status 0 (success). | `return 0` |
| `;` at end of every statement | Required in C. Forget it and the compiler complains. | Python uses newlines instead. |

So C is just Python with:

- explicit types,
- braces instead of indentation,
- semicolons after statements,
- a `main()` function that's the entry point,
- a separate compile step.

That's the entire difference, syntactically. The hard parts of C are *semantic* (memory, pointers, undefined behavior). We get there in Part I.

## Variables in C

Python:

```python
name = "Sudhanshu"
age = 22
pi = 3.14159
```

C:

```c
#include <stdio.h>

int main(void)
{
    char name[] = "Sudhanshu";    // a string of characters
    int age = 22;                 // an integer
    double pi = 3.14159;          // a floating-point number

    printf("name = %s\n", name);
    printf("age = %d\n", age);
    printf("pi = %f\n", pi);

    return 0;
}
```

Differences:

- **You declare the type.** `int age = 22;` says "age is an integer." C wants this; Python figures it out.
- **Strings are not as simple.** A C string is "an array of characters." We'll see this in detail in Part I, chapter 4. For now `char name[] = "..."` works.
- **`printf` is fancier.** It uses **format specifiers**:
  - `%d` for `int`,
  - `%f` for `double` (floating point),
  - `%s` for string,
  - `%c` for a single character.
  - `\n` for newline (always remember it; `printf` doesn't auto-add).

Compile and run:

```bash
gcc vars.c -o vars
./vars
```

## Arithmetic in C

Same operators, mostly:

```c
#include <stdio.h>

int main(void)
{
    int a = 5;
    int b = 3;

    printf("a + b = %d\n", a + b);   // 8
    printf("a - b = %d\n", a - b);   // 2
    printf("a * b = %d\n", a * b);   // 15
    printf("a / b = %d\n", a / b);   // 1   ← integer division!
    printf("a %% b = %d\n", a % b);  // 2
    return 0;
}
```

Note the surprise: `5 / 3` in C is `1`, not `1.6666...`. **Integer division** drops the remainder. To get a fractional result, you need at least one of the operands to be a `double`:

```c
double a = 5;
double b = 3;
printf("%f\n", a / b);   // 1.666667
```

This is the kind of detail that bites beginners. You'll memorize it after one bug.

(`%%` in `printf` prints a literal `%`. The single `%` is for format specifiers.)

## `if` / `else` in C

Python:

```python
if age >= 18:
    print("Adult")
else:
    print("Minor")
```

C:

```c
if (age >= 18) {
    printf("Adult\n");
} else {
    printf("Minor\n");
}
```

Differences:

- **Parentheses around the condition.** `if (age >= 18)`, not `if age >= 18`.
- **Braces around blocks.** Mandatory if more than one statement (and good practice always).
- **No `elif` keyword.** You use `else if`:

```c
if (score >= 90) {
    grade = 'A';
} else if (score >= 80) {
    grade = 'B';
} else {
    grade = 'F';
}
```

- **Logical operators are symbols, not words:**

| Python | C |
|---|---|
| `and` | `&&` |
| `or` | `\|\|` |
| `not` | `!` |
| `True` | `1` (or `true` if you `#include <stdbool.h>`) |
| `False` | `0` (or `false`) |

```c
if (age >= 18 && age < 65) { ... }
if (!is_logged_in) { ... }
```

## Loops in C

Python:

```python
for i in range(5):
    print(i)
```

C:

```c
#include <stdio.h>

int main(void)
{
    for (int i = 0; i < 5; i = i + 1) {
        printf("%d\n", i);
    }
    return 0;
}
```

The C `for` loop has **three parts** in parentheses:

```
for (initializer ; condition ; update)
```

- `int i = 0` — declare and initialize.
- `i < 5` — keep looping while this is true.
- `i = i + 1` (or `i++`, same thing) — do this each iteration.

Once you see this pattern a few times it becomes natural.

C also has `while` and `do { } while`, identical to Python's `while` and a slight variant:

```c
int i = 0;
while (i < 5) {
    printf("%d\n", i);
    i++;
}
```

`i++` means "add 1 to i." Equivalent to `i = i + 1`. You'll see `++` everywhere.

## FizzBuzz, side by side

Python:

```python
for n in range(1, 21):
    if n % 15 == 0:
        print("FizzBuzz")
    elif n % 3 == 0:
        print("Fizz")
    elif n % 5 == 0:
        print("Buzz")
    else:
        print(n)
```

C:

```c
#include <stdio.h>

int main(void)
{
    for (int n = 1; n <= 20; n++) {
        if (n % 15 == 0)      printf("FizzBuzz\n");
        else if (n % 3 == 0)  printf("Fizz\n");
        else if (n % 5 == 0)  printf("Buzz\n");
        else                  printf("%d\n", n);
    }
    return 0;
}
```

Compile and run:

```bash
gcc fizzbuzz.c -o fizzbuzz -Wall -Wextra
./fizzbuzz
```

`-Wall -Wextra` means "turn on all warnings." Always use them. The compiler will tell you about likely bugs.

## Functions in C

Python:

```python
def add(a, b):
    return a + b

print(add(2, 3))
```

C:

```c
#include <stdio.h>

int add(int a, int b)
{
    return a + b;
}

int main(void)
{
    printf("%d\n", add(2, 3));
    return 0;
}
```

The function declaration is more explicit:

- `int add(int a, int b)` — "function `add` that returns `int`, takes two `int` parameters."
- The function definition must be **above** any code that calls it (or you must declare a **prototype** at the top).

Functions in C are pure recipe: type in, type out. No magic.

### Multiple-function example

```c
#include <stdio.h>

int is_even(int n)
{
    return n % 2 == 0;     // returns 1 (true) or 0 (false)
}

int sum_evens_up_to(int N)
{
    int total = 0;
    for (int i = 1; i <= N; i++) {
        if (is_even(i)) {
            total = total + i;
        }
    }
    return total;
}

int main(void)
{
    printf("%d\n", sum_evens_up_to(10));    // 30
    printf("%d\n", sum_evens_up_to(100));   // 2550
    return 0;
}
```

Same shape as Python. The skeleton transfers directly.

## Reading input

C's input is uglier. Don't worry about it for now; we'll do it properly in Part I. For chapters where you need it, you can use:

```c
int n;
printf("Enter a number: ");
scanf("%d", &n);
printf("You entered %d.\n", n);
```

The `&n` is a *pointer* to `n` — `scanf` writes the value back into the variable through the pointer. We'll explain pointers in Part I, chapter 4. For now, just type it.

## The compile-edit-run loop

In Python:

```bash
nano hello.py
python3 hello.py
```

In C:

```bash
nano hello.c
gcc hello.c -o hello -Wall -Wextra
./hello
```

You add a `gcc` step. The compiler will catch many mistakes for you.

A typical session looks like:

```
$ gcc hello.c -o hello -Wall -Wextra
hello.c: In function 'main':
hello.c:5:5: error: 'pirntf' undeclared (first use in this function); did you mean 'printf'?
    5 |     pirntf("hello\n");
      |     ^~~~~~
      |     printf
```

Read the error: file `hello.c`, line 5, you typed `pirntf` instead of `printf`. Fix and rebuild.

This loop — write, compile, fix errors, rebuild — is the heartbeat of C development. Get used to it.

## Try it (30 minutes)

Build the C version of your Python `temperature_v2.py` (the menu-driven temperature converter from chapter 8).

Skeleton:

```c
#include <stdio.h>

double f_to_c(double f)
{
    return (f - 32.0) * 5.0 / 9.0;
}

double c_to_f(double c)
{
    return c * 9.0 / 5.0 + 32.0;
}

int main(void)
{
    while (1) {
        printf("\n1. F -> C   2. C -> F   3. quit\nchoice: ");
        int choice;
        scanf("%d", &choice);

        if (choice == 3) break;

        double t;
        printf("temperature: ");
        scanf("%lf", &t);

        if (choice == 1) {
            printf("%f F = %f C\n", t, f_to_c(t));
        } else if (choice == 2) {
            printf("%f C = %f F\n", t, c_to_f(t));
        } else {
            printf("invalid choice\n");
        }
    }
    return 0;
}
```

Build and run. Type some inputs.

(`scanf("%lf", ...)` reads a `double`. The `l` is for "long.")

## Differences you must internalize

| Concept | Python | C | Why C is "harder" |
|---|---|---|---|
| Variables | `x = 5` | `int x = 5;` | You must say the type. |
| Strings | `"abc"` is a string | `"abc"` is an array of `char` ending with `\0` | Manipulating strings requires care. |
| Memory | automatic | manual (`malloc`/`free`) | Power and footguns both. |
| Errors | runtime, friendly | compile-time mostly, less friendly | But faster and gives more control. |
| Build step | none | `gcc` | One more thing to learn. |
| Speed | slow | fast | Why kernels are in C. |

C is harder because the *machine model is exposed*. That's also why it's the language of operating systems, drivers, and game engines. You need control over every byte. Python hides all that and trades it for speed.

You're now allowed to feel like C is annoying. Push through. By the end of Part I it'll feel natural.

## Self-check

- What's the C equivalent of Python's `if x: print("yes")`?
- Why does `5/3` give `1` in C but `1.6666...` in Python?
- What does `gcc hello.c -o hello -Wall` do, in your own words?
- Why does C need braces and Python doesn't?

You can now read C. You can now compile C. You're ready for Part I.

But first: one more chapter. The most important one in this part.

➡️ Next: [Chapter 10 — When things break](10-when-things-break.md)
