#include "map_metadata.hpp"

namespace av::core::schemas {

nlohmann::json MapMetadata::to_json() const {
    nlohmann::json traj_arr = nlohmann::json::array();
    for (const auto& p : trajectory_) {
        traj_arr.push_back({
            {"timestamp_ns", p.timestamp_ns},
            {"position", {{"x", p.position.x}, {"y", p.position.y}, {"z", p.position.z}}},
            {"orientation", {{"x", p.orientation.x}, {"y", p.orientation.y}, {"z", p.orientation.z}, {"w", p.orientation.w}}}
        });
    }

    return {
        {"schema_version", schema_version_},
        {"map_id", map_id_},
        {"timestamp_ns", timestamp_ns_},
        {"current_pose", {
            {"timestamp_ns", current_pose_.timestamp_ns},
            {"position", {{"x", current_pose_.position.x}, {"y", current_pose_.position.y}, {"z", current_pose_.position.z}}},
            {"orientation", {{"x", current_pose_.orientation.x}, {"y", current_pose_.orientation.y}, {"z", current_pose_.orientation.z}, {"w", current_pose_.orientation.w}}}
        }},
        {"trajectory", traj_arr},
        {"map_geometry_format", map_geometry_format_},
        {"point_count", point_count_},
        {"tracking_state", to_string(tracking_state_)},
        {"source_sensor", source_sensor_},
        {"scanner_node", scanner_node_},
        {"session_id", session_id_},
        {"coordinate_frame_metadata", coordinate_frame_metadata_},
        {"provenance", provenance_.to_json()}
    };
}

MapMetadata MapMetadata::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for MapMetadata");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("MapMetadata", version, CURRENT_SCHEMA_VERSION);
    }

    MapMetadata meta;
    meta.schema_version_ = version;
    meta.map_id_ = j.value("map_id", "");
    meta.timestamp_ns_ = j.value("timestamp_ns", static_cast<uint64_t>(0));

    if (j.contains("current_pose") && j["current_pose"].is_object()) {
        const auto& p = j["current_pose"];
        meta.current_pose_.timestamp_ns = p.value("timestamp_ns", static_cast<uint64_t>(0));
        if (p.contains("position")) {
            meta.current_pose_.position.x = p["position"].value("x", 0.0);
            meta.current_pose_.position.y = p["position"].value("y", 0.0);
            meta.current_pose_.position.z = p["position"].value("z", 0.0);
        }
        if (p.contains("orientation")) {
            meta.current_pose_.orientation.x = p["orientation"].value("x", 0.0);
            meta.current_pose_.orientation.y = p["orientation"].value("y", 0.0);
            meta.current_pose_.orientation.z = p["orientation"].value("z", 0.0);
            meta.current_pose_.orientation.w = p["orientation"].value("w", 1.0);
        }
    }

    if (j.contains("trajectory") && j["trajectory"].is_array()) {
        for (const auto& item : j["trajectory"]) {
            Pose3D pose;
            pose.timestamp_ns = item.value("timestamp_ns", static_cast<uint64_t>(0));
            if (item.contains("position")) {
                pose.position.x = item["position"].value("x", 0.0);
                pose.position.y = item["position"].value("y", 0.0);
                pose.position.z = item["position"].value("z", 0.0);
            }
            if (item.contains("orientation")) {
                pose.orientation.x = item["orientation"].value("x", 0.0);
                pose.orientation.y = item["orientation"].value("y", 0.0);
                pose.orientation.z = item["orientation"].value("z", 0.0);
                pose.orientation.w = item["orientation"].value("w", 1.0);
            }
            meta.trajectory_.push_back(pose);
        }
    }

    meta.map_geometry_format_ = j.value("map_geometry_format", "PCL_PCD");
    meta.point_count_ = j.value("point_count", static_cast<uint64_t>(0));
    meta.tracking_state_ = tracking_state_from_string(j.value("tracking_state", "NO_TRACKING"));
    meta.source_sensor_ = j.value("source_sensor", "");
    meta.scanner_node_ = j.value("scanner_node", "");
    meta.session_id_ = j.value("session_id", "");

    if (j.contains("coordinate_frame_metadata") && j["coordinate_frame_metadata"].is_object()) {
        meta.coordinate_frame_metadata_ = j["coordinate_frame_metadata"].get<std::map<std::string, std::string>>();
    }

    if (j.contains("provenance")) {
        meta.provenance_ = CalibrationProvenance::from_json(j["provenance"]);
    }

    return meta;
}

} // namespace av::core::schemas
