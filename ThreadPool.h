#pragma once

#include "TaskQueue.h"
#include <vector>
#include <functional>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <random>
#include <memory>

class ThreadPoolV5 {
public:
    struct TaskData {
        std::function<void()> func;
        std::chrono::steady_clock::time_point enqueue_time;
    };

public:
    ThreadPoolV5() = default;
    ~ThreadPoolV5() { terminate(false); }

public:
    void initialize(size_t queues_count, size_t workers_per_queue);
    void terminate(bool wait_for_tasks = true);

    void pause();
    void resume();

public:
    template <typename task_t, typename... arguments>
    void add_task(task_t&& task, arguments&&... parameters);

public:
    ThreadPoolV5(const ThreadPoolV5& other) = delete;
    ThreadPoolV5(ThreadPoolV5&& other) = delete;
    ThreadPoolV5& operator=(const ThreadPoolV5& rhs) = delete;
    ThreadPoolV5& operator=(ThreadPoolV5&& rhs) = delete;

private:
    void routine(size_t queue_index);

    mutable std::shared_mutex lifecycle_mtx_;

    struct QueueContext {
        TaskQueue<TaskData> queue;
        mutable std::mutex mtx;
        mutable std::condition_variable cv;
    };

    std::vector<std::unique_ptr<QueueContext>> queues_ctx_;
    std::vector<std::thread> workers_;

    size_t queues_count_ = 0;
    size_t workers_per_queue_ = 0;

    std::atomic<bool> is_initializing_{false};
    std::atomic<bool> is_ready_{false};

    std::atomic<bool> paused_{false};
    std::atomic<bool> stop_graceful_{false};
    std::atomic<bool> stop_immediate_{false};
};

template <typename task_t, typename... arguments>
void ThreadPoolV5::add_task(task_t&& task, arguments&&... parameters) {
    std::shared_lock<std::shared_mutex> lock(lifecycle_mtx_);

    if (!is_ready_.load() || stop_graceful_.load() || stop_immediate_.load()) return;

    auto bind = std::bind(std::forward<task_t>(task), std::forward<arguments>(parameters)...);
    TaskData new_task{bind, std::chrono::steady_clock::now()};

    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, queues_count_ - 1);
    size_t target_queue = dist(generator);

    queues_ctx_[target_queue]->queue.emplace(new_task);
    queues_ctx_[target_queue]->cv.notify_one();
}