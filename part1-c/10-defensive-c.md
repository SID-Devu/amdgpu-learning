# Chapter 10 — Defensive C: const, restrict, error handling, kernel style

The difference between code that ships and code that gets rejected on review is not cleverness — it's discipline. This short chapter codifies the habits that survive review on the kernel mailing list.

## 10.1 Const-correctness

Apply `const` aggressively to arguments and locals you don't mutate.

```c
size_t my_strlen(const char *s);                  /* good */
int    parse(const char *src, char *dst, size_t cap);
```

Returning a `const T *` says "you may read but not modify." Use it for accessors that expose internal data:

```c
const struct gpu_caps *gpu_get_caps(const struct gpu *g);
```

Top-level data tables that never change: `static const`. Then they live in `.rodata`, are protected by the kernel's `mark_rodata_ro`, and the compiler can fold reads.

```c
static const u32 lut[256] = { /* ... */ };
```

## 10.2 `restrict`

When you know two pointer args don't alias, mark them `restrict`. The compiler can vectorize. Real example:

```c
void axpy(size_t n,
          float a,
          const float * restrict x,
          float       * restrict y)
{
    for (size_t i = 0; i < n; i++) y[i] += a * x[i];
}
```

In hot loops in math/graphics code, `restrict` can be a 2x speedup.

## 10.3 Error handling

C has no exceptions. Pick a discipline and stick to it. The kernel uses **negative errno on failure, 0 on success, positive count on certain reads/writes**:

```c
int do_thing(void);
/* return 0 on success, -EINVAL/-ENOMEM/etc on error */
```

For pointers that might be errors, the kernel uses `ERR_PTR()`:

```c
struct foo *foo_create(void)
{
    struct foo *f = kzalloc(sizeof *f, GFP_KERNEL);
    if (!f)
        return ERR_PTR(-ENOMEM);
    /* ... */
    return f;
}

struct foo *f = foo_create();
if (IS_ERR(f))
    return PTR_ERR(f);
```

`ERR_PTR(-12)` returns a pointer in the top page (high addresses, kernel-only) that encodes the error. `IS_ERR` checks. `PTR_ERR` decodes. This avoids out-parameters in the common case.

## 10.4 Cleanup with `goto`

The pattern from the previous chapter, restated as a rule:

> **Every function with > 1 fallible step uses goto for unwind.**

```c
int do_complex(struct dev *d)
{
    int ret;

    ret = step_a(d);
    if (ret) return ret;

    ret = step_b(d);
    if (ret) goto out_undo_a;

    ret = step_c(d);
    if (ret) goto out_undo_b;

    return 0;

out_undo_b: undo_b(d);
out_undo_a: undo_a(d);
    return ret;
}
```

Don't nest 8 `if` blocks. Don't duplicate cleanup. Goto chains are *the* idiom.

## 10.5 The kernel coding style — what you must absolutely follow

Read `Documentation/process/coding-style.rst` in any kernel tree. Highlights:

- Tabs, 8 spaces wide. **Always tabs in kernel code**, not spaces.
- 80-column soft limit (now 100 in some areas).
- Brace placement: opening brace at end of line for control flow; on its own line for functions:

  ```c
  if (x) {
      foo();
  }

  void func(void)
  {
      ...
  }
  ```

- `if (!ptr)` not `if (ptr == NULL)`.
- Names are `lowercase_with_underscores`. CamelCase is forbidden.
- One declaration per line; no `int a, b, *c;`.
- Variables declared at top of block.
- Functions short — under 80 lines if possible.
- No typedefs for pointers (`typedef struct foo *foo_t` — forbidden).
- Don't put complex logic in macros if a `static inline` works.
- No mixed-case multi-line comments. Use `/* ... */`, kernel uses `//` sparingly (now allowed).
- `Signed-off-by:` your real name + email on every commit.

Run `scripts/checkpatch.pl` on your patches before sending.

## 10.6 `assert`, `BUG_ON`, `WARN_ON`

In userspace, `assert(p != NULL)` aborts the program when the condition is false. Disabled by `-DNDEBUG`. Don't put logic in assertions; they evaporate.

In kernel:

```c
WARN_ON(condition);   /* dump stack and continue */
WARN_ON_ONCE(condition);
BUG_ON(condition);    /* "this is unrecoverable", may panic */
```

`BUG_ON` is rarely correct — prefer `WARN_ON` plus an error return. The kernel must keep going even if a sub-driver hits weirdness; killing the kernel for a recoverable error is hostile to the user.

## 10.7 Avoiding undefined behavior

A short checklist you can paste above any patch:

- [ ] No signed integer overflow without `__builtin_*overflow` / `check_*overflow`.
- [ ] No shift past width.
- [ ] No misaligned access (use `memcpy` for unaligned loads).
- [ ] No use-after-free; every `free` followed by `= NULL` if the var lives on.
- [ ] No data race on shared mutable state without lock or atomic.
- [ ] All `malloc`/`kmalloc` checked.
- [ ] All paths free what they own.
- [ ] No infinite recursion.
- [ ] Return value of every fallible call checked.

Build dev with `-Wall -Wextra -Wshadow -Wstrict-overflow -Wundef -Werror -fsanitize=address,undefined -fstack-protector-strong`. Kernel: `KASAN=y`, `UBSAN=y`, `KCSAN=y` for race detection.

## 10.8 A sample function reviewed

Bad:

```c
char *get(int n) {
    char *p = malloc(n);
    sprintf(p, "%d", n);
    return p;
}
```

Problems: no NULL check on `malloc`; `sprintf` may overflow if `n` is large enough and the buffer is `n` bytes; caller has to free; nothing documents that.

Better:

```c
/**
 * get_decimal_string - format @n as a decimal string.
 * @n:    integer to format.
 * @out:  caller-provided buffer.
 * @cap:  size of @out in bytes.
 *
 * Return: number of bytes written (excluding trailing NUL),
 *         or -EINVAL if @out is NULL or @cap is too small.
 */
int get_decimal_string(int n, char *out, size_t cap)
{
    int len;

    if (!out || cap == 0)
        return -EINVAL;

    len = snprintf(out, cap, "%d", n);
    if (len < 0 || (size_t)len >= cap)
        return -EOVERFLOW;
    return len;
}
```

Caller-allocated, safe `snprintf`, documented contract, errno return. This is the shape of code that ships.

## 10.9 Exercises

1. Take the bad version above and turn it into the better one. Add a unit test that passes a 1-byte buffer.
2. Convert any tutorial code you have to use kernel-style indentation, 80-col limit, goto-chain cleanup. Run `scripts/checkpatch.pl --strict` against it. Fix every warning.
3. Write `safe_atoi(const char *s, int *out)` that returns 0 on success, `-EINVAL` on garbage, `-ERANGE` on overflow.
4. Read `mm/slub.c` (a kernel core file) and identify ten places that follow this style: const args, errno returns, goto-chain cleanup, `WARN_ON_ONCE` for invariants.

---

End of Part I. Move to [Part II — C++ for systems programmers](../part2-cpp/11-c-to-cpp.md).
