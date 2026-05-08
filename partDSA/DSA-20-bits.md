# DSA 20 — Bit manipulation tricks

> The kernel is full of bit tricks. Memorize the standard set.

## The basics (cross-link [Foundations A3](../part0-zero/foundations/A3-boolean-logic.md))

C operators:
- `&` AND, `|` OR, `^` XOR, `~` NOT
- `<<` left shift, `>>` right shift
- For unsigned types these are well-defined; for signed, right shift is implementation-defined for negatives. **Always use unsigned for bit math.**

## Standard idioms

### Set bit `i`
```c
x |= (1U << i);
```

### Clear bit `i`
```c
x &= ~(1U << i);
```

### Toggle bit `i`
```c
x ^= (1U << i);
```

### Test bit `i`
```c
if (x & (1U << i)) { /* set */ }
```

### Lowest set bit (isolate)
```c
x & -x         // returns 0...010...0, the lowest set bit
```
Used to walk through set bits one at a time, e.g., in segment trees / Fenwick.

### Clear lowest set bit
```c
x & (x - 1)
```
If this equals 0, then x had exactly one bit set (i.e., is a power of two).

### Is `x` a power of two?
```c
(x != 0) && ((x & (x - 1)) == 0)
```

### Count set bits (popcount)
```c
int popcount(unsigned x) {
    int c = 0;
    while (x) { x &= x - 1; c++; }   // each iteration kills lowest set bit
    return c;
}
```
Or use the compiler builtin: `__builtin_popcount(x)` (compiles to single instruction on modern CPUs with `popcnt`).

### Count trailing zeros / leading zeros
```c
__builtin_ctz(x)   // count trailing zeros (UB if x==0)
__builtin_clz(x)   // count leading zeros
```
These map to single CPU instructions on x86 (`bsf`, `lzcnt`) and ARM (`clz`, `rbit+clz`).

### Round up to next power of two
```c
unsigned next_pow2(unsigned x) {
    if (x == 0) return 1;
    return 1U << (32 - __builtin_clz(x - 1));
}
```

### Modulo by a power of two
```c
x % n   →   x & (n - 1)    // when n is a power of two
```
Single AND instead of an integer divide. The kernel does this for ring buffers all the time.

### Swap two ints without temp (just don't)
```c
a ^= b; b ^= a; a ^= b;
```
Cute, but no faster on modern CPUs (the compiler optimizes a temp away). And blows up if `a == b` (zeros both!). Use a temp.

## Bitfields and packed flags

```c
#define FLAG_READY   (1U << 0)
#define FLAG_DIRTY   (1U << 1)
#define FLAG_LOCKED  (1U << 2)

unsigned flags;

flags |= FLAG_READY | FLAG_DIRTY;     // set
flags &= ~FLAG_DIRTY;                  // clear
if (flags & FLAG_LOCKED) ...           // test
```

This is **everywhere in the kernel**. Read any header — `O_RDONLY`, `MAP_PRIVATE`, `GFP_KERNEL`, etc. are all bit flags.

## Manipulating bit ranges (multi-bit fields)

To read bits `[hi:lo]` from `x`:
```c
unsigned bits = (x >> lo) & ((1U << (hi - lo + 1)) - 1);
```

To write a value `v` (small enough) into bits `[hi:lo]`:
```c
unsigned mask = ((1U << (hi - lo + 1)) - 1) << lo;
x = (x & ~mask) | ((v << lo) & mask);
```

The kernel uses `GENMASK(hi, lo)` and `FIELD_PREP/FIELD_GET` macros (`include/linux/bitfield.h`) — read them.

## Atomic bit operations

Multi-threaded code must use atomic bit ops to avoid lost updates:

- C11: `atomic_fetch_or`, `atomic_fetch_and`, etc.
- Kernel: `set_bit(nr, addr)`, `clear_bit(nr, addr)`, `test_and_set_bit(nr, addr)` — `include/linux/bitops.h`. Each compiles to a hardware atomic on the right architecture.

## Bitmaps

A **bitmap** is an array of integers, treated as a flat sequence of bits. Each bit represents one item ("is it allocated?", "is it dirty?", etc.).

Kernel `bitmap_*` API: `bitmap_zero`, `bitmap_set`, `bitmap_clear`, `bitmap_find_first_zero`. Used for: PIDs, page allocator buddy bitmaps, GPU VMID allocation.

```c
unsigned long pmap[BITS_TO_LONGS(N)] = {0};
bitmap_set(pmap, idx, 1);
if (test_bit(idx, pmap)) ...
```

## Real-world tricks

### Reverse bits in a byte
Several techniques (lookup table for speed; bit-reversal permutation as a clever expression).

### Count bits set without branches
Brian Kernighan's loop above. Or use parallel-bit-count techniques (SWAR — SIMD Within A Register). Or the `popcnt` instruction.

### Detect if integer is divisible by 3 (bit-only)
A surprising amount of literature exists. Practical answer: just use `%`.

### Get the position of the only set bit
`__builtin_ctz(x)` if x has exactly one bit set.

### Saturating addition
```c
unsigned sat_add(unsigned a, unsigned b) {
    unsigned r = a + b;
    return r < a ? UINT_MAX : r;
}
```
Used in counters that should clamp instead of wrap.

### Branchless absolute value
```c
int abs_branchless(int x) {
    int mask = x >> 31;        // -1 if negative, 0 if non-negative
    return (x ^ mask) - mask;
}
```

### Branchless min/max (use compiler; usually it's smarter)
```c
int min_branchless(int a, int b) { return b ^ ((a ^ b) & -(a < b)); }
```
Often slower than the obvious `(a < b) ? a : b` — modern compilers generate `cmov`. Don't outsmart them.

## In the kernel

- **Page flags**: `flags` field on `struct page` is a bitmask of PG_locked, PG_dirty, etc.
- **GFP flags**: `__GFP_KERNEL`, etc. — flags ORed into one int.
- **PCI capabilities**: bit positions in PCI config space.
- **GPU register fields**: tons of multi-bit fields. AMD's `WREG32_FIELD` and `RREG32_FIELD` macros for read-modify-write.
- **`include/linux/bitops.h`** — your friend.

## Common bugs

1. **Signed shift right** on negative ints → implementation-defined.
2. **Shift count >= width** of type → undefined behavior (`x << 32` for `unsigned int` is UB!).
3. **Forgetting parentheses** in macros: `#define MASK(n) 1<<(n)` will fail when MASK is part of an expression. Always parenthesize.
4. **Mixing signed and unsigned** in bit math → integer promotion surprises.
5. **Endianness** when serializing bit fields across the wire.

## Try it

1. Write `popcount` four ways: naive loop, Kernighan's, lookup table, builtin. Compare speed on 100M iterations.
2. Write `next_pow2` correctly (handle 0 and 1).
3. Implement a simple bitmap allocator: array of unsigned longs, `alloc()` returns lowest free bit, `free()` clears it. Use `__builtin_ctz`/`__builtin_ctzl`.
4. Read `include/linux/bitfield.h`. Decode `FIELD_PREP(GENMASK(7, 4), 5)`. What does it produce? Try in a small test.
5. Read `bitmap_set` / `bitmap_clear` source. Identify how partial-long boundary handling works.

## Read deeper

- *Hacker's Delight* by Henry S. Warren — the bit-trick bible.
- "Bit Twiddling Hacks" — Sean Eron Anderson's classic page (search it).
- `arch/x86/include/asm/bitops.h` — hand-tuned x86 asm for set/clear/test bit.

Next → [Two pointers, sliding window, fast/slow](DSA-21-two-pointers.md).
