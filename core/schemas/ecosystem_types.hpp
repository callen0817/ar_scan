#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

/**
 * Knowledge vs. Validation distinction:
 * Tracks the empirical degree of hardware verification in AV Scan.
 */
enum class ValidationTier {
    DOCUMENTED,         // Model documented from official vendor specs / datasheets
    SDK_SUPPORTED,      // Vendor SDK integrated, compiled, and API-mapped
    SDK_INTERROGATED,   // Communication verified with physical/simulated hardware via SDK
    PHYSICALLY_TESTED,  // End-to-end stream collected and evaluated on physical testbench
    AV_VALIDATED        // Metric and temporal calibration fully qualified in AV Scan
};

inline std::string to_string(ValidationTier tier) {
    switch (tier) {
        case ValidationTier::DOCUMENTED: return "DOCUMENTED";
        case ValidationTier::SDK_SUPPORTED: return "SDK_SUPPORTED";
        case ValidationTier::SDK_INTERROGATED: return "SDK_INTERROGATED";
        case ValidationTier::PHYSICALLY_TESTED: return "PHYSICALLY_TESTED";
        case ValidationTier::AV_VALIDATED: return "AV_VALIDATED";
        default: return "DOCUMENTED";
    }
}

inline ValidationTier validation_tier_from_string(const std::string& str) {
    if (str == "SDK_SUPPORTED") return ValidationTier::SDK_SUPPORTED;
    if (str == "SDK_INTERROGATED") return ValidationTier::SDK_INTERROGATED;
    if (str == "PHYSICALLY_TESTED") return ValidationTier::PHYSICALLY_TESTED;
    if (str == "AV_VALIDATED") return ValidationTier::AV_VALIDATED;
    return ValidationTier::DOCUMENTED;
}

/**
 * Commercial hardware generation lifecycle.
 */
enum class ProductionStatus {
    CURRENT,            // Currently produced flagship hardware
    PREVIOUS,           // Preceding generation; actively supported
    LEGACY,             // Older hardware branch requiring legacy SDK or protocol handling
    DISCONTINUED        // Archived or end-of-life hardware
};

inline std::string to_string(ProductionStatus status) {
    switch (status) {
        case ProductionStatus::CURRENT: return "CURRENT";
        case ProductionStatus::PREVIOUS: return "PREVIOUS";
        case ProductionStatus::LEGACY: return "LEGACY";
        case ProductionStatus::DISCONTINUED: return "DISCONTINUED";
        default: return "CURRENT";
    }
}

inline ProductionStatus production_status_from_string(const std::string& str) {
    if (str == "PREVIOUS") return ProductionStatus::PREVIOUS;
    if (str == "LEGACY") return ProductionStatus::LEGACY;
    if (str == "DISCONTINUED") return ProductionStatus::DISCONTINUED;
    return ProductionStatus::CURRENT;
}

/**
 * Source provenance of the specification / drivers.
 */
enum class SourceProvenance {
    OFFICIAL_CURRENT_SDK,
    OFFICIAL_LEGACY_SDK,
    OFFICIAL_DRIVER,
    OFFICIAL_ARCHIVED_DRIVER,
    OFFICIAL_DOCUMENTATION,
    AV_VALIDATED
};

inline std::string to_string(SourceProvenance prov) {
    switch (prov) {
        case SourceProvenance::OFFICIAL_CURRENT_SDK: return "OFFICIAL_CURRENT_SDK";
        case SourceProvenance::OFFICIAL_LEGACY_SDK: return "OFFICIAL_LEGACY_SDK";
        case SourceProvenance::OFFICIAL_DRIVER: return "OFFICIAL_DRIVER";
        case SourceProvenance::OFFICIAL_ARCHIVED_DRIVER: return "OFFICIAL_ARCHIVED_DRIVER";
        case SourceProvenance::OFFICIAL_DOCUMENTATION: return "OFFICIAL_DOCUMENTATION";
        case SourceProvenance::AV_VALIDATED: return "AV_VALIDATED";
        default: return "OFFICIAL_DOCUMENTATION";
    }
}

inline SourceProvenance source_provenance_from_string(const std::string& str) {
    if (str == "OFFICIAL_CURRENT_SDK") return SourceProvenance::OFFICIAL_CURRENT_SDK;
    if (str == "OFFICIAL_LEGACY_SDK") return SourceProvenance::OFFICIAL_LEGACY_SDK;
    if (str == "OFFICIAL_DRIVER") return SourceProvenance::OFFICIAL_DRIVER;
    if (str == "OFFICIAL_ARCHIVED_DRIVER") return SourceProvenance::OFFICIAL_ARCHIVED_DRIVER;
    if (str == "AV_VALIDATED") return SourceProvenance::AV_VALIDATED;
    return SourceProvenance::OFFICIAL_DOCUMENTATION;
}

