#pragma once

#include <string>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

namespace mtfss {

class FileManager {
public:
    FileManager() = default;
    ~FileManager() = default;

    bool save_file(const std::string& username, const std::string& filename, SOCKET sock, uint64_t file_size);
    static uint64_t get_file_size(const std::string& username, const std::string& filename);
    static bool stream_file_to_socket(const std::string& username, const std::string& filename, SOCKET sock);
    static std::string list_files(const std::string& username);
    static bool delete_file(const std::string& username, const std::string& filename);
    static bool rename_file(const std::string& username, const std::string& old_name, const std::string& new_name);
    static void discard_data(SOCKET sock, uint64_t size);
};

} // namespace mtfss