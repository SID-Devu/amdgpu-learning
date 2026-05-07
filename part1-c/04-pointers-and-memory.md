# Chapter 4 — Pointers, arrays, and the C memory model

If you cannot draw the memory layout of a function call on a whiteboard, you cannot write driver code. Most kernel bugs are pointer bugs. Most security holes are pointer bugs. We will spend more time here than the language deserves, because the *concept* matters more than the syntax.

## 4.1 What a pointer is

A pointer is an **integer** that names a memory location. On a 64-bit Linux machine, every pointer is 8 bytes. On a 32-bit machine, 4 bytes. There is nothing magic about it.

```c
int x = 42;
int *p = &x;     /* p holds the address of x */
*p = 99;         /* dereference: write through p */
printf("%d\n", x);   /* 99 */
```

Mental picture:

```
  Address      Memory
  ────────     ──────────
  0x7fffe000   42 → 99      (x)
  0x7fffe008   0x7fffe000   (p)
```

`&x` produces the address. `*p` accesses the memory at that address. `p` itself is a variable (it lives at `0x7fffe008`), and `&p` would be `0x7fffe008`.

## 4.2 The four memory regions

A typical Unix process has these memory regions, growing in opposite directions:

```
high addresses
┌───────────────────────────┐
│       Stack               │  ← grows down: function locals, return addrs
│       (~8 MB default)     │
├───────────────────────────┤
│         ↓                 │
│                           │
│         ↑                 │
├───────────────────────────┤
│       Heap                │  ← grows up: malloc()'d memory
├───────────────────────────┤
│       BSS                 │  ← zero-initialized globals
├───────────────────────────┤
│       Data                │  ← initialized globals
├───────────────────────────┤
│       Text                │  ← code, read-only
└───────────────────────────┘
low addresses
```

Each region has different lifetimes and permissions:

- **Text** is read-execute. Writing to it is UB and on Linux causes SIGSEGV.
- **Data/BSS** are read-write, persist for the lifetime of the program.
- **Heap** is what you `malloc()`. You're responsible for freeing.
- **Stack** is automatic. Locals appear when a function is entered and **disappear** when it returns.

A pointer to a stack local *outlives* the local only at your peril:

```c
int *bad(void) {
    int x = 5;
    return &x;     /* DANGLING POINTER */
}
```

This compiles. The compiler may even warn. Calling `*bad()` is UB. The stack frame is reused immediately by the next function call.

In the kernel, the same applies: stack is small (8 KB or 16 KB *total* per kernel thread on x86_64). You cannot `int big[2048]` on the kernel stack — it will silently corrupt other CPU state.

## 4.3 Pointer arithmetic

Pointer arithmetic is in **units of the pointee type**:

```c
int a[4] = {10, 20, 30, 40};
int *p = a;          /* p points to a[0] */
printf("%d\n", *p);      /* 10 */
printf("%d\n", *(p+1));  /* 20 — moved 4 bytes */
printf("%d\n", p[2]);    /* 30 — same as *(p+2) */
```

