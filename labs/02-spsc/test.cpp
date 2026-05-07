#include "spsc.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

constexpr std::size_t kCount = 10'000'000;

int main() {
    spsc<std::uint64_t, 1024> q;

    std::atomic<bool> done{false};
    std::uint64_t sum = 0;

    auto t0 = std::chrono::steady_clock::now();

    std::thread consumer([&] {
        std::uint64_t v;
        std::uint64_t got = 0;
        while (got < kCount) {
            if (q.pop(v)) {
                sum += v;
                got++;
            } else {
                std::this_thread::yield();
            }
        }
        done = true;
    });

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kCount; i++) {
            while (!q.push(i)) std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    auto t1 = std::chrono::steady_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

    std::uint64_t expected = (kCount * (std::uint64_t)(kCount - 1)) / 2;
    std::printf("sum=%lu (expected %lu) %s\n", sum, expected,
                sum == expected ? "OK" : "FAIL");
    std::printf("throughput: %.2f Mops/s\n", (double)kCount / (ns / 1e9) / 1e6);
    return sum == expected ? 0 : 1;
}