/**
 * 6-Tier Hierarchical Sensor Resolution Pipeline:
 * CONNECTED SENSOR ->
 *   1. Exact AV-validated profile?
 *   2. Known model definition?
 *   3. Recognized vendor SDK?
 *   4. Recognized standard transport?
 *   5. Generic M14 profiler?
 *   6. Capability check -> match compatible backend or report UNSUPPORTED / NONE.
 */
enum class ResolutionTier {
    TIER1_EXACT_VALIDATED_PROFILE,
    TIER2_KNOWN_MODEL_DEFINITION,
    TIER3_RECOGNIZED_VENDOR_SDK,
    TIER4_RECOGNIZED_STANDARD_TRANSPORT,
    TIER5_GENERIC_PROFILER,
    TIER6_UNSUPPORTED_SPATIAL_CAPTURE
};

inline std::string to_string(ResolutionTier tier) {
    switch (tier) {
        case ResolutionTier::TIER1_EXACT_VALIDATED_PROFILE: return "TIER1_EXACT_VALIDATED_PROFILE";
        case ResolutionTier::TIER2_KNOWN_MODEL_DEFINITION: return "TIER2_KNOWN_MODEL_DEFINITION";
        case ResolutionTier::TIER3_RECOGNIZED_VENDOR_SDK: return "TIER3_RECOGNIZED_VENDOR_SDK";
        case ResolutionTier::TIER4_RECOGNIZED_STANDARD_TRANSPORT: return "TIER4_RECOGNIZED_STANDARD_TRANSPORT";
        case ResolutionTier::TIER5_GENERIC_PROFILER: return "TIER5_GENERIC_PROFILER";
        case ResolutionTier::TIER6_UNSUPPORTED_SPATIAL_CAPTURE: return "TIER6_UNSUPPORTED_SPATIAL_CAPTURE";
        default: return "TIER5_GENERIC_PROFILER";
    }
}

/**
 * Hardware revision specific capability flags.
 */
struct HardwareRevisionFeatures {
    bool has_lidar{false};
    bool has_rgb{false};                // Critical distinction: e.g. Ouster Rev8 RGB vs Rev7
    bool has_depth{false};
    bool has_imu{false};
    bool has_gnss{false};
    bool has_ptp{false};
    bool has_pps{false};
    bool has_hardware_sync{false};
    std::vector<std::string> return_modes;         // e.g. ["SINGLE", "DUAL", "STRONGEST", "LAST"]
    std::string native_coordinate_system{"FLU"};   // FLU (+X forward, +Y left, +Z up)
    std::vector<std::string> point_cloud_fields;   // e.g. ["x", "y", "z", "intensity", "range", "rgb"]
    std::vector<double> scan_rates_hz;             // e.g. [10.0, 20.0]
    std::string factory_calibration_api;          // e.g. "DIFOP_EEPROM", "FLASH_PARAMS", "FACTORY_EEPROM"
    std::string timestamp_source;                  // e.g. "HARDWARE_PTP", "NTP", "HOST_RECEIVE"

    nlohmann::json to_json() const {
        return {
            {"has_lidar", has_lidar},
            {"has_rgb", has_rgb},
            {"has_depth", has_depth},
            {"has_imu", has_imu},
            {"has_gnss", has_gnss},
            {"has_ptp", has_ptp},
            {"has_pps", has_pps},
            {"has_hardware_sync", has_hardware_sync},
            {"return_modes", return_modes},
            {"native_coordinate_system", native_coordinate_system},
            {"point_cloud_fields", point_cloud_fields},
            {"scan_rates_hz", scan_rates_hz},
            {"factory_calibration_api", factory_calibration_api},
            {"timestamp_source", timestamp_source}
        };
    }

