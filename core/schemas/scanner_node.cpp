#include "scanner_node.hpp"

namespace av::core::schemas {

nlohmann::json ScannerNode::to_json() const {
    nlohmann::json sensors_arr = nlohmann::json::array();
    for (const auto& s : attached_sensors_) {
        sensors_arr.push_back(s.to_json());
    }

    return {
        {"schema_version", schema_version_},
        {"node_id", node_id_},
        {"hostname", hostname_},
        {"platform", platform_},
        {"ip_address", ip_address_},
        {"role", role_},
        {"status", status_},
        {"attached_sensors", sensors_arr}
    };
}

ScannerNode ScannerNode::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for ScannerNode");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("ScannerNode", version, CURRENT_SCHEMA_VERSION);
    }

    ScannerNode node;
    node.schema_version_ = version;
    node.node_id_ = j.value("node_id", "");
    node.hostname_ = j.value("hostname", "");
    node.platform_ = j.value("platform", "JETSON_LINUX");
    node.ip_address_ = j.value("ip_address", "127.0.0.1");
    node.role_ = j.value("role", "PRIMARY_ORCHESTRATOR");
    node.status_ = j.value("status", "ONLINE");

    if (j.contains("attached_sensors") && j["attached_sensors"].is_array()) {
        for (const auto& s_json : j["attached_sensors"]) {
            node.attached_sensors_.push_back(SensorInstance::from_json(s_json));
        }
    }

    return node;
}

} // namespace av::core::schemas
