#include "project.hpp"

namespace av::core::schemas {

nlohmann::json Project::to_json() const {
    return {
        {"schema_version", schema_version_},
        {"project_id", project_id_},
        {"name", name_},
        {"root_path", root_path_},
        {"description", description_},
        {"created_at_ns", created_at_ns_},
        {"active_session_id", active_session_id_},
        {"sessions", sessions_},
        {"metadata", metadata_}
    };
}

Project Project::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for Project");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("Project", version, CURRENT_SCHEMA_VERSION);
    }

    Project p;
    p.schema_version_ = version;
    p.project_id_ = j.value("project_id", "");
    p.name_ = j.value("name", "");
    p.root_path_ = j.value("root_path", "");
    p.description_ = j.value("description", "");
    p.created_at_ns_ = j.value("created_at_ns", static_cast<uint64_t>(0));
    p.active_session_id_ = j.value("active_session_id", "");

    if (j.contains("sessions") && j["sessions"].is_array()) {
        p.sessions_ = j["sessions"].get<std::vector<std::string>>();
    }

    if (j.contains("metadata")) {
        p.metadata_ = j["metadata"];
    }

    return p;
}

} // namespace av::core::schemas
