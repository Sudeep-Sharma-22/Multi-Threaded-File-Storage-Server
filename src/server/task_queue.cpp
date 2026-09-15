#include "task_queue.h"
#include <iostream>

namespace mtfss {

TaskQueue::TaskQueue(size_t max_size)
    : max_size_(max_size), shutdown_(false) {}

bool TaskQueue::push(Task task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);

        not_full_.wait(lock, [this] {
            return shutdown_ || (max_size_ == 0) || (queue_.size() < max_size_);
        });

        if (shutdown_) {
            return false;
        }

        queue_.push(std::move(task));
    }

    not_empty_.notify_one();
    return true;
}

bool TaskQueue::pop(Task& task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);

        not_empty_.wait(lock, [this] {
            return shutdown_ || !queue_.empty();
        });

        if (shutdown_ && queue_.empty()) {
            return false;
        }

        task = std::move(queue_.front());
        queue_.pop();
    }

    not_full_.notify_one();
    return true;
}

void TaskQueue::shutdown() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
}

size_t TaskQueue::size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.size();
}

bool TaskQueue::is_shutdown() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return shutdown_;
}

} // namespace mtfss