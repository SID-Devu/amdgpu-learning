#include "thread_pool.hpp"
#include <cstdio>
#include <vector>

int main() {
    ThreadPool pool(std::thread::hardware_concurrency());

    std::vector<std::future<int>> futs;
    for (int i = 0; i < 1000; i++)
        futs.push_back(pool.submit([i] { return i * i; }));

    long sum = 0;
    for (auto& f : futs) sum += f.get();

    long expected = 0;
    for (int i = 0; i < 1000; i++) expected += i * i;

    std::printf("sum=%ld expected=%ld %s\n",
                sum, expected, sum == expected ? "OK" : "FAIL");
    return sum == expected ? 0 : 1;
}
