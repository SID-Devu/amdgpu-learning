# Chapter 6 — Bit manipulation: the language of hardware

Hardware speaks in bits. A control register is a packed array of 1-bit and N-bit fields. To configure a GPU display engine you set bit 12 of register `0x6118`. To write a kernel driver, bit operations must be as natural to you as addition.

## 6.1 The six basic operators

```c
a & b     /* AND */
a | b     /* OR  */
a ^ b     /* XOR */
~a        /* NOT */
a << n    /* shift left  by n bits */
a >> n    /* shift right by n bits */
```

Truth table for two bits `x`, `y`:

| x | y | x&y | x\|y | x^y | ~x |
|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | 0 | 1 |
| 0 | 1 | 0 | 1 | 1 | 1 |
| 1 | 0 | 0 | 1 | 1 | 0 |
| 1 | 1 | 1 | 1 | 0 | 0 |

A few rules to memorize:

- `x | (1 << n)` — set bit `n`.
- `x & ~(1 << n)` — clear bit `n`.
- `x ^ (1 << n)` — toggle bit `n`.
- `(x >> n) & 1` — read bit `n`.

Watch out: shifting by ≥ width is UB. `int x; x << 32;` is UB on 32-bit `int`. Always check.

## 6.2 The Linux kernel idioms

`<linux/bits.h>` provides:

```c
#define BIT(nr)               (1UL << (nr))
#define BIT_ULL(nr)           (1ULL << (nr))
#define GENMASK(h, l)         (((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG-1-(h))))
```

So `BIT(3)` is `0x8`, and `GENMASK(15, 8)` is `0xFF00` — the mask for bits 8 through 15 inclusive.

Setting / clearing fields:

```c
#define FIELD_SET(reg, mask, shift, val) \
    (((reg) & ~(mask)) | (((val) << (shift)) & (mask)))

u32 v = readl(addr);
v = FIELD_SET(v, GENMASK(7,0), 0, 0xAB);
writel(v, addr);
```

The kernel has cleaner primitives in `<linux/bitfield.h>`:

```c
#define MY_MODE_MASK   GENMASK(3, 1)

u32 v = readl(addr);
u32 mode = FIELD_GET(MY_MODE_MASK, v);

v &= ~MY_MODE_MASK;
v |= FIELD_PREP(MY_MODE_MASK, 5);
writel(v, addr);
```

`FIELD_GET` extracts and right-shifts. `FIELD_PREP` shifts a value into the field's position. Use these in real driver code; they're checked by `BUILD_BUG_ON_NOT_POWER_OF_2()` etc.

## 6.3 Common bit tricks

You will see all of these in real kernel and GPU code.

### Test if a number is a power of two

```c
bool is_pow2(unsigned x) { return x && !(x & (x - 1)); }
```

### Round up to the next multiple of `align` (when `align` is a power of two)

```c
u64 round_up(u64 x, u64 align) {
    return (x + align - 1) & ~(align - 1);
}
```

The kernel's `ALIGN(x, a)` macro does exactly this. Used in DMA, page allocation, GPU buffer sizing.

### Round down

```c
u64 round_down(u64 x, u64 a) { return x & ~(a - 1); }
```

### Population count (number of set bits)

```c
unsigned popcount(uint32_t x) {
    return __builtin_popcount(x);
}
```

GCC emits the `popcnt` instruction on x86 with `-mpopcnt`, otherwise a fallback. The kernel macro is `hweight32()`.

### Find first / last set

```c
__builtin_ctz(x)   /* count trailing zeros — index of lowest set bit */
__builtin_clz(x)   /* count leading zeros */
```

Kernel: `__ffs()`, `__fls()`, `ffz()`, etc. Useful for: "I have a bitmap of free slots; give me one." The GPU scheduler uses these.

### Bit reverse, byte swap

```c
__builtin_bswap32(x)   /* swap byte order */
```

Kernel: `__swab32()`, `cpu_to_be32()`, etc.

## 6.4 Bitmaps

The kernel has a `<linux/bitmap.h>` API for arbitrarily-large bitmaps:

```c
DECLARE_BITMAP(map, 256);   /* 256 bits, stored as 4 unsigned long */
bitmap_zero(map, 256);
set_bit(42, map);
clear_bit(42, map);
test_bit(42, map);
unsigned long n = find_first_zero_bit(map, 256);
```

`set_bit` and friends are **atomic** by default (because they're often used to coordinate state across CPUs). The non-atomic versions are `__set_bit`, `__clear_bit`. Use the atomic ones unless you know you have exclusive access.

The amdgpu driver uses bitmaps to track free VMIDs, free SDMA queues, in-flight CSAs, and many other resources.

## 6.5 Reading a register: full pattern

```c
struct gpu_dev {
    void __iomem *mmio;
};

#define REG_CTRL        0x0040
#define CTRL_ENABLE     BIT(0)
#define CTRL_RESET      BIT(1)
#define CTRL_MODE_MASK  GENMASK(7, 4)
#define CTRL_MODE_SHIFT 4

static int gpu_set_mode(struct gpu_dev *g, u8 mode)
{
    u32 v;

    if (mode > 0xF)
        return -EINVAL;

    v  = readl(g->mmio + REG_CTRL);
    v &= ~CTRL_MODE_MASK;
    v |= (mode << CTRL_MODE_SHIFT) & CTRL_MODE_MASK;
    v |= CTRL_ENABLE;
    writel(v, g->mmio + REG_CTRL);

    /* readback to flush posted write */
    (void)readl(g->mmio + REG_CTRL);
    return 0;
}
```

Note the read-modify-write pattern. **Never write a register without reading first** unless the entire register's meaning is well-understood — you would clobber bits the hardware uses. The "readback to flush" is critical on PCIe: writes are *posted* and won't necessarily reach the device before the function returns.

We will spend an entire chapter on MMIO ordering. For now, internalize the RMW pattern.

## 6.6 Bit hacks you should at least recognize

```c
/* Swap two ints without a temp (don't actually do this). */
a ^= b; b ^= a; a ^= b;

/* abs without branch (assuming arithmetic shift). */
int mask = x >> 31;
int abs = (x + mask) ^ mask;

/* min(x, y) without branch. */
int min = y ^ ((x ^ y) & -(x < y));

/* Round to next power of two (32-bit). */
x--;
x |= x >> 1; x |= x >> 2; x |= x >> 4;
x |= x >> 8; x |= x >> 16;
x++;
```

You will not write these in production C — the compiler does them. But you must read them in assembly and in tightly-tuned graphics math.

## 6.7 Exercises

1. Implement `set`, `clear`, `flip`, `test` on a 64-bit `uint64_t` bitmap.
2. Implement `is_pow2`, `next_pow2`, `log2_floor` for `uint32_t`. Compare your `popcount` to `__builtin_popcount`.
3. Implement `align_up(x, a)` and `align_down(x, a)` for *non-power-of-two* `a`. Then a power-of-two version. Compare assembly with `gcc -O2 -S`.
4. Pack three values `(mode:4, channel:3, enabled:1)` into a `u8`. Then unpack. Then do the same with explicit shifts and masks vs. bitfields and compare assembly.
5. Read `arch/x86/include/asm/bitops.h` in the kernel source. Identify how `set_bit` is implemented (hint: `lock btsl`).

---

Next: [Chapter 7 — Dynamic memory and writing your own allocator](07-allocators.md).