    static HardwareRevisionFeatures from_json(const nlohmann::json& j) {
        HardwareRevisionFeatures f;
        if (j.contains("has_lidar")) f.has_lidar = j["has_lidar"].get<bool>();
        if (j.contains("has_rgb")) f.has_rgb = j["has_rgb"].get<bool>();
        if (j.contains("has_depth")) f.has_depth = j["has_depth"].get<bool>();
        if (j.contains("has_imu")) f.has_imu = j["has_imu"].get<bool>();
        if (j.contains("has_gnss")) f.has_gnss = j["has_gnss"].get<bool>();
        if (j.contains("has_ptp")) f.has_ptp = j["has_ptp"].get<bool>();
        if (j.contains("has_pps")) f.has_pps = j["has_pps"].get<bool>();
        if (j.contains("has_hardware_sync")) f.has_hardware_sync = j["has_hardware_sync"].get<bool>();
        if (j.contains("return_modes")) f.return_modes = j["return_modes"].get<std::vector<std::string>>();
        if (j.contains("native_coordinate_system")) f.native_coordinate_system = j["native_coordinate_system"].get<std::string>();
        if (j.contains("point_cloud_fields")) f.point_cloud_fields = j["point_cloud_fields"].get<std::vector<std::string>>();
        if (j.contains("scan_rates_hz")) f.scan_rates_hz = j["scan_rates_hz"].get<std::vector<double>>();
        if (j.contains("factory_calibration_api")) f.factory_calibration_api = j["factory_calibration_api"].get<std::string>();
        if (j.contains("timestamp_source")) f.timestamp_source = j["timestamp_source"].get<std::string>();
        return f;
    }
};

/**
 * Firmware and SDK Compatibility Matrix boundaries.
 */
struct CompatibilityMatrix {
    std::string minimum_firmware;
    std::string maximum_firmware;
    std::vector<std::string> supported_sdk_versions;
    std::string preferred_sdk_version;
    std::string legacy_sdk_version;
    std::string protocol_version;
    bool legacy_adapter_required{false};

    nlohmann::json to_json() const {
        return {
            {"minimum_firmware", minimum_firmware},
            {"maximum_firmware", maximum_firmware},
            {"supported_sdk_versions", supported_sdk_versions},
            {"preferred_sdk_version", preferred_sdk_version},
            {"legacy_sdk_version", legacy_sdk_version},
            {"protocol_version", protocol_version},
            {"legacy_adapter_required", legacy_adapter_required}
        };
    }

    static CompatibilityMatrix from_json(const nlohmann::json& j) {
        CompatibilityMatrix c;
        if (j.contains("minimum_firmware")) c.minimum_firmware = j["minimum_firmware"].get<std::string>();
        if (j.contains("maximum_firmware")) c.maximum_firmware = j["maximum_firmware"].get<std::string>();
        if (j.contains("supported_sdk_versions")) c.supported_sdk_versions = j["supported_sdk_versions"].get<std::vector<std::string>>();
        if (j.contains("preferred_sdk_version")) c.preferred_sdk_version = j["preferred_sdk_version"].get<std::string>();
        if (j.contains("legacy_sdk_version")) c.legacy_sdk_version = j["legacy_sdk_version"].get<std::string>();
        if (j.contains("protocol_version")) c.protocol_version = j["protocol_version"].get<std::string>();
        if (j.contains("legacy_adapter_required")) c.legacy_adapter_required = j["legacy_adapter_required"].get<bool>();
        return c;
    }
};

/**
 * Vendor SDK specification details.
 */
struct VendorSdkInfo {
    std::string vendor_name;
    std::string family_name;
    std::string current_sdk;
    std::string current_driver;
    std::string current_ros_driver;
    std::string sdk_license;
    std::vector<std::string> supported_os;

    nlohmann::json to_json() const {
        return {
            {"vendor_name", vendor_name},
            {"family_name", family_name},
            {"current_sdk", current_sdk},
            {"current_driver", current_driver},
            {"current_ros_driver", current_ros_driver},
            {"sdk_license", sdk_license},
            {"supported_os", supported_os}
        };
    }

    static VendorSdkInfo from_json(const nlohmann::json& j) {
        VendorSdkInfo s;
        if (j.contains("vendor_name")) s.vendor_name = j["vendor_name"].get<std::string>();
        if (j.contains("family_name")) s.family_name = j["family_name"].get<std::string>();
        if (j.contains("current_sdk")) s.current_sdk = j["current_sdk"].get<std::string>();
        if (j.contains("current_driver")) s.current_driver = j["current_driver"].get<std::string>();
        if (j.contains("current_ros_driver")) s.current_ros_driver = j["current_ros_driver"].get<std::string>();
        if (j.contains("sdk_license")) s.sdk_license = j["sdk_license"].get<std::string>();
        if (j.contains("supported_os")) s.supported_os = j["supported_os"].get<std::vector<std::string>>();
        return s;
    }
};

/**
 * AV Scan mapping support.
 */
struct AvSupportInfo {
    std::string adapter_class;
    std::string backend;
    std::string slam_family;
    std::string tested_sdk_version;
    std::string tested_driver_version;
    std::string tested_firmware;
    ValidationTier validation_status{ValidationTier::DOCUMENTED};

