#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <cstdint>

namespace mtfss {

class Client {
public:
    Client(const std::string& host, uint16_t port);
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect_to_server();
    void disconnect();
    bool is_connected() const { return socket_ != INVALID_SOCKET; }

    bool login(const std::string& username);
    bool upload_file(const std::string& filepath);
    bool download_file(const std::string& filename);
    bool list_files();
    bool delete_file(const std::string& filename);
    bool rename_file(const std::string& old_name, const std::string& new_name);
    bool quit();

private:
    bool send_command(uint32_t cmd, const std::string& filename = "",
                      const char* payload = nullptr, size_t payload_size = 0);
    bool receive_response();

    std::string host_;
    uint16_t port_;
    SOCKET socket_;
    std::string username_;
};

} // namespace mtfss