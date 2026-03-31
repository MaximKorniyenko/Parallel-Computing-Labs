#include "ThreadPool.h"

void ThreadPoolV5::initialize(size_t queues_count, size_t workers_per_queue) {
    bool expected = false;
    if (!is_initializing_.compare_exchange_strong(expected, true)) return;

    std::unique_lock<std::shared_mutex> lock(lifecycle_mtx_);
    if (stop_graceful_.load() || stop_immediate_.load() || queues_count == 0 || workers_per_queue == 0) return;

    queues_count_ = queues_count;
    workers_per_queue_ = workers_per_queue;

    for (size_t i = 0; i < queues_count_; ++i) {
        queues_ctx_.push_back(std::make_unique<QueueContext>());
    }

    for (size_t q = 0; q < queues_count_; ++q) {
        for (size_t w = 0; w < workers_per_queue_; ++w) {
            workers_.emplace_back(&ThreadPoolV5::routine, this, q);
        }
    }

    is_ready_.store(true);
}

void ThreadPoolV5::pause() {
    paused_.store(true);
}

void ThreadPoolV5::resume() {
    std::shared_lock<std::shared_mutex> lock(lifecycle_mtx_);

    if (!is_ready_.load() || stop_graceful_.load() || stop_immediate_.load()) return;

    paused_.store(false);
    for (size_t i = 0; i < queues_count_; ++i) {
        if (queues_ctx_[i]) queues_ctx_[i]->cv.notify_all();
    }
}

void ThreadPoolV5::terminate(bool wait_for_tasks) {
    std::unique_lock<std::shared_mutex> lock(lifecycle_mtx_);
    if (stop_graceful_.load() || stop_immediate_.load()) return;

    if (wait_for_tasks) {
        stop_graceful_.store(true);
    } else {
        stop_immediate_.store(true);
    }

    paused_.store(false);

    for (size_t i = 0; i < queues_count_; ++i) {
        if (queues_ctx_[i]) queues_ctx_[i]->cv.notify_all();
    }

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
    queues_ctx_.clear();
    queues_count_ = 0;
    workers_per_queue_ = 0;
    is_ready_.store(false);
    stop_graceful_.store(false);
    stop_immediate_.store(false);
    is_initializing_.store(false);
}

void ThreadPoolV5::routine(size_t queue_index) {
    while (true) {
        bool task_acquired = false;
        TaskData task;

        {
            auto& ctx = queues_ctx_[queue_index];
            std::unique_lock<std::mutex> lock(ctx->mtx);

            auto wait_condition = [this, &ctx, &task_acquired, &task] {
                if (stop_immediate_.load()) return true;

                if (paused_.load()) return false;

                task_acquired = ctx->queue.pop(task);

                return task_acquired || (stop_graceful_.load() && ctx->queue.is_empty());
            };

            ctx->cv.wait(lock, wait_condition);
        }

        if (stop_immediate_.load()) {
            return;
        }

        if (stop_graceful_.load() && !task_acquired) {
            return;
        }

        if (task_acquired) {
            task.func();
        }
    }
}