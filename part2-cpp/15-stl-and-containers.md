# Chapter 15 — STL and writing your own container

The Standard Template Library is C++'s killer feature. You must know what's in it, when to reach for what, and what each costs. Then we'll build a small container to demystify the implementation.

## 15.1 What's in the STL

| Container | Underlying | Insert | Lookup | Notes |
|---|---|---|---|---|
| `std::vector<T>` | contiguous array | amortized O(1) push_back | O(1) random | THE default container |
| `std::array<T,N>` | fixed array | n/a | O(1) | stack-friendly |
| `std::deque<T>` | array of arrays | O(1) push_front/back | O(1) random | rarely needed |
| `std::list<T>` | doubly-linked | O(1) anywhere | O(n) | rarely; cache-hostile |
| `std::forward_list<T>` | singly-linked | O(1) | O(n) | very rarely |
| `std::map<K,V>` | red-black tree | O(log n) | O(log n) | ordered |
| `std::set<T>` | red-black tree | O(log n) | O(log n) | |
| `std::unordered_map<K,V>` | hash table | avg O(1) | avg O(1) | high constant factor |
| `std::unordered_set<T>` | hash table | avg O(1) | avg O(1) | |
| `std::stack<T>` | adapter | | | wraps deque |
| `std::queue<T>` | adapter | | | wraps deque |
| `std::priority_queue<T>` | adapter | O(log n) push/pop | | binary heap |

> **Default to `std::vector<T>`** unless you have a specific reason. Cache locality beats algorithmic complexity for small-to-medium n.

## 15.2 `<algorithm>`

```cpp
#include <algorithm>
std::sort(v.begin(), v.end());
std::sort(v.begin(), v.end(), [](auto&a, auto&b){ return a.x < b.x; });
auto it = std::find(v.begin(), v.end(), 42);
auto it = std::lower_bound(v.begin(), v.end(), 42);
auto sum = std::accumulate(v.begin(), v.end(), 0);
v.erase(std::remove_if(v.begin(), v.end(),
        [](int x){ return x < 0; }), v.end());        /* erase-remove idiom */
```

C++20 `std::ranges` cleans this up:

```cpp
std::ranges::sort(v);
auto it = std::ranges::find(v, 42);
```

## 15.3 Iterators

Iterators are generalized pointers. Five categories:

- **Input** (read once, advance) — e.g. stream iterators.
- **Output** (write once, advance).
- **Forward** (re-read OK).
- **Bidirectional** — `std::list`.
- **Random-access** — `std::vector`, raw pointers.

Algorithms specify what they need. `std::sort` needs random-access; `std::find` only forward.

In C++20, **ranges** generalize iterator pairs into `std::ranges::range` objects.

## 15.4 Iterator invalidation — the silent killer

| Op | Invalidates |
|---|---|
| `vector::push_back` if grew | all iterators/references |
| `vector::insert/erase` | iterators ≥ point; references |
| `unordered_map` rehash | all iterators (on insert) |
| `map::erase` | only the erased iterator |
| `list::insert` | none |

Reading a stale iterator is UB. ASan + iterator debug mode (`-D_GLIBCXX_DEBUG`) catches most.

## 15.5 The cost of small operations

Approximate, modern x86, in-cache:

| Op | Cycles |
|---|---|
| Integer add | 1 |
| L1 hit | 4 |
| L2 hit | 12 |
| L3 hit | 30 |
| DRAM access | 100–300 |
| Branch mispredict | 15–20 |
| `std::vector::push_back` (no grow) | ~10 |
| `std::unordered_map::find` | 100–300 (chain follow) |
| `std::map::find` | 200–500 (log n with cache misses) |
| `std::shared_ptr` copy | 30 (atomic inc) |
| Heap alloc (`malloc`) | 100–1000 |

Cache misses dominate. `std::vector` linear search beats `std::map<int,int>` lookup until n is well into the hundreds.

## 15.6 Allocators (briefly)

Every STL container has an `Allocator` template parameter. Default is `std::allocator<T>` which calls `::operator new`. For systems work you sometimes want custom allocators (arena, pool, GPU pinned memory).

```cpp
template <class T>
struct PoolAlloc {
    using value_type = T;
    T* allocate(size_t n);
    void deallocate(T*, size_t);
};
```

Don't write one until you measure the standard one is the bottleneck.

## 15.7 Building your own `Vector<T>`

Now we implement a small `std::vector` clone to understand growth, exception safety, allocator awareness, and move ops.

