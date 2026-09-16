#pragma once

#include <string>

namespace av::core::logging {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

inline std::string to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "INFO";
    }
}

inline LogLevel log_level_from_string(const std::string& str) {
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "WARNING") return LogLevel::WARNING;
    if (str == "ERROR") return LogLevel::ERROR;
    if (str == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::INFO;
}

} // namespace av::core::logging
