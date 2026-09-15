#include "thread_pool.h"
#include "logger.h"
#include <iostream>

namespace mtfss {

ThreadPool::ThreadPool(int num_workers, TaskQueue& queue)
    : queue_(queue), stopped_(false)
{
    LOG_INFO("[ThreadPool] Starting " << num_workers << " worker threads");

    workers_.reserve(num_workers);
    for (int i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&ThreadPool::worker_loop, this, i);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) {
        return;
    }

    LOG_INFO("[ThreadPool] Shutting down...");

    queue_.shutdown();

    for (size_t i = 0; i < workers_.size(); ++i) {
        if (workers_[i].joinable()) {
            workers_[i].join();
            LOG_INFO("[ThreadPool] Worker-" << i << " joined");
        }
    }

    LOG_INFO("[ThreadPool] All workers stopped");
}

void ThreadPool::worker_loop(int worker_id) {
    LOG_INFO("[Worker-" << worker_id << "] Started");

    while (true) {
        Task task;

        if (!queue_.pop(task)) {
            break;
        }

        try {
            task();
        } catch (const std::exception& e) {
            LOG_ERROR("[Worker-" << worker_id << "] Task threw exception: "
                      << e.what());
        } catch (...) {
            LOG_ERROR("[Worker-" << worker_id << "] Task threw unknown exception");
        }
    }

    LOG_INFO("[Worker-" << worker_id << "] Exiting");
}

} // namespace mtfss