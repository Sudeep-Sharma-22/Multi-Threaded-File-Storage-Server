#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "task_queue.h"
#include "thread_pool.h"

#include <cstdint>
#include <atomic>
#include <string>
#include <memory>

namespace mtfss {

class Server {
public:
    Server(uint16_t port, int num_workers);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop();

private:
    void handle_client(SOCKET client_socket, const std::string& client_addr);

    uint16_t port_;
    SOCKET   listen_socket_;
    std::atomic<bool> running_;

    TaskQueue                   task_queue_;
    std::unique_ptr<ThreadPool> thread_pool_;
    
    std::atomic<int> active_connections_;
};

} // namespace mtfss