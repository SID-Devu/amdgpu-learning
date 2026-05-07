# Appendix — Kernel coding style (the rules that get patches accepted)

The full canonical doc lives at `Documentation/process/coding-style.rst`. This is the operational subset you must internalize before sending a patch.

## 1. Indentation

- **Tabs**, exactly **8 columns wide**. Not 4. Not spaces. Tabs.
- This is non-negotiable. Run `:set ts=8 sw=8 noexpandtab` in vim or the equivalent in your editor.
- The wide tab is intentional: if you nest deeper than 3 levels, your function is too complex.

## 2. Line length

- **80 columns** is still the soft limit. Up to **100 columns** is tolerated for human readability when wrapping makes things worse. Don't wrap a printk message just to make it fit; let it run.

## 3. Braces

- Opening brace **on the same line** as `if`, `while`, `for`, `switch`, `do`.
- Opening brace **on its own line** for `function definitions` and `struct/union` definitions.

```c
int foo(int x)
{
    if (x > 0) {
        do_thing();
    } else {
        do_other();
    }
}
```

- Single-statement bodies don't need braces *unless* one branch of an `if`/`else` has braces — then both must.

## 4. Naming

- `snake_case` for functions, variables, struct members.
- `SCREAMING_SNAKE` for `#define` and enum constants.
- Local variables: short. `i`, `n`, `len`, `tmp`. The kernel is not Java.
- Functions exposed in headers: prefix with the subsystem (`amdgpu_vm_init`, `drm_dev_register`).
- No camelCase. No `m_` Hungarian prefixes.
- `goto err_*:` labels for cleanup.

## 5. Comments

- `/* like this */` for multi-line. `//` is allowed but `/* */` is more common.
- Comments explain **why**, never **what**.
- Function header comments use **kernel-doc**:

```c
/**
 * amdgpu_bo_create - allocate a BO
 * @adev: amdgpu device
 * @size: bytes
 * @domain: AMDGPU_GEM_DOMAIN_*
 *
 * Allocates a buffer object suitable for the requested domain.
 * Caller must drop the reference with amdgpu_bo_unref().
 *
 * Return: 0 on success, -ENOMEM on allocation failure.
 */
```

## 6. Functions

- One function does **one thing**.
- Aim for **<60 lines**. If it's longer, you almost certainly need to split.
- **<5 parameters**. Pack into a struct otherwise.
- Single exit point via `goto err_*` for cleanup. The "errors are forward gotos" pattern is universal:

```c
int probe(struct device *dev)
{
    int ret;

    ret = step_a();
    if (ret)
        return ret;

    ret = step_b();
    if (ret)
        goto err_a;

    ret = step_c();
    if (ret)
        goto err_b;

    return 0;

err_b:
    undo_b();
err_a:
    undo_a();
    return ret;
}
```

## 7. Locking

- Document every lock in a comment near its declaration: what it protects, what may not be held while taking it, IRQ vs process context.
- Acquire in a **fixed order** project-wide. The kernel runs `lockdep` to enforce this; respect it.

## 8. Macros

- Lowercase macros that look like functions are *frowned on* unless you have a real reason.
- Always parenthesize macro arguments and the whole expansion:

```c
#define MIN(a, b) ({ typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
```

(But prefer `min()` from `<linux/minmax.h>`.)

## 9. Don't

- **Don't typedef structs.** Use `struct foo`. (Kernel rule, opposite of glibc style.) Exceptions are well-established (`u32`, `pid_t`, opaque handles).
- **Don't use bool in structs** that pack tightly. Use `u8` or bitfields where size matters.
- **Don't add new printk's without a level**. Always `pr_err`/`pr_warn`/`pr_info`/`pr_debug` (or `dev_*`).
- **Don't break userspace.** "Don't break userspace" is the prime directive. If your patch makes an ABI change, justify it in the commit msg or it gets nacked.

## 10. Run before sending

```bash
scripts/checkpatch.pl --strict --codespell -g HEAD
scripts/get_maintainer.pl <patch.mbox>
make C=2 W=1 drivers/gpu/drm/amd/amdgpu/<your_file>.o
```

`checkpatch.pl` will catch 80% of style issues; the human reviewer will catch the rest.

## 11. Commit message

- **Subject line**: `subsystem: short imperative summary` ≤72 chars.
  - Good: `drm/amdgpu: Fix NULL deref in vm_pt_create()`
  - Bad: `Fixed bug`
- **Body**: explain *why*, in present tense imperative. Reference the bug, the failing path, the symptom, the fix.
- **Tags** at the bottom in this order:
  ```
  Fixes: 1234567890ab ("drm/amdgpu: original commit subject")
  Cc: stable@vger.kernel.org   # if user-visible bugfix
  Reviewed-by: Reviewer Name <r@example.com>
  Signed-off-by: Your Name <you@amd.com>
  ```

If you can write good kernel commit messages, you can write anything.
