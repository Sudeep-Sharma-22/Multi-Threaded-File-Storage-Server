#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstddef>

namespace mtfss {

using Task = std::function<void()>;

class TaskQueue {
public:
    explicit TaskQueue(size_t max_size = 64);
    ~TaskQueue() = default;

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    // Blocks if queue is full. Returns false if shut down.
    bool push(Task task);

    // Blocks if queue is empty. Returns false if shut down and empty.
    bool pop(Task& task);

    void shutdown();

    size_t size() const;
    bool   is_shutdown() const;

private:
    std::queue<Task>        queue_;
    size_t                  max_size_;
    mutable std::mutex      mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    bool                    shutdown_;
};

} // namespace mtfss