#ifndef LAB3_TASK_QUEUE_H
#define LAB3_TASK_QUEUE_H

#include <queue>
#include <thread>
#include <mutex>
#include <shared_mutex>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

template <typename task_type_T>
class TaskQueue {
public:
    inline TaskQueue() = default;
    inline ~TaskQueue() { clear(); };

    [[nodiscard]] inline bool is_empty() const;
    [[nodiscard]] inline size_t size() const;
public:
    inline void clear();
    inline bool pop(task_type_T &task);

    template <typename ... arguments>
    inline void emplace(arguments&& ...);
public:
    TaskQueue(const TaskQueue& other) = delete;
    TaskQueue(TaskQueue&& other) = delete;
    TaskQueue& operator=(const TaskQueue& rhs) = delete;
    TaskQueue& operator=(TaskQueue&& rhs) = delete;
private:
    mutable read_write_lock rw_lock_;
    std::queue<task_type_T> tasks_;
};

#endif //LAB3_TASK_QUEUE_H