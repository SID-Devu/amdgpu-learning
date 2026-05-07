# Chapter 8 — The preprocessor, headers, linkage, build systems

The C preprocessor is a textual macro system that runs *before* the compiler sees your code. The kernel uses it heavily. So does every driver. You must read and write macros confidently.

## 8.1 Directives

```c
#include <header.h>     /* search system paths */
#include "header.h"     /* search local first, then system */
#define X 42
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#undef X
#if   defined(CONFIG_FOO)
#elif defined(CONFIG_BAR)
#else
#endif
#ifdef CONFIG_FOO
#ifndef HEADER_H
#error "must define X"
#warning "deprecated"
#pragma once
```

`#include` is **textual paste**. The compiler then sees the union of all the included files as one giant translation unit.

## 8.2 Macro pitfalls

```c
#define SQ(x) x*x
SQ(1+2)            /* expands to 1+2*1+2 = 5, not 9 */
```

Always parenthesize: `#define SQ(x) ((x)*(x))`.

```c
#define INCR(x) ((x)+1)
INCR(i++)          /* still safe — no double-eval */

#define MAX(a,b) ((a) > (b) ? (a) : (b))
MAX(i++, j++)      /* increments i or j twice. BAD. */
```

To avoid double-evaluation, kernel uses statement expressions (GCC extension):

```c
#define max(a, b) ({               \
    typeof(a) _a = (a);            \
    typeof(b) _b = (b);            \
    _a > _b ? _a : _b;             \
})
```

`typeof` and `({ … })` are GCC extensions that the kernel relies on.

## 8.3 `#` and `##`

```c
#define STR(x)   #x          /* stringify */
#define CAT(a,b) a##b        /* token paste */

STR(hello)         /* "hello" */
CAT(foo, _bar)     /* foo_bar */
```

The kernel uses these in tracing macros and in `EXPORT_SYMBOL`.

## 8.4 Header guards and `#pragma once`

Every header must be safe to include twice:

```c
/* foo.h */
#ifndef FOO_H
#define FOO_H
/* declarations */
#endif
```

Or:

```c
#pragma once
```

`#pragma once` is a near-universal extension (GCC, Clang, MSVC). Kernel uses include guards. Either is fine in your own code; be consistent.

## 8.5 What goes in a header

In a header, put:

- Type declarations (`struct`, `enum`, `typedef`).
- Function *declarations* (prototypes).
- `extern` global declarations.
- Inline functions or `static inline` helpers.
- Macros.

Do **not** put:

- Function *definitions* (bodies). They will be redefined in every TU that includes the header → linker error.
- Global variable definitions. (Same reason.)

Exception: `static inline` and `inline` functions are OK in headers because each TU gets its own copy or because the symbol has special linkage.

## 8.6 Linkage

Every name has a *linkage*:

- **External linkage**: visible to other translation units (TUs). Default for non-`static` file-scope names.
- **Internal linkage**: visible only within a TU. `static` at file scope.
- **None**: local variables, function parameters.

If you define a function `foo` in two .c files without `static`, the linker complains: multiple definition.

Static helper inside a .c:

```c
static int helper(int x) { return x + 1; }
```

You may define this in a header *only* if you also `static inline` it.

## 8.7 The `inline` keyword

`inline` is a hint. The compiler can inline non-`inline` functions and decline to inline `inline` ones. To *force*:

```c
__attribute__((always_inline)) static inline int hot(int x) { return x; }
```

Or to forbid:

```c
__attribute__((noinline)) int slow(int x) { return x; }
```

Kernel macros: `noinline`, `__always_inline`, `__force_inline`.

## 8.8 The build pipeline of a real C project

```
src/main.c     ─┐
src/util.c     ─┼─[preprocess+compile]→ *.o ─[link]→ executable
src/io.c       ─┘
                          ↑
include/ headers ─────────┘ via -I
external libs   ──────────────────────────[ld -l]
```

Build systems:

