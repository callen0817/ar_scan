#pragma once

#include "log_level.hpp"
#include <string>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

namespace av::core::logging {

struct LogRecord {
    uint64_t timestamp_ns{0};
    std::string module;
    std::string scanner_node;
    std::string sensor;
    std::string session;
    LogLevel severity{LogLevel::INFO};
    std::string event;
    std::string error;

    static uint64_t now_ns() {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    nlohmann::json to_json() const {
        nlohmann::json j = {
            {"timestamp_ns", timestamp_ns},
            {"module", module},
            {"scanner_node", scanner_node},
            {"sensor", sensor},
            {"session", session},
            {"severity", to_string(severity)},
            {"event", event}
        };
        if (!error.empty()) {
            j["error"] = error;
        }
        return j;
    }
};

} // namespace av::core::logging
