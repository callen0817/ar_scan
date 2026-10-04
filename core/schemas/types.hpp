#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

// Versioning constant for M1 schemas
constexpr int CURRENT_SCHEMA_VERSION = 1;

class SchemaVersionException : public std::runtime_error {
public:
    SchemaVersionException(const std::string& schema_name, int found, int expected)
        : std::runtime_error("Schema version mismatch for '" + schema_name + 
                             "': found version " + std::to_string(found) + 
                             ", expected up to " + std::to_string(expected)),
          schema_name_(schema_name), found_version_(found), expected_version_(expected) {}

    const std::string& schema_name() const noexcept { return schema_name_; }
    int found_version() const noexcept { return found_version_; }
    int expected_version() const noexcept { return expected_version_; }

private:
    std::string schema_name_;
    int found_version_;
    int expected_version_;
};

enum class CalibrationSource {
    DEVICE_FACTORY,
    OFFICIAL_SDK,
    OFFICIAL_DOCUMENTATION,
    OFFICIAL_DRIVER,
    AV_CALIBRATED,
    USER_MEASURED,
    INFERRED_FROM_GEOMETRY,
    UNKNOWN
};

inline std::string to_string(CalibrationSource src) {
    switch (src) {
        case CalibrationSource::DEVICE_FACTORY: return "DEVICE_FACTORY";
        case CalibrationSource::OFFICIAL_SDK: return "OFFICIAL_SDK";
        case CalibrationSource::OFFICIAL_DOCUMENTATION: return "OFFICIAL_DOCUMENTATION";
        case CalibrationSource::OFFICIAL_DRIVER: return "OFFICIAL_DRIVER";
        case CalibrationSource::AV_CALIBRATED: return "AV_CALIBRATED";
        case CalibrationSource::USER_MEASURED: return "USER_MEASURED";
        case CalibrationSource::INFERRED_FROM_GEOMETRY: return "INFERRED_FROM_GEOMETRY";
        default: return "UNKNOWN";
    }
}

inline CalibrationSource calibration_source_from_string(const std::string& str) {
    if (str == "DEVICE_FACTORY") return CalibrationSource::DEVICE_FACTORY;
    if (str == "OFFICIAL_SDK") return CalibrationSource::OFFICIAL_SDK;
    if (str == "OFFICIAL_DOCUMENTATION") return CalibrationSource::OFFICIAL_DOCUMENTATION;
    if (str == "OFFICIAL_DRIVER") return CalibrationSource::OFFICIAL_DRIVER;
    if (str == "AV_CALIBRATED") return CalibrationSource::AV_CALIBRATED;
    if (str == "USER_MEASURED") return CalibrationSource::USER_MEASURED;
    if (str == "INFERRED_FROM_GEOMETRY") return CalibrationSource::INFERRED_FROM_GEOMETRY;
    return CalibrationSource::UNKNOWN;
}

enum class ProfileStatus {
    DRAFT,
    EXPERIMENTAL,
    VALIDATED,
    REJECTED
};

inline std::string to_string(ProfileStatus status) {
    switch (status) {
        case ProfileStatus::DRAFT: return "DRAFT";
        case ProfileStatus::EXPERIMENTAL: return "EXPERIMENTAL";
        case ProfileStatus::VALIDATED: return "VALIDATED";
        case ProfileStatus::REJECTED: return "REJECTED";
        default: return "DRAFT";
    }
}

inline ProfileStatus profile_status_from_string(const std::string& str) {
    if (str == "VALIDATED") return ProfileStatus::VALIDATED;
    if (str == "EXPERIMENTAL") return ProfileStatus::EXPERIMENTAL;
    if (str == "REJECTED") return ProfileStatus::REJECTED;
    return ProfileStatus::DRAFT;
}

enum class SensorRelationshipType {
    RIGID,
    INDEPENDENT
};

inline std::string to_string(SensorRelationshipType rel) {
    switch (rel) {
        case SensorRelationshipType::RIGID: return "RIGID";
        case SensorRelationshipType::INDEPENDENT: return "INDEPENDENT";
        default: return "RIGID";
    }
}

inline SensorRelationshipType relationship_type_from_string(const std::string& str) {
    if (str == "INDEPENDENT") return SensorRelationshipType::INDEPENDENT;
    return SensorRelationshipType::RIGID;
}

