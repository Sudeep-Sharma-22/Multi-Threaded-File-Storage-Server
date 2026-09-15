#pragma once

#include "task_queue.h"

#include <vector>
#include <thread>
#include <atomic>

namespace mtfss {

class ThreadPool {
public:
    explicit ThreadPool(int num_workers, TaskQueue& queue);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void shutdown();

    int num_workers() const { return static_cast<int>(workers_.size()); }

private:
    void worker_loop(int worker_id);

    std::vector<std::thread> workers_;
    TaskQueue&               queue_;
    std::atomic<bool>        stopped_;
};

} // namespace mtfss