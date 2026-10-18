#pragma once

#include "core/schemas/scanner_config.hpp"
#include "core/schemas/spatial_data.hpp"
#include "core/schemas/types.hpp"
#include "core/interfaces/istorage_engine.hpp"

#include <string>
#include <vector>

namespace av::core::fusion {

struct ExtrinsicCalibrationParams {
    double coarse_voxel_m{0.25};
    double fine_voxel_m{0.08};
    double normal_radius_m{0.40};
    double fpfh_radius_m{0.60};
    double max_correspondence_dist_m{0.35};
    double inlier_threshold_m{0.15};
    int max_gicp_iterations{80};
    bool use_trajectory_prior{true};
    bool enforce_rigid_se3{true};
};

struct ExtrinsicCalibrationResult {
    bool converged{false};
    schemas::ExtrinsicStatus status{schemas::ExtrinsicStatus::UNKNOWN};
    std::string reference_sensor_id;
    std::string secondary_sensor_id;
    std::string pair_key; // "<reference_sensor_id><-<secondary_sensor_id>"
    std::string method{"TRAJECTORY_HAND_EYE_GICP_SE3"};

    schemas::Transform3D transform; // translation, quaternion rotation, flattened 16-element matrix4x4
    double roll_deg{0.0};
    double pitch_deg{0.0};
    double yaw_deg{0.0};
    double scale_factor{1.0};

    bool used_trajectory_prior{false};
    size_t matched_trajectory_poses{0};
    double trajectory_rmse_m{0.0};

    double fitness_score{0.0};
    double inlier_rmse_m{999.0};
    double overlap_ratio{0.0};
    size_t inlier_count{0};
    size_t evaluated_points{0};
    double confidence{0.0};
    double computation_time_ms{0.0};
};

class ExtrinsicCalibrator {
public:
    ExtrinsicCalibrator() = default;
    ~ExtrinsicCalibrator() = default;

    // Stage 1: Trajectory Umeyama / Hand-Eye Pose Alignment
    static bool alignTrajectoriesUmeyama(
        const std::vector<schemas::Pose3D>& ref_traj,
        const std::vector<schemas::Pose3D>& sec_traj,
        schemas::Transform3D& out_transform,
        size_t& out_matched_poses,
        double& out_traj_rmse_m);

    // Stage 2: Full Trajectory + Environmental Cloud-to-Cloud Extrinsic Calibration
    static ExtrinsicCalibrationResult calibrateSensorPair(
        const std::string& ref_sensor_id,
        const std::string& sec_sensor_id,
        const std::vector<schemas::PointXYZI>& ref_cloud,
        const std::vector<schemas::PointXYZI>& sec_cloud,
        const std::vector<schemas::Pose3D>& ref_traj = {},
        const std::vector<schemas::Pose3D>& sec_traj = {},
        const ExtrinsicCalibrationParams& params = ExtrinsicCalibrationParams{});

    // Persist computed extrinsic calibration into a ScannerConfig and optionally save via IStorageEngine
    static bool persistCalibrationToConfig(
        schemas::ScannerConfig& config,
        const ExtrinsicCalibrationResult& result,
        const std::string& supporting_capture_id = "",
        interfaces::IStorageEngine* storage = nullptr);

    // Helper: Convert 4x4 row-major matrix to Transform3D and Euler angles (deg)
    static schemas::Transform3D matrixToTransform3D(
        const double mat[4][4],
        double* out_roll_deg = nullptr,
        double* out_pitch_deg = nullptr,
        double* out_yaw_deg = nullptr);
};

} // namespace av::core::fusion
