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

template <typename task_type_T>
bool TaskQueue<task_type_T>::is_empty() const
{
    read_lock _(rw_lock_);
    return tasks_.empty();
}

template <typename task_type_T>
size_t TaskQueue<task_type_T>::size() const
{
    read_lock _(rw_lock_);
    return tasks_.size();
}

template <typename task_type_T>
void TaskQueue<task_type_T>::clear()
{
    write_lock _(rw_lock_);
    while (!tasks_.empty())
    {
        tasks_.pop();
    }
}

template <typename task_type_T>
bool TaskQueue<task_type_T>::pop(task_type_T& task)
{
    write_lock _(rw_lock_);
    if (tasks_.empty())
    {
        return false;
    }
    else
    {
        task = std::move(tasks_.front());
        tasks_.pop();
        return true;
    }
}

template <typename task_type_T>
template <typename... arguments>
void TaskQueue<task_type_T>::emplace(arguments&&... parameters) {
    write_lock _(rw_lock_);
    tasks_.emplace(std::forward<arguments>(parameters)...);
}

#endif //LAB3_TASK_QUEUE_H