#include "protocol.h"
#include "network_utils.h"

#include <vector>
#include <iostream>

#ifndef htonll
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((uint64_t)((x) >> 32)))
#endif
#ifndef ntohll
#define ntohll(x) htonll(x)
#endif

namespace mtfss {

bool send_header(SOCKET sock, const MessageHeader& hdr) {
    MessageHeader net_hdr;
    net_hdr.command = htonl(hdr.command);
    net_hdr.status = htonl(hdr.status);
    net_hdr.payload_length = htonll(hdr.payload_length);
    net_hdr.filename_length = htonll(hdr.filename_length);
    net_hdr.username_length = htonll(hdr.username_length);
    return send_all(sock, reinterpret_cast<const char*>(&net_hdr), sizeof(MessageHeader));
}

bool recv_header(SOCKET sock, MessageHeader& hdr) {
    if (!recv_all(sock, reinterpret_cast<char*>(&hdr), sizeof(MessageHeader))) {
        return false;
    }
    hdr.command = ntohl(hdr.command);
    hdr.status = ntohl(hdr.status);
    hdr.payload_length = ntohll(hdr.payload_length);
    hdr.filename_length = ntohll(hdr.filename_length);
    hdr.username_length = ntohll(hdr.username_length);
    return true;
}

bool send_message(SOCKET sock, const MessageHeader& hdr,
                  const std::string& username,
                  const std::string& filename,
                  const char* payload, size_t payload_size) {

    if (!send_header(sock, hdr)) return false;

    if (hdr.username_length > 0) {
        if (!send_all(sock, username.c_str(), static_cast<size_t>(hdr.username_length)))
            return false;
    }

    if (hdr.filename_length > 0) {
        if (!send_all(sock, filename.c_str(), static_cast<size_t>(hdr.filename_length)))
            return false;
    }

    if (hdr.payload_length > 0 && payload != nullptr) {
        if (!send_all(sock, payload, payload_size))
            return false;
    }

    return true;
}

bool recv_message(SOCKET sock, MessageHeader& hdr,
                  std::string& username,
                  std::string& filename,
                  std::vector<char>& payload) {

    if (!recv_header(sock, hdr)) return false;

    if (hdr.username_length > 0) {
        username.resize(static_cast<size_t>(hdr.username_length));
        if (!recv_all(sock, &username[0], static_cast<size_t>(hdr.username_length)))
            return false;
    } else {
        username.clear();
    }

    if (hdr.filename_length > 0) {
        filename.resize(static_cast<size_t>(hdr.filename_length));
        if (!recv_all(sock, &filename[0], static_cast<size_t>(hdr.filename_length)))
            return false;
    } else {
        filename.clear();
    }

    if (hdr.payload_length > 0) {
        payload.resize(static_cast<size_t>(hdr.payload_length));
        if (!recv_all(sock, payload.data(), static_cast<size_t>(hdr.payload_length)))
            return false;
    } else {
        payload.clear();
    }

    return true;
}

bool send_message_meta(SOCKET sock, const MessageHeader& hdr,
                       const std::string& username,
                       const std::string& filename) {
    if (!send_header(sock, hdr)) return false;

    if (hdr.username_length > 0) {
        if (!send_all(sock, username.c_str(), static_cast<size_t>(hdr.username_length)))
            return false;
    }

    if (hdr.filename_length > 0) {
        if (!send_all(sock, filename.c_str(), static_cast<size_t>(hdr.filename_length)))
            return false;
    }

    return true;
}

bool recv_message_meta(SOCKET sock, MessageHeader& hdr,
                       std::string& username,
                       std::string& filename) {
    if (!recv_header(sock, hdr)) return false;

    if (hdr.username_length > 0) {
        username.resize(static_cast<size_t>(hdr.username_length));
        if (!recv_all(sock, &username[0], static_cast<size_t>(hdr.username_length)))
            return false;
    } else {
        username.clear();
    }

    if (hdr.filename_length > 0) {
        filename.resize(static_cast<size_t>(hdr.filename_length));
        if (!recv_all(sock, &filename[0], static_cast<size_t>(hdr.filename_length)))
            return false;
    } else {
        filename.clear();
    }

    // Payload is intentionally NOT read here; caller streams it.
    return true;
}

} // namespace mtfss