#pragma once

#include "log_level.hpp"
#include "log_record.hpp"
#include <string>
#include <fstream>
#include <mutex>
#include <memory>

namespace av::core::logging {

class Logger {
public:
    static Logger& instance();

    void init(const std::string& log_file_path = "", LogLevel min_level = LogLevel::INFO);
    void close();

    void set_min_level(LogLevel level);
    LogLevel min_level() const noexcept { return min_level_; }

    void set_default_scanner_node(const std::string& node_id);
    const std::string& default_scanner_node() const noexcept { return default_scanner_node_; }

    void log(const LogRecord& record);

    void log(LogLevel level,
             const std::string& module,
             const std::string& event,
             const std::string& error = "",
             const std::string& sensor = "",
             const std::string& session = "");

    void debug(const std::string& module, const std::string& event, const std::string& sensor = "", const std::string& session = "") {
        log(LogLevel::DEBUG, module, event, "", sensor, session);
    }

    void info(const std::string& module, const std::string& event, const std::string& sensor = "", const std::string& session = "") {
        log(LogLevel::INFO, module, event, "", sensor, session);
    }

    void warn(const std::string& module, const std::string& event, const std::string& sensor = "", const std::string& session = "") {
        log(LogLevel::WARNING, module, event, "", sensor, session);
    }

    void error(const std::string& module, const std::string& event, const std::string& error_msg, const std::string& sensor = "", const std::string& session = "") {
        log(LogLevel::ERROR, module, event, error_msg, sensor, session);
    }

    void critical(const std::string& module, const std::string& event, const std::string& error_msg, const std::string& sensor = "", const std::string& session = "") {
        log(LogLevel::CRITICAL, module, event, error_msg, sensor, session);
    }

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::mutex mutex_;
    LogLevel min_level_{LogLevel::INFO};
    std::string default_scanner_node_{"scanar-01"};
    std::ofstream file_stream_;
    bool console_enabled_{true};
};

} // namespace av::core::logging

#define AV_LOG_DEBUG(module, event) av::core::logging::Logger::instance().debug(module, event)
#define AV_LOG_INFO(module, event) av::core::logging::Logger::instance().info(module, event)
#define AV_LOG_WARN(module, event) av::core::logging::Logger::instance().warn(module, event)
#define AV_LOG_ERROR(module, event, err) av::core::logging::Logger::instance().error(module, event, err)
#define AV_LOG_CRITICAL(module, event, err) av::core::logging::Logger::instance().critical(module, event, err)
