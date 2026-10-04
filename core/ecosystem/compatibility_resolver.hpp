#pragma once

#include "core/schemas/ecosystem_types.hpp"
#include "core/schemas/model_definition.hpp"
#include "core/schemas/device_profile.hpp"
#include "vendor_ecosystem.hpp"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace av::core::ecosystem {

struct HardwareInterrogationData {
    std::string vendor_id;
    std::string product_id;
    std::string vendor_name;
    std::string product_name;
    std::string firmware_version;
    std::string serial_number;
    std::string transport_type; // "USB_UVC", "USB_SERIAL", "ETHERNET", etc.
    std::string primary_endpoint;
    std::vector<std::string> observed_endpoints;
    bool has_verified_rgb{false};
    bool has_verified_depth{false};
    bool has_verified_lidar{false};
    bool has_verified_imu{false};
};

struct CompatibilityResolutionResult {
    schemas::ResolutionTier resolution_tier{schemas::ResolutionTier::TIER5_GENERIC_PROFILER};
    std::string resolved_model_id;
    std::string vendor;
    std::string model;
    std::string hardware_revision;
    schemas::ProductionStatus production_status{schemas::ProductionStatus::CURRENT};
    
    // SDK and Adapter resolution
    std::string selected_adapter;       // e.g. "OusterAdapter", "OusterLegacyAdapter", "GenericUvcSensorAdapter", "NONE"
    std::string selected_sdk_version;
    bool requires_legacy_adapter{false};
    bool firmware_supported{true};
    
    // SLAM & Reconstruction matching
    std::string slam_family;            // "LIO", "RGBD", "MONOCULAR_NEURAL_RECONSTRUCTION", "UNKNOWN"
    std::string recommended_backend;    // "FAST-LIO2", "RTAB-Map", "LingBotStreamingBackend", "NONE"
    bool is_capturable{false};
    bool native_rgb{false};             // Distinction: Rev8 RGB vs Rev7
    
    schemas::ValidationTier validation_tier{schemas::ValidationTier::DOCUMENTED};
    std::string status_summary;
    
    schemas::DeviceProfile generated_profile;
};

class CompatibilityResolver {
public:
    static CompatibilityResolver& instance();

    CompatibilityResolver();

    /**
     * Executes the strict 6-tier lookup hierarchy:
     * 1. Exact AV-validated profile on disk?
     * 2. Known model definition in ecosystem knowledge base?
     * 3. Recognized vendor SDK?
     * 4. Recognized standard transport (UVC, CDC-ACM, Ethernet)?
     * 5. Generic M14 profiler?
     * 6. Capability verification & backend match (or safe unsupported fallback).
     */
    CompatibilityResolutionResult resolveHardware(
        const HardwareInterrogationData& hw,
        const std::filesystem::path& repo_root = ""
    );

    /**
     * Resolves firmware version compatibility against a model definition,
     * determining whether current or legacy SDK/adapter should be activated.
     */
    bool resolveFirmwareCompatibility(
        const schemas::DeviceModelDefinition& model,
        const std::string& firmware_version,
        std::string& out_adapter,
        std::string& out_sdk_version,
        bool& out_requires_legacy
    );

    /**
     * Synthesizes a truthful DeviceProfile compliant with schemas based on the resolution result.
     */
    schemas::DeviceProfile synthesizeProfile(
        const HardwareInterrogationData& hw,
        const CompatibilityResolutionResult& res
    );

private:
    VendorEcosystemRegistry& registry_;
};

} // namespace av::core::ecosystem
