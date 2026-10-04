#include "capture.hpp"

namespace av::core::schemas {

nlohmann::json Capture::to_json() const {
    return {
        {"schema_version", schema_version_},
        {"capture_id", capture_id_},
        {"session_id", session_id_},
        {"sensor_instance_id", sensor_instance_id_},
        {"owner_node_uuid", owner_node_uuid_},
        {"requested_start_time_ns", requested_start_time_ns_},
        {"start_timestamp_ns", start_timestamp_ns_},
        {"requested_end_time_ns", requested_end_time_ns_},
        {"end_timestamp_ns", end_timestamp_ns_},
        {"clock_sync_source", to_string(clock_sync_source_)},
        {"estimated_clock_offset_ns", estimated_clock_offset_ns_},
        {"observation_count", observation_count_},
        {"raw_data_path", raw_data_path_},
        {"trajectory_path", trajectory_path_},
        {"map_path", map_path_},
        {"status", status_},
        {"provenance", provenance_.to_json()}
    };
}

Capture Capture::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for Capture");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("Capture", version, CURRENT_SCHEMA_VERSION);
    }

    Capture cap;
    cap.schema_version_ = version;
    cap.capture_id_ = j.value("capture_id", "");
    cap.session_id_ = j.value("session_id", "");
    cap.sensor_instance_id_ = j.value("sensor_instance_id", "");
    cap.owner_node_uuid_ = j.value("owner_node_uuid", "");
    cap.requested_start_time_ns_ = j.value("requested_start_time_ns", static_cast<uint64_t>(0));
    cap.start_timestamp_ns_ = j.value("start_timestamp_ns", static_cast<uint64_t>(0));
    cap.requested_end_time_ns_ = j.value("requested_end_time_ns", static_cast<uint64_t>(0));
    cap.end_timestamp_ns_ = j.value("end_timestamp_ns", static_cast<uint64_t>(0));
    
    if (j.contains("clock_sync_source")) {
        cap.clock_sync_source_ = clock_sync_source_from_string(j["clock_sync_source"].get<std::string>());
    }
    cap.estimated_clock_offset_ns_ = j.value("estimated_clock_offset_ns", static_cast<int64_t>(0));

    cap.observation_count_ = j.value("observation_count", static_cast<uint64_t>(0));
    cap.raw_data_path_ = j.value("raw_data_path", "");
    cap.trajectory_path_ = j.value("trajectory_path", "");
    cap.map_path_ = j.value("map_path", "");
    cap.status_ = j.value("status", "ACTIVE");

    if (j.contains("provenance")) {
        cap.provenance_ = CalibrationProvenance::from_json(j["provenance"]);
    }

    return cap;
}

} // namespace av::core::schemas