`p + 1` advances by `sizeof(*p)` bytes. This is why `void *` cannot be incremented in standard C (you don't know the size of `void`); GCC allows it as an extension.

The corollary: `a[i]` is *literally* `*(a + i)`. So `a[i] == i[a]` is true (don't do this, but be aware C array indexing is symmetric).

## 4.4 Arrays decay to pointers (and when they don't)

This is the most-asked C interview question, after "explain volatile."

```c
void f(int a[]);     /* identical to: void f(int *a); */
void f(int a[10]);   /* identical too! the 10 is ignored */

int main(void) {
    int x[10];
    f(x);            /* x decays to int * */
    printf("%zu\n", sizeof(x));  /* 40 */

    int *p = x;
    printf("%zu\n", sizeof(p));  /* 8 */
}
```

When you pass an array to a function, you pass a pointer. The function does **not** know the array length. You must pass it explicitly:

```c
void sum(const int *a, size_t n) {
    int s = 0;
    for (size_t i = 0; i < n; i++) s += a[i];
    /* … */
}
```

The kernel pattern: every buffer is a `(ptr, len)` pair. `struct iov_iter`, `struct sg_table`, `struct scatterlist`, every DMA API — all `(ptr, len)`. Hardcode this into your fingers.

Inside `sizeof`, `&`, and the very first expression of a string literal initializer (`char s[] = "hi";`), arrays do **not** decay. Everywhere else, they do.

## 4.5 Strings

A C string is just a `char` array terminated by a `'\0'` byte. There is no string type. There is no length prefix.

```c
char s[] = "hello";   /* 6 bytes: 'h','e','l','l','o','\0' */
char *p  = "hello";   /* p points to a string literal in read-only memory */
```

`p[0] = 'H'` is UB. `s[0] = 'H'` is fine.

`<string.h>` gives you:

```c
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);   /* DANGEROUS: no length */
char  *strncpy(char *dst, const char *src, size_t n); /* may not NUL-terminate */
size_t strlcpy(char *dst, const char *src, size_t n);  /* GNU/BSD ext: safer */
int    strcmp(const char *a, const char *b);
char  *memcpy(void *dst, const void *src, size_t n);
char  *memset(void *dst, int c, size_t n);
```

In the kernel: never `strcpy`. Use `strscpy()` (kernel-internal). Always check lengths. Most CVEs in C strings come from buffer overflows.

## 4.6 The two-headed sword: `char *` and `void *`

- `char *` (and `unsigned char *`) is allowed to alias *any* type. So you can `memcpy(buf, &my_struct, sizeof my_struct)`.
- `void *` is the generic pointer type. Every `T *` implicitly converts to `void *` and back without a cast.

```c
void *p = malloc(sizeof(int));
int *ip = p;       /* OK in C; in C++ you'd need a cast */
```

In kernel code you'll see:

```c
struct foo *f = kzalloc(sizeof *f, GFP_KERNEL);
```

`kzalloc` returns `void *`. We assign to `struct foo *`. No cast needed in C. Notice `sizeof *f` — this is the kernel idiom: you don't repeat the type name, so refactoring is safe.

## 4.7 Arrays of pointers, pointers to pointers

```c
char *names[] = { "Alice", "Bob", "Carol" };  /* array of 3 pointers */
char **np = names;
printf("%s\n", *np);       /* "Alice" */
printf("%s\n", *(np+1));   /* "Bob" */
```

Argv is the canonical example:

```c
int main(int argc, char **argv);
```

`argv` is a pointer to the first element of an array of `char *`. Each element points to a NUL-terminated string. `argv[argc]` is `NULL`.

## 4.8 Function pointers

```c
int add(int a, int b) { return a + b; }
int (*fp)(int, int) = add;
printf("%d\n", fp(2, 3));    /* 5 */
```

Read it as: "fp is a pointer to a function taking (int,int) returning int." The parentheses around `*fp` are required, otherwise it would be "function returning int*."

The kernel uses function pointers everywhere — `struct file_operations`, `struct drm_driver`, `struct pci_driver` — because that's how polymorphism works in C. We dedicate Chapter 9 to this.

## 4.9 The `restrict` keyword

`restrict` is a C99 promise: "for the lifetime of this pointer, the only way I will access this object is through this pointer." It lets the compiler optimize as if no aliasing exists.

```c
void copy(int * restrict dst, const int * restrict src, size_t n);
```

If `dst` and `src` overlap, the result is UB. `memcpy` is `restrict`; `memmove` is not. The kernel uses `__restrict__` (GCC spelling) sparingly, but in performance-critical math (BLAS, GPU kernel code) `restrict` is essential.

## 4.10 Common pointer bugs and how to find them

| Bug | Diagnostic |
|---|---|
| Use after free | ASan: `heap-use-after-free` |
| Double free | ASan: `attempting double-free` |
| Buffer overflow | ASan: `heap-buffer-overflow` |
| Stack buffer overflow | ASan: `stack-buffer-overflow` |
| Memory leak | ASan: `LeakSanitizer` summary |
| Uninitialized read | MSan / Valgrind: `Conditional jump … depends on uninitialised value` |
| Dangling stack ptr | `-Wreturn-stack-address` (clang) |
| Null deref | gdb: SIGSEGV, address `0x0` |

Build everything during dev with:

```bash
gcc -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer
```

You will catch 95% of pointer bugs before they ship.

## 4.11 The C memory model in one paragraph

Memory is a flat array of bytes addressed by integers. Pointers are typed integers. Allocations come from the stack (automatic, scope-bound), the heap (manual, `malloc`/`free`), or static storage (globals/locals declared `static`). The compiler is free to reorder reads/writes within a single thread as long as the *as-if* rule holds. Across threads or between CPU and device, you need barriers (Chapter 25). Across processes, memory is *not* shared except via explicit `mmap(MAP_SHARED)` or shm primitives.

## 4.12 Exercises

1. Implement `my_strlen`, `my_strcpy`, `my_strcmp`, `my_memcpy`, `my_memset` from scratch. Test each. This is a standard interview warm-up.
2. Implement a function `void reverse(char *s)` that reverses an in-place string. Don't allocate.
3. Implement `int parse_int(const char *s, int *out)` that returns 0 on success, -1 on failure. Detect overflow.
4. Build a `char **split(const char *s, char delim)` that returns a NULL-terminated array of newly-allocated strings. Write the matching `void free_split(char **)`.
5. Run all of the above under ASan and Valgrind. Fix every leak and overflow.

---

Next: [Chapter 5 — Structs, unions, bitfields, packing, alignment](05-structs-unions-alignment.md).
