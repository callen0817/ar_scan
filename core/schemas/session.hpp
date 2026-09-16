#pragma once

#include "types.hpp"
#include "capture.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class Session {
public:
    Session() = default;

    Session(const std::string& session_id,
            const std::string& project_id,
            const std::string& name = "",
            uint64_t start_time_ns = 0)
        : schema_version_(CURRENT_SCHEMA_VERSION),
          session_id_(session_id),
          project_id_(project_id),
          name_(name),
          start_time_ns_(start_time_ns),
          status_(SessionStatus::INITIALIZING) {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& session_id() const noexcept { return session_id_; }
    const std::string& project_id() const noexcept { return project_id_; }
    const std::string& name() const noexcept { return name_; }
    uint64_t start_time_ns() const noexcept { return start_time_ns_; }
    uint64_t end_time_ns() const noexcept { return end_time_ns_; }
    SessionStatus status() const noexcept { return status_; }
    const std::vector<std::string>& scanner_nodes() const noexcept { return scanner_nodes_; }
    const std::vector<Capture>& captures() const noexcept { return captures_; }

    void set_session_id(const std::string& id) { session_id_ = id; }
    void set_project_id(const std::string& pid) { project_id_ = pid; }
    void set_name(const std::string& name) { name_ = name; }
    void set_start_time_ns(uint64_t t) { start_time_ns_ = t; }
    void set_end_time_ns(uint64_t t) { end_time_ns_ = t; }
    void set_status(SessionStatus st) { status_ = st; }
    void add_scanner_node(const std::string& node_id) { scanner_nodes_.push_back(node_id); }
    void add_capture(const Capture& cap) { captures_.push_back(cap); }

    nlohmann::json to_json() const;
    static Session from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string session_id_;
    std::string project_id_;
    std::string name_;
    uint64_t start_time_ns_{0};
    uint64_t end_time_ns_{0};
    SessionStatus status_{SessionStatus::INITIALIZING};
    std::vector<std::string> scanner_nodes_;
    std::vector<Capture> captures_;
};

} // namespace av::core::schemas
