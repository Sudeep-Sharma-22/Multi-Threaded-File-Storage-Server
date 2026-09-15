#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <cstdint>

namespace mtfss {

struct FileMetadata {
    std::string filename;
    uint64_t size;
    std::string upload_time;
};

class MetadataManager {
public:
    static MetadataManager& get_instance();

    MetadataManager(const MetadataManager&) = delete;
    MetadataManager& operator=(const MetadataManager&) = delete;

    void add_file(const std::string& directory, const std::string& filename, uint64_t size);
    void remove_file(const std::string& directory, const std::string& filename);
    bool file_exists(const std::string& directory, const std::string& filename) const;
    uint64_t get_file_size(const std::string& directory, const std::string& filename) const;
    std::vector<FileMetadata> list_directory(const std::string& directory) const;

    void remove_directory(const std::string& directory);

    void rebuild_from_disk(const std::string& storage_dir);

private:
    MetadataManager() = default;
    ~MetadataManager() = default;

    std::string normalize_path(const std::string& path) const;

    // Outer map: directory path -> (inner map: filename -> metadata)
    std::unordered_map<std::string, std::unordered_map<std::string, FileMetadata>> index_;
    mutable std::shared_mutex mutex_;
};

} // namespace mtfss