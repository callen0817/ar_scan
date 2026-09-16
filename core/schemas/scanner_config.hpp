#pragma once

#include "types.hpp"
#include "calibration_provenance.hpp"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class ScannerConfig {
public:
    ScannerConfig() = default;

    ScannerConfig(const std::string& config_id,
                  const std::string& name,
                  const std::string& node_id,
                  SensorRelationshipType rel_type = SensorRelationshipType::RIGID)
        : schema_version_(CURRENT_SCHEMA_VERSION),
          config_id_(config_id),
          name_(name),
          scanner_node_id_(node_id),
          extrinsic_status_(ExtrinsicStatus::UNKNOWN),
          relationship_type_(rel_type) {}

    // Getters
    int schema_version() const noexcept { return schema_version_; }
    const std::string& config_id() const noexcept { return config_id_; }
    const std::string& name() const noexcept { return name_; }
    const std::string& scanner_node_id() const noexcept { return scanner_node_id_; }
    const std::vector<std::string>& sensor_instances() const noexcept { return sensor_instances_; }
    ExtrinsicStatus extrinsic_status() const noexcept { return extrinsic_status_; }
    SensorRelationshipType relationship_type() const noexcept { return relationship_type_; }
    const std::map<std::string, Transform3D>& relative_transforms() const noexcept { return relative_transforms_; }
    const std::string& method() const noexcept { return method_; }
    double confidence() const noexcept { return confidence_; }
    const std::vector<std::string>& supporting_captures() const noexcept { return supporting_captures_; }
    const nlohmann::json& residual_error_stats() const noexcept { return residual_error_stats_; }
    const std::vector<std::string>& sensor_serials() const noexcept { return sensor_serials_; }
    const std::vector<std::string>& firmware() const noexcept { return firmware_; }
    const std::string& validation_date() const noexcept { return validation_date_; }
    const std::vector<std::string>& validation_history() const noexcept { return validation_history_; }
    const CalibrationProvenance& provenance() const noexcept { return provenance_; }

    // Setters
    void set_config_id(const std::string& id) { config_id_ = id; }
    void set_name(const std::string& name) { name_ = name; }
    void set_scanner_node_id(const std::string& nid) { scanner_node_id_ = nid; }
    void set_sensor_instances(const std::vector<std::string>& s) { sensor_instances_ = s; }
    void add_sensor_instance(const std::string& sid) { sensor_instances_.push_back(sid); }
    void set_extrinsic_status(ExtrinsicStatus st) { extrinsic_status_ = st; }
    void set_relationship_type(SensorRelationshipType rel) { relationship_type_ = rel; }
    void set_relative_transform(const std::string& pair_key, const Transform3D& tf) { relative_transforms_[pair_key] = tf; }
    void set_method(const std::string& m) { method_ = m; }
    void set_confidence(double c) { confidence_ = c; }
    void set_supporting_captures(const std::vector<std::string>& sc) { supporting_captures_ = sc; }
    void set_residual_error_stats(const nlohmann::json& res) { residual_error_stats_ = res; }
    void set_sensor_serials(const std::vector<std::string>& ss) { sensor_serials_ = ss; }
    void set_firmware(const std::vector<std::string>& fw) { firmware_ = fw; }
    void set_validation_date(const std::string& d) { validation_date_ = d; }
    void add_validation_history(const std::string& note) { validation_history_.push_back(note); }
    void set_provenance(const CalibrationProvenance& p) { provenance_ = p; }

    nlohmann::json to_json() const;
    static ScannerConfig from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string config_id_;
    std::string name_;
    std::string scanner_node_id_;
    std::vector<std::string> sensor_instances_;
    ExtrinsicStatus extrinsic_status_{ExtrinsicStatus::UNKNOWN};
    SensorRelationshipType relationship_type_{SensorRelationshipType::RIGID};

    std::map<std::string, Transform3D> relative_transforms_;
    std::string method_;
    double confidence_{0.0};
    std::vector<std::string> supporting_captures_;
    nlohmann::json residual_error_stats_{nlohmann::json::object()};

    std::vector<std::string> sensor_serials_;
    std::vector<std::string> firmware_;
    std::string validation_date_;
    std::vector<std::string> validation_history_;
    CalibrationProvenance provenance_;
};

} // namespace av::core::schemas