```cpp
template <class T>
class Vector {
public:
    Vector() : data_(nullptr), size_(0), cap_(0) {}

    explicit Vector(size_t n) : Vector() { reserve(n); for (size_t i=0;i<n;i++) emplace_back(); }

    Vector(const Vector& o) : Vector() {
        reserve(o.size_);
        for (size_t i = 0; i < o.size_; i++) push_back(o.data_[i]);
    }
    Vector(Vector&& o) noexcept
        : data_(o.data_), size_(o.size_), cap_(o.cap_) {
        o.data_ = nullptr; o.size_ = 0; o.cap_ = 0;
    }
    Vector& operator=(Vector o) noexcept {     /* copy-and-swap */
        swap(o); return *this;
    }
    ~Vector() {
        clear();
        ::operator delete(data_);
    }

    void swap(Vector& o) noexcept {
        std::swap(data_, o.data_);
        std::swap(size_, o.size_);
        std::swap(cap_,  o.cap_);
    }

    size_t size()     const noexcept { return size_; }
    size_t capacity() const noexcept { return cap_; }

    T&       operator[](size_t i)       { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    T*       begin()       { return data_; }
    T*       end()         { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end()   const { return data_ + size_; }

    void reserve(size_t n) {
        if (n <= cap_) return;
        T *new_data = static_cast<T*>(::operator new(n * sizeof(T)));
        size_t i = 0;
        try {
            for (; i < size_; i++) new (new_data + i) T(std::move_if_noexcept(data_[i]));
        } catch (...) {
            for (size_t j = 0; j < i; j++) new_data[j].~T();
            ::operator delete(new_data);
            throw;
        }
        for (size_t j = 0; j < size_; j++) data_[j].~T();
        ::operator delete(data_);
        data_ = new_data;
        cap_  = n;
    }

    template <class... Args>
    T& emplace_back(Args&&... args) {
        if (size_ == cap_) reserve(cap_ ? cap_ * 2 : 4);
        new (data_ + size_) T(std::forward<Args>(args)...);
        return data_[size_++];
    }
    void push_back(const T& v) { emplace_back(v); }
    void push_back(T&& v)      { emplace_back(std::move(v)); }

    void pop_back() { data_[--size_].~T(); }

    void clear() {
        for (size_t i = 0; i < size_; i++) data_[i].~T();
        size_ = 0;
    }

private:
    T     *data_;
    size_t size_, cap_;
};
```

Things to note:

- We use `::operator new` for raw memory and *placement new* (`new (ptr) T(args)`) to construct in-place. We call destructors explicitly with `data_[i].~T()`. This separation lets us own *capacity* separate from *constructed objects*.
- `move_if_noexcept` moves only when the move ctor is `noexcept`; otherwise copies, preserving the strong exception guarantee on `reserve`.
- `swap` then `operator=(Vector o)` is the **copy-and-swap idiom**, automatic exception-safe assignment.
- Growth factor 2× is standard; some implementations use 1.5× for better realloc behavior.

This is ~80 lines and approximately what `libstdc++` does in 4000.

## 15.8 STL string and `string_view`

```cpp
#include <string>
#include <string_view>

void log(std::string_view sv);     /* take by view, no copy, no allocate */
log("hello");
log(std::string("world"));
```

`std::string_view` is a non-owning `(ptr, len)` pair. It's the C++17 way to take a "string parameter" without forcing the caller to allocate. Beware: don't return a `string_view` to a temporary string — UB.

`std::span<T>` (C++20) is the same idea for arbitrary contiguous ranges.

## 15.9 Hash maps in practice

`std::unordered_map` is OK. For perf-sensitive systems code, look at:

- `tsl::robin_map` — open-addressing.
- `absl::flat_hash_map` — Google's; better cache behavior.
- `boost::unordered_flat_map`.

ROCm-related projects often use Abseil. Know the trade-offs.

## 15.10 Exercises

1. Implement `Vector<T>` exactly as above. Write a test with `int`, with `std::string`, and with a class that throws on copy.
2. Implement `String` with small-string optimization (≤15 bytes inline, longer on heap).
3. Implement an open-addressing hash map with linear probing for `Map<K,V>`.
4. Benchmark `std::vector<int>::push_back` 1e7 times with reserve vs. without. Plot.
5. Implement an LRU cache using your `Vector`-backed allocator and `std::unordered_map`. Test with 1e6 random ops.

---

Next: [Chapter 16 — C++ for low-level work: PIMPL, polymorphism cost, ABI](16-cpp-low-level.md).
