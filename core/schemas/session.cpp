#include "session.hpp"

namespace av::core::schemas {

nlohmann::json Session::to_json() const {
    nlohmann::json cap_arr = nlohmann::json::array();
    for (const auto& c : captures_) {
        cap_arr.push_back(c.to_json());
    }

    return {
        {"schema_version", schema_version_},
        {"session_id", session_id_},
        {"project_id", project_id_},
        {"name", name_},
        {"start_time_ns", start_time_ns_},
        {"end_time_ns", end_time_ns_},
        {"status", to_string(status_)},
        {"scanner_nodes", scanner_nodes_},
        {"captures", cap_arr}
    };
}

Session Session::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for Session");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("Session", version, CURRENT_SCHEMA_VERSION);
    }

    Session s;
    s.schema_version_ = version;
    s.session_id_ = j.value("session_id", "");
    s.project_id_ = j.value("project_id", "");
    s.name_ = j.value("name", "");
    s.start_time_ns_ = j.value("start_time_ns", static_cast<uint64_t>(0));
    s.end_time_ns_ = j.value("end_time_ns", static_cast<uint64_t>(0));
    s.status_ = session_status_from_string(j.value("status", "INITIALIZING"));

    if (j.contains("scanner_nodes") && j["scanner_nodes"].is_array()) {
        s.scanner_nodes_ = j["scanner_nodes"].get<std::vector<std::string>>();
    }

    if (j.contains("captures") && j["captures"].is_array()) {
        for (const auto& c_json : j["captures"]) {
            s.captures_.push_back(Capture::from_json(c_json));
        }
    }

    return s;
}

} // namespace av::core::schemas
