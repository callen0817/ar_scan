#include "scanner_config.hpp"

namespace av::core::schemas {

nlohmann::json ScannerConfig::to_json() const {
    nlohmann::json transforms_json = nlohmann::json::object();
    for (const auto& [key, tf] : relative_transforms_) {
        transforms_json[key] = tf.to_json();
    }

    return {
        {"schema_version", schema_version_},
        {"config_id", config_id_},
        {"name", name_},
        {"scanner_node_id", scanner_node_id_},
        {"sensor_instances", sensor_instances_},
        {"extrinsic_status", to_string(extrinsic_status_)},
        {"relationship_type", to_string(relationship_type_)},
        {"relative_transforms", transforms_json},
        {"method", method_},
        {"confidence", confidence_},
        {"supporting_captures", supporting_captures_},
        {"residual_error_stats", residual_error_stats_},
        {"sensor_serials", sensor_serials_},
        {"firmware", firmware_},
        {"validation_date", validation_date_},
        {"validation_history", validation_history_},
        {"provenance", provenance_.to_json()}
    };
}

ScannerConfig ScannerConfig::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for ScannerConfig");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("ScannerConfig", version, CURRENT_SCHEMA_VERSION);
    }

    ScannerConfig cfg;
    cfg.schema_version_ = version;
    cfg.config_id_ = j.value("config_id", "");
    cfg.name_ = j.value("name", "");
    cfg.scanner_node_id_ = j.value("scanner_node_id", "");

    if (j.contains("sensor_instances") && j["sensor_instances"].is_array()) {
        cfg.sensor_instances_ = j["sensor_instances"].get<std::vector<std::string>>();
    }

    cfg.extrinsic_status_ = extrinsic_status_from_string(j.value("extrinsic_status", "UNKNOWN"));
    cfg.relationship_type_ = relationship_type_from_string(j.value("relationship_type", "RIGID"));

    if (j.contains("relative_transforms") && j["relative_transforms"].is_object()) {
        for (auto it = j["relative_transforms"].begin(); it != j["relative_transforms"].end(); ++it) {
            cfg.relative_transforms_[it.key()] = Transform3D::from_json(it.value());
        }
    }

    cfg.method_ = j.value("method", "");
    cfg.confidence_ = j.value("confidence", 0.0);

    if (j.contains("supporting_captures") && j["supporting_captures"].is_array()) {
        cfg.supporting_captures_ = j["supporting_captures"].get<std::vector<std::string>>();
    }

    if (j.contains("residual_error_stats")) cfg.residual_error_stats_ = j["residual_error_stats"];
    if (j.contains("sensor_serials") && j["sensor_serials"].is_array()) {
        cfg.sensor_serials_ = j["sensor_serials"].get<std::vector<std::string>>();
    }
    if (j.contains("firmware") && j["firmware"].is_array()) {
        cfg.firmware_ = j["firmware"].get<std::vector<std::string>>();
    }

    cfg.validation_date_ = j.value("validation_date", "");

    if (j.contains("validation_history") && j["validation_history"].is_array()) {
        cfg.validation_history_ = j["validation_history"].get<std::vector<std::string>>();
    }

    if (j.contains("provenance")) {
        cfg.provenance_ = CalibrationProvenance::from_json(j["provenance"]);
    }

    return cfg;
}

} // namespace av::core::schemas
