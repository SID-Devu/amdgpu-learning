# Chapter 6 — Your first program (in Python)

Today we learn what code can actually *do*. We'll use Python as training wheels because it's friendly. Don't worry — Part I onwards is all C.

Make sure you've done the "Try it" from chapter 5 (`hello.py` working). Now we'll grow it.

## Three things every program does

1. **Stores values** (variables).
2. **Computes** with them (arithmetic, text manipulation).
3. **Prints** results (or sends them somewhere).

That's the whole game. Decisions and loops (next chapter) are added on top.

## Variables

A **variable** is a name for a value. In Python:

```python
name = "Sudhanshu"
age = 22
pi = 3.14159
```

You read this as "assign the value `"Sudhanshu"` to the name `name`." Now whenever you use `name`, Python substitutes the value.

```python
print(name)
print(age)
print(pi)
```

Save as `vars.py` and run with `python3 vars.py`:

```
Sudhanshu
22
3.14159
```

You can change a variable later:

```python
age = 23
print(age)         # now prints 23
```

The old value is gone. Variables are not permanent — they hold whatever you last assigned.

## Types of values

Python has a few basic kinds of values:

| Type | Example | Description |
|---|---|---|
| **integer** (`int`) | `42`, `-7`, `0` | whole numbers |
| **float** (`float`) | `3.14`, `-0.5`, `2.0` | numbers with decimals |
| **string** (`str`) | `"hello"`, `"abc"` | text, in quotes |
| **boolean** (`bool`) | `True`, `False` | yes/no, on/off |

You don't declare types in Python. The value's type is whatever it is.

## Arithmetic

Python knows math:

```python
a = 5
b = 3
print(a + b)        # 8
print(a - b)        # 2
print(a * b)        # 15
print(a / b)        # 1.6666...
print(a // b)       # 1     (integer division)
print(a % b)        # 2     (remainder)
print(a ** b)       # 125   (a to the power b)
```

`%` is called **modulo** or "remainder." `5 % 3` is `2` because 5 ÷ 3 is 1, remainder 2. You will see `%` constantly in real code (especially for ring buffers — already a hint of Part VI).

## Strings

Strings are text. They live in quotes:

```python
greeting = "hello"
name = "Sudhanshu"
message = greeting + ", " + name + "!"
print(message)        # hello, Sudhanshu!
```

`+` on strings means **concatenate** (glue together).

A friendlier way is **f-strings**:

```python
print(f"hello, {name}!")
```

The `f` before the `"` lets you embed variables in `{...}`. This is what you'll use in real life.

## Reading input

Programs are more interesting when they accept input from a human:

```python
name = input("What is your name? ")
print(f"Nice to meet you, {name}!")
```

`input("prompt: ")` displays the prompt, waits for the user to type, and returns whatever they typed *as a string*.

If you want a number, you have to convert:

```python
age_str = input("How old are you? ")
age = int(age_str)              # convert string to integer
print(f"In 10 years you will be {age + 10}.")
```

If the user types "twenty", `int(...)` will crash because "twenty" is not a number-looking string. We'll handle errors in chapter 10.

## Comments

A line starting with `#` is a **comment**. Python ignores it. Comments are for humans:

```python
# this calculates the area of a circle
radius = 5
area = 3.14159 * radius * radius
print(area)
```

Use comments to explain **why** code is doing something non-obvious, not **what** it does. (Good code should be readable on its own.)

## Putting it together

Type this exactly into `~/ldd-textbook/hello-py/temperature.py`:

```python
# convert Fahrenheit to Celsius
print("Fahrenheit to Celsius converter")
print("--------------------------------")

f_str = input("Enter a temperature in F: ")
f = float(f_str)

c = (f - 32) * 5 / 9

print(f"{f} F is {c} C")
```

Run it:

```bash
python3 temperature.py
```

Try a few inputs: `32` (should give 0), `212` (should give 100), `98.6` (body temperature, should give 37).

That's a useful program. You are no longer a beginner-beginner.

## Lists (a peek)

A list holds multiple values:

```python
numbers = [10, 20, 30, 40]
print(numbers[0])        # 10  (first element, indexed from 0!)
print(numbers[3])        # 40
print(len(numbers))      # 4

numbers.append(50)
print(numbers)           # [10, 20, 30, 40, 50]
```

Indexes start at **0**, not 1. This is the universal convention in programming. The first element is index 0. Get used to it now.

We'll use lists in the next chapter for loops.

## Try it (15 minutes)

Write a program `~/ldd-textbook/hello-py/calc.py` that:

1. Prints a banner: `My tiny calculator`.
2. Asks for two numbers.
3. Prints their sum, difference, product, and quotient.

Expected run:

```
My tiny calculator
First number: 6
Second number: 4
6 + 4 = 10
6 - 4 = 2
6 * 4 = 24
6 / 4 = 1.5
```

Hints:
- Use `input(...)` and `float(...)` to read numbers.
- Use f-strings to format output.

If your code prints `10.0` instead of `10`, that's because `float` keeps the decimal. Don't worry about it.

## Common beginner mistakes (and how to fix)

**`SyntaxError: invalid syntax`** — usually means you have a typo (missing quote, missing parenthesis). Look at the line number Python prints. The error is on or just before that line.

**`NameError: name 'foo' is not defined`** — you used a variable before assigning to it. Or you misspelled it (`Print` instead of `print`).

**`TypeError: can only concatenate str (not "int") to str`** — you tried to do `"hello" + 5`. Strings and numbers don't add. Use `str(5)` to convert: `"hello" + str(5)`.

**`ValueError: invalid literal for int() with base 10: 'abc'`** — you tried `int("abc")` and "abc" isn't a number. Validate input or accept floats.

When you see one of these, stop, read the line number, look at that exact line. The fix is almost always 1 character.

## Self-check

- What does `=` mean in `age = 23`? (Answer: assign, not "equals.")
- Why does `int("hello")` crash but `int("42")` doesn't?
- Why are list indexes from 0 and not 1? (You don't have to know the historical answer — just memorize "from 0.")
- What's the difference between `5 / 2` and `5 // 2` in Python?

You can compute. Tomorrow: making the program decide and repeat.

➡️ Next: [Chapter 7 — Decisions and repeats](07-decisions-and-repeats.md)
