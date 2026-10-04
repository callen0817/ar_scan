#include "compatibility_resolver.hpp"
#include "core/logging/logger.hpp"
#include <fstream>
#include <sstream>

namespace av::core::ecosystem {

CompatibilityResolver& CompatibilityResolver::instance() {
    static CompatibilityResolver res;
    return res;
}

CompatibilityResolver::CompatibilityResolver()
    : registry_(VendorEcosystemRegistry::instance()) {}

static int parseFirmwareMajor(const std::string& fw) {
    if (fw.empty()) return 0;
    try {
        size_t dot = fw.find('.');
        if (dot != std::string::npos) {
            return std::stoi(fw.substr(0, dot));
        }
        return std::stoi(fw);
    } catch (...) {
        return 0;
    }
}

bool CompatibilityResolver::resolveFirmwareCompatibility(
    const schemas::DeviceModelDefinition& model,
    const std::string& firmware_version,
    std::string& out_adapter,
    std::string& out_sdk_version,
    bool& out_requires_legacy
) {
    const auto& compat = model.compatibility();
    const auto& av_sup = model.av_support();

    out_adapter = av_sup.adapter_class;
    out_sdk_version = compat.preferred_sdk_version;
    out_requires_legacy = compat.legacy_adapter_required;

    if (firmware_version.empty()) {
        return true; // Default to preferred if not interrogatable
    }

    int fw_major = parseFirmwareMajor(firmware_version);
    int min_major = parseFirmwareMajor(compat.minimum_firmware);

    // Ouster Rev8/Rev7/Gen1 rule:
    // If hardware is Ouster and fw < 3.0 (e.g. 2.x), route to OusterLegacyAdapter without forcing upgrade!
    if (model.vendor() == "Ouster") {
        if (fw_major >= 4 && model.hardware_revision() == "Rev8") {
            out_adapter = "OusterAdapter";
            out_sdk_version = "0.16.2";
            out_requires_legacy = false;
            return true;
        } else if (fw_major == 3 || (fw_major < 4 && model.hardware_revision() == "Rev7")) {
            out_adapter = "OusterAdapter";
            out_sdk_version = "0.15.2";
            out_requires_legacy = false;
            return true;
        } else if (fw_major <= 2 || model.production_status() == schemas::ProductionStatus::LEGACY) {
            out_adapter = "OusterLegacyAdapter";
            out_sdk_version = "0.14.2";
            out_requires_legacy = true;
            return true;
        }
    }

    if (fw_major < min_major) {
        if (!compat.legacy_sdk_version.empty()) {
            out_adapter = av_sup.adapter_class + "Legacy";
            out_sdk_version = compat.legacy_sdk_version;
            out_requires_legacy = true;
            return true;
        }
        return false;
    }

    return true;
}

CompatibilityResolutionResult CompatibilityResolver::resolveHardware(
    const HardwareInterrogationData& hw,
    const std::filesystem::path& repo_root
) {
    CompatibilityResolutionResult res;

    // =========================================================================
    // TIER 1: Exact AV-validated profile on disk?
    // =========================================================================
    if (!repo_root.empty()) {
        std::vector<std::filesystem::path> search_dirs = {
            repo_root / "profiles" / "validated",
            repo_root / "profiles" / "experimental"
        };

        for (const auto& dir : search_dirs) {
            if (!std::filesystem::exists(dir)) continue;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.path().extension() == ".json") {
                    try {
                        std::ifstream ifs(entry.path());
                        nlohmann::json j = nlohmann::json::parse(ifs);
                        std::string p_hw_id = j.value("physical_device_id", "");
                        std::string p_name = j.value("name", "");
                        
                        // Check exact physical identity match
                        bool match = false;
                        if (!hw.serial_number.empty() && p_hw_id.find(hw.serial_number) != std::string::npos) {
                            match = true;
                        } else if (!hw.vendor_id.empty() && !hw.product_id.empty() &&
                                   p_hw_id.find(hw.vendor_id) != std::string::npos &&
                                   p_hw_id.find(hw.product_id) != std::string::npos) {
                            match = true;
                        }

                        if (match) {
                            res.resolution_tier = schemas::ResolutionTier::TIER1_EXACT_VALIDATED_PROFILE;
                            res.resolved_model_id = entry.path().stem().string();
                            res.vendor = j.value("manufacturer", hw.vendor_name);
                            res.model = p_name;
                            res.slam_family = j.value("slam_family", "UNKNOWN");
                            res.recommended_backend = j.value("slam_backend", "NONE");
                            res.is_capturable = (res.recommended_backend != "NONE" && !res.recommended_backend.empty());
                            res.validation_tier = schemas::ValidationTier::AV_VALIDATED;
                            res.status_summary = "Tier 1: Matched exact disk profile (" + res.resolved_model_id + ")";
                            res.generated_profile = synthesizeProfile(hw, res);
                            return res;
                        }
                    } catch (...) {}
                }
            }
        }
    }

    // =========================================================================
    // TIER 2: Known model definition in vendor ecosystem registry?
    // =========================================================================
    const schemas::DeviceModelDefinition* model_def = registry_.findModelByHardware(
        hw.vendor_id,
        hw.product_id,
        hw.vendor_name,
        hw.product_name,
        hw.primary_endpoint
    );

    if (model_def) {
        res.resolution_tier = schemas::ResolutionTier::TIER2_KNOWN_MODEL_DEFINITION;
        res.resolved_model_id = model_def->model_id();
        res.vendor = model_def->vendor();
        res.model = model_def->model();
        res.hardware_revision = model_def->hardware_revision();
        res.production_status = model_def->production_status();
        res.validation_tier = model_def->av_support().validation_status;
        res.native_rgb = model_def->features().has_rgb;

        // Resolve firmware compatibility
        std::string adapter, sdk_ver;
        bool requires_legacy = false;
        bool fw_ok = resolveFirmwareCompatibility(*model_def, hw.firmware_version, adapter, sdk_ver, requires_legacy);
        
        res.selected_adapter = adapter;
        res.selected_sdk_version = sdk_ver;
        res.requires_legacy_adapter = requires_legacy;
        res.firmware_supported = fw_ok;
        res.slam_family = model_def->av_support().slam_family;
        res.recommended_backend = model_def->av_support().backend;
        res.is_capturable = (res.recommended_backend != "NONE" && !res.recommended_backend.empty());
        
        if (requires_legacy) {
            res.status_summary = "Tier 2: Known Model (" + model_def->model() + ") [Legacy Hardware/SDK Adapter Active]";
        } else {
            res.status_summary = "Tier 2: Known Model (" + model_def->model() + " " + res.hardware_revision + ")";
        }

        res.generated_profile = synthesizeProfile(hw, res);
        return res;
    }

    // =========================================================================
    // TIER 3: Recognized vendor SDK (known vendor but uncataloged model revision)
    if (!hw.vendor_name.empty()) {
        auto vendor_models = registry_.getModelsByVendor(hw.vendor_name);
        if (!vendor_models.empty()) {
            const auto& base = vendor_models.front();
        res.resolution_tier = schemas::ResolutionTier::TIER3_RECOGNIZED_VENDOR_SDK;
        res.resolved_model_id = base.vendor() + "_interrogated_sdk_sensor";
        res.vendor = base.vendor();
        res.model = hw.product_name.empty() ? (base.vendor() + " SDK Sensor") : hw.product_name;
        res.hardware_revision = "Interrogated";
        res.production_status = schemas::ProductionStatus::CURRENT;
        res.selected_adapter = base.av_support().adapter_class;
        res.selected_sdk_version = base.vendor_sdk().current_sdk;
        res.slam_family = base.av_support().slam_family;
        res.recommended_backend = base.av_support().backend;
        res.is_capturable = (res.recommended_backend != "NONE" && !res.recommended_backend.empty());
        res.validation_tier = schemas::ValidationTier::SDK_INTERROGATED;
        res.status_summary = "Tier 3: Recognized Vendor SDK (" + base.vendor() + ") -> Interrogating hardware";
        res.generated_profile = synthesizeProfile(hw, res);
        return res;
        }
    }

    // =========================================================================
    // TIER 4: Recognized standard transport (UVC video, CDC-ACM serial, etc.)
    // =========================================================================
    if (hw.transport_type == "USB_UVC" && hw.has_verified_rgb) {
        res.resolution_tier = schemas::ResolutionTier::TIER4_RECOGNIZED_STANDARD_TRANSPORT;
        res.resolved_model_id = "standard_uvc_camera";
        res.vendor = hw.vendor_name.empty() ? "Generic" : hw.vendor_name;
        res.model = hw.product_name.empty() ? "UVC Video Camera" : hw.product_name;
        res.hardware_revision = "Standard";
        res.selected_adapter = "GenericUvcSensorAdapter";
        res.selected_sdk_version = "V4L2";
        res.slam_family = "MONOCULAR_NEURAL_RECONSTRUCTION";
        res.recommended_backend = "LingBotStreamingBackend";
        res.is_capturable = true;
        res.validation_tier = schemas::ValidationTier::SDK_SUPPORTED;
        res.status_summary = "Tier 4: Standard UVC Video Stream -> Monocular Neural SLAM";
        res.generated_profile = synthesizeProfile(hw, res);
        return res;
    }

    // =========================================================================
    // TIER 5 & TIER 6: Generic M14 Profiler & Capability Interlock
    // =========================================================================
    // If hardware lacks verified video, depth, or lidar (e.g. Seeed SenseCAP Watcher)
    if (!hw.has_verified_rgb && !hw.has_verified_depth && !hw.has_verified_lidar) {
        res.resolution_tier = schemas::ResolutionTier::TIER6_UNSUPPORTED_SPATIAL_CAPTURE;
        res.resolved_model_id = "unknown_unsupported_hardware";
        res.vendor = hw.vendor_name.empty() ? "Unknown" : hw.vendor_name;
        res.model = hw.product_name.empty() ? "Unknown Physical Device" : hw.product_name;
        res.hardware_revision = "Unknown";
        res.selected_adapter = "NONE";
        res.selected_sdk_version = "NONE";
        res.slam_family = "UNKNOWN";
        res.recommended_backend = "NONE";
        res.is_capturable = false;
        res.validation_tier = schemas::ValidationTier::DOCUMENTED;
        res.status_summary = "Tier 6: Unsupported Device (No compatible spatial capture backend established)";
        res.generated_profile = synthesizeProfile(hw, res);
        return res;
    }

    // Fallback: Tier 5 Generic Profiler
    res.resolution_tier = schemas::ResolutionTier::TIER5_GENERIC_PROFILER;
    res.resolved_model_id = "generic_profiled_sensor";
    res.vendor = hw.vendor_name;
    res.model = hw.product_name;
    res.selected_adapter = "NONE";
    res.slam_family = "UNKNOWN";
    res.recommended_backend = "NONE";
    res.is_capturable = false;
    res.validation_tier = schemas::ValidationTier::DOCUMENTED;
    res.status_summary = "Tier 5: Generic Profiler Interrogated";
    res.generated_profile = synthesizeProfile(hw, res);
    return res;
}

