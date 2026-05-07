# Chapter 16 — C++ for low-level work: PIMPL, polymorphism cost, ABI

C++ has features for high-level expressiveness, but in systems code we often turn most of them off. This chapter is about *which subset* of C++ you actually use in driver-runtimes, and what each feature costs in instructions, cache lines, and binary size.

## 16.1 The cost of virtual functions

```cpp
struct Base   { virtual void op() = 0; };
struct Foo:Base { void op() override { /* … */ } };
```

Each polymorphic class has a **vtable** — a small static array of function pointers. Each instance has a hidden **vptr** at offset 0 pointing to the vtable. A virtual call is two indirect loads:

```
load   r0, [obj]        ; vptr
load   r1, [r0 + 8]     ; slot for op()
call   r1
```

Cost: ~3-5 ns vs ~0.3 ns for a direct call, primarily from branch-prediction and cache. In a tight loop this can dominate. Mesa, ROCm and CUDA hot paths often avoid virtuals in favor of:

- CRTP (compile-time polymorphism, Chapter 13),
- `std::variant` + `std::visit`,
- function pointers in a struct (the C kernel pattern from Chapter 9),
- branchy switch on an enum.

For *cold* paths (config, initialization), virtual is fine.

## 16.2 PIMPL — Pointer to IMPLementation

PIMPL hides implementation behind an opaque pointer in the header. Pros: build-time decoupling, ABI stability, less recompilation. Cons: an extra heap allocation, an extra indirection.

```cpp
/* foo.h */
class Foo {
public:
    Foo();
    ~Foo();
    Foo(Foo&&) noexcept;
    Foo& operator=(Foo&&) noexcept;

    void do_thing();

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};
```

```cpp
/* foo.cpp */
#include "foo.h"
#include <vector>          /* heavy header — only included here */

struct Foo::Impl {
    std::vector<int> data;
    /* … */
};

Foo::Foo() : p_(std::make_unique<Impl>()) {}
Foo::~Foo() = default;
Foo::Foo(Foo&&) noexcept = default;
Foo& Foo::operator=(Foo&&) noexcept = default;

void Foo::do_thing() { p_->data.push_back(0); }
```

Use PIMPL for shared-library APIs you must keep ABI-stable, or for headers included from many places.

## 16.3 ABI — what it is and why you care

ABI (Application Binary Interface) is the low-level contract: how arguments are passed in registers, how `vptr` is laid out, how the linker mangles names. C++ ABI has been stable on Linux (Itanium ABI) since GCC 5 (2015), but small changes happen.

Key ABI facts:

- **Adding a virtual** to a class changes its vtable layout. Every binary that uses the class must be recompiled.
- **Adding a non-static data member** changes its size. Same.
- **Changing an inline function** in a header that's compiled into multiple shared libraries can produce ODR (one-definition rule) violations.
- **Returning a `std::string`** crosses the ABI; old/new libstdc++ used different layouts ("CXX11 ABI"). You'll see `-D_GLIBCXX_USE_CXX11_ABI=0` in old code.

For shared-library work (HSA runtime, libamdhip64.so), you keep ABI stable by:

1. Using opaque pointer / PIMPL.
2. Versioning symbols (`__attribute__((symver(...)))` or ld scripts).
3. Designing C-style APIs at the boundary, with C++ inside.

The HSA runtime, ROCr, libdrm — all expose **C** APIs over C++ implementations for exactly this reason.

## 16.4 Symbol mangling and `extern "C"`

C++ mangles names to encode types and namespaces:

```bash
nm libfoo.so | c++filt
```

`amd::Buffer::flush() const` becomes something like `_ZNK3amd6Buffer5flushEv`.

To export a function from C++ for C linkage:

```cpp
extern "C" {
    void amd_init(void);
    int  amd_doit(int x);
}
```

Inside `extern "C"`, no overloading is allowed (no name mangling). All public APIs of HSA, OpenCL, ROCm runtime are `extern "C"`.

## 16.5 Linkage and ODR

The One-Definition Rule: every entity may be defined in at most one TU, *except* templates and inline functions, which may be defined identically in many. Violations → UB.

Common ODR breakers:

- Non-`inline` functions defined in headers.
- Class definitions that differ between TUs (e.g. due to `#ifdef`).
- Mixing CXX11 ABI and pre-CXX11 ABI builds.

`-Wodr` (link-time) catches some.

## 16.6 The "vocabulary types" of systems C++

Use these consistently:

- `std::string` for owned strings; `std::string_view` for parameters.
- `std::vector<T>` as the default container; `std::span<T>` for parameters.
- `std::unique_ptr<T>` for ownership; raw `T*` for non-owning.
- `std::optional<T>` for "may not have a value."
- `std::variant<…>` for tagged unions.
- `std::filesystem::path` — for file paths.
- `std::array<T, N>` — fixed-size, stack-resident.

## 16.7 Exception strategy

For systems code there are three valid stances:

1. **No exceptions at all** (`-fno-exceptions`). Used by LLVM core, parts of Mesa, and some embedded code. Errors via `std::expected`-style returns, `bool`, or `enum`.
2. **Exceptions for unrecoverable** errors only. ROCm runtime is roughly here.
3. **Exceptions everywhere.** Some app code; rare in systems.

When in Rome: match the codebase. If you join a `-fno-exceptions` codebase, don't sneak in `throw`.

## 16.8 The `noreturn`, `[[likely]]`, `[[nodiscard]]` attributes

```cpp
[[noreturn]] void abort_msg(const char *m);

[[nodiscard]] int read_byte();   /* warns if return ignored */

if (cond) [[likely]] { … } else [[unlikely]] { … }
```

`[[nodiscard]]` on factory functions and error returns prevents bugs.

## 16.9 Inline assembly (briefly)

You will rarely need inline asm in C++. If you do:

```cpp
unsigned long rdtsc() {
    unsigned hi, lo;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long)hi << 32) | lo;
}
```

Read GCC's "Extended Asm" doc once. The kernel uses inline asm constantly for atomic ops, special instructions, and barriers.

## 16.10 Where C++ shines in our domain

- **Mesa**: Vulkan/OpenGL drivers; lots of C, modern C++ in the new bits (`mesa/anv`, `mesa/vulkan`).
- **ROCm runtime (ROCr/HSA)**: C++17 with C ABI on top.
- **HIP runtime / ROCclr**: C++ all the way down.
- **MIOpen, rocBLAS, RCCL**: C++ libraries.
- **MIGraphX**: C++ inference engine.
- **LLVM/Clang/AMDGPU backend**: heavy C++.

Get comfortable with each of: smart pointers, STL containers, RAII, template metaprogramming basics, lambdas. You don't need template wizardry, but you must be fluent in everyday modern C++.

## 16.11 Exercises

1. Take any C kernel header pattern (e.g. `struct file_operations`) and re-implement it in C++ as a `class` with virtual methods. Estimate the runtime cost per call vs. the function-pointer struct.
2. Take your `Vector<T>` from Chapter 15 and convert it to a PIMPL'd `class FixedVec` whose header doesn't expose `<vector>`.
3. Run `nm -D --defined-only libstdc++.so.6 | head`; pick a symbol; demangle it with `c++filt`.
4. Write a header library `result.h` that defines `template <class T, class E> class Result` similar to `std::expected`. Provide `bool ok()`, `T value()`, `E error()`.
5. Compile a tiny C++ file once with `-fno-exceptions` and once without. Compare binary sizes.

---

End of Part II. Move to [Part III — Operating systems internals](../part3-os/17-what-is-an-os.md).