enum class ExtrinsicStatus {
    UNKNOWN,
    ESTIMATED,
    VALIDATED,
    FACTORY_KNOWN
};

inline std::string to_string(ExtrinsicStatus status) {
    switch (status) {
        case ExtrinsicStatus::UNKNOWN: return "UNKNOWN";
        case ExtrinsicStatus::ESTIMATED: return "ESTIMATED";
        case ExtrinsicStatus::VALIDATED: return "VALIDATED";
        case ExtrinsicStatus::FACTORY_KNOWN: return "FACTORY_KNOWN";
        default: return "UNKNOWN";
    }
}

inline ExtrinsicStatus extrinsic_status_from_string(const std::string& str) {
    if (str == "ESTIMATED") return ExtrinsicStatus::ESTIMATED;
    if (str == "VALIDATED") return ExtrinsicStatus::VALIDATED;
    if (str == "FACTORY_KNOWN") return ExtrinsicStatus::FACTORY_KNOWN;
    return ExtrinsicStatus::UNKNOWN;
}

enum class TrackingState {
    NO_TRACKING,
    INITIALIZING,
    TRACKING_OK,
    TRACKING_DEGRADED,
    TRACKING_LOST
};

inline std::string to_string(TrackingState state) {
    switch (state) {
        case TrackingState::NO_TRACKING: return "NO_TRACKING";
        case TrackingState::INITIALIZING: return "INITIALIZING";
        case TrackingState::TRACKING_OK: return "TRACKING_OK";
        case TrackingState::TRACKING_DEGRADED: return "TRACKING_DEGRADED";
        case TrackingState::TRACKING_LOST: return "TRACKING_LOST";
        default: return "NO_TRACKING";
    }
}

inline TrackingState tracking_state_from_string(const std::string& str) {
    if (str == "INITIALIZING") return TrackingState::INITIALIZING;
    if (str == "TRACKING_OK") return TrackingState::TRACKING_OK;
    if (str == "TRACKING_DEGRADED") return TrackingState::TRACKING_DEGRADED;
    if (str == "TRACKING_LOST") return TrackingState::TRACKING_LOST;
    return TrackingState::NO_TRACKING;
}

enum class SessionStatus {
    INITIALIZING,
    ACTIVE,
    STOPPED,
    SAVED
};

inline std::string to_string(SessionStatus status) {
    switch (status) {
        case SessionStatus::INITIALIZING: return "INITIALIZING";
        case SessionStatus::ACTIVE: return "ACTIVE";
        case SessionStatus::STOPPED: return "STOPPED";
        case SessionStatus::SAVED: return "SAVED";
        default: return "INITIALIZING";
    }
}

inline SessionStatus session_status_from_string(const std::string& str) {
    if (str == "ACTIVE") return SessionStatus::ACTIVE;
    if (str == "STOPPED") return SessionStatus::STOPPED;
    if (str == "SAVED") return SessionStatus::SAVED;
    return SessionStatus::INITIALIZING;
}

struct Vector3D {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vector3D, x, y, z)
};

struct Quaternion {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{1.0};

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Quaternion, x, y, z, w)
};

struct Pose3D {
    uint64_t timestamp_ns{0};
    Vector3D position;
    Quaternion orientation;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Pose3D, timestamp_ns, position, orientation)
};

struct Transform3D {
    Vector3D translation;
    Quaternion rotation;
    std::vector<double> matrix4x4; // Optional flattened 4x4 row-major transform matrix

    nlohmann::json to_json() const {
        return {
            {"translation", translation},
            {"rotation", rotation},
            {"matrix4x4", matrix4x4}
        };
    }

    static Transform3D from_json(const nlohmann::json& j) {
        Transform3D t;
        if (j.contains("translation")) t.translation = j["translation"].get<Vector3D>();
        if (j.contains("rotation")) t.rotation = j["rotation"].get<Quaternion>();
        if (j.contains("matrix4x4")) t.matrix4x4 = j["matrix4x4"].get<std::vector<double>>();
        return t;
    }
};

enum class NodePlatform {
    LINUX_ARM64,
    LINUX_X64,
    WINDOWS_X64,
    MACOS_ARM64,
    MACOS_X64,
    IOS_ARM64,
    UNKNOWN
};

