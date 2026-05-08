# A3 — Boolean logic: AND, OR, NOT, XOR

These four operations are the alphabet of digital hardware. Inside a CPU or a GPU, billions of transistors are arranged into networks of these four. **Everything else is built from them.**

## The four operations

### NOT (also written `!`, `~`, ¬)

Flip a bit.

| input | NOT input |
|---|---|
| 0 | 1 |
| 1 | 0 |

```
~0b0011 = 0b1100
```

C has both `!` (logical, gives 1 or 0) and `~` (bitwise, flips every bit). Important difference:

```c
!0xF0     // 0   (because 0xF0 is nonzero, "true", so NOT it is "false")
~0xF0     // 0x...0F (every bit flipped)
```

### AND (also written `&`, `&&`, ∧)

True only if **both** are true. Like multiplication of bits.

| A | B | A AND B |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 0 |
| 1 | 0 | 0 |
| 1 | 1 | 1 |

```
0b1100
0b1010
------
0b1000     ← bit-by-bit AND
```

Two C operators: `&` (bitwise) and `&&` (logical, short-circuits).

### OR (also written `|`, `||`, ∨)

True if **either** is true.

| A | B | A OR B |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 1 |

```
0b1100
0b1010
------
0b1110
```

C: `|` (bitwise) and `||` (logical, short-circuits).

### XOR (exclusive OR, also `^`, ⊕)

True if **exactly one** is true. Equivalent to "they're different."

| A | B | A XOR B |
|---|---|---|
| 0 | 0 | 0 |
| 0 | 1 | 1 |
| 1 | 0 | 1 |
| 1 | 1 | 0 |

```
0b1100
0b1010
------
0b0110     ← differs in 2nd-from-left and rightmost
```

C: `^` (bitwise). No logical XOR keyword (you write `(a && !b) || (!a && b)` or just use `!=`).

XOR has a magic property: `a ^ a = 0`, and `a ^ 0 = a`. This is why XOR is used in hashes, error correction, and the classic "swap without a temp variable" trick:

```c
a = a ^ b;
b = a ^ b;     // now b = original a
a = a ^ b;     // now a = original b
```

(Don't actually use this in real code; the compiler does better with a temp.)

## Logical vs bitwise (the most common beginner trap)

```c
int  flags = 0xC;       // 0b1100
int  mask  = 0x9;       // 0b1001

flags & mask     // 0b1000 = 0x8 (bitwise AND each pair)
flags && mask    // 1 (both nonzero, so logically true)

flags | mask     // 0b1101 = 0xD
flags || mask    // 1
```

Rule: `&` and `|` operate on every bit individually. `&&` and `||` operate on the value as a whole and return either 0 or 1.

In `if`s and conditions, almost always `&&` and `||`. In bit-twiddling, almost always `&` and `|`. Don't mix them up.

## Truth tables tell you everything

Anything you can describe with a truth table can be built from AND/OR/NOT. That's mathematically proven.

For example, "true if exactly two of A, B, C are true":

| A | B | C | result |
|---|---|---|---|
| 0 | 0 | 0 | 0 |
| 0 | 0 | 1 | 0 |
| 0 | 1 | 0 | 0 |
| 0 | 1 | 1 | 1 |
| 1 | 0 | 0 | 0 |
| 1 | 0 | 1 | 1 |
| 1 | 1 | 0 | 1 |
| 1 | 1 | 1 | 0 |

You can mechanically translate that into a Boolean formula. The hardware engineers who design GPUs do exactly this for every signal.

## Why this matters for software

You're writing software, not hardware, but the same operators show up because they're efficient on a CPU.

### Flag fields

A driver often packs many true/false properties into one integer:

```c
#define IRQ_DISABLED   (1u << 0)   // 0x01
#define DMA_READY      (1u << 1)   // 0x02
#define ERROR          (1u << 2)   // 0x04
#define HOTPLUG        (1u << 3)   // 0x08

uint32_t status = 0;

status |= DMA_READY;                    // mark DMA ready
status |= ERROR | HOTPLUG;              // OR in two at once
if (status & ERROR) { /* handle */ }
status &= ~ERROR;                       // clear error
```

This pattern is in every kernel driver.

### Masks

To extract bits 4..7 of a register:

```c
uint32_t reg  = readl(REG_X);
uint32_t mask = 0xF0;                    // bits 4..7
uint32_t val  = (reg & mask) >> 4;
```

### Boolean math identities (memorize)

```
A AND 0 = 0          A OR 0 = A          A XOR 0 = A
A AND 1 = A          A OR 1 = 1          A XOR 1 = NOT A
A AND A = A          A OR A = A          A XOR A = 0
NOT NOT A = A
NOT(A AND B) = NOT A OR NOT B            ← De Morgan
NOT(A OR B)  = NOT A AND NOT B           ← De Morgan
```

De Morgan is the trick to swap AND/OR through a NOT. Useful when refactoring conditions.

## Hardware view (one paragraph, then we move on)

A "transistor" is a tiny switch controlled by an electrical voltage. A few transistors arranged make a "gate" — AND gate, OR gate, NOT gate (called an inverter). Wire millions of gates together → flip-flops, registers, adders, multipliers. Wire billions of gates → a GPU. The whole pyramid stands on AND/OR/NOT.

Knowing this is why senior engineers can intuit *why* certain CPU instructions are cheap and certain ones are expensive: the cheap ones map to a few gate-delays; the expensive ones (like float divide) require many cascaded blocks.

## Try it

1. What is `0b1100 ^ 0b1010` in binary, in hex, in decimal?
2. What is `~0xFF` interpreted as a 32-bit unsigned int? (Answer: `0xFFFFFF00`.)
3. Write a one-liner: clear the bottom 12 bits of `x`. (Answer: `x &= ~0xFFFu;` — clears bits 0..11.)
4. Why is `x % 16` the same as `x & 0xF`? (Answer: 16 = 2⁴, so `% 16` keeps just the bottom 4 bits, which is what `& 0xF` does. Faster on hardware.)
5. Translate "the device is ready AND not in error mode" into C, given `status = uint32_t`, `DMA_READY = bit 1`, `ERROR = bit 2`. (Answer: `if ((status & DMA_READY) && !(status & ERROR))`.)

If 4 of 5 are right, you have boolean logic. Move on.
