#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

namespace av::core::logging {

Logger& Logger::instance() {
    static Logger s_instance;
    return s_instance;
}

Logger::~Logger() {
    close();
}

void Logger::init(const std::string& log_file_path, LogLevel min_level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = min_level;

    if (!log_file_path.empty()) {
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        file_stream_.open(log_file_path, std::ios::out | std::ios::app);
        if (!file_stream_.is_open()) {
            std::cerr << "[AV_LOGGER] Warning: Unable to open log file: " << log_file_path << std::endl;
        }
    }
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_stream_.is_open()) {
        file_stream_.flush();
        file_stream_.close();
    }
}

void Logger::set_min_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

void Logger::set_default_scanner_node(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_scanner_node_ = node_id;
}

void Logger::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (record.severity < min_level_) {
        return;
    }

    // Console output format: [TIME] [SEV] [MODULE] [NODE/SENSOR] EVENT (ERROR)
    if (console_enabled_) {
        std::time_t sec = static_cast<std::time_t>(record.timestamp_ns / 1000000000ULL);
        std::tm tm_buf;
        localtime_r(&sec, &tm_buf);

        std::ostream& out = (record.severity >= LogLevel::ERROR) ? std::cerr : std::cout;

        // ANSI Color codes
        const char* color_code = "\033[0m";
        switch (record.severity) {
            case LogLevel::DEBUG: color_code = "\033[36m"; break;   // Cyan
            case LogLevel::INFO: color_code = "\033[32m"; break;    // Green
            case LogLevel::WARNING: color_code = "\033[33m"; break; // Yellow
            case LogLevel::ERROR: color_code = "\033[31m"; break;   // Red
            case LogLevel::CRITICAL: color_code = "\033[1;31m"; break; // Bold Red
        }

        char time_str[32];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

        out << color_code << "[" << time_str << "] "
            << "[" << to_string(record.severity) << "] "
            << "[" << record.module << "] ";

        if (!record.scanner_node.empty()) {
            out << "(" << record.scanner_node;
            if (!record.sensor.empty()) {
                out << ":" << record.sensor;
            }
            out << ") ";
        }

        out << record.event;
        if (!record.error.empty()) {
            out << " | Error: " << record.error;
        }
        out << "\033[0m" << std::endl;
    }

    // Structured JSON Lines sink for persistent diagnostic inspection
    if (file_stream_.is_open()) {
        file_stream_ << record.to_json().dump() << "\n";
        file_stream_.flush();
    }
}

void Logger::log(LogLevel level,
                 const std::string& module,
                 const std::string& event,
                 const std::string& error,
                 const std::string& sensor,
                 const std::string& session) {
    LogRecord rec;
    rec.timestamp_ns = LogRecord::now_ns();
    rec.module = module;
    rec.scanner_node = default_scanner_node_;
    rec.sensor = sensor;
    rec.session = session;
    rec.severity = level;
    rec.event = event;
    rec.error = error;
    log(rec);
}

} // namespace av::core::logging
