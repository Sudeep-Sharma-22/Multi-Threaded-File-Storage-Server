#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "server.h"
#include "logger.h"
#include "metadata_manager.h"
#include "../common/types.h"

#include <iostream>
#include <stdexcept>

class WinsockGuard {
public:
    WinsockGuard() {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
    }
    ~WinsockGuard() { WSACleanup(); }
    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;
};

static mtfss::Server* g_server = nullptr;

BOOL WINAPI ctrl_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_CLOSE_EVENT) {
        LOG_INFO("\n[Main] Shutdown signal received...");
        if (g_server) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}

int main() {
    try {
        WinsockGuard winsock;

        mtfss::Server server(mtfss::DEFAULT_PORT, mtfss::DEFAULT_THREAD_POOL);
        g_server = &server;

        SetConsoleCtrlHandler(ctrl_handler, TRUE);

        mtfss::MetadataManager::get_instance().rebuild_from_disk("storage");
        server.start();

        g_server = nullptr;
        LOG_INFO("[Main] Server exited cleanly");

    } catch (const std::exception& e) {
        LOG_ERROR("[Main] Fatal error: " << e.what());
        return 1;
    }

    return 0;
}