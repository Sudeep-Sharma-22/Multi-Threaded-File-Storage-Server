#include "logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <filesystem>

namespace mtfss {

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : running_(true) {
    worker_ = std::thread(&Logger::process_queue, this);
}

Logger::~Logger() {
    running_ = false;
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& message) {
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

    std::string filename = std::filesystem::path(file).filename().string();

    LogMessage msg{level, filename, line, message, ss.str()};

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
    }
    cv_.notify_one();
}

void Logger::process_queue() {
    while (running_ || !queue_.empty()) {
        LogMessage msg;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });

            if (!running_ && queue_.empty()) {
                return;
            }

            msg = std::move(queue_.front());
            queue_.pop();
        }

        std::string level_str;
        switch (msg.level) {
            case LogLevel::INFO:  level_str = "INFO "; break;
            case LogLevel::WARN:  level_str = "WARN "; break;
            case LogLevel::ERR: level_str = "ERROR"; break;
            case LogLevel::DEBUG: level_str = "DEBUG"; break;
        }

        std::ostream& out = (msg.level == LogLevel::ERR) ? std::cerr : std::cout;

        out << "[" << msg.timestamp << "] [" << level_str << "] ["
            << msg.file << ":" << msg.line << "] " << msg.message << "\n";
    }
}

} // namespace mtfss