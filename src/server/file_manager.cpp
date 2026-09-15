#include "file_manager.h"
#include "logger.h"
#include "metadata_manager.h"
#include "../common/network_utils.h"
#include "../common/types.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <memory>

namespace fs = std::filesystem;

namespace {
    class FileLockManager {
    private:
        struct LockEntry {
            std::shared_mutex mutex;
            uint32_t ref_count = 0;
        };
        std::mutex map_mutex_;
        std::unordered_map<std::string, std::shared_ptr<LockEntry>> locks_;

    public:
        static FileLockManager& get_instance() {
            static FileLockManager instance;
            return instance;
        }

        std::shared_ptr<LockEntry> acquire_lock_ref(const std::string& path_key) {
            std::lock_guard<std::mutex> lock(map_mutex_);
            if (locks_.find(path_key) == locks_.end()) {
                locks_[path_key] = std::make_shared<LockEntry>();
            }
            locks_[path_key]->ref_count++;
            return locks_[path_key];
        }

        void release_lock_ref(const std::string& path_key) {
            std::lock_guard<std::mutex> lock(map_mutex_);
            auto it = locks_.find(path_key);
            if (it != locks_.end()) {
                it->second->ref_count--;
                if (it->second->ref_count == 0) {
                    locks_.erase(it);
                }
            }
        }
    };

    bool is_valid_filename(const std::string& filename) {
        if (filename.empty() || filename == "." || filename == "..") return false;
        if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) return false;
        if (filename.find("..") != std::string::npos) return false;
        return true;
    }
}

