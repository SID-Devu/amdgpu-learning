# Chapter 12 — Object lifetime, special members, rule of 0/3/5

C++ controls the lifecycle of every object via *special member functions*. Get these wrong and you have memory corruption, double-free, or thread races. Get them right and your code is leak-proof and exception-safe.

## 12.1 The six special member functions

For every class, the compiler may generate:

```cpp
class T {
public:
    T();                          /* default constructor */
    T(const T& other);            /* copy constructor */
    T(T&& other) noexcept;        /* move constructor   (C++11) */
    T& operator=(const T& other); /* copy assignment */
    T& operator=(T&& other) noexcept; /* move assignment (C++11) */
    ~T();                         /* destructor */
};
```

If you don't define any, you get all six. If you define one, the rules about which are auto-generated change. Memorize the safe rule:

> **Rule of 0**: design so you don't need any. Use members that already manage themselves (`std::unique_ptr`, `std::vector`, `std::string`).
>
> **Rule of 5**: if you define one of {dtor, copy ctor, copy assign, move ctor, move assign}, define (or `delete`) all five.
>
> **Rule of 3** (pre-C++11): same idea without moves.

## 12.2 The order of construction and destruction

Members are constructed in *declaration* order, regardless of the order in the initializer list. They are destroyed in *reverse* declaration order. Base classes are constructed before derived members and destroyed after.

```cpp
class Cache {
public:
    Cache(size_t cap) : data_(cap), tail_(0) {}    /* OK */
private:
    std::vector<int> data_;
    size_t           tail_;
};
```

This compiles and works because `data_` is declared first. If you reverse declaration order to `tail_, data_`, the initializer list still constructs `tail_` first. Compilers may warn about "out of order initialization." Always **match the initializer list to declaration order**.

## 12.3 Construction options: default, value, copy, move

```cpp
std::string a;          /* default-constructed */
std::string b("hello"); /* constructed from a const char* */
std::string c = b;      /* copy */
std::string d(b);       /* copy */
std::string e = std::move(b); /* move; b is now valid-but-unspecified */
```

A `move` doesn't necessarily change anything. It is a *cast* from `T&` to `T&&` that lets the destination "steal" the source's resources. After the move, the source must remain destructible and assignable, but its content is undefined.

`std::move` is **not** a function that moves. It's a type cast. The actual transfer happens in the move constructor / move assignment of the destination.

## 12.4 A concrete RAII type with the rule of 5

```cpp
template <class T>
class UniquePtr {
public:
    UniquePtr() noexcept : p_(nullptr) {}
    explicit UniquePtr(T *p) noexcept : p_(p) {}

    ~UniquePtr() { delete p_; }

    /* No copying */
    UniquePtr(const UniquePtr&)            = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    /* Move */
    UniquePtr(UniquePtr&& other) noexcept : p_(other.p_) {
        other.p_ = nullptr;
    }
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete p_;
            p_ = other.p_;
            other.p_ = nullptr;
        }
        return *this;
    }

    T*  get()        const noexcept { return p_; }
    T&  operator*()  const          { return *p_; }
    T*  operator->() const noexcept { return p_; }

    explicit operator bool() const noexcept { return p_ != nullptr; }

    T* release() noexcept { T *t = p_; p_ = nullptr; return t; }
    void reset(T *p = nullptr) noexcept { delete p_; p_ = p; }

private:
    T *p_;
};
```

Read every line. This is `std::unique_ptr` minus the deleter template parameter. We:

- Disable copy (a unique pointer cannot be copied).
- Implement move (transfers ownership).
- Provide `release` (give up ownership without deleting) and `reset` (replace, deleting the old).

`noexcept` is critical. Many STL algorithms only use the move constructor if it's `noexcept`; otherwise they fall back to copy.

## 12.5 Why `noexcept` matters

Consider `std::vector<T>` growing. It must move (or copy) all elements to a bigger buffer. If `T`'s move can throw, then mid-move an exception could leave the vector in a weird state. C++'s strong exception guarantee says: "if you can't move-without-throwing, copy instead."

For systems code, you usually want `noexcept` move ops. They are a perf win and a correctness win.

## 12.6 Inheritance and virtual destructors

If a class has any `virtual` method, its destructor *must* be virtual:

```cpp
class Base {
public:
    virtual ~Base() = default;
    virtual void op() = 0;          /* pure virtual */
};

class Derived : public Base {
public:
    void op() override { /* ... */ }
};

Base *b = new Derived;
delete b;       /* calls Derived::~Derived() because dtor is virtual */
```

If `~Base()` weren't virtual, deleting through a `Base*` would skip `Derived::~Derived` — UB. Many CUDA/HIP-era bugs stem from missing `virtual` destructors.

`override` (C++11) tells the compiler "this method overrides a base virtual"; if the signature mismatches, you get a compile error. **Always** write `override`.

## 12.7 Slicing

```cpp
Derived d;
Base   b = d;     /* SLICED: only Base part copied; vtable lost */
```

Passing derived-by-value where a base-by-value is expected silently slices. Always pass polymorphic objects by reference or pointer:

```cpp
void use(const Base& b);
void use(const Base *b);
```

## 12.8 `final`

```cpp
class Sealed final { ... };
class D : public Base { void op() final override; };
```

`final` on a class disallows further inheritance. `final` on a method disallows further override. Use it when you mean it; it lets the compiler devirtualize.

## 12.9 `default` and `delete`

```cpp
class T {
public:
    T() = default;        /* explicitly want compiler-generated */
    T(const T&) = delete; /* explicitly want it to NOT exist */
};
```

If you make a class non-copyable, prefer `= delete` over making the copy ctor private without a definition. The error is at compile time and clearer.

## 12.10 Initializer lists, `{}` initialization

C++11 brace-init:

```cpp
int x{};         /* zero-init */
int a[]{1,2,3};
std::vector<int> v{1,2,3};
struct P{ int x; int y; };
P p{1, 2};
P q = {1, 2};

/* Important: brace-init disallows narrowing */
int n{3.5};      /* error: narrows double to int */
```

Prefer `{}` over `()` for initialization. It's safer and works uniformly across types. Exceptions: when the type has a `std::initializer_list` constructor, `{}` may pick that overload (e.g. `std::vector<int>{10}` is `[10]`, not "10 default-inits").

## 12.11 Avoiding common lifetime bugs

- **Returning reference to local**: UB.
- **Returning span/view of a temporary**: UB.
- **Storing a reference in a struct then outliving the referent**: UB.
- **Calling a virtual method from a constructor**: only the current class's version runs (the derived class isn't constructed yet).
- **Throwing from a destructor**: program likely terminates if another exception is in flight.

## 12.12 Exercises

1. Write `class Buffer` that owns a `char *` allocated with `new[]`. Implement the rule of 5. Test: copy, move, resize, exception during copy.
2. Make `Buffer` non-copyable but movable. Implement `operator+=` for concatenation.
3. Convert your `class FileHandle` (Chapter 11) into a templated `Handle<Resource, Closer>` with a deleter callable.
4. Make a base `class GpuObject` with virtual destructor, two derived `class Buffer` and `class Texture`. Store both in `std::vector<std::unique_ptr<GpuObject>>` and call virtual methods.
5. Read `<memory>` (`/usr/include/c++/*/memory`) and find `std::unique_ptr`'s actual definition. Compare to your version.

---

Next: [Chapter 13 — Templates, concepts, and the type system](13-templates.md).