    nlohmann::json to_json() const {
        return {
            {"adapter_class", adapter_class},
            {"backend", backend},
            {"slam_family", slam_family},
            {"tested_sdk_version", tested_sdk_version},
            {"tested_driver_version", tested_driver_version},
            {"tested_firmware", tested_firmware},
            {"validation_status", to_string(validation_status)}
        };
    }

    static AvSupportInfo from_json(const nlohmann::json& j) {
        AvSupportInfo a;
        if (j.contains("adapter_class")) a.adapter_class = j["adapter_class"].get<std::string>();
        if (j.contains("backend")) a.backend = j["backend"].get<std::string>();
        if (j.contains("slam_family")) a.slam_family = j["slam_family"].get<std::string>();
        if (j.contains("tested_sdk_version")) a.tested_sdk_version = j["tested_sdk_version"].get<std::string>();
        if (j.contains("tested_driver_version")) a.tested_driver_version = j["tested_driver_version"].get<std::string>();
        if (j.contains("tested_firmware")) a.tested_firmware = j["tested_firmware"].get<std::string>();
        if (j.contains("validation_status")) a.validation_status = validation_tier_from_string(j["validation_status"].get<std::string>());
        return a;
    }
};

/**
 * Detailed research provenance and official specification trail.
 */
struct ProvenanceRecord {
    std::string documentation_url;
    std::string manual_reference;
    std::string official_sdk_version;
    std::string protocol_specification;
    std::string last_verified_date{"2026-09"};

    nlohmann::json to_json() const {
        return {
            {"documentation_url", documentation_url},
            {"manual_reference", manual_reference},
            {"official_sdk_version", official_sdk_version},
            {"protocol_specification", protocol_specification},
            {"last_verified_date", last_verified_date}
        };
    }

    static ProvenanceRecord from_json(const nlohmann::json& j) {
        ProvenanceRecord p;
        if (j.contains("documentation_url")) p.documentation_url = j["documentation_url"].get<std::string>();
        if (j.contains("manual_reference")) p.manual_reference = j["manual_reference"].get<std::string>();
        if (j.contains("official_sdk_version")) p.official_sdk_version = j["official_sdk_version"].get<std::string>();
        if (j.contains("protocol_specification")) p.protocol_specification = j["protocol_specification"].get<std::string>();
        if (j.contains("last_verified_date")) p.last_verified_date = j["last_verified_date"].get<std::string>();
        return p;
    }
};

/**
 * Explicit 8-point runtime capability verification matrix (A-H).
 * Strictly distinguishes catalog knowledge from active runtime support.
 */
struct RuntimeIntegrationMatrix {
    bool catalog_knowledge_exists{true};
    std::string official_sdk_identified;
    bool sdk_integrated{false};
    bool hardware_interrogation_implemented{false};
    bool runtime_adapter_implemented{false};
    bool slam_backend_implemented{false};
    bool physically_tested{false};
    bool gui_validated{false};

    nlohmann::json to_json() const {
        return {
            {"catalog_knowledge_exists", catalog_knowledge_exists},
            {"official_sdk_identified", official_sdk_identified},
            {"sdk_integrated", sdk_integrated},
            {"hardware_interrogation_implemented", hardware_interrogation_implemented},
            {"runtime_adapter_implemented", runtime_adapter_implemented},
            {"slam_backend_implemented", slam_backend_implemented},
            {"physically_tested", physically_tested},
            {"gui_validated", gui_validated}
        };
    }

    static RuntimeIntegrationMatrix from_json(const nlohmann::json& j) {
        RuntimeIntegrationMatrix m;
        if (j.contains("catalog_knowledge_exists")) m.catalog_knowledge_exists = j["catalog_knowledge_exists"].get<bool>();
        if (j.contains("official_sdk_identified")) m.official_sdk_identified = j["official_sdk_identified"].get<std::string>();
        if (j.contains("sdk_integrated")) m.sdk_integrated = j["sdk_integrated"].get<bool>();
        if (j.contains("hardware_interrogation_implemented")) m.hardware_interrogation_implemented = j["hardware_interrogation_implemented"].get<bool>();
        if (j.contains("runtime_adapter_implemented")) m.runtime_adapter_implemented = j["runtime_adapter_implemented"].get<bool>();
        if (j.contains("slam_backend_implemented")) m.slam_backend_implemented = j["slam_backend_implemented"].get<bool>();
        if (j.contains("physically_tested")) m.physically_tested = j["physically_tested"].get<bool>();
        if (j.contains("gui_validated")) m.gui_validated = j["gui_validated"].get<bool>();
        return m;
    }
};

} // namespace av::core::schemas
