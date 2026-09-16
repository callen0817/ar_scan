#pragma once

#include "types.hpp"
#include "calibration_provenance.hpp"
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class MapMetadata {
public:
    MapMetadata() = default;

    MapMetadata(const std::string& map_id,
                const std::string& session_id,
                const std::string& source_sensor,
                const std::string& scanner_node)
        : schema_version_(CURRENT_SCHEMA_VERSION),
          map_id_(map_id),
          source_sensor_(source_sensor),
          scanner_node_(scanner_node),
          session_id_(session_id) {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& map_id() const noexcept { return map_id_; }
    uint64_t timestamp_ns() const noexcept { return timestamp_ns_; }
    const Pose3D& current_pose() const noexcept { return current_pose_; }
    const std::vector<Pose3D>& trajectory() const noexcept { return trajectory_; }
    const std::string& map_geometry_format() const noexcept { return map_geometry_format_; }
    uint64_t point_count() const noexcept { return point_count_; }
    TrackingState tracking_state() const noexcept { return tracking_state_; }
    const std::string& source_sensor() const noexcept { return source_sensor_; }
    const std::string& scanner_node() const noexcept { return scanner_node_; }
    const std::string& session_id() const noexcept { return session_id_; }
    const std::map<std::string, std::string>& coordinate_frame_metadata() const noexcept { return coordinate_frame_metadata_; }
    const CalibrationProvenance& provenance() const noexcept { return provenance_; }

    void set_map_id(const std::string& id) { map_id_ = id; }
    void set_timestamp_ns(uint64_t t) { timestamp_ns_ = t; }
    void set_current_pose(const Pose3D& p) { current_pose_ = p; }
    void add_trajectory_pose(const Pose3D& p) { trajectory_.push_back(p); }
    void set_trajectory(const std::vector<Pose3D>& traj) { trajectory_ = traj; }
    void set_map_geometry_format(const std::string& fmt) { map_geometry_format_ = fmt; }
    void set_point_count(uint64_t cnt) { point_count_ = cnt; }
    void set_tracking_state(TrackingState st) { tracking_state_ = st; }
    void set_source_sensor(const std::string& s) { source_sensor_ = s; }
    void set_scanner_node(const std::string& n) { scanner_node_ = n; }
    void set_session_id(const std::string& s) { session_id_ = s; }
    void set_coordinate_frame_metadata(const std::map<std::string, std::string>& cf) { coordinate_frame_metadata_ = cf; }
    void set_provenance(const CalibrationProvenance& p) { provenance_ = p; }

    nlohmann::json to_json() const;
    static MapMetadata from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string map_id_;
    uint64_t timestamp_ns_{0};
    Pose3D current_pose_;
    std::vector<Pose3D> trajectory_;
    std::string map_geometry_format_{"PCL_PCD"};
    uint64_t point_count_{0};
    TrackingState tracking_state_{TrackingState::NO_TRACKING};
    std::string source_sensor_;
    std::string scanner_node_;
    std::string session_id_;
    std::map<std::string, std::string> coordinate_frame_metadata_;
    CalibrationProvenance provenance_;
};

} // namespace av::core::schemas
