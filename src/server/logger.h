#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace mtfss {

enum class LogLevel {
    INFO,
    WARN,
    ERR,
    DEBUG
};

class Logger {
public:
    static Logger& get_instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const char* file, int line, const std::string& message);

    ~Logger();

private:
    Logger();
    void process_queue();

    struct LogMessage {
        LogLevel level;
        std::string file;
        int line;
        std::string message;
        std::string timestamp;
    };

    std::queue<LogMessage> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::atomic<bool> running_;
};

} // namespace mtfss

#include <sstream>

#define LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    mtfss::Logger::get_instance().log(mtfss::LogLevel::INFO, __FILE__, __LINE__, _oss.str()); \
} while(0)

#define LOG_WARN(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    mtfss::Logger::get_instance().log(mtfss::LogLevel::WARN, __FILE__, __LINE__, _oss.str()); \
} while(0)

#define LOG_ERROR(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    mtfss::Logger::get_instance().log(mtfss::LogLevel::ERR, __FILE__, __LINE__, _oss.str()); \
} while(0)

#define LOG_DEBUG(msg) do { \
    std::ostringstream _oss; _oss << msg; \
    mtfss::Logger::get_instance().log(mtfss::LogLevel::DEBUG, __FILE__, __LINE__, _oss.str()); \
} while(0)