schemas::DeviceProfile CompatibilityResolver::synthesizeProfile(
    const HardwareInterrogationData& hw,
    const CompatibilityResolutionResult& res
) {
    schemas::DeviceProfile p;
    std::string serial = hw.serial_number.empty() ? "0000" : hw.serial_number;
    std::string pid = res.resolved_model_id + "_" + serial;
    
    p.set_profile_id(pid);
    p.set_model(res.model.empty() ? "Discovered Sensor" : res.model);
    p.set_manufacturer(res.vendor.empty() ? "Unknown" : res.vendor);
    p.set_device_family(res.slam_family);
    p.set_hardware_revision(res.hardware_revision);
    p.set_slam_family(res.slam_family);
    p.set_slam_backend(res.recommended_backend);
    p.set_driver(res.selected_adapter);
    p.set_sdk(res.selected_sdk_version);
    p.set_profile_status(res.validation_tier == schemas::ValidationTier::AV_VALIDATED ? 
                         schemas::ProfileStatus::VALIDATED : schemas::ProfileStatus::EXPERIMENTAL);

    nlohmann::json contract = nlohmann::json::object();
    contract["status"] = res.is_capturable ? "READY_FOR_CAPTURE" : "NOT_CAPTURABLE";
    contract["resolution_tier"] = to_string(res.resolution_tier);
    contract["requires_legacy_adapter"] = res.requires_legacy_adapter;
    contract["native_rgb"] = res.native_rgb;

    p.set_output_contract(contract);
    return p;
}

} // namespace av::core::ecosystem
