#include "capability_matcher.hpp"
#include <algorithm>

namespace av::core::profiling {

namespace {

bool hasCapability(const std::vector<std::string>& caps, const std::string& target) {
    for (const auto& c : caps) {
        if (c == target) return true;
        // Case-insensitive substring match for flexibility
        std::string lower_c = c;
        std::string lower_t = target;
        std::transform(lower_c.begin(), lower_c.end(), lower_c.begin(), ::tolower);
        std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
        if (lower_c.find(lower_t) != std::string::npos) return true;
    }
    return false;
}

bool hasStreamType(const std::vector<schemas::StreamInfo>& streams, const std::string& target_type) {
    for (const auto& s : streams) {
        std::string lower_s = s.type;
        std::string lower_t = target_type;
        std::transform(lower_s.begin(), lower_s.end(), lower_s.begin(), ::tolower);
        std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
        if (lower_s.find(lower_t) != std::string::npos) return true;
    }
    return false;
}

} // anonymous namespace

BackendMatchResult CapabilityMatcher::matchBackend(
    const std::vector<std::string>& capabilities,
    const std::vector<schemas::StreamInfo>& streams,
    const std::string& detected_driver
) {
    BackendMatchResult res;

    bool has_pointcloud = hasCapability(capabilities, "POINT_CLOUD") || hasStreamType(streams, "POINT_CLOUD");
    bool has_imu = hasCapability(capabilities, "IMU") || hasStreamType(streams, "IMU");
    bool has_depth = hasCapability(capabilities, "DEPTH") || hasStreamType(streams, "DEPTH");
    bool has_rgb = hasCapability(capabilities, "RGB") || hasStreamType(streams, "RGB");
    bool has_stereo = hasCapability(capabilities, "STEREO") || hasStreamType(streams, "STEREO");

    // Rule 1: LiDAR + IMU -> LiDAR-Inertial Odometry (LIO)
    if (has_pointcloud && has_imu) {
        res.slam_family = ReconstructionFamily::LIDAR_INERTIAL_ODOMETRY;
        res.slam_family_name = "LIO";
        res.recommended_slam_backend = "FAST-LIO2";
        res.recommended_geometry_backend = "FAST-LIO2";
        res.required_streams = {"POINT_CLOUD", "IMU"};
        res.matched_capabilities = {"POINT_CLOUD_3D", "INTERNAL_IMU_6DOF"};
        res.confidence = 1.0;
        res.rationale = "High-rate 3D point cloud stream accompanied by 6-DoF inertial measurements matches LiDAR-Inertial Odometry state estimation.";
        res.recommended_slam_parameters = {
            {"max_iterations", 4},
            {"point_filter_num", 2},
            {"fov_degree", 360.0},
            {"cube_side_length", 1000.0},
            {"blind_zone_radius", 0.2},
            {"extrinsic_est_en", false}
        };
        return res;
    }

    // Rule 2: Active Depth + RGB -> RGB-D Visual Odometry
    if (has_depth && has_rgb) {
        res.slam_family = ReconstructionFamily::RGBD_ODOMETRY_SLAM;
        res.slam_family_name = "RGBD";
        res.recommended_slam_backend = "RTAB-Map";
        res.recommended_geometry_backend = "RGBD_DEPTH_RECONSTRUCTION";
        res.required_streams = {"DEPTH", "RGB"};
        res.matched_capabilities = {"ACTIVE_STEREO_DEPTH", "RGB_CAMERA"};
        res.confidence = 1.0;
        res.rationale = "Synchronized RGB and structured depth streams enable direct RGB-D visual odometry with loop closure.";
        res.recommended_slam_parameters = {
            {"dof_mode", "6DOF"},
            {"confidence_threshold", 0.65},
            {"max_depth_m", 4.0}
        };
        return res;
    }

    // Rule 3: Monocular RGB (without depth or pointcloud) -> Monocular Neural Streaming
    if (has_rgb && !has_depth && !has_pointcloud) {
        res.slam_family = ReconstructionFamily::MONOCULAR_NEURAL_RECONSTRUCTION;
        res.slam_family_name = "MONOCULAR_NEURAL_RECONSTRUCTION";
        res.recommended_slam_backend = "LingBotStreamingBackend";
        res.recommended_geometry_backend = "LINGBOT_STREAMING_RECONSTRUCTION";
        res.required_streams = {"RGB"};
        res.matched_capabilities = {"RGB_CAMERA"};
        res.confidence = 0.95;
        res.rationale = "Single monocular RGB stream matches foundation model neural spatial reconstruction with native pose encoding.";
        res.recommended_slam_parameters = {
            {"dof_mode", "6DOF"},
            {"geometry_rate_hz", 1.25},
            {"rgb_nominal_hz", 30.0}
        };
        return res;
    }

    // Rule 4: Stereo Tracking + IMU -> Visual-Inertial Odometry (VIO)
    if (has_stereo && has_imu) {
        res.slam_family = ReconstructionFamily::VISUAL_INERTIAL_ODOMETRY;
        res.slam_family_name = "VIO";
        res.recommended_slam_backend = "CarinaDeviceProvider";
        res.recommended_geometry_backend = "EXPERIMENTAL_METRIC_STEREO";
        res.required_streams = {"STEREO", "IMU"};
        res.matched_capabilities = {"STEREO_TRACKING", "HEAD_TRACKING_IMU"};
        res.confidence = 0.90;
        res.rationale = "Synchronized stereo tracking optics and high-rate IMU match onboard visual-inertial state estimation.";
        res.recommended_slam_parameters = {
            {"dof_mode", "6DOF"},
            {"carina_vio_nominal_hz", 52.0},
            {"imu_nominal_hz", 775.0}
        };
        return res;
    }

    // Default Fallback
    res.slam_family = ReconstructionFamily::NONE;
    res.slam_family_name = "UNKNOWN";
    res.confidence = 0.0;
    res.rationale = "No recognized capability combination matched known reconstruction backends.";
    return res;
}

} // namespace av::core::profiling
