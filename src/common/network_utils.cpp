#include "network_utils.h"
#include <iostream>

namespace mtfss {

bool send_all(SOCKET sock, const char* data, size_t length) {
    size_t total_sent = 0;

    while (total_sent < length) {
        int bytes_sent = send(
            sock,
            data + total_sent,
            static_cast<int>(length - total_sent),
            0
        );

        if (bytes_sent == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAECONNRESET && err != WSAETIMEDOUT) {
                std::cerr << "[net] send() failed: WSA error " << err << std::endl;
            }
            return false;
        }

        if (bytes_sent == 0) {
            std::cerr << "[net] send() returned 0 -- connection closed" << std::endl;
            return false;
        }

        total_sent += static_cast<size_t>(bytes_sent);
    }

    return true;
}

bool recv_all(SOCKET sock, char* buffer, size_t length) {
    size_t total_received = 0;

    while (total_received < length) {
        int bytes_received = recv(
            sock,
            buffer + total_received,
            static_cast<int>(length - total_received),
            0
        );

        if (bytes_received == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAECONNRESET && err != WSAETIMEDOUT) {
                std::cerr << "[net] recv() failed: WSA error " << err << std::endl;
            }
            return false;
        }

        if (bytes_received == 0) {
            return false;
        }

        total_received += static_cast<size_t>(bytes_received);
    }

    return true;
}

} // namespace mtfss