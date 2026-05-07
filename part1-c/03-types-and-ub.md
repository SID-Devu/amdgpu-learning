# Chapter 3 — Types, integer promotion, and undefined behavior

C's type system looks simple. It is not. It is the source of more security bugs and more wrong driver code than any other feature of the language. We must tame it now or pay later.

## 3.1 The fundamental types

```c
char        /* 1 byte, signedness implementation-defined */
signed char /* 1 byte, signed */
unsigned char
short            /* >= 16 bits */
int              /* >= 16 bits, almost always 32 on Linux */
long             /* 32 bits on 32-bit Linux, 64 bits on 64-bit Linux */
long long        /* >= 64 bits */
float, double, long double
```

The C standard promises minimum widths, **not exact widths**. The kernel and any portable code must use the fixed-width types from `<stdint.h>` (userspace) or `<linux/types.h>` (kernel):

```c
uint8_t  u8;
uint16_t u16;
uint32_t u32;
uint64_t u64;
int8_t   s8;
/* … and signed variants … */
```

Kernel code uses `u8`, `u16`, `u32`, `u64`, `s32`, etc. — typedefs in `linux/types.h`. Get used to those.

Why this matters for driver code: hardware registers are *fixed width*. A 32-bit register is `u32`, period. Using `int` invites trouble on a future architecture or compiler.

## 3.2 Sizes and `sizeof`

```c
#include <stdio.h>
#include <stdint.h>

int main(void) {
    printf("char  %zu\n", sizeof(char));
    printf("short %zu\n", sizeof(short));
    printf("int   %zu\n", sizeof(int));
    printf("long  %zu\n", sizeof(long));
    printf("ll    %zu\n", sizeof(long long));
    printf("ptr   %zu\n", sizeof(void *));
    printf("u32   %zu\n", sizeof(uint32_t));
    return 0;
}
```

On a 64-bit Linux machine, you'll see `1, 2, 4, 8, 8, 8, 4`. On a 32-bit ARM Cortex-M, `1, 2, 4, 4, 8, 4, 4`. The pointer width and the `long` width track the architecture's bit width on Unix-likes (the **LP64** model). Windows uses **LLP64** where `long` is 32 bits even on 64-bit Windows. Real driver code must not assume.

`sizeof` is an *operator*, not a function. It evaluates at compile time for fixed types. `sizeof(int)` does not compile to a function call.

## 3.3 Implicit conversions and integer promotion

Here be dragons. When you mix types, the compiler silently converts. The rules:

