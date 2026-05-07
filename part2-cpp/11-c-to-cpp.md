# Chapter 11 — From C to C++: classes, RAII, references

The Linux *kernel* is C. But the AMD GPU stack in *userspace* — Mesa, ROCm runtime (HSA), HIP runtime, ROCclr, MIOpen, RCCL, rocBLAS, MIGraphX — is overwhelmingly C++. So is most NVIDIA software (CUDA runtime, cuDNN). You must be fluent.

This part isn't a complete C++ course. It's the slice of C++ that systems engineers use, in the order that matters for our work.

## 11.1 What C++ adds over C

The big four:

1. **Classes / methods / access control** — encapsulation as a language feature.
2. **RAII (Resource Acquisition Is Initialization)** — destructors give automatic cleanup.
3. **Templates** — type-generic code at compile time.
4. **References** — non-null aliases instead of pointers.

There are dozens of smaller features (operator overloading, exceptions, lambdas, `auto`, namespaces, `constexpr`), but the four above are why C++ exists.

## 11.2 Compiling C++

```bash
g++ -std=c++17 -Wall -Wextra -O2 -g hello.cpp -o hello
clang++ -std=c++20 ... # newer dialect
```

`g++` is just `gcc` with C++ defaults. The Linux kernel uses C; userspace GPU stacks are typically C++17 today. ROCm has been moving to C++17/20.

```cpp
#include <iostream>

int main()
{
    std::cout << "Hello, C++\n";
}
```

`std::cout << x` is a function call (`operator<<`). It is, in many ways, a worse `printf` — slower, harder to localize. Most systems C++ avoids `iostream` and uses `fmt::print` (C++20: `std::print`) or sticks to `printf`.

## 11.3 Namespaces

```cpp
namespace amd {
namespace gpu {
    int do_thing();
}}
```

Or (C++17):

```cpp
namespace amd::gpu {
    int do_thing();
}
```

Use:

```cpp
amd::gpu::do_thing();
using namespace amd;     // pull in everything (avoid in headers)
gpu::do_thing();
```

Headers must never have `using namespace` at top level.

## 11.4 Classes

```cpp
class Counter {
public:
    Counter() : value_(0) {}
    void  inc() { ++value_; }
    int   get() const { return value_; }
private:
    int value_;
};

Counter c;
c.inc();
int v = c.get();
```

Notes:

- `private` members aren't accessible outside.
- `const` after the parameter list means "this method does not modify state."
- The `:` syntax is the **member-initializer list** — preferred over assigning in the body.
- Trailing underscore on member names is a common convention (Google, ROCm).

## 11.5 RAII — the single biggest reason to use C++

A class's *destructor* runs when the object goes out of scope. So **every resource** can be tied to an object's lifetime. No manual `free`, no `goto out_unmap`.

```cpp
class FileHandle {
public:
    explicit FileHandle(const char *path)
        : fp_(std::fopen(path, "rb")) {
        if (!fp_) throw std::runtime_error("open failed");
    }
    ~FileHandle() { if (fp_) std::fclose(fp_); }

    FileHandle(const FileHandle&)            = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    FILE *get() const { return fp_; }
private:
    FILE *fp_;
};

void load() {
    FileHandle f("/etc/hostname");
    /* read from f.get() */
}   /* fclose runs automatically here */
```

This pattern is used for:

- file descriptors,
- mutex locks (`std::lock_guard`),
- heap allocations (`std::unique_ptr`),
- DMA buffers,
- CUDA/HIP streams,
- GPU command buffers.

ROCm, Mesa, and CUDA samples are filled with RAII wrappers. Internalize it.

The `= delete` lines suppress copy. Without them, copying `FileHandle` would `fclose` the same FILE twice. We'll discuss copy/move in Chapter 12.

## 11.6 References

A reference is an alias to an object. Once bound, it cannot be rebound. It cannot be null.

```cpp
int  x = 5;
int& r = x;     /* r is x */
r = 10;         /* x is now 10 */
```

Use references for:

- Function parameters that should not be null and should not be copied:
  ```cpp
  void f(const std::string& s);   /* no copy, no null */
  ```
- Returning aliases.

In kernel-style C, we pass `T *`. In C++, prefer `T&` (or `const T&`) for parameters that "are" the object, not just optionally point to one. Use `T*` when the parameter may be null.

## 11.7 The four kinds of `T`

C++ distinguishes:

- `T` — value (object).
- `T&` — lvalue reference (alias).
- `T&&` — rvalue reference (movable temporary). C++11.
- `T*` — pointer.

`const` orthogonal to all. So `const std::string&` means "alias to a string I won't modify." `std::string&&` means "this is a temporary, you can steal its contents."

We will revisit `&&` and move semantics in Chapter 14.

## 11.8 `new` and `delete` (and why we won't use them)

```cpp
int *p  = new int(5);
delete p;

int *a = new int[100];
delete[] a;
```

Manual `new`/`delete` is C++'s `malloc`/`free`. Modern C++ avoids both — we use `std::unique_ptr`/`std::shared_ptr`/STL containers, which manage memory for us via RAII. We will write our own `unique_ptr` in Chapter 14 to demystify the technique.

## 11.9 References vs. pointers — a rule of thumb

| Need | Use |
|---|---|
| Optionally-null, may rebind | `T *` |
| Never-null, never-rebound, doesn't own | `T &` |
| Never-null, never-rebound, owns | `T` (value) or `std::unique_ptr` |
| Shared ownership | `std::shared_ptr<T>` |
| Out-parameter | `T *` (or return a struct) |
| Input parameter, large object | `const T &` |
| Input parameter, small (≤16B) | `T` (by value) |

## 11.10 Headers and TUs in C++

Same translation-unit model as C. But:

- `class` definitions go in headers.
- Member function *declarations* go in the class body; *definitions* may go in the header (as `inline` or `template`) or a `.cpp`.
- Template definitions almost always live in headers (more in Chapter 13).

Build systems are CMake or Meson; you'll see them in Mesa and ROCm.

## 11.11 Exercises

1. Translate `Counter` from above to a C version using opaque types and `_init`/`_destroy`. Notice how much shorter the C++ version is.
2. Implement `class Vec3 { float x, y, z; };` with `+`, `-`, `dot`, `cross`, `length`. Make every member function `const` where possible.
3. Implement `FileHandle` exactly as shown. Then copy it; observe the compile error caused by `= delete`. Remove the deletes; make a copy; observe a double-`fclose` (try it under ASan).
4. Read `include/CL/sycl.hpp` or any ROCm runtime header. Identify three RAII wrappers.

---

Next: [Chapter 12 — Object lifetime, special members, rule of 0/3/5](12-lifetime-and-rules.md).
