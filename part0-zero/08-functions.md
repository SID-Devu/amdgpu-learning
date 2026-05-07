# Chapter 8 — Functions

You've now seen variables, decisions, and loops. With those alone you could write programs forever. But you'd repeat yourself a lot.

A **function** is a *named* piece of code that you can call by its name. It's the single most important tool for keeping your code clean.

## The motivation

Imagine you want to print a friendly greeting in 5 places:

```python
print("=" * 30)
print("   Welcome to the program  ")
print("=" * 30)
```

If you copy-paste this 5 times and later want to change the wording, you'd have to find and edit every copy. Mistakes happen. Functions fix this.

## Defining a function

```python
def banner():
    print("=" * 30)
    print("   Welcome to the program  ")
    print("=" * 30)
```

Read it: `def` (define) `banner` (the name) `()` (no inputs) `:`. Then the body, indented.

Now to use it:

```python
banner()                # call the function

print("here is some content")

banner()                # call again
```

Output:

```
==============================
   Welcome to the program  
==============================
here is some content
==============================
   Welcome to the program  
==============================
```

You wrote the code *once*, used it *twice*. Change it in one place, both calls update.

## Arguments (input)

Functions are more useful when they take **arguments**:

```python
def banner(message):
    print("=" * 30)
    print("   " + message + "   ")
    print("=" * 30)

banner("Hello, world!")
banner("Welcome back")
```

Output:

```
==============================
   Hello, world!   
==============================
==============================
   Welcome back   
==============================
```

`message` is a **parameter** — a placeholder name for whatever the caller passes in.

You can have multiple parameters:

```python
def add(a, b):
    print(a + b)

add(2, 3)         # 5
add(10, 20)       # 30
```

## Return values (output)

Functions can also **return** a value, so the caller can use it:

```python
def add(a, b):
    return a + b

result = add(2, 3)
print(result)             # 5
print(add(10, 20))        # 30
print(add(add(1, 2), 4))  # 7
```

`return` exits the function and gives back the value.

A function without `return` returns a special value `None`, which means "nothing here."

## A useful example

```python
def is_even(n):
    return n % 2 == 0

def sum_evens_up_to(N):
    total = 0
    for i in range(1, N + 1):
        if is_even(i):
            total = total + i
    return total

print(sum_evens_up_to(10))      # 30
print(sum_evens_up_to(100))     # 2550
```

Notice we built `sum_evens_up_to` *out of* `is_even`. This is the whole point: small functions combine into bigger ones.

## Why functions matter (for real)

Every Linux kernel driver function looks like this:

```c
static int my_driver_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    /* code */
    return 0;
}
```

Same idea — a function with parameters and a return value. The Linux kernel is **6 million lines** of mostly small functions calling each other. If you understand how functions work, you understand 80% of the structure.

## Variable scope

A variable created **inside** a function only exists there:

```python
def foo():
    secret = 42
    print(secret)

foo()
print(secret)        # NameError: 'secret' is not defined
```

`secret` lives only inside `foo`. Once the function returns, it's gone. This is called **local scope**.

A variable **outside** any function is **global**:

```python
counter = 0           # global

def bump():
    global counter    # tell Python "I want the global, not a new local"
    counter = counter + 1

bump()
bump()
bump()
print(counter)        # 3
```

You usually want to avoid globals — they make code harder to reason about. Pass things in as arguments and return values out instead.

## Default arguments

You can provide defaults so the caller doesn't have to specify everything:

```python
def greet(name, greeting="Hello"):
    print(f"{greeting}, {name}!")

greet("Sudhanshu")               # Hello, Sudhanshu!
greet("Sudhanshu", "Hi there")   # Hi there, Sudhanshu!
```

## Putting it together

```python
def banner(text):
    line = "=" * (len(text) + 4)
    print(line)
    print(f"  {text}  ")
    print(line)

def is_prime(n):
    if n < 2:
        return False
    for d in range(2, n):
        if n % d == 0:
            return False
    return True

def primes_up_to(N):
    result = []
    for n in range(2, N + 1):
        if is_prime(n):
            result.append(n)
    return result

banner("Primes up to 30")
print(primes_up_to(30))
```

Save as `~/ldd-textbook/hello-py/primes.py` and run.

Output:

```
=========================
  Primes up to 30  
=========================
[2, 3, 5, 7, 11, 13, 17, 19, 23, 29]
```

Three small functions, each does one thing, the last one composes them.

## What makes a good function

1. **One job.** If the name has "and" in it, it should probably be two functions.
2. **Short.** A function that doesn't fit on your screen is too long. Aim for under 30 lines.
3. **Honest name.** `add(a, b)` should add. Not subtract. Not print.
4. **Return, don't print.** Whenever possible, *return* the answer; let the caller decide whether to print, save to a file, or something else. Mixing computation and printing makes code less reusable.

Same principles apply to C functions in the kernel. Keep them small, focused, well-named.

## Try it (20 minutes)

Write `~/ldd-textbook/hello-py/temperature_v2.py` that has two functions:

```python
def f_to_c(f):
    # convert Fahrenheit to Celsius and return
    ...

def c_to_f(c):
    # convert Celsius to Fahrenheit and return
    ...
```

Then a small program that loops asking the user:

```
What do you want?
  1. F → C
  2. C → F
  3. quit
Choice:
```

and uses the right function.

Hint: outer `while True:` loop, `if/elif/else` inside, `break` on choice 3.

## Common mistakes

- **Calling a function without parentheses.** `banner` (no parens) is the function itself, not a call. `banner()` calls it. Beginners often write `banner` and wonder why nothing happens.
- **Forgetting `return`.** A function with no `return` returns `None`. If you do `x = compute()` and `compute` doesn't return, `x` will be `None` and later code will crash with weird errors.
- **Modifying globals.** Don't reach out of a function to change variables defined elsewhere. Pass parameters in, return values out.
- **Reusing parameter names.** Inside `def foo(x):`, the local `x` shadows any global `x`. Usually fine, but if you wanted the global one, you have a bug.

## Self-check

- What's the difference between defining a function and calling it?
- What's the difference between an argument and a parameter? (Trivia: parameter = the name in the function definition; argument = the value passed at call time. Most people use the words interchangeably; nobody will judge you.)
- Why is "one function, one job" a good rule?
- What does a function return if you don't write `return`?

You can now write reusable code. That's a real skill. Tomorrow we move from Python to C — the language you'll actually be writing for the rest of this book.

➡️ Next: [Chapter 9 — From Python to C](09-from-python-to-c.md)
