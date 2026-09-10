#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <mutex>

namespace utils {

    enum class LogLevel { DEBUG, INFO, WARN, ERROR };

    class Logger {
    public:
        static Logger& instance() {
            static Logger inst;
            return inst;
        }

        void setLevel(LogLevel level) { minLevel_ = level; }

        void debug(const std::string& tag, const std::string& msg) {
            log(LogLevel::DEBUG, tag, msg);
        }
        void info(const std::string& tag, const std::string& msg) {
            log(LogLevel::INFO, tag, msg);
        }
        void warn(const std::string& tag, const std::string& msg) {
            log(LogLevel::WARN, tag, msg);
        }
        void error(const std::string& tag, const std::string& msg) {
            log(LogLevel::ERROR, tag, msg);
        }

    private:
        Logger() = default;
        LogLevel minLevel_ = LogLevel::DEBUG;
        std::mutex mtx_;

        void log(LogLevel level, const std::string& tag, const std::string& msg) {
            if (level < minLevel_) return;
            std::lock_guard<std::mutex> lock(mtx_);
            std::cout << "[" << timestamp() << "] "
                      << "[" << levelStr(level) << "] "
                      << "[" << tag << "] "
                      << msg << std::endl;
        }

        static std::string timestamp() {
            auto now = std::time(nullptr);
            std::tm tm_buf{};
#ifdef _WIN32
            localtime_s(&tm_buf, &now);
#else
            localtime_r(&now, &tm_buf);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }

        static const char* levelStr(LogLevel level) {
            switch (level) {
                case LogLevel::DEBUG: return "DEBUG";
                case LogLevel::INFO:  return "INFO ";
                case LogLevel::WARN:  return "WARN ";
                case LogLevel::ERROR: return "ERROR";
                default: return "?????";
            }
        }
    };

    // 便捷宏
    #define LOG_DEBUG(tag, msg) utils::Logger::instance().debug(tag, msg)
    #define LOG_INFO(tag, msg)  utils::Logger::instance().info(tag, msg)
    #define LOG_WARN(tag, msg)  utils::Logger::instance().warn(tag, msg)
    #define LOG_ERROR(tag, msg) utils::Logger::instance().error(tag, msg)

} // namespace utils
