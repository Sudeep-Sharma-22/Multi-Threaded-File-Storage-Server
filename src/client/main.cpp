#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include "client.h"
#include "cli.h"
#include "../common/types.h"

#include <iostream>
#include <stdexcept>
#include <string>

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

int main(int argc, char* argv[]) {
    try {
        WinsockGuard winsock;

        std::string host = "127.0.0.1";
        uint16_t port = mtfss::DEFAULT_PORT;

        if (argc >= 2) host = argv[1];
        if (argc >= 3) port = static_cast<uint16_t>(std::stoi(argv[2]));

        mtfss::Client client(host, port);
        if (!client.connect_to_server()) {
            std::cerr << "Failed to connect to server at " << host << ":" << port << std::endl;
            std::cerr << "Make sure the server is running first." << std::endl;
            return 1;
        }

        mtfss::CLI cli(client);
        cli.run();

    } catch (const std::exception& e) {
        std::cerr << "[Client] Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}