namespace mtfss {

bool FileManager::save_file(const std::string& username, const std::string& filename, SOCKET sock, uint64_t file_size) {
    try {
        fs::path user_dir = fs::path("storage") / username;
        if (!fs::exists(user_dir)) {
            fs::create_directories(user_dir);
        }

        if (!is_valid_filename(filename)) {
            LOG_ERROR("[FileManager] Invalid filename detected: " << filename);
            discard_data(sock, file_size);
            return false;
        }

        fs::path file_path = user_dir / filename;
        std::string path_key = file_path.string();

        auto lock_entry = FileLockManager::get_instance().acquire_lock_ref(path_key);
        std::unique_lock<std::shared_mutex> write_lock(lock_entry->mutex);

        std::ofstream outfile(file_path, std::ios::binary);
        if (!outfile) {
            LOG_ERROR("[FileManager] Failed to create file: " << file_path);
            discard_data(sock, file_size);
            write_lock.unlock();
            FileLockManager::get_instance().release_lock_ref(path_key);
            return false;
        }

        uint64_t bytes_remaining = file_size;
        std::vector<char> buffer(TRANSFER_BUFFER_SIZE);

        while (bytes_remaining > 0) {
            size_t chunk_size = static_cast<size_t>(std::min(static_cast<uint64_t>(buffer.size()), bytes_remaining));
            
            if (!recv_all(sock, buffer.data(), chunk_size)) {
                LOG_ERROR("[FileManager] Network error while receiving file.");
                outfile.close();
                fs::remove(file_path); 
                return false;
            }
            
            outfile.write(buffer.data(), chunk_size);
            bytes_remaining -= chunk_size;
        }

        outfile.close();
        MetadataManager::get_instance().add_file("storage/" + username, filename, file_size);
        LOG_INFO("[FileManager] Saved file: " << file_path.string() << " (" << file_size << " bytes)");
        
        write_lock.unlock();
        FileLockManager::get_instance().release_lock_ref(path_key);
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("[FileManager] Exception during save: " << e.what());
        discard_data(sock, file_size);
        return false;
    }
}

void FileManager::discard_data(SOCKET sock, uint64_t size) {
    uint64_t bytes_remaining = size;
    std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
    
    while (bytes_remaining > 0) {
        size_t chunk_size = static_cast<size_t>(std::min(static_cast<uint64_t>(buffer.size()), bytes_remaining));
        if (!recv_all(sock, buffer.data(), chunk_size)) {
            break; 
        }
        bytes_remaining -= chunk_size;
    }
}

uint64_t FileManager::get_file_size(const std::string& username, const std::string& filename) {
    std::string dir = "storage/" + username;
    if (!MetadataManager::get_instance().file_exists(dir, filename)) {
        return static_cast<uint64_t>(-1);
    }
    return MetadataManager::get_instance().get_file_size(dir, filename);
}

bool FileManager::stream_file_to_socket(const std::string& username, const std::string& filename, SOCKET sock) {
    try {
        if (!is_valid_filename(filename)) return false;

        fs::path file_path = fs::path("storage") / username / filename;
        std::string path_key = file_path.string();

        auto lock_entry = FileLockManager::get_instance().acquire_lock_ref(path_key);
        std::shared_lock<std::shared_mutex> read_lock(lock_entry->mutex);

        std::ifstream infile(file_path, std::ios::binary);
        if (!infile) {
            LOG_ERROR("[FileManager] Failed to open file for reading: " << file_path);
            read_lock.unlock();
            FileLockManager::get_instance().release_lock_ref(path_key);
            return false;
        }

        std::vector<char> buffer(TRANSFER_BUFFER_SIZE);
        uint64_t total_sent = 0;
        
        while (infile) {
            infile.read(buffer.data(), buffer.size());
            size_t bytes_read = infile.gcount();
            if (bytes_read > 0) {
                if (!send_all(sock, buffer.data(), bytes_read)) {
                    LOG_ERROR("[FileManager] Network error while sending file.");
                    read_lock.unlock();
                    FileLockManager::get_instance().release_lock_ref(path_key);
                    return false;
                }
                total_sent += bytes_read;
            }
        }
        
        LOG_INFO("[FileManager] Sent file: " << file_path.string() << " (" << total_sent << " bytes)");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("[FileManager] Exception during send: " << e.what());
        return false;
    }
}

std::string FileManager::list_files(const std::string& username) {
    std::string dir = "storage/" + username;
    auto files = MetadataManager::get_instance().list_directory(dir);
    if (files.empty()) return "(Empty)";
    
    std::string result;
    for (const auto& meta : files) {
        result += meta.filename + " (" + std::to_string(meta.size) + " bytes) - " + meta.upload_time + "\n";
    }
    return result;
}

bool FileManager::delete_file(const std::string& username, const std::string& filename) {
    try {
        if (!is_valid_filename(filename)) return false;

        fs::path file_path = fs::path("storage") / username / filename;
        std::string path_key = file_path.string();

        auto lock_entry = FileLockManager::get_instance().acquire_lock_ref(path_key);
        std::unique_lock<std::shared_mutex> write_lock(lock_entry->mutex);

        bool success = fs::remove(file_path);
        if (success) {
            MetadataManager::get_instance().remove_file("storage/" + username, filename);
        }
        
        write_lock.unlock();
        FileLockManager::get_instance().release_lock_ref(path_key);
        return success;
    } catch (...) {
        return false;
    }
}

bool FileManager::rename_file(const std::string& username, const std::string& old_name, const std::string& new_name) {
    try {
        if (!is_valid_filename(old_name) || !is_valid_filename(new_name)) return false;

        fs::path old_path = fs::path("storage") / username / old_name;
        fs::path new_path = fs::path("storage") / username / new_name;
        
        std::string old_key = old_path.string();
        std::string new_key = new_path.string();
        
        if (old_key == new_key) return true;

        auto old_lock_entry = FileLockManager::get_instance().acquire_lock_ref(old_key);
        auto new_lock_entry = FileLockManager::get_instance().acquire_lock_ref(new_key);

        std::unique_lock<std::shared_mutex> lock1(old_lock_entry->mutex, std::defer_lock);
        std::unique_lock<std::shared_mutex> lock2(new_lock_entry->mutex, std::defer_lock);
        std::lock(lock1, lock2);
        
        if (!fs::exists(old_path) || fs::exists(new_path)) {
            lock1.unlock();
            lock2.unlock();
            FileLockManager::get_instance().release_lock_ref(old_key);
            FileLockManager::get_instance().release_lock_ref(new_key);
            return false;
        }
        
        fs::rename(old_path, new_path);
        
        std::string dir = "storage/" + username;
        uint64_t size = MetadataManager::get_instance().get_file_size(dir, old_name);
        MetadataManager::get_instance().remove_file(dir, old_name);
        MetadataManager::get_instance().add_file(dir, new_name, size);

        lock1.unlock();
        lock2.unlock();
        FileLockManager::get_instance().release_lock_ref(old_key);
        FileLockManager::get_instance().release_lock_ref(new_key);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace mtfss