#include "sensor_instance.hpp"

namespace av::core::schemas {

nlohmann::json SensorInstance::to_json() const {
    return {
        {"schema_version", schema_version_},
        {"instance_id", instance_id_},
        {"scanner_node_id", scanner_node_id_},
        {"device_profile_id", device_profile_id_},
        {"serial_number", serial_number_},
        {"transport_endpoint", transport_endpoint_},
        {"mount_label", mount_label_},
        {"status", to_string(status_)}
    };
}

SensorInstance SensorInstance::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for SensorInstance");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("SensorInstance", version, CURRENT_SCHEMA_VERSION);
    }

    SensorInstance inst;
    inst.schema_version_ = version;
    inst.instance_id_ = j.value("instance_id", "");
    inst.scanner_node_id_ = j.value("scanner_node_id", "");
    inst.device_profile_id_ = j.value("device_profile_id", "");
    inst.serial_number_ = j.value("serial_number", "");
    inst.transport_endpoint_ = j.value("transport_endpoint", "");
    inst.mount_label_ = j.value("mount_label", "DEFAULT");
    inst.status_ = sensor_status_from_string(j.value("status", "DISCONNECTED"));
    return inst;
}

} // namespace av::core::schemas
