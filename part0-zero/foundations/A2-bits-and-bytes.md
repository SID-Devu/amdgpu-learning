# A2 — Bits, bytes, words

In chapter A1 you learned what a binary digit (a **bit**) is. Now we group them.

## The hierarchy

| Unit | Size | Notes |
|---|---|---|
| **bit** | 1 binary digit (0 or 1) | smallest possible |
| **nibble** | 4 bits | one hex digit; rarely named |
| **byte** | 8 bits | the basic addressable unit on every modern CPU |
| **half-word** | 16 bits / 2 bytes | C: `int16_t`, `uint16_t` |
| **word** | 32 bits / 4 bytes (on x86_64/AArch64) | C: `int32_t`, `uint32_t` |
| **double word / "qword"** | 64 bits / 8 bytes | C: `int64_t`, `uint64_t` |

"Word" is squishy. On x86, "word" historically meant 16 bits; on AArch64, 32 bits. **In Linux kernel C, prefer the explicit names: `u8`, `u16`, `u32`, `u64`** (or `uint8_t` etc. in userspace).

## The byte is the fundamental unit

Every memory address points to one byte. When you read or write memory, you read or write some integer number of bytes. The CPU has special instructions to deal with 1, 2, 4, or 8 bytes at a time.

```
Address    Byte
0x1000   ┌──────┐
0x1001   │      │
0x1002   │ each │
0x1003   │ box  │
0x1004   │ is 8 │
0x1005   │ bits │
0x1006   │      │
0x1007   └──────┘
```

A 32-bit `int` at address `0x1000` occupies `0x1000`, `0x1001`, `0x1002`, `0x1003` — four consecutive bytes.

## Sizes (memorize)

```
8 bits        = 1 byte
16 bits       = 2 bytes
32 bits       = 4 bytes
64 bits       = 8 bytes
128 bits      = 16 bytes
512 bits      = 64 bytes  ← cache line size on most x86
4096 bits     = 512 bytes
4 KiB         = 4096 bytes  ← page size
2 MiB         = huge page
1 GiB         = sometimes huge page
```

## How big is a byte's range?

A byte (8 bits) holds 2⁸ = 256 values.

| As C type | Range |
|---|---|
| `unsigned char` / `u8` | 0 to 255 (`0x00` to `0xFF`) |
| `signed char` / `s8` | −128 to 127 |

A 32-bit value:

| As C type | Range |
|---|---|
| `u32` | 0 to ~4.29 billion |
| `s32` | ~−2.15 billion to ~2.15 billion |

A 64-bit value: ~18 quintillion unsigned. (For the curious: `u64` max = `0xFFFF_FFFF_FFFF_FFFF`.)

You will hit overflow bugs eventually. Always know how big your integer can get.

## KB vs KiB — the trap

There are **two** conventions for "kilobyte":

| Symbol | Meaning | Used by |
|---|---|---|
| KB (or kB) | 1000 bytes (decimal) | hard drive marketing, network speed |
| KiB | 1024 bytes (binary) | RAM, kernel sizes, programmers |

The same trap for MB / MiB, GB / GiB:

```
1 KiB = 1024 bytes   = 2^10
1 MiB = 1024 KiB     = 2^20 = 1,048,576 bytes
1 GiB = 1024 MiB     = 2^30
1 TiB = 1024 GiB     = 2^40
```

In kernel code and this book, when we say "4 KB page" we mean 4 KiB = 4096 bytes. SI purists are correct that "kB" should mean 1000, but tradition wins.

A "16 GB" HDD is 16,000,000,000 decimal bytes. A "16 GiB" RAM stick is 17,179,869,184 binary bytes. **They look the same from the marketing slide; they are not.**

## Endianness — which byte goes first?

A 32-bit integer like `0x12345678` is 4 bytes: `12`, `34`, `56`, `78`. But how are they ordered in memory?

- **Little-endian**: lowest byte first. `0x1000` = `0x78`, `0x1001` = `0x56`, ...
- **Big-endian**: highest byte first. `0x1000` = `0x12`, `0x1001` = `0x34`, ...

x86 and AArch64 (in normal mode) are **little-endian**. Network protocols are big-endian ("network byte order"). When parsing wire formats you'll need `htonl()`, `ntohl()`, etc.

Test it on your machine:

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t n = 0x12345678;
    uint8_t  *p = (uint8_t *)&n;
    printf("%02x %02x %02x %02x\n", p[0], p[1], p[2], p[3]);
    // little-endian prints: 78 56 34 12
}
```

## Alignment

CPUs are picky. Reading a `u32` is fastest when the address is a multiple of 4. Reading a `u64` from an address that's a multiple of 8. Misaligned reads are either slow (x86 tolerates them) or trap (some ARM modes).

```
Aligned u32:    address 0x1000 (multiple of 4)
                address 0x1004
                address 0x1008
                
Misaligned u32: address 0x1001 ← bad
```

Compiler will pad structs to keep things aligned:

```c
struct s {
    uint8_t  a;     // 1 byte, then 3 padding bytes
    uint32_t b;     // 4 bytes, total 8
    uint8_t  c;     // 1 byte, then 3 padding bytes (struct must end aligned for arrays)
};
sizeof(struct s) == 12
```

In Part I chapter 5 we do this in detail. For now, just know that data has natural lanes.

## Bit ordering inside a byte

Bit 0 of a byte is **the rightmost (least significant)** bit. So:

```
binary:    0  1  0  1  1  0  0  1   = 0x59
bit:       7  6  5  4  3  2  1  0
```

You'll see notation like `[7:0]` meaning "bits 7 through 0" (the whole byte) or `[15:8]` meaning "the high byte of a 16-bit value." Read this comfortably; hardware datasheets use it constantly.

## Common bit-twiddling idioms

These come up in every driver. We'll write them in C; same idea in Python.

### Set bit N

```c
x |= (1u << N);     // OR with a mask of just bit N
```

### Clear bit N

```c
x &= ~(1u << N);    // AND with a mask that has 0 only at bit N
```

### Test bit N

```c
if (x & (1u << N)) { ... }
```

### Toggle bit N

```c
x ^= (1u << N);     // XOR
```

### Extract bits [hi:lo]

```c
uint32_t field = (x >> lo) & ((1u << (hi - lo + 1)) - 1);
```

This is *the* GPU register pattern. A 32-bit register has 5 bit-fields packed in; you extract them with shifts and masks.

We'll have a whole chapter (Part I, ch 6) on this. Bookmark it.

## Try it

1. How many bits in `u32`? In `u64`? (32 / 64.)
2. What is `0x80` in binary, and which bit is set? (Answer: `1000 0000`, bit 7.)
3. If `int x = 0xFF;` what is `x & 0x0F`? (Answer: `0x0F` — bottom nibble.)
4. Is your CPU little-endian? (Almost certainly yes. Verify with the snippet above.)
5. Why does padding exist in structs?
