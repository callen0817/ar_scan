#pragma once

#include "types.hpp"
#include "calibration_provenance.hpp"
#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

struct StreamInfo {
    std::string name;
    std::string type;         // e.g., "POINT_CLOUD", "RGB", "DEPTH", "IMU"
    std::string format;       // e.g., "XYZI_PCL", "RGB8", "DEPTH16", "6DOF_IMU"
    double nominal_rate_hz{0.0};
    bool enabled{true};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StreamInfo, name, type, format, nominal_rate_hz, enabled)
};

class DeviceProfile {
public:
    DeviceProfile() = default;

    // Core Identifiers
    const std::string& profile_id() const noexcept { return profile_id_; }
    void set_profile_id(const std::string& id) { profile_id_ = id; }

    int schema_version() const noexcept { return schema_version_; }
    const std::string& profile_version() const noexcept { return profile_version_; }
    void set_profile_version(const std::string& v) { profile_version_ = v; }

    const std::string& manufacturer() const noexcept { return manufacturer_; }
    void set_manufacturer(const std::string& m) { manufacturer_ = m; }

    const std::string& model() const noexcept { return model_; }
    void set_model(const std::string& m) { model_ = m; }

    const std::string& device_family() const noexcept { return device_family_; }
    void set_device_family(const std::string& df) { device_family_ = df; }

    ProfileStatus profile_status() const noexcept { return profile_status_; }
    void set_profile_status(ProfileStatus s) { profile_status_ = s; }

    const std::string& hardware_revision() const noexcept { return hardware_revision_; }
    void set_hardware_revision(const std::string& hr) { hardware_revision_ = hr; }

    const std::string& firmware() const noexcept { return firmware_; }
    void set_firmware(const std::string& fw) { firmware_ = fw; }

    // Capabilities & Interfaces
    const std::vector<std::string>& capabilities() const noexcept { return capabilities_; }
    void set_capabilities(const std::vector<std::string>& c) { capabilities_ = c; }

    const std::string& transport() const noexcept { return transport_; }
    void set_transport(const std::string& t) { transport_ = t; }

    const std::string& driver() const noexcept { return driver_; }
    void set_driver(const std::string& d) { driver_ = d; }

    const std::string& sdk() const noexcept { return sdk_; }
    void set_sdk(const std::string& s) { sdk_ = s; }

    // Streams & Timing
    const std::vector<StreamInfo>& streams() const noexcept { return streams_; }
    void set_streams(const std::vector<StreamInfo>& s) { streams_ = s; }

    const std::vector<std::string>& timestamp_sources() const noexcept { return timestamp_sources_; }
    void set_timestamp_sources(const std::vector<std::string>& ts) { timestamp_sources_ = ts; }

    const std::string& synchronization() const noexcept { return synchronization_; }
    void set_synchronization(const std::string& sync) { synchronization_ = sync; }

    // Coordinate Frames & Calibration
    const std::map<std::string, std::string>& coordinate_frames() const noexcept { return coordinate_frames_; }
    void set_coordinate_frames(const std::map<std::string, std::string>& cf) { coordinate_frames_ = cf; }

    const nlohmann::json& intrinsics() const noexcept { return intrinsics_; }
    void set_intrinsics(const nlohmann::json& i) { intrinsics_ = i; }

    const nlohmann::json& internal_extrinsics() const noexcept { return internal_extrinsics_; }
    void set_internal_extrinsics(const nlohmann::json& ie) { internal_extrinsics_ = ie; }

    const std::vector<CalibrationProvenance>& calibration_sources() const noexcept { return calibration_sources_; }
    void set_calibration_sources(const std::vector<CalibrationProvenance>& cs) { calibration_sources_ = cs; }

    // SLAM Strategy
    const std::string& slam_family() const noexcept { return slam_family_; }
    void set_slam_family(const std::string& sf) { slam_family_ = sf; }

    const std::string& slam_backend() const noexcept { return slam_backend_; }
    void set_slam_backend(const std::string& sb) { slam_backend_ = sb; }

    const nlohmann::json& slam_parameters() const noexcept { return slam_parameters_; }
    void set_slam_parameters(const nlohmann::json& sp) { slam_parameters_ = sp; }

    // Pipeline Preprocessing & Output Contract
    const nlohmann::json& preprocessing() const noexcept { return preprocessing_; }
    void set_preprocessing(const nlohmann::json& pp) { preprocessing_ = pp; }

    const nlohmann::json& output_contract() const noexcept { return output_contract_; }
    void set_output_contract(const nlohmann::json& oc) { output_contract_ = oc; }

    const nlohmann::json& validation_results() const noexcept { return validation_results_; }
    void set_validation_results(const nlohmann::json& vr) { validation_results_ = vr; }

    const CalibrationProvenance& provenance() const noexcept { return provenance_; }
    void set_provenance(const CalibrationProvenance& p) { provenance_ = p; }

    // Serialization
    nlohmann::json to_json() const;
    static DeviceProfile from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string profile_id_;
    std::string profile_version_{"1.0.0"};
    std::string manufacturer_;
    std::string model_;
    std::string device_family_;
    ProfileStatus profile_status_{ProfileStatus::EXPERIMENTAL};

    std::string hardware_revision_;
    std::string firmware_;

    std::vector<std::string> capabilities_;
    std::string transport_;
    std::string driver_;
    std::string sdk_;

    std::vector<StreamInfo> streams_;
    std::vector<std::string> timestamp_sources_;
    std::string synchronization_;

    std::map<std::string, std::string> coordinate_frames_;
    nlohmann::json intrinsics_{nlohmann::json::object()};
    nlohmann::json internal_extrinsics_{nlohmann::json::object()};
    std::vector<CalibrationProvenance> calibration_sources_;

    std::string slam_family_;
    std::string slam_backend_;
    nlohmann::json slam_parameters_{nlohmann::json::object()};

    nlohmann::json preprocessing_{nlohmann::json::object()};
    nlohmann::json output_contract_{nlohmann::json::object()};
    nlohmann::json validation_results_{nlohmann::json::object()};
    CalibrationProvenance provenance_;
};

} // namespace av::core::schemas
