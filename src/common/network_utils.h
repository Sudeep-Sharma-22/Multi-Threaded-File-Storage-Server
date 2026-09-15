#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace mtfss { struct MessageHeader; }

namespace mtfss {

bool send_all(SOCKET sock, const char* data, size_t length);
bool recv_all(SOCKET sock, char* buffer, size_t length);

bool send_header(SOCKET sock, const MessageHeader& hdr);
bool recv_header(SOCKET sock, MessageHeader& hdr);

bool send_message(SOCKET sock, const MessageHeader& hdr,
                  const std::string& username,
                  const std::string& filename,
                  const char* payload = nullptr, size_t payload_size = 0);

bool recv_message(SOCKET sock, MessageHeader& hdr,
                  std::string& username,
                  std::string& filename,
                  std::vector<char>& payload);

} // namespace mtfss