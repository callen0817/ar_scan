#pragma once

#include <string>
#include <vector>
#include <map>
#include "core/schemas/spatial_data.hpp"
#include "core/schemas/types.hpp"
#include "core/schemas/scanner_config.hpp"
#include "core/fusion/extrinsic_calibrator.hpp"

namespace av::core::fusion {

struct SensorFusionInput {
    std::string sensor_id;
    std::string sensor_role; // e.g., "AIRY", "GEMINI", "VITURE", "CAMERA"
    std::vector<schemas::PointXYZI> cloud;
    std::vector<schemas::Pose3D> trajectory;
    bool is_optical_rgb_source = false;
    float geometry_weight = 1.0f;

    // Live accuracy & health metrics for dynamic anchor selection
    bool is_tracking_healthy = true;      // True when TRACKING_OK; false when LOST / DISCONNECTED / failed
    bool is_degraded = false;             // True when tracking is DEGRADED (low features / degeneracy)
    bool is_metric_scale_verified = true; // True for LiDAR / stereo RGB-D / VIO; false for unverified monocular
    double slam_rate_hz = 10.0;           // Live SLAM / odometry update rate
    double base_accuracy_score = 0.0;     // <= 0 auto-assigns by sensor modality (AIRY=100, GEMINI=85, VITURE=75, CAMERA=45)
};

struct MultiSensorFusionParams {
    float voxel_leaf_size_m = 0.01f;           // 1cm voxel fusion grid (<= 0 disables voxel merging)
    bool transfer_optical_rgb_to_lidar = true; // Project RGB from Gemini/Camera onto uncolored LiDAR points
    float rgb_transfer_radius_m = 0.05f;       // 5cm search radius for optical RGB projection
    bool color_by_sensor_role = false;         // Diagnostic mode: tint points by sensor role
    bool auto_calibrate_missing = true;        // Automatically run M7 ExtrinsicCalibrator if transform missing
    bool real_time_mode = false;               // Fast real-time streaming fusion mode for live viewport overlap
    ExtrinsicCalibrationParams calibration_params;
};

struct MultiSensorFusionResult {
    bool success = false;
    std::string status_message;
    std::string reference_sensor_id;

    // Dynamic anchor telemetry
    std::string anchor_sensor_id;
    std::string anchor_sensor_role;
    double anchor_accuracy_score = 0.0;
    bool anchor_failover_active = false;
    std::string anchor_rationale;
    std::map<std::string, double> sensor_accuracy_scores;

    std::vector<schemas::PointXYZI> fused_cloud;
    std::vector<schemas::Pose3D> fused_trajectory;

    size_t total_input_points = 0;
    size_t fused_output_points = 0;
    size_t colorized_lidar_points = 0;
    double computation_time_ms = 0.0;

    std::map<std::string, size_t> per_sensor_point_counts;
    std::map<std::string, schemas::Transform3D> applied_transforms;
    std::map<std::string, ExtrinsicCalibrationResult> calibration_results;
};

/**
 * @brief Milestone 8: Modular Real-Time & Offline Multi-Sensor Spatial Fusion Engine.
 *
 * Transforms secondary sensor point clouds and 6-DoF trajectories into the primary
 * reference sensor's Canonical AV coordinate frame using calibrated rigid SE(3)
 * extrinsics from ScannerConfig (or on-the-fly M7 ExtrinsicCalibrator), projects
 * optical RGB texture onto metric LiDAR geometry, and performs weighted voxel fusion.
 * Dynamically selects the sensor with the strongest live data accuracy as the anchor
 * and automatically fails over if the primary sensor degrades or drops out.
 */
class MultiSensorFusionEngine {
public:
    MultiSensorFusionEngine() = default;
    ~MultiSensorFusionEngine() = default;

    /**
     * @brief Compute nominal base accuracy score for a sensor role/ID (before live health modifiers).
     */
    static double computeNominalAccuracyScore(const SensorFusionInput& input);
    static double computeNominalAccuracyScore(const std::string& sensor_id, bool is_metric_scale_verified = true);

    /**
     * @brief Compute live effective accuracy score factoring in modality, scale verification,
     *        tracking health, degradation, SLAM rate, and point cloud density.
     */
    static double computeSensorAccuracyScore(const SensorFusionInput& input);

    /**
     * @brief Dynamically select the strongest sensor input as the anchor.
     * @return Index into `inputs` of the selected anchor sensor (or 0 if empty).
     */
    static size_t selectDynamicAnchorIndex(
        const std::vector<SensorFusionInput>& inputs,
        std::string* out_rationale = nullptr,
        bool* out_failover_active = nullptr);

    /**
     * @brief Invert a rigid SE(3) Transform3D: T_{A<-B}^{-1} = T_{B<-A}.
     */
    static schemas::Transform3D invertTransform3D(const schemas::Transform3D& tf);

    /**
     * @brief Extract a 4x4 row-major matrix from a schemas::Transform3D
     *        (using matrix4x4 if populated, or translation + quaternion rotation).
     */
    static void transform3DToMatrix(const schemas::Transform3D& tf, double out_matrix[4][4]);

    /**
     * @brief Transform a single point cloud using a schemas::Transform3D or 4x4 matrix.
     */
    static std::vector<schemas::PointXYZI> transformPointCloud(
        const std::vector<schemas::PointXYZI>& cloud,
        const schemas::Transform3D& tf);

    static std::vector<schemas::PointXYZI> transformPointCloud(
        const std::vector<schemas::PointXYZI>& cloud,
        const double matrix[4][4]);

    /**
     * @brief Transform a 6-DoF trajectory using a 4x4 row-major rigid matrix:
     *        T_ref_world = T_ref_sec * T_sec_world
     */
    static std::vector<schemas::Pose3D> transformTrajectory(
        const std::vector<schemas::Pose3D>& trajectory,
        const schemas::Transform3D& tf);

    static std::vector<schemas::Pose3D> transformTrajectory(
        const std::vector<schemas::Pose3D>& trajectory,
        const double matrix[4][4]);

    /**
     * @brief Fuse multiple sensor inputs into the dynamically selected anchor (or specified reference) sensor's frame.
     */
    MultiSensorFusionResult fuseSensors(
        const std::vector<SensorFusionInput>& inputs,
        schemas::ScannerConfig& config,
        const MultiSensorFusionParams& params = MultiSensorFusionParams(),
        const std::string& reference_sensor_id = "");
};

} // namespace av::core::fusion
