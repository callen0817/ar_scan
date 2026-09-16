#pragma once

#include "types.hpp"
#include "calibration_provenance.hpp"
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class Capture {
public:
    Capture() = default;

    Capture(const std::string& capture_id,
            const std::string& session_id,
            const std::string& sensor_instance_id,
            uint64_t start_time_ns = 0)
        : schema_version_(CURRENT_SCHEMA_VERSION),
          capture_id_(capture_id),
          session_id_(session_id),
          sensor_instance_id_(sensor_instance_id),
          start_timestamp_ns_(start_time_ns) {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& capture_id() const noexcept { return capture_id_; }
    const std::string& session_id() const noexcept { return session_id_; }
    const std::string& sensor_instance_id() const noexcept { return sensor_instance_id_; }
    uint64_t start_timestamp_ns() const noexcept { return start_timestamp_ns_; }
    uint64_t end_timestamp_ns() const noexcept { return end_timestamp_ns_; }
    uint64_t observation_count() const noexcept { return observation_count_; }
    const std::string& raw_data_path() const noexcept { return raw_data_path_; }
    const std::string& trajectory_path() const noexcept { return trajectory_path_; }
    const std::string& map_path() const noexcept { return map_path_; }
    const std::string& status() const noexcept { return status_; }
    const CalibrationProvenance& provenance() const noexcept { return provenance_; }

    void set_capture_id(const std::string& id) { capture_id_ = id; }
    void set_session_id(const std::string& sid) { session_id_ = sid; }
    void set_sensor_instance_id(const std::string& iid) { sensor_instance_id_ = iid; }
    void set_start_timestamp_ns(uint64_t t) { start_timestamp_ns_ = t; }
    void set_end_timestamp_ns(uint64_t t) { end_timestamp_ns_ = t; }
    void set_observation_count(uint64_t count) { observation_count_ = count; }
    void increment_observations() { ++observation_count_; }
    void set_raw_data_path(const std::string& path) { raw_data_path_ = path; }
    void set_trajectory_path(const std::string& path) { trajectory_path_ = path; }
    void set_map_path(const std::string& path) { map_path_ = path; }
    void set_status(const std::string& s) { status_ = s; }
    void set_provenance(const CalibrationProvenance& p) { provenance_ = p; }

    nlohmann::json to_json() const;
    static Capture from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string capture_id_;
    std::string session_id_;
    std::string sensor_instance_id_;
    uint64_t start_timestamp_ns_{0};
    uint64_t end_timestamp_ns_{0};
    uint64_t observation_count_{0};
    std::string raw_data_path_;
    std::string trajectory_path_;
    std::string map_path_;
    std::string status_{"ACTIVE"};
    CalibrationProvenance provenance_;
};

} // namespace av::core::schemas
