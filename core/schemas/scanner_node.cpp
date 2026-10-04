#include "scanner_node.hpp"

namespace av::core::schemas {

nlohmann::json ScannerNode::to_json() const {
    nlohmann::json sensors_arr = nlohmann::json::array();
    for (const auto& s : attached_sensors_) {
        sensors_arr.push_back(s.to_json());
    }

    return {
        {"schema_version", schema_version_},
        {"node_id", node_id()},
        {"node_uuid", node_uuid()},
        {"display_name", display_name()},
        {"hostname", hostname_},
        {"platform", platform_},
        {"hardware_metadata", hardware_metadata_},
        {"av_scan_version", av_scan_version_},
        {"protocol_version", protocol_version_},
        {"ip_address", ip_address_},
        {"network_endpoints", network_endpoints_},
        {"capabilities", capabilities_},
        {"role", role_},
        {"session_role", to_string(session_role_)},
        {"status", status_},
        {"online_state", to_string(online_state_)},
        {"clock_sync_source", to_string(clock_sync_source_)},
        {"estimated_clock_offset_ns", estimated_clock_offset_ns_},
        {"clock_uncertainty_ns", clock_uncertainty_ns_},
        {"last_seen_timestamp_ns", last_seen_timestamp_ns_},
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
    node.node_uuid_ = j.value("node_uuid", "");
    node.node_id_ = j.value("node_id", "");
    if (node.node_uuid_.empty() && !node.node_id_.empty()) {
        node.node_uuid_ = node.node_id_;
    } else if (node.node_id_.empty() && !node.node_uuid_.empty()) {
        node.node_id_ = node.node_uuid_;
    }

    node.display_name_ = j.value("display_name", "");
    node.hostname_ = j.value("hostname", "");
    node.platform_ = j.value("platform", "LINUX_ARM64");
    node.hardware_metadata_ = j.value("hardware_metadata", "");
    node.av_scan_version_ = j.value("av_scan_version", "1.0.0");
    node.protocol_version_ = j.value("protocol_version", "1.0");
    node.ip_address_ = j.value("ip_address", "127.0.0.1");

    if (j.contains("network_endpoints") && j["network_endpoints"].is_array()) {
        node.network_endpoints_ = j["network_endpoints"].get<std::vector<std::string>>();
    }

    if (j.contains("capabilities") && j["capabilities"].is_array()) {
        node.capabilities_ = j["capabilities"].get<std::vector<std::string>>();
    }

    if (j.contains("session_role")) {
        node.session_role_ = node_session_role_from_string(j["session_role"].get<std::string>());
        node.role_ = to_string(node.session_role_);
    } else {
        node.role_ = j.value("role", "SESSION_COORDINATOR");
        node.session_role_ = node_session_role_from_string(node.role_);
    }

    if (j.contains("online_state")) {
        node.online_state_ = node_online_state_from_string(j["online_state"].get<std::string>());
        node.status_ = to_string(node.online_state_);
    } else {
        node.status_ = j.value("status", "ONLINE");
        node.online_state_ = node_online_state_from_string(node.status_);
    }

    if (j.contains("clock_sync_source")) {
        node.clock_sync_source_ = clock_sync_source_from_string(j["clock_sync_source"].get<std::string>());
    }
    node.estimated_clock_offset_ns_ = j.value("estimated_clock_offset_ns", static_cast<int64_t>(0));
    node.clock_uncertainty_ns_ = j.value("clock_uncertainty_ns", static_cast<uint64_t>(0));
    node.last_seen_timestamp_ns_ = j.value("last_seen_timestamp_ns", static_cast<uint64_t>(0));

    if (j.contains("attached_sensors") && j["attached_sensors"].is_array()) {
        for (const auto& s_json : j["attached_sensors"]) {
            node.attached_sensors_.push_back(SensorInstance::from_json(s_json));
        }
    }

    return node;
}

} // namespace av::core::schemas
