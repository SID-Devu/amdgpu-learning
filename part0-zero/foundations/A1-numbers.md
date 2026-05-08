# A1 — Numbers: binary, hex, decimal

You will see numbers like `0xDEADBEEF`, `0b10110100`, `1 << 30` everywhere in driver code. They're not strange — they're just **the same number written in a different base**.

## How counting works (you already know this)

When you write `347` you mean:

```
3 × 100 + 4 × 10 + 7 × 1
3 × 10²  + 4 × 10¹ + 7 × 10⁰
```

We use **base 10** because we have 10 fingers. Each digit can be 0–9. The position of a digit multiplies its value by a power of 10.

That's a *choice*, not a law. Computers chose differently.

## Binary — base 2

A bit can be 0 or 1. Stack bits in positions, like decimal but with powers of 2:

```
     1   0   1   1   0   1   0   0     ← bits
   128  64  32  16   8   4   2   1     ← place values

= 128 + 32 + 16 + 4 = 180
```

Conventions:

- Written `0b10110100` in C/Python (the `0b` prefix says "binary").
- Bit positions are counted from the right, starting at **bit 0**: the rightmost bit is bit 0, the next bit 1, etc.

Why binary? Because hardware is a bunch of switches. A switch is on or off. 1 or 0. That's it.

## Hex — base 16

Binary is annoying to read (`0b1011010001110100`...). Hex is denser. One hex digit = exactly **4 bits**:

| Hex | Decimal | Binary |
|---|---|---|
| `0` | 0 | `0000` |
| `1` | 1 | `0001` |
| `2` | 2 | `0010` |
| `3` | 3 | `0011` |
| `4` | 4 | `0100` |
| `5` | 5 | `0101` |
| `6` | 6 | `0110` |
| `7` | 7 | `0111` |
| `8` | 8 | `1000` |
| `9` | 9 | `1001` |
| `A` | 10 | `1010` |
| `B` | 11 | `1011` |
| `C` | 12 | `1100` |
| `D` | 13 | `1101` |
| `E` | 14 | `1110` |
| `F` | 15 | `1111` |

Memorize this table. Right now. It will save you 1000 hours over your career.

A hex number is written with `0x` prefix:

```
0xCAFE = C    A    F    E
       = 1100 1010 1111 1110     ← 16 bits
       = 51966 in decimal
```

You read hex by chunking into 4-bit pieces. `0xDEADBEEF` is 8 hex digits = 32 bits = 4 bytes. Common GPU register addresses look exactly like this.

## Why both?

| Base | Where you see it |
|---|---|
| Decimal | Sizes ("buffer is 4096 bytes"), counts ("3 wavefronts"), array indices |
| Hex | Addresses (`0xFFFF8000`), bit masks (`0x000000FF`), magic constants |
| Binary | Reading individual bit fields in registers (rare; usually still hex) |

## Converting in your head

### Decimal → binary

Keep dividing by 2, write down the remainders **bottom up**:

```
13 / 2 = 6  rem 1
 6 / 2 = 3  rem 0
 3 / 2 = 1  rem 1
 1 / 2 = 0  rem 1
```

Read remainders from bottom: `1101` = 13. Check: 8 + 4 + 1 = 13. ✓

### Decimal → hex

Same trick with 16:

```
255 / 16 = 15  rem 15 → F
 15 / 16 = 0   rem 15 → F
```

So 255 = `0xFF`.

### Binary → hex

Group bits into chunks of 4, **starting from the right**, then look up each chunk:

```
0b 1011 0100   →  B    4   →  0xB4
```

If the bit count isn't a multiple of 4, pad with leading zeros: `0b101 → 0b0101 → 0x5`.

### Hex → binary

Each hex digit becomes 4 bits:

```
0xCAFE → 1100 1010 1111 1110
```

That's it. Hex ↔ binary is mechanical.

## Powers of 2 — you must know these cold

| Power | Decimal | Hex |
|---|---|---|
| 2⁰ | 1 | 0x1 |
| 2¹ | 2 | 0x2 |
| 2² | 4 | 0x4 |
| 2³ | 8 | 0x8 |
| 2⁴ | 16 | 0x10 |
| 2⁵ | 32 | 0x20 |
| 2⁶ | 64 | 0x40 |
| 2⁷ | 128 | 0x80 |
| 2⁸ | 256 | 0x100 |
| 2⁹ | 512 | 0x200 |
| 2¹⁰ | 1024 | 0x400 |
| 2¹² | 4096 | 0x1000 ← page size |
| 2¹⁶ | 65,536 | 0x10000 |
| 2²⁰ | ~1 million | 0x100000 ← 1 MiB |
| 2³⁰ | ~1 billion | 0x40000000 ← 1 GiB |
| 2³² | ~4.3 billion | 0x100000000 ← 32-bit limit |
| 2⁶⁴ | huge | (16 hex digits) ← 64-bit limit |

**Page size = 4096 = 0x1000 = 2¹².** This will appear so many times it'll burn into your brain.

## Quick conversions to memorize

```
0x10 = 16
0x20 = 32
0x40 = 64
0x100 = 256
0x1000 = 4096           ← page
0xFF = 255              ← max byte
0xFFFF = 65535          ← max 16-bit
0xFFFFFFFF = 4294967295 ← max 32-bit
```

## Negative numbers — two's complement (just FYI)

Computers represent `-1` in 32-bit as `0xFFFFFFFF`. This is "two's complement." You don't need to internalize it now; just know:

- All bits 1 = -1.
- High bit 1 = negative (for signed types).
- Add 1 to `0xFFFFFFFF`, you get `0x00000000` (it overflows, like an odometer).

## A real-world example

In `amdgpu` you'll see code like:

```c
#define AMDGPU_FENCE_OWNER_UNDEFINED   ((void *)0ul)
#define AMDGPU_GEM_DOMAIN_VRAM         (1 << 2)   /* = 4 */
#define AMDGPU_GEM_DOMAIN_GTT          (1 << 1)   /* = 2 */
#define AMDGPU_GEM_DOMAIN_CPU          (1 << 0)   /* = 1 */
```

`(1 << 2)` is the same as `0b100` is the same as `0x4` is the same as `4`. They're using bits as flags: "VRAM" = bit 2, "GTT" = bit 1, "CPU" = bit 0. You can OR them: `VRAM | GTT = 0b110 = 0x6 = 6`.

You will see this pattern everywhere. The whole binary/hex apparatus exists to make it readable.

## Try it

Convert without a calculator:

1. `0x40` in decimal? (Answer: 64)
2. `170` in hex? (Answer: 0xAA)
3. `0b11010110` in hex? (Hint: split as `1101 0110`. Answer: 0xD6)
4. What's `1 << 12`? (Answer: 4096 = 0x1000 = page size)
5. What's `0xFF & 0x0F`? (Hint: AND of bits — see chapter A3. Answer: 0x0F = 15)

If you got 4 of 5, you're solid. Move on.
