#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

namespace mtfss {

enum class CommandType : uint32_t {
    LOGIN    = 1,
    UPLOAD   = 2,
    DOWNLOAD = 3,
    LIST     = 4,
    DELETE_F = 5,    // DELETE is a Windows macro
    RENAME   = 6,
    QUIT     = 7
};

enum class StatusCode : uint32_t {
    OK             = 200,
    CREATED        = 201,
    BAD_REQUEST    = 400,
    NOT_FOUND      = 404,
    CONFLICT       = 409,
    INTERNAL_ERROR = 500
};

// Wire format: fixed 32-byte header, no compiler padding.
#pragma pack(push, 1)
struct MessageHeader {
    uint32_t command;
    uint32_t status;
    uint64_t payload_length;
    uint64_t filename_length;
    uint64_t username_length;
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 32, "MessageHeader must be exactly 32 bytes");

inline MessageHeader make_request(CommandType cmd,
                                  uint64_t payload_len = 0,
                                  uint64_t filename_len = 0,
                                  uint64_t username_len = 0) {
    MessageHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.command         = static_cast<uint32_t>(cmd);
    hdr.status          = 0;
    hdr.payload_length  = payload_len;
    hdr.filename_length = filename_len;
    hdr.username_length = username_len;
    return hdr;
}

inline MessageHeader make_response(CommandType cmd, StatusCode status,
                                   uint64_t payload_len = 0,
                                   uint64_t filename_len = 0) {
    MessageHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    hdr.command         = static_cast<uint32_t>(cmd);
    hdr.status          = static_cast<uint32_t>(status);
    hdr.payload_length  = payload_len;
    hdr.filename_length = filename_len;
    hdr.username_length = 0;
    return hdr;
}

inline const char* command_to_string(uint32_t cmd) {
    switch (static_cast<CommandType>(cmd)) {
        case CommandType::LOGIN:    return "LOGIN";
        case CommandType::UPLOAD:   return "UPLOAD";
        case CommandType::DOWNLOAD: return "DOWNLOAD";
        case CommandType::LIST:     return "LIST";
        case CommandType::DELETE_F: return "DELETE";
        case CommandType::RENAME:   return "RENAME";
        case CommandType::QUIT:     return "QUIT";
        default:                    return "UNKNOWN";
    }
}

inline const char* status_to_string(uint32_t status) {
    switch (static_cast<StatusCode>(status)) {
        case StatusCode::OK:             return "200 OK";
        case StatusCode::CREATED:        return "201 Created";
        case StatusCode::BAD_REQUEST:    return "400 Bad Request";
        case StatusCode::NOT_FOUND:      return "404 Not Found";
        case StatusCode::CONFLICT:       return "409 Conflict";
        case StatusCode::INTERNAL_ERROR: return "500 Internal Error";
        default:                         return "??? Unknown";
    }
}

bool recv_message(SOCKET sock, MessageHeader& hdr,
                  std::string& username,
                  std::string& filename,
                  std::vector<char>& payload);

// Send/receive header + metadata only; payload is streamed separately by caller.
bool send_message_meta(SOCKET sock, const MessageHeader& hdr,
                       const std::string& username,
                       const std::string& filename);

bool recv_message_meta(SOCKET sock, MessageHeader& hdr,
                       std::string& username,
                       std::string& filename);

} // namespace mtfss