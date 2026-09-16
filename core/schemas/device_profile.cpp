#include "device_profile.hpp"

namespace av::core::schemas {

nlohmann::json DeviceProfile::to_json() const {
    nlohmann::json calib_arr = nlohmann::json::array();
    for (const auto& c : calibration_sources_) {
        calib_arr.push_back(c.to_json());
    }

    nlohmann::json streams_arr = nlohmann::json::array();
    for (const auto& s : streams_) {
        streams_arr.push_back({
            {"name", s.name},
            {"type", s.type},
            {"format", s.format},
            {"nominal_rate_hz", s.nominal_rate_hz},
            {"enabled", s.enabled}
        });
    }

    return {
        {"schema_version", schema_version_},
        {"profile_id", profile_id_},
        {"profile_version", profile_version_},
        {"manufacturer", manufacturer_},
        {"model", model_},
        {"device_family", device_family_},
        {"profile_status", to_string(profile_status_)},
        {"hardware_revision", hardware_revision_},
        {"firmware", firmware_},
        {"capabilities", capabilities_},
        {"transport", transport_},
        {"driver", driver_},
        {"sdk", sdk_},
        {"streams", streams_arr},
        {"timestamp_sources", timestamp_sources_},
        {"synchronization", synchronization_},
        {"coordinate_frames", coordinate_frames_},
        {"intrinsics", intrinsics_},
        {"internal_extrinsics", internal_extrinsics_},
        {"calibration_sources", calib_arr},
        {"slam_family", slam_family_},
        {"slam_backend", slam_backend_},
        {"slam_parameters", slam_parameters_},
        {"preprocessing", preprocessing_},
        {"output_contract", output_contract_},
        {"validation_results", validation_results_},
        {"provenance", provenance_.to_json()}
    };
}

DeviceProfile DeviceProfile::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for DeviceProfile");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("DeviceProfile", version, CURRENT_SCHEMA_VERSION);
    }

    DeviceProfile p;
    p.schema_version_ = version;
    p.profile_id_ = j.value("profile_id", "");
    p.profile_version_ = j.value("profile_version", "1.0.0");
    p.manufacturer_ = j.value("manufacturer", "");
    p.model_ = j.value("model", "");
    p.device_family_ = j.value("device_family", "");
    p.profile_status_ = profile_status_from_string(j.value("profile_status", "EXPERIMENTAL"));
    p.hardware_revision_ = j.value("hardware_revision", "");
    p.firmware_ = j.value("firmware", "");

    if (j.contains("capabilities") && j["capabilities"].is_array()) {
        p.capabilities_ = j["capabilities"].get<std::vector<std::string>>();
    }

    p.transport_ = j.value("transport", "");
    p.driver_ = j.value("driver", "");
    p.sdk_ = j.value("sdk", "");

    if (j.contains("streams") && j["streams"].is_array()) {
        for (const auto& item : j["streams"]) {
            StreamInfo s;
            s.name = item.value("name", "");
            s.type = item.value("type", "");
            s.format = item.value("format", "");
            s.nominal_rate_hz = item.value("nominal_rate_hz", 0.0);
            s.enabled = item.value("enabled", true);
            p.streams_.push_back(s);
        }
    }

    if (j.contains("timestamp_sources") && j["timestamp_sources"].is_array()) {
        p.timestamp_sources_ = j["timestamp_sources"].get<std::vector<std::string>>();
    }

    p.synchronization_ = j.value("synchronization", "");

    if (j.contains("coordinate_frames") && j["coordinate_frames"].is_object()) {
        p.coordinate_frames_ = j["coordinate_frames"].get<std::map<std::string, std::string>>();
    }

    if (j.contains("intrinsics")) p.intrinsics_ = j["intrinsics"];
    if (j.contains("internal_extrinsics")) p.internal_extrinsics_ = j["internal_extrinsics"];

    if (j.contains("calibration_sources") && j["calibration_sources"].is_array()) {
        for (const auto& cs_json : j["calibration_sources"]) {
            p.calibration_sources_.push_back(CalibrationProvenance::from_json(cs_json));
        }
    }

    p.slam_family_ = j.value("slam_family", "");
    p.slam_backend_ = j.value("slam_backend", "");
    if (j.contains("slam_parameters")) p.slam_parameters_ = j["slam_parameters"];
    if (j.contains("preprocessing")) p.preprocessing_ = j["preprocessing"];
    if (j.contains("output_contract")) p.output_contract_ = j["output_contract"];
    if (j.contains("validation_results")) p.validation_results_ = j["validation_results"];

    if (j.contains("provenance")) {
        p.provenance_ = CalibrationProvenance::from_json(j["provenance"]);
    }

    return p;
}

} // namespace av::core::schemas
