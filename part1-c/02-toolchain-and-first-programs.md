# Chapter 2 — The toolchain and your first C programs

We learn C the way professionals use it: by understanding the toolchain. If you can't read a linker error or a `-Wall` warning, you can't write driver code.

## 2.1 The five tools you must own

| Tool | What it does | Why you care |
|---|---|---|
| `gcc` / `clang` | C compiler | Builds the kernel and every driver. |
| `make` | Build system | Linux kernel uses Kbuild, a Make extension. |
| `gdb` | Debugger | Step through code, inspect memory, diagnose crashes. |
| `objdump` / `readelf` / `nm` | Inspect binaries | Read assembly of your code, find symbols. |
| `git` | Source control | The kernel and Mesa live in git; you must be fluent. |

Install on Ubuntu:

```bash
sudo apt install build-essential gdb clang lld llvm \
                 binutils git valgrind strace ltrace
```

## 2.2 Hello, world

Create `hello.c`:

```c
#include <stdio.h>

int main(void)
{
    printf("Hello, world\n");
    return 0;
}
```

Compile and run:

```bash
gcc -Wall -Wextra -O2 -g -o hello hello.c
./hello
```

You always want `-Wall -Wextra` (warnings on) and `-g` (debug info). `-O2` is what production code uses; do not get used to `-O0` or you'll write code the optimizer breaks.

### Decoding what just happened

- `#include <stdio.h>` is a **preprocessor** directive. It textually pastes the contents of `stdio.h` into your file.
- `int main(void)` is the **entry point** the runtime calls. Returning `0` means success; non-zero means error.
- `printf` is a **library function** — it lives in `libc.so.6`, dynamically loaded at runtime.
- The actual entry point is not `main`. It's `_start` (provided by `crt1.o`), which sets up argv/envp/stack canaries, calls `__libc_start_main`, which calls `main`.

To see all four toolchain steps individually:

```bash
gcc -E hello.c -o hello.i      # Preprocess
gcc -S hello.i -o hello.s      # Compile
gcc -c hello.s -o hello.o      # Assemble
gcc hello.o    -o hello        # Link
```

Inspect the object file:

```bash
nm hello.o            # symbols (T = text/code, U = undefined, etc.)
objdump -d hello.o    # disassemble
readelf -a hello.o    # ELF metadata
```

A driver engineer must be able to look at `objdump -d driver.ko` and figure out what's wrong. Practice this on hello world.

## 2.3 The structure of a real C program

A "real" program has multiple files. Here is a tiny library plus a main driver:

```c
/* math_util.h */
#ifndef MATH_UTIL_H
#define MATH_UTIL_H

int add(int a, int b);
int mul(int a, int b);

#endif
```

```c
/* math_util.c */
#include "math_util.h"

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
```

```c
/* main.c */
#include <stdio.h>
#include "math_util.h"

int main(void)
{
    printf("2+3 = %d\n", add(2, 3));
    printf("2*3 = %d\n", mul(2, 3));
    return 0;
}
```

Build:

```bash
gcc -Wall -Wextra -O2 -g -c math_util.c    # → math_util.o
gcc -Wall -Wextra -O2 -g -c main.c         # → main.o
gcc main.o math_util.o -o app
./app
```

Three points to internalize:

1. **Header files declare; .c files define.** A function declared in a header has its body in some .c file. The linker matches them up.
2. **Every .c file is compiled independently** into a `.o`. The compiler doesn't see across files (that's why headers exist). This is called the **translation unit** model.
3. **The linker** stitches `.o` files plus libraries into the final executable. Linker errors mean *missing or duplicated definitions across .o files*.

## 2.4 A first Makefile

```make
CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -g
OBJS    := main.o math_util.o

app: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c math_util.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o app
```

Save as `Makefile`, then `make`. The kernel build system (Kbuild) is an elaborate version of this.

## 2.5 Reading compiler warnings is a skill

This compiles silently with `-O2` but is dead wrong:

```c
#include <stdio.h>
int main(void) {
    int x;
    printf("%d\n", x);   /* x is uninitialized */
    return 0;
}
```

Add `-Wall -Wextra -Wuninitialized -O2`:

```
warning: 'x' is used uninitialized [-Wuninitialized]
```

In a driver, an uninitialized variable can leak kernel memory to userspace. **Treat warnings as errors.** Add `-Werror` to your `CFLAGS`. Real kernel code is built with hundreds of warning flags enabled.

## 2.6 Debugging your first segfault

```c
#include <stdio.h>
int main(void) {
    int *p = NULL;
    *p = 42;       /* boom */
    return 0;
}
```

Compile with `-g`, run, see `Segmentation fault`. Now:

```bash
gcc -g segfault.c -o segfault
gdb ./segfault
(gdb) run
(gdb) bt          # backtrace: where did it crash
(gdb) frame 0
(gdb) print p     # $1 = (int *) 0x0
```

If you can't reach for gdb the second something crashes, you cannot work on drivers. Drivers crash in obscure ways and gdb (or `crash`/`kdb`/`pstore`) is the only way through.

## 2.7 The five rules of writing C from day one

1. **Initialize every variable.** `int x = 0;` not `int x;`.
2. **Check every return code.** `malloc`, `fopen`, `read`, `ioctl` — all of them can fail.
3. **Free everything you allocate, exactly once.** This is harder than it sounds.
4. **Run with sanitizers.** `gcc -fsanitize=address,undefined -g` in development.
5. **Compile with `-Wall -Wextra -Werror`.** No exceptions.

Internalize these now. They are the difference between a junior who ships and a junior whose patches get rejected.

## 2.8 Exercises

1. Build `hello.c` four times: with `-O0`, `-O1`, `-O2`, `-O3`. Compare the assembly using `objdump -d hello`. Notice what the optimizer does.
2. Deliberately break the program (typo a function name, omit a return type, leave out `;`) and read each error message until you can predict what each error corresponds to.
3. Write a C program that allocates 1 MB with `malloc`, fills it with the pattern `0xDEADBEEF`, prints the first and last 8 bytes as hex, and frees it. Run it under `valgrind` to confirm zero leaks.

---

Next: [Chapter 3 — Types, integer promotion, and undefined behavior](03-types-and-ub.md).
