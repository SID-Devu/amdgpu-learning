# Chapter 7 — Decisions and repeats

A program with only assignments and prints is boring. The two ideas that make programs powerful are:

1. **Decisions** — do this if some condition is true, otherwise do something else.
2. **Repeats** — do something many times.

That's it. Every program in the world, including a 100,000-line GPU driver, is built from these two ideas.

## Decisions: `if` / `elif` / `else`

```python
age = int(input("Age: "))

if age >= 18:
    print("You can vote.")
else:
    print("Too young to vote.")
```

Read it like English. `if some condition, do block; else, do block.`

Notice **two** important Python rules:

1. **The colon `:`** at the end of `if` and `else`.
2. **Indentation** — the lines inside the block are indented (4 spaces by convention). Python uses indentation to know what's "inside" the `if`.

If you indent wrong, Python complains:

```
IndentationError: expected an indented block
```

This is unique to Python (C uses braces). Don't worry, in C you'll write `{` and `}` and the indentation is just for humans.

### Multiple branches with `elif`

```python
score = int(input("Score (0-100): "))

if score >= 90:
    grade = "A"
elif score >= 80:
    grade = "B"
elif score >= 70:
    grade = "C"
elif score >= 60:
    grade = "D"
else:
    grade = "F"

print(f"Grade: {grade}")
```

`elif` = "else if." Python checks each in order; first one that's true wins.

### Comparisons

```python
==     equal to       (note: TWO equals; one equals is assignment!)
!=     not equal to
<      less than
<=     less than or equal
>      greater than
>=     greater than or equal
```

This is the most common beginner bug:

```python
if x = 5:           # WRONG — this is assignment, syntax error
    ...

if x == 5:          # right
    ...
```

### Combining: `and`, `or`, `not`

```python
if age >= 18 and age < 65:
    print("Working age.")

if grade == "A" or grade == "B":
    print("Good job.")

if not is_logged_in:
    print("Please log in.")
```

These are spelled out as words in Python. In C you'll see `&&`, `||`, `!`. Same idea.

### Boolean values

```python
is_done = True
is_failed = False

if is_done and not is_failed:
    print("Success!")
```

`True` and `False` (capitalized) are the two boolean values.

## Repeats: `while`

`while` keeps doing something *as long as* a condition is true.

```python
count = 1
while count <= 5:
    print(count)
    count = count + 1

print("done")
```

Output:

```
1
2
3
4
5
done
```

Walk through the logic in your head:

- `count` starts at 1.
- Is `1 <= 5`? Yes. Print 1. Then `count = 1 + 1 = 2`.
- Is `2 <= 5`? Yes. Print 2. `count = 3`.
- ... and so on.
- Is `6 <= 5`? No. Stop the loop. Print "done."

The single biggest danger of `while`: **infinite loops**. If you forget to update the condition variable:

```python
count = 1
while count <= 5:
    print(count)
    # forgot count = count + 1 !
```

This prints `1` forever. **Press `Ctrl+C` to stop.** This is what `Ctrl+C` is for.

## Repeats: `for`

`for` is for going through a sequence (a list, a range of numbers, the lines of a file).

### Range of numbers

```python
for i in range(5):
    print(i)
```

Prints `0, 1, 2, 3, 4`. (Note: 0 to 4, not 1 to 5. `range(5)` makes 5 numbers starting at 0.)

```python
for i in range(1, 6):
    print(i)        # 1, 2, 3, 4, 5

for i in range(0, 10, 2):
    print(i)        # 0, 2, 4, 6, 8 (step 2)
```

### Going through a list

```python
fruits = ["apple", "banana", "cherry"]
for fruit in fruits:
    print(f"I like {fruit}.")
```

Output:

```
I like apple.
I like banana.
I like cherry.
```

`fruit` is a fresh variable each time, taking the next value from the list.

### `for` is just convenient `while`

You could write the same logic with `while`:

```python
fruits = ["apple", "banana", "cherry"]
i = 0
while i < len(fruits):
    print(f"I like {fruits[i]}.")
    i = i + 1
```

Both are valid. `for ... in` is shorter and more readable, so prefer it.

## `break` and `continue`

Sometimes you want to **stop a loop early** or **skip an iteration**.

```python
# print numbers 1..10, but stop at 7
for n in range(1, 11):
    if n == 7:
        break              # exit the loop entirely
    print(n)
```

```python
# print only odd numbers in 1..10
for n in range(1, 11):
    if n % 2 == 0:
        continue           # skip the rest of this iteration
    print(n)
```

Use them when they make the code clearer; not as a habit.

## A real-feeling example: the FizzBuzz

This is the classic interview warm-up. Write a program that prints numbers 1 to 20, but:

- if the number is divisible by 3, print "Fizz" instead.
- if it's divisible by 5, print "Buzz".
- if it's divisible by both, print "FizzBuzz".

Save as `~/ldd-textbook/hello-py/fizzbuzz.py`:

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

Run it. Expected output starts:

```
1
2
Fizz
4
Buzz
Fizz
7
...
```

Why `n % 15` first? Because 15 is `3 × 5` — if it's divisible by 15 it's divisible by both 3 and 5. If we checked `n % 3` first, "FizzBuzz" inputs would already match "Fizz" and we'd never reach the both-case.

This kind of ordering thinking is what programming is really about.

## Combining decisions and repeats: number guessing

```python
import random            # standard library: random numbers

secret = random.randint(1, 100)
guesses = 0

print("I'm thinking of a number from 1 to 100.")

while True:                          # loop forever (we'll break out)
    guess = int(input("Your guess: "))
    guesses = guesses + 1

    if guess < secret:
        print("Too low.")
    elif guess > secret:
        print("Too high.")
    else:
        print(f"Correct! It took you {guesses} guesses.")
        break
```

Save as `guess.py` and play it. This is your first program with real interactivity.

Notes:
- `import random` brings in a built-in module of utilities.
- `random.randint(1, 100)` gives a random integer 1..100 inclusive.
- `while True:` loops forever; the only way out is `break`.

## Try it (20 minutes)

Write `~/ldd-textbook/hello-py/sum-evens.py` that:

1. Asks the user for a number `N`.
2. Computes the sum of all even numbers from 1 to N (inclusive).
3. Prints the result.

Expected:

```
N: 10
Sum of evens 1..10 = 30
```

(2 + 4 + 6 + 8 + 10 = 30.)

Two ways to write it:

- Using `for` and `if`.
- Using `for` with `range(2, N+1, 2)` (cleaner — no `if` needed).

Try both, see which you find more readable.

## Common beginner mistakes

- **Using `=` instead of `==`** in conditions. Will refuse to run.
- **Forgetting the colon `:`** at the end of `if`, `elif`, `else`, `while`, `for`. Python will say `SyntaxError: expected ':'`.
- **Wrong indentation.** Mix of tabs and spaces is a classic. Pick one (4 spaces) and stick to it. Most editors handle this for you.
- **Off-by-one errors.** `range(5)` is 0..4, not 1..5. `range(1, 5)` is 1..4, not 1..5. To get 1..5 use `range(1, 6)`. You will hit this constantly. It's normal.

## Self-check

- What's the difference between `=` and `==`?
- What's an infinite loop, and how do you stop one that's running?
- Why is the order of `elif` checks important in FizzBuzz?
- How do you read each line of a list in a `for` loop?

You can now write programs that decide and repeat. That's most of programming. Tomorrow: how to package code so you don't repeat yourself.

➡️ Next: [Chapter 8 — Functions](08-functions.md)
