# Chapter 13 — Templates, concepts, and the type system

Templates are how C++ does generic programming. They are also the principal source of "1500-line error messages." Used correctly, they are how the STL, Eigen, fmt, and most of ROCm runtime are built.

## 13.1 Function templates

```cpp
template <class T>
T max_of(T a, T b) { return a > b ? a : b; }

int  m1 = max_of(3, 4);              /* T = int */
auto m2 = max_of(1.0, 2.0);          /* T = double */
auto m3 = max_of<int>(3, 4.0);       /* explicit T */
```

The compiler *instantiates* a separate function for each unique `T` you use. Templates are not erased at runtime; they generate concrete code at compile time. This is unlike Java/C# generics.

## 13.2 Class templates

```cpp
template <class T>
class Vec {
public:
    Vec() : data_(nullptr), size_(0), cap_(0) {}
    /* … */
    void push(T v);
    T&   operator[](size_t i);
private:
    T     *data_;
    size_t size_, cap_;
};

template <class T>
void Vec<T>::push(T v) { /* … */ }
```

Note the funny syntax for out-of-line member definitions. Most class-template implementations live in headers because the compiler must see the body to instantiate.

## 13.3 Specialization and overloading

You can provide a custom implementation for a specific type:

```cpp
template <class T> struct is_pointer { static constexpr bool value = false; };
template <class T> struct is_pointer<T*> { static constexpr bool value = true; };
```

`is_pointer<int>::value == false`, `is_pointer<int*>::value == true`. This is partial specialization.

Function templates don't have partial specialization, but they do have overloading; use overloads to specialize behavior.

## 13.4 Template type deduction

Three contexts:

```cpp
template <class T> void f(T x);   /* by value: T = remove_cvref */
template <class T> void g(T& x);  /* by ref: T may be const */
template <class T> void h(T&& x); /* forwarding ref: special rules */
```

`T&&` in a template parameter is a *forwarding reference* (sometimes called "universal reference"): it deduces to `T&` if the argument is an lvalue and `T` if rvalue. Combined with `std::forward<T>(x)`, this is how `emplace_back`, `make_unique`, etc. perfect-forward arguments.

```cpp
template <class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

Read each piece:

- `class... Args` — variadic template parameter pack.
- `Args&&...` — pack of forwarding references.
- `std::forward<Args>(args)...` — preserves lvalue/rvalue-ness of each.

## 13.5 SFINAE and `enable_if`

"Substitution Failure Is Not An Error." If template substitution produces an invalid type, the candidate is silently removed from the overload set instead of producing a hard error.

```cpp
template <class T,
          class = std::enable_if_t<std::is_integral_v<T>>>
T abs_int(T v) { return v < 0 ? -v : v; }
```

`enable_if_t<true>` is `void`; `enable_if_t<false>` is ill-formed → SFINAE removes the overload. Hideously verbose. C++20 cleaned this up with **concepts**.

## 13.6 Concepts (C++20)

```cpp
template <class T>
concept Integral = std::is_integral_v<T>;

template <Integral T>
T abs_int(T v) { return v < 0 ? -v : v; }
```

Or:

```cpp
template <class T>
requires Integral<T>
T abs_int(T v) { ... }
```

Or short:

```cpp
auto abs_int(Integral auto v) { ... }
```

Concepts give clear error messages and act as documentation. ROCm and modern Mesa are starting to adopt C++20 concepts.

## 13.7 Variadic templates and parameter packs

```cpp
template <class... Ts>
void print_all(Ts... args) {
    (std::cout << ... << args) << '\n';   /* C++17 fold expression */
}

print_all(1, "hi", 3.14);
```

Older syntax used recursion:

```cpp
void print_all() {}                       /* base */
template <class T, class... Ts>
void print_all(T x, Ts... xs) {
    std::cout << x << ' ';
    print_all(xs...);
}
```

Fold expressions (C++17) made this much cleaner.

## 13.8 `constexpr` and compile-time evaluation

```cpp
constexpr int factorial(int n) { return n <= 1 ? 1 : n * factorial(n-1); }
constexpr int x = factorial(10);   /* computed at compile time */
```

`constexpr` lets a function run at compile time when its inputs are constants. C++20 expands this with `consteval` (always at compile time) and `constinit` (must be initialized at compile time but is a runtime variable thereafter).

`constexpr if`:

```cpp
template <class T>
auto safe_abs(T v) {
    if constexpr (std::is_signed_v<T>)
        return v < 0 ? -v : v;
    else
        return v;
}
```

The unused branch is *not compiled* for that instantiation. This replaces many SFINAE tricks.

## 13.9 Type traits

In `<type_traits>`:

```cpp
std::is_same_v<T, U>
std::is_integral_v<T>
std::is_pointer_v<T>
std::is_const_v<T>
std::remove_cv_t<T>
std::decay_t<T>
std::conditional_t<cond, A, B>
```

Use these in metaprogramming and as concepts' building blocks.

## 13.10 CRTP — Curiously Recurring Template Pattern

A C++ idiom for static polymorphism:

```cpp
template <class Derived>
class Base {
public:
    void run() { static_cast<Derived*>(this)->doit(); }
};

class Foo : public Base<Foo> {
public:
    void doit() { /* … */ }
};

Foo f;
f.run();    /* dispatches to Foo::doit at compile time */
```

No vtable. Useful for hot paths where virtual dispatch is too expensive. Eigen, ROCm runtime command-list builders, and some Mesa code use CRTP.

## 13.11 The cost of templates

- **Compile time** explodes with template usage. ROCm device builds can take minutes per file.
- **Binary size** can balloon if many instantiations exist.
- **Error messages** are infamous; learn to read them by ignoring 80% of the noise and finding the *first* diagnostic before the cascade.

Mitigations:

- `extern template class Foo<int>;` to prevent instantiation in this TU.
- Move template definitions behind PIMPL (Chapter 16) when possible.

## 13.12 Exercises

1. Implement `template <class T> class Vector` with `push_back`, `operator[]`, `size`, copy/move semantics, and exception safety. Compare to `std::vector`.
2. Implement `make_unique<T>(args...)` using forwarding references.
3. Implement `Optional<T>` (a poor-man's `std::optional`).
4. Implement a `Tuple<Ts...>` using inheritance recursion. It will be ugly. Then read how the standard implements it.
5. Read `template<class T> class shared_ptr` from `<memory>` (it's intricate). Identify the control block, the `weak_ptr` slot, the deleter type-erasure.

---

Next: [Chapter 14 — Modern C++: move semantics, smart pointers, lambdas](14-modern-cpp.md).
