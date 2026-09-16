#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "core/logging/logger.hpp"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace av::core::logging;

int main() {
    std::cout << "=== Running Logging Unit Tests ===" << std::endl;

    fs::path test_log = fs::current_path() / "test_logging_output.log";
    if (fs::exists(test_log)) {
        fs::remove(test_log);
    }

    Logger::instance().init(test_log.string(), LogLevel::DEBUG);
    Logger::instance().set_default_scanner_node("test_node");

    AV_LOG_DEBUG("UnitTest", "Test debug message");
    AV_LOG_INFO("UnitTest", "Test info message");
    AV_LOG_WARN("UnitTest", "Test warning message");
    AV_LOG_ERROR("UnitTest", "Test error message", "ERR_CODE_42");

    Logger::instance().close();

    // Verify log file was written and is valid JSON Lines
    assert(fs::exists(test_log));
    std::ifstream ifs(test_log);
    assert(ifs.is_open());

    std::string line;
    int line_count = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto j = nlohmann::json::parse(line);
        assert(j.contains("timestamp_ns"));
        assert(j.contains("module"));
        assert(j.contains("severity"));
        assert(j.contains("event"));
        assert(j["module"] == "UnitTest");
        ++line_count;
    }

    assert(line_count == 4);
    fs::remove(test_log);

    std::cout << "  -> Structured JSONL lines verified: " << line_count << std::endl;
    std::cout << "=== All Logging Unit Tests PASSED ===" << std::endl;
    return 0;
}
