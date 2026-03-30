#include "TaskQueue.h"

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
        task = std::move(tasks_.pop());
        return true;
    }
}

template <typename task_type_T>
template <typename... arguments>
void TaskQueue<task_type_T>::emplace(arguments&&... parameters) {
    write_lock _(rw_lock_);
    tasks_.emplace(std::forward<arguments>(parameters)...);
}