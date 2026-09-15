#include "metadata_manager.h"
#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>

namespace mtfss {

MetadataManager& MetadataManager::get_instance() {
    static MetadataManager instance;
    return instance;
}

std::string MetadataManager::normalize_path(const std::string& path) const {
    std::string normalized = path;
    for (char& c : normalized) {
        if (c == '\\') c = '/';
    }
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

void MetadataManager::add_file(const std::string& directory, const std::string& filename, uint64_t size) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf);
#endif
    ss << std::put_time(&buf, "%Y-%m-%d %X");

    std::string dir = normalize_path(directory);

    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_[dir][filename] = FileMetadata{filename, size, ss.str()};
}

void MetadataManager::remove_file(const std::string& directory, const std::string& filename) {
    std::string dir = normalize_path(directory);
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (index_.count(dir)) {
        index_[dir].erase(filename);
        if (index_[dir].empty()) {
            index_.erase(dir);
        }
    }
}

void MetadataManager::remove_directory(const std::string& directory) {
    std::string dir = normalize_path(directory);
    std::unique_lock<std::shared_mutex> lock(mutex_);
    for (auto it = index_.begin(); it != index_.end(); ) {
        if (it->first == dir || (it->first.rfind(dir + "/", 0) == 0)) {
            it = index_.erase(it);
        } else {
            ++it;
        }
    }
}

bool MetadataManager::file_exists(const std::string& directory, const std::string& filename) const {
    std::string dir = normalize_path(directory);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = index_.find(dir);
    if (it != index_.end()) {
        return it->second.count(filename) > 0;
    }
    return false;
}

uint64_t MetadataManager::get_file_size(const std::string& directory, const std::string& filename) const {
    std::string dir = normalize_path(directory);
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = index_.find(dir);
    if (it != index_.end()) {
        auto file_it = it->second.find(filename);
        if (file_it != it->second.end()) {
            return file_it->second.size;
        }
    }
    return 0;
}

std::vector<FileMetadata> MetadataManager::list_directory(const std::string& directory) const {
    std::string dir = normalize_path(directory);
    std::vector<FileMetadata> result;
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = index_.find(dir);
    if (it != index_.end()) {
        for (const auto& [name, meta] : it->second) {
            result.push_back(meta);
        }
    }
    return result;
}

void MetadataManager::rebuild_from_disk(const std::string& storage_dir) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    index_.clear();

    if (!std::filesystem::exists(storage_dir)) {
        LOG_INFO("[MetadataManager] Storage directory does not exist yet. Starting fresh.");
        return;
    }

    try {
        for (const auto& user_entry : std::filesystem::directory_iterator(storage_dir)) {
            if (user_entry.is_directory()) {
                std::string username = user_entry.path().filename().string();
                std::string dir_key = normalize_path("storage/" + username);

                for (const auto& file_entry : std::filesystem::directory_iterator(user_entry.path())) {
                    if (file_entry.is_regular_file()) {
                        std::string filename = file_entry.path().filename().string();
                        uint64_t size = std::filesystem::file_size(file_entry.path());

                        auto now = std::chrono::system_clock::now();
                        auto in_time_t = std::chrono::system_clock::to_time_t(now);
                        std::stringstream ss;
                        struct tm buf;
                        #ifdef _WIN32
                        localtime_s(&buf, &in_time_t);
                        #else
                        localtime_r(&in_time_t, &buf);
                        #endif
                        ss << std::put_time(&buf, "%Y-%m-%d %X");

                        index_[dir_key][filename] = FileMetadata{filename, size, ss.str() + " (Recovered)"};
                    }
                }
            }
        }
        LOG_INFO("[MetadataManager] Successfully rebuilt metadata index from filesystem.");
    } catch (const std::exception& e) {
        LOG_ERROR("[MetadataManager] Failed to rebuild from disk: " << e.what());
    }
}

} // namespace mtfss