- Any operand smaller than `int` is **promoted** to `int` (or `unsigned int` if the value won't fit in `int`).
- If both operands are the same after promotion, you're fine.
- If they differ, the *lower-rank* one is converted to the higher-rank one. Mixing signed/unsigned: signed gets converted to unsigned. **This is a frequent bug source.**

Example:

```c
unsigned int u = 1;
int s = -1;
if (s < u) {
    /* never executed */
} else {
    puts("WAT");
}
```

Why? `s` is converted to `unsigned int`, becoming `0xFFFFFFFF`, which is greater than `1`. This kind of bug has crashed kernels.

Another classic:

```c
size_t len = something_returning_size_t();
int i;
for (i = 0; i < len - 1; i++) { ... }
```

If `len == 0`, then `len - 1` underflows to `SIZE_MAX`, and `i < SIZE_MAX` is true forever. **Always compare loop variables in the same type as the limit.**

## 3.4 Signed integer overflow is undefined behavior

```c
int x = INT_MAX;
x = x + 1;     /* UNDEFINED BEHAVIOR */
```

UB means the standard puts no constraint on what happens. The compiler is allowed to assume it never happens and optimize accordingly. Real example:

```c
/* before: */ if (x + 1 < x) overflow();
/* compiler: x+1 < x is impossible if no UB, so delete the branch */
```

This famously bit a TCP sequence-number check. Use `unsigned` for arithmetic that might wrap (unsigned overflow is well-defined: it wraps modulo 2^n). Or use the GCC builtins:

```c
int r;
if (__builtin_add_overflow(a, b, &r)) {
    /* overflow */
}
```

The kernel has `check_add_overflow()`, `array_size()`, `struct_size()`, and friends in `<linux/overflow.h>` for exactly this purpose.

## 3.5 Other undefined behaviors you must memorize

These are the classics. Each has shipped at least one CVE.

| UB | Example | What can happen |
|---|---|---|
| Signed overflow | `INT_MAX + 1` | Anything; checks deleted |
| Use after free | `free(p); *p = 0;` | Memory corruption |
| Double free | `free(p); free(p);` | Heap corruption |
| Read of uninitialized | `int x; printf("%d", x);` | Garbage; info leak |
| Out-of-bounds read/write | `int a[4]; a[5] = 0;` | Stack/heap overflow |
| Null pointer deref | `*(int *)0 = 1;` | Crash, or silent in some kernels |
| Strict aliasing violation | Reading `int` via `float *` | Wrong values |
| Misaligned access | `*(uint32_t*)(buf+1)` on ARM | SIGBUS |
| Data race | Two threads write same var without sync | Anything |
| Modifying string literal | `char *p = "hi"; p[0]='H';` | SIGSEGV |
| Shift past width | `int x; x << 32` | Implementation-defined → UB |

Run with `-fsanitize=undefined,address` in dev. UBSan and ASan will catch most of these.

## 3.6 Strict aliasing

The compiler is allowed to assume that an `int *` and a `float *` never point to the same memory. So:

```c
void f(float *fp, int *ip) {
    *ip = 1;
    *fp = 0.0f;
    /* compiler may assume *ip is still 1, even though *fp may have written to it */
}
```

If you actually need to reinterpret bytes, use `memcpy`:

```c
uint32_t bits;
float    f = 1.0f;
memcpy(&bits, &f, sizeof bits);
```

The compiler optimizes that to a register move. It is the *only* portable, defined way to bit-cast in C. C++ adds `std::bit_cast` (since C++20). The Linux kernel uses `READ_ONCE()` / `WRITE_ONCE()` macros that wrap volatile-cast assignments for similar reasons.

`char *` and `unsigned char *` *are* allowed to alias anything. That's why `memcpy`/`memset` take `void *`.

## 3.7 The `volatile` keyword

`volatile` tells the compiler "this memory may change underneath you, so don't optimize away reads/writes." It is **only** for:

- memory-mapped hardware registers,
- variables shared with signal handlers,
- variables shared with interrupt handlers in single-threaded contexts.

`volatile` is **not** a substitute for atomics or locks. It does not order accesses across CPUs. It does not make multi-threaded code correct. Re-read that sentence.

In the Linux kernel, you almost never write `volatile` directly. You use `readl()`/`writel()` for MMIO and `READ_ONCE()`/`WRITE_ONCE()` for shared variables. Both expand to volatile casts internally, but the API expresses *intent*.

## 3.8 The `const` keyword

`const` means "I won't modify this through this name." It is documentation for humans and an optimization hint for the compiler.

```c
const int N = 1024;
void print(const char *s);  /* this fn won't modify *s */
```

Two subtle points:

```c
const int *p;        /* pointer to const int — *p is const, p is not */
int * const p;       /* const pointer to int — p is const, *p is not */
const int * const p; /* both */
```

Read declarations right-to-left. The kernel uses `const` everywhere for parameters that aren't modified; learn to do the same. Putting tables of function pointers (`struct file_operations`) in `const` lets them go in read-only memory, hardening the kernel.

## 3.9 The `static` and `extern` keywords

- `static` at file scope (outside a function): "this name is private to this translation unit." Use this for any helper not exported.
- `static` inside a function: "this variable persists between calls." Avoid in driver code; it's per-CPU global state and a hazard.
- `extern`: "this name is defined elsewhere." Used in headers for global variables. Driver code rarely needs it.

A real-world rule of thumb in the kernel: **everything is `static` unless explicitly exported with `EXPORT_SYMBOL`.** This keeps the namespace clean and lets the compiler inline aggressively.

## 3.10 Type aliases for hardware

Driver headers define register layouts:

```c
struct __attribute__((packed)) my_dev_regs {
    u32 ctrl;
    u32 status;
    u32 irq_mask;
    u32 _reserved;
    u32 doorbell;
};
```

`__attribute__((packed))` disables padding. Without it the compiler may insert pad bytes for alignment, breaking your map onto hardware registers. We will revisit packing in chapter 5.

## 3.11 Exercises

1. Write a program that computes `INT_MAX + 1` two ways: as `int` and as `unsigned`. Compile with `-O2` and look at the assembly. The signed version may be optimized away.
2. Write a function `bool safe_add(int a, int b, int *out)` using `__builtin_add_overflow`.
3. On your machine, write code that reads the bytes of a `float 1.0f` and prints them in hex. (Hint: `memcpy` to a `uint32_t` and print with `%08x`.)
4. Predict the output of:
   ```c
   unsigned long size = 1;
   if (size - 2 < 1)
       puts("yes");
   ```
   then run it. Explain.
5. Read through `linux/types.h` (in `/usr/src/linux-headers-*/include/linux/types.h`) and identify every typedef. Recognize that this file is the foundation of every kernel datatype.

---

Next: [Chapter 4 — Pointers, arrays, and the C memory model](04-pointers-and-memory.md).
