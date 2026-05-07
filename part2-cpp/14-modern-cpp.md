# Chapter 14 — Modern C++: move semantics, smart pointers, lambdas

This chapter is the slice of "modern C++" you'll see in any post-2015 systems codebase. ROCm, Mesa, LLVM, MIOpen: they all live here.

## 14.1 Why move semantics

Pre-C++11, returning a `std::vector<int>` from a function copied the contents. For a 1 MB vector that meant `memcpy`-ing a megabyte. The compiler often elided the copy via *return value optimization* (RVO), but you couldn't rely on it.

C++11 added rvalue references and move constructors. Now:

```cpp
std::vector<int> make() {
    std::vector<int> v(1'000'000);
    return v;          /* move (or RVO) */
}
auto big = make();     /* no copy */
```

The vector's move constructor swaps three pointers. Done.

## 14.2 Lvalues, rvalues, xvalues

- **lvalue**: has a name and address. `int x;` — `x` is an lvalue.
- **rvalue**: temporary, can be moved-from. `f()`, `5`, `std::move(x)`.
- **xvalue** ("expiring"): rvalue that names something. `std::move(x)`.

`T&` binds only to lvalues. `T&&` binds only to rvalues. `const T&` binds to both — so it's the safe parameter type when you only read.

```cpp
void f(int& x);        /* only lvalues */
void f(int&& x);       /* only rvalues */
void f(const int& x);  /* both */

int a = 5;
f(a);     /* picks f(int&) */
f(5);     /* picks f(int&&) */
```

## 14.3 `std::move` and `std::forward`

```cpp
std::string s = "hello";
std::string t = std::move(s);    /* s is now valid-but-unspecified */
```

`std::move(x)` is `static_cast<T&&>(x)`. It does nothing at runtime; it changes the type so that move overloads are picked.

`std::forward<T>(x)` is move-or-not depending on `T`:

```cpp
template <class T>
void wrap(T&& arg) {
    inner(std::forward<T>(arg));   /* preserves lvalue/rvalueness */
}
```

In a function template, prefer `std::forward<T>(arg)` to `std::move(arg)` for forwarding references, otherwise you'll always move and break callers passing lvalues.

## 14.4 Smart pointers

```cpp
#include <memory>

auto p = std::make_unique<int>(42);     /* unique_ptr<int> */
auto s = std::make_shared<int>(42);     /* shared_ptr<int> */
```

Use:

- **`unique_ptr<T>`**: sole ownership. Move-only. Zero overhead vs. raw pointer.
- **`shared_ptr<T>`**: reference-counted. Has a control block (~16-24 bytes extra). Atomic refcount; thread-safe to copy/destroy a `shared_ptr` from multiple threads (but the *pointee* still needs its own synchronization).
- **`weak_ptr<T>`**: non-owning observer of a `shared_ptr`. Use to break cycles or to cache something whose lifetime is owned elsewhere.

`make_unique` / `make_shared`:

- `make_shared` allocates the control block + object together, saving an allocation, but the object lifetime is tied to `weak_ptr` count too (memory not freed until last `weak_ptr` dies).

In ROCm/HIP code you will see lots of `unique_ptr<Resource>` for GPU streams, kernels, modules.

## 14.5 Custom deleters

```cpp
struct FileCloser {
    void operator()(FILE *f) const { if (f) std::fclose(f); }
};

std::unique_ptr<FILE, FileCloser> fp(std::fopen("/etc/hostname", "rb"));
```

Or with a lambda:

```cpp
auto fp = std::unique_ptr<FILE, void(*)(FILE*)>(
    std::fopen("/etc/hostname", "rb"),
    [](FILE *f) { if (f) std::fclose(f); });
```

Same trick wraps file descriptors, hipStream_t, drm_fd, etc.

## 14.6 Lambdas

```cpp
auto add = [](int a, int b) { return a + b; };
add(2, 3);

auto print = [out = std::ofstream("/tmp/log")](auto&&... a) mutable {
    ((out << a << ' '), ...);
    out << '\n';
};
```

Lambdas are anonymous classes with `operator()`. The capture list `[…]` declares what they capture:

- `[]` — nothing.
- `[x]` — by value.
- `[&x]` — by reference.
- `[=]` — all locals by value (avoid).
- `[&]` — all by reference (avoid).
- `[this]` — the enclosing object's `this` pointer (lifetime hazard).
- `[x = 5]` — initialized capture (C++14).
- `[x = std::move(p)]` — move-into-capture.

`mutable` lets the lambda mutate by-value captures.

Beware capturing a reference to a stack variable that outlives the lambda. Beware capturing `this` and the object dying.

## 14.7 `std::function` vs. function pointers vs. templates

```cpp
#include <functional>
std::function<int(int,int)> f = add;     /* type-erased callable */
```

`std::function` is convenient but:

- May allocate (small-buffer-optimized for tiny callables).
- Has indirect call overhead.

For perf-critical callbacks, prefer template parameter:

```cpp
template <class F>
void each(int *a, size_t n, F&& fn) { for (size_t i = 0; i < n; i++) fn(a[i]); }
```

Or function pointer when capture is unnecessary.

## 14.8 `auto`, `decltype`, `decltype(auto)`

```cpp
auto x = 5;                 /* int */
auto& r = container[0];     /* reference */
auto v = std::vector{1,2,3};

decltype(x) y = 7;          /* int */
decltype(auto) z = f();     /* preserves reference-ness from f */
```

In modern code, `auto` for locals and range-for loops:

```cpp
for (auto& kv : map) { kv.second++; }
```

In headers and APIs, prefer explicit types — `auto` for return types only when necessary or for very local code.

## 14.9 Range-based for, structured bindings

```cpp
for (const auto& [key, value] : map) { /* ... */ }    /* C++17 */
auto [a, b, c] = std::tuple{1, 2.5, "hi"};
```

These are the polish that makes C++ usable. Use them.

## 14.10 `std::optional`, `std::variant`, `std::expected`

```cpp
std::optional<int> parse(const std::string& s);
auto v = parse(s);
if (v) use(*v);

std::variant<int, std::string> u = 5;
std::visit([](auto&& x){ std::cout << x; }, u);

std::expected<int, std::string> safe();    /* C++23 */
```

`std::expected` is the ergonomic equivalent of returning `int` and an out-error. ROCm and Mesa are slowly moving to it.

## 14.11 `noexcept` revisited

Mark functions `noexcept` when they will not throw. The compiler can:

- skip exception-table generation,
- pick more efficient code paths in STL containers,
- enable strong exception guarantees in callers.

For destructors and move ops, `noexcept` is almost always correct. For other functions, only mark `noexcept` if you are *certain*.

## 14.12 Exercises

1. Write `class String` that owns a heap buffer. Implement copy, move, `operator+=`, `c_str`, with proper `noexcept` markings.
2. Write `template <class T> class SharedRef` (a poor-man's `shared_ptr`) with a control block.
3. Write a `for_each` that takes a `std::function<void(int)>` and another templated on a callable. Benchmark calling each in a tight loop. Compare assembly.
4. Implement an `LRUCache<K,V>` using `std::list<std::pair<K,V>>` plus `std::unordered_map<K, list::iterator>`.
5. Convert any C-style program you wrote in Part I to modern C++ using RAII, smart pointers, and STL containers. Notice the line-count drop.

---

Next: [Chapter 15 — STL and writing your own container](15-stl-and-containers.md).
