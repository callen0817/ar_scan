#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class Project {
public:
    Project() = default;

    Project(const std::string& project_id,
            const std::string& name,
            const std::string& root_path = "",
            const std::string& description = "")
        : schema_version_(CURRENT_SCHEMA_VERSION),
          project_id_(project_id),
          name_(name),
          root_path_(root_path),
          description_(description) {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& project_id() const noexcept { return project_id_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& root_path() const noexcept { return root_path_; }
    const std::string& description() const noexcept { return description_; }
    uint64_t created_at_ns() const noexcept { return created_at_ns_; }
    const std::string& active_session_id() const noexcept { return active_session_id_; }
    const std::vector<std::string>& sessions() const noexcept { return sessions_; }
    const nlohmann::json& metadata() const noexcept { return metadata_; }

    void set_project_id(const std::string& id) { project_id_ = id; }
    void set_name(const std::string& n) { name_ = n; }
    void set_root_path(const std::string& p) { root_path_ = p; }
    void set_description(const std::string& d) { description_ = d; }
    void set_created_at_ns(uint64_t t) { created_at_ns_ = t; }
    void set_active_session_id(const std::string& sid) { active_session_id_ = sid; }
    void add_session(const std::string& sid) { sessions_.push_back(sid); }
    void set_metadata(const nlohmann::json& meta) { metadata_ = meta; }

    nlohmann::json to_json() const;
    static Project from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string project_id_;
    std::string name_;
    std::string root_path_;
    std::string description_;
    uint64_t created_at_ns_{0};
    std::string active_session_id_;
    std::vector<std::string> sessions_;
    nlohmann::json metadata_{nlohmann::json::object()};
};

} // namespace av::core::schemas