inline std::string to_string(NodePlatform p) {
    switch (p) {
        case NodePlatform::LINUX_ARM64: return "LINUX_ARM64";
        case NodePlatform::LINUX_X64: return "LINUX_X64";
        case NodePlatform::WINDOWS_X64: return "WINDOWS_X64";
        case NodePlatform::MACOS_ARM64: return "MACOS_ARM64";
        case NodePlatform::MACOS_X64: return "MACOS_X64";
        case NodePlatform::IOS_ARM64: return "IOS_ARM64";
        default: return "UNKNOWN";
    }
}

inline NodePlatform node_platform_from_string(const std::string& str) {
    if (str == "LINUX_ARM64" || str == "JETSON_LINUX") return NodePlatform::LINUX_ARM64;
    if (str == "LINUX_X64") return NodePlatform::LINUX_X64;
    if (str == "WINDOWS_X64") return NodePlatform::WINDOWS_X64;
    if (str == "MACOS_ARM64") return NodePlatform::MACOS_ARM64;
    if (str == "MACOS_X64") return NodePlatform::MACOS_X64;
    if (str == "IOS_ARM64") return NodePlatform::IOS_ARM64;
    return NodePlatform::UNKNOWN;
}

enum class NodeSessionRole {
    SESSION_COORDINATOR,
    SESSION_PARTICIPANT,
    UNASSIGNED
};

inline std::string to_string(NodeSessionRole r) {
    switch (r) {
        case NodeSessionRole::SESSION_COORDINATOR: return "SESSION_COORDINATOR";
        case NodeSessionRole::SESSION_PARTICIPANT: return "SESSION_PARTICIPANT";
        default: return "UNASSIGNED";
    }
}

inline NodeSessionRole node_session_role_from_string(const std::string& str) {
    if (str == "SESSION_COORDINATOR" || str == "PRIMARY_ORCHESTRATOR") return NodeSessionRole::SESSION_COORDINATOR;
    if (str == "SESSION_PARTICIPANT" || str == "CAPTURE_WORKER") return NodeSessionRole::SESSION_PARTICIPANT;
    return NodeSessionRole::UNASSIGNED;
}

enum class NodeOnlineState {
    ONLINE,
    OFFLINE,
    CAPTURING,
    DEGRADED
};

inline std::string to_string(NodeOnlineState s) {
    switch (s) {
        case NodeOnlineState::ONLINE: return "ONLINE";
        case NodeOnlineState::OFFLINE: return "OFFLINE";
        case NodeOnlineState::CAPTURING: return "CAPTURING";
        case NodeOnlineState::DEGRADED: return "DEGRADED";
        default: return "OFFLINE";
    }
}

inline NodeOnlineState node_online_state_from_string(const std::string& str) {
    if (str == "ONLINE") return NodeOnlineState::ONLINE;
    if (str == "CAPTURING") return NodeOnlineState::CAPTURING;
    if (str == "DEGRADED") return NodeOnlineState::DEGRADED;
    return NodeOnlineState::OFFLINE;
}

enum class ClockSyncSource {
    SYSTEM_UNSYNCED,
    NTP,
    PTP,
    GNSS,
    AV_ESTIMATED
};

inline std::string to_string(ClockSyncSource s) {
    switch (s) {
        case ClockSyncSource::SYSTEM_UNSYNCED: return "SYSTEM_UNSYNCED";
        case ClockSyncSource::NTP: return "NTP";
        case ClockSyncSource::PTP: return "PTP";
        case ClockSyncSource::GNSS: return "GNSS";
        case ClockSyncSource::AV_ESTIMATED: return "AV_ESTIMATED";
        default: return "SYSTEM_UNSYNCED";
    }
}

inline ClockSyncSource clock_sync_source_from_string(const std::string& str) {
    if (str == "NTP") return ClockSyncSource::NTP;
    if (str == "PTP") return ClockSyncSource::PTP;
    if (str == "GNSS") return ClockSyncSource::GNSS;
    if (str == "AV_ESTIMATED") return ClockSyncSource::AV_ESTIMATED;
    return ClockSyncSource::SYSTEM_UNSYNCED;
}

} // namespace av::core::schemas

