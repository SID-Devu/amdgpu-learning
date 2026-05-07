// Single-producer single-consumer lock-free ring buffer.
// One producer thread calls push(); one consumer thread calls pop().

#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>

template <class T, std::size_t N>
class spsc {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for this demo");

public:
    bool push(const T& v) noexcept {
        const auto h = head_.load(std::memory_order_relaxed);
        const auto t = tail_.load(std::memory_order_acquire);
        if (((h + 1) & (N - 1)) == t) return false;     // full
        data_[h] = v;
        head_.store((h + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool pop(T& out) noexcept {
        const auto t = tail_.load(std::memory_order_relaxed);
        const auto h = head_.load(std::memory_order_acquire);
        if (h == t) return false;                       // empty
        out = data_[t];
        tail_.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

private:
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) T data_[N];
};
