#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t n) : stop_(false) {
        for (std::size_t i = 0; i < n; ++i)
            workers_.emplace_back([this] { worker_loop(); });
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> g(m_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    template <class F, class... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> g(m_);
            if (stop_) throw std::runtime_error("submit on stopped pool");
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> g(m_);
                cv_.wait(g, [&] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread>           workers_;
    std::queue<std::function<void()>>  tasks_;
    std::mutex                          m_;
    std::condition_variable             cv_;
    bool                                stop_;
};
