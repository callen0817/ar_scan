#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/device_profile.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace av::core::profiling {

enum class ReconstructionFamily {
    LIDAR_INERTIAL_ODOMETRY,           // LIO (e.g. FAST-LIO2 / FAST-LIVO2)
    RGBD_ODOMETRY_SLAM,                // RGB-D SLAM (e.g. RTAB-Map)
    MONOCULAR_NEURAL_RECONSTRUCTION,   // Monocular RGB (LingBot-Map Foundation Model)
    VISUAL_INERTIAL_ODOMETRY,          // VIO (e.g. Carina VIO)
    NONE
};

struct BackendMatchResult {
    ReconstructionFamily slam_family{ReconstructionFamily::NONE};
    std::string slam_family_name;
    std::string recommended_slam_backend;
    std::string recommended_geometry_backend;
    std::vector<std::string> required_streams;
    std::vector<std::string> matched_capabilities;
    nlohmann::json recommended_slam_parameters;
    double confidence{1.0};
    std::string rationale;
};

class CapabilityMatcher {
public:
    /**
     * Deduces compatible SLAM/Reconstruction family and backend based strictly
     * on sensor capabilities and available stream modalities.
     * 
     * Does NOT match based on hardcoded device models or names.
     */
    static BackendMatchResult matchBackend(
        const std::vector<std::string>& capabilities,
        const std::vector<schemas::StreamInfo>& streams,
        const std::string& detected_driver = ""
    );
};

} // namespace av::core::profiling
