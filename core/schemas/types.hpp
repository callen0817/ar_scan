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
    VALIDATED,
    EXPERIMENTAL,
    REJECTED
};

inline std::string to_string(ProfileStatus status) {
    switch (status) {
        case ProfileStatus::VALIDATED: return "VALIDATED";
        case ProfileStatus::EXPERIMENTAL: return "EXPERIMENTAL";
        case ProfileStatus::REJECTED: return "REJECTED";
        default: return "EXPERIMENTAL";
    }
}

inline ProfileStatus profile_status_from_string(const std::string& str) {
    if (str == "VALIDATED") return ProfileStatus::VALIDATED;
    if (str == "REJECTED") return ProfileStatus::REJECTED;
    return ProfileStatus::EXPERIMENTAL;
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

} // namespace av::core::schemas