- **Make / GNU Make**: text-based, used by the kernel (Kbuild) and many older C projects.
- **CMake**: declarative, generates Makefiles or Ninja files. Mesa, ROCm, LLVM use CMake.
- **Meson + Ninja**: simpler than CMake, also used by Mesa.
- **Bazel**: Google's; rare in the Linux ecosystem.

You will primarily use Make for the kernel and CMake/Meson for userspace.

### A real Makefile pattern

```make
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -g -Iinclude
LDFLAGS :=
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=build/%.o)
BIN     := build/app

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

-include $(OBJ:.o=.d)

clean:
	rm -rf build

.PHONY: all clean
```

The `-MMD -MP` flags emit `.d` files listing each `.o`'s header dependencies. Then `-include` them so changing a header rebuilds everything that uses it. Get this right and you save hours of debugging stale builds.

## 8.9 `pkg-config`

For external libraries:

```bash
pkg-config --cflags --libs libdrm
# -I/usr/include/libdrm  -ldrm
```

```make
CFLAGS  += $(shell pkg-config --cflags libdrm)
LDFLAGS += $(shell pkg-config --libs libdrm)
```

When you write a userspace GPU tool with libdrm, you'll do exactly this.

## 8.10 Static vs. dynamic libraries

```bash
# Static lib
ar rcs libutil.a util.o
gcc main.o -L. -lutil -o app    # links util.o into app

# Shared lib
gcc -shared -fPIC -o libutil.so util.o
gcc main.o -L. -lutil -o app
```

Static: code is copied into the executable; bigger; no runtime dep.
Shared: code is loaded at runtime via `ld.so`; smaller; ABI compatibility matters.

The kernel itself is one giant statically-linked executable (`vmlinux`) plus dynamically-loadable modules (`*.ko` — compiled relocatable objects).

## 8.11 The kernel build system (Kbuild)

The kernel uses `make` plus a top-level `Kbuild` system. A simple module's Makefile:

```make
obj-m := hello.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

`obj-m := hello.o` says "build hello.ko." The build runs in the kernel build tree (`-C $(KDIR)`) but compiles files from your dir (`M=$(PWD)`). We will build our first module in Chapter 29.

For a multi-file module:

```make
obj-m := mydrv.o
mydrv-y := main.o util.o regs.o
```

For tristate options (`y`/`m`/`n`):

```make
obj-$(CONFIG_MYDRV) += mydrv.o
```

The `Kconfig` file declares the option; `make menuconfig` lets the user choose.

## 8.12 Reading a real driver Makefile

```make
# drivers/gpu/drm/amd/amdgpu/Makefile (excerpt)
amdgpu-y := amdgpu_drv.o amdgpu_kms.o amdgpu_device.o \
            amdgpu_atombios.o amdgpu_bo_list.o \
            amdgpu_cs.o amdgpu_object.o amdgpu_gem.o \
            amdgpu_mn.o amdgpu_sa.o amdgpu_sync.o \
            amdgpu_fence.o amdgpu_ttm.o amdgpu_pll.o \
            ...

obj-$(CONFIG_DRM_AMDGPU) += amdgpu.o
```

This builds the giant `amdgpu.ko` module. When you contribute a new file to amdgpu you'll add it here.

## 8.13 Exercises

1. Write `MAX`, `MIN`, `CLAMP`, `ARRAY_SIZE`, `STRINGIFY`, `CONCAT` macros. Make them work on any types; protect against double-evaluation using `typeof`.
2. Build a 3-file project (header + 2 .c) with a Makefile that supports `make`, `make clean`, and dependency tracking via `-MMD`.
3. Use `pkg-config` to write a Makefile that links against `libpng`. Compile a tiny program that opens a PNG.
4. Write a `Kconfig` and `Makefile` pair so that your `hello.ko` module from Chapter 29 (you'll write it later) can be built either as `=y` (built-in) or `=m` (module) when integrated into a kernel tree.
5. Read the top of `Makefile` in the kernel tree; identify how `KBUILD_CFLAGS` is constructed.

---

Next: [Chapter 9 — Function pointers, callbacks, opaque types, plugin design](09-function-pointers.md).
