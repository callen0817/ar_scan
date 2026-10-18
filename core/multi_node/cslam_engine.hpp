#pragma once

#include "core/schemas/spatial_data.hpp"
#include "core/schemas/types.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <Eigen/Dense>

namespace av::core::multi_node {

enum class CSlamAlignmentState {
    ANCHOR,                        // Reference scanner (index 0) defining the global coordinate origin
    ANCHOR_REFERENCE = ANCHOR,     // Alias for ANCHOR
    UNALIGNED_IMU_OFFSET,          // No overlap yet: oriented by relative IMU & separated by arbitrary offset distance
    OVERLAP_LOCKED                 // Overlap detected: snapped together via 4-DoF/SE(3) registration into unified frame
};

inline std::string to_string(CSlamAlignmentState st) {
    switch (st) {
        case CSlamAlignmentState::ANCHOR: return "ANCHOR";
        case CSlamAlignmentState::UNALIGNED_IMU_OFFSET: return "UNALIGNED_IMU_OFFSET";
        case CSlamAlignmentState::OVERLAP_LOCKED: return "OVERLAP_LOCKED";
    }
    return "UNALIGNED_IMU_OFFSET";
}

inline CSlamAlignmentState cslam_alignment_state_from_string(const std::string& s) {
    if (s == "ANCHOR" || s == "ANCHOR_REFERENCE") return CSlamAlignmentState::ANCHOR;
    if (s == "OVERLAP_LOCKED") return CSlamAlignmentState::OVERLAP_LOCKED;
    return CSlamAlignmentState::UNALIGNED_IMU_OFFSET;
}

struct CSlamColor {
    uint8_t r{0};
    uint8_t g{210};
    uint8_t b{255};
    std::string hex{"#00D2FF"};
};

struct CSlamMatrix4x4 {
    double matrix[4][4]{
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
};

struct CSlamNodeSubmap {
    std::string node_uuid;
    std::string display_name;
    size_t scanner_index{0};
    CSlamColor distinct_color;
    uint8_t color_r{0};
    uint8_t color_g{210};
    uint8_t color_b{255};

    // Relative IMU orientation (gravity-aligned +Z Up attitude & IMU heading prior)
    schemas::Quaternion imu_orientation{0.0, 0.0, 0.0, 1.0};
    schemas::Pose3D current_local_pose;

    // Local point cloud submap and trajectory in the scanner's own frame
    std::vector<schemas::PointXYZI> raw_points;
    std::vector<schemas::Pose3D> trajectory;

    // Alignment state & transforms
    CSlamAlignmentState alignment_state{CSlamAlignmentState::UNALIGNED_IMU_OFFSET};
    schemas::Vector3D arbitrary_offset_m{0.0, 0.0, 0.0};
    schemas::Transform3D snapped_transform; // T_world_from_node when OVERLAP_LOCKED
    CSlamMatrix4x4 T_world_from_scanner;    // Effective 4x4 world transform matrix

    // Overlap registration quality metrics
    double overlap_ratio{0.0};
    double inlier_rmse_m{0.0};
    std::string matched_anchor_node_uuid;
    uint64_t last_update_ns{0};
};

using ScannerSubmapState = CSlamNodeSubmap;

class CSlamEngine {
public:
    CSlamEngine();
    ~CSlamEngine() = default;

    // Reset all C-SLAM state (e.g., when starting/leaving a session)
    void reset();
    void clear() { reset(); }

    // Configure separation distance (meters) for unaligned scanners & overlap snap thresholds
    void setSeparationDistanceM(double dist_m);
    double getSeparationDistanceM() const;

    void setOverlapSnapThreshold(double min_overlap_ratio, double max_rmse_m = 0.20);
    double getOverlapSnapThreshold() const { return overlap_snap_threshold_; }

    // Unlimited distinct scanner color generator (curated high-contrast + Golden-Angle HSV)
    static CSlamColor getScannerDistinctColor(size_t scanner_index);

    // Deterministic spatial separation offset for unaligned scanner k > 0
    static schemas::Vector3D computeSeparationOffset(size_t scanner_index, double base_spacing_m = 12.0);

    // Register or update a scanner node's metadata & IMU orientation
    size_t registerOrUpdateNode(const std::string& node_uuid,
                                const std::string& display_name,
                                const schemas::Quaternion& imu_orientation = {0.0, 0.0, 0.0, 1.0},
                                bool is_anchor = false);

    size_t registerScanner(const std::string& node_uuid,
                           const std::string& display_name,
                           bool is_anchor = false) {
        return registerOrUpdateNode(node_uuid, display_name, {0.0, 0.0, 0.0, 1.0}, is_anchor);
    }

    // Ingest local or remote point cloud submap for a scanner node
    // If replace_cloud is false, incrementally merges & voxel-deduplicates points
    void ingestSubmap(const std::string& node_uuid,
                      const std::string& display_name,
                      const std::vector<schemas::PointXYZI>& points,
                      const schemas::Quaternion& imu_orientation = {0.0, 0.0, 0.0, 1.0},
                      const schemas::Pose3D& local_pose = schemas::Pose3D(),
                      bool replace_cloud = true,
                      bool auto_evaluate_overlap = true,
                      const std::vector<schemas::Pose3D>& trajectory = {});

    void updateScannerSubmap(const std::string& node_uuid,
                             const std::string& display_name,
                             const std::vector<schemas::PointXYZI>& points,
                             const std::vector<schemas::Pose3D>& trajectory,
                             const schemas::Quaternion& imu_orientation = {0.0, 0.0, 0.0, 1.0},
                             bool auto_evaluate_overlap = true) {
        schemas::Pose3D latest_pose = trajectory.empty() ? schemas::Pose3D() : trajectory.back();
        ingestSubmap(node_uuid, display_name, points, imu_orientation, latest_pose, true, auto_evaluate_overlap, trajectory);
    }

    void updateScannerSubmap(const std::string& node_uuid,
                             const std::string& display_name,
                             const std::vector<schemas::PointXYZI>& points,
                             const schemas::Quaternion& imu_orientation = {0.0, 0.0, 0.0, 1.0},
                             const schemas::Pose3D& local_pose = schemas::Pose3D()) {
        ingestSubmap(node_uuid, display_name, points, imu_orientation, local_pose, true, true, {});
    }

    // Evaluate spatial overlap between all UNALIGNED_IMU_OFFSET scanners and the anchored map.
    // Any scanner exceeding the overlap threshold automatically snaps to OVERLAP_LOCKED.
    // Returns the number of newly snapped scanners.
    size_t evaluateAndSnapOverlaps();

    // Compute the effective 4x4 world transform for a node:
    // - ANCHOR: R(imu_orientation), t = (0,0,0)
    // - UNALIGNED_IMU_OFFSET: R(imu_orientation), t = arbitrary_offset_m
    // - OVERLAP_LOCKED: snapped_transform (T_world_from_node)
    static Eigen::Matrix4f getEffectiveWorldMatrix(const CSlamNodeSubmap& node);
    static void syncWorldTransformMatrix(CSlamNodeSubmap& node);

    // Generate the real-time merged point cloud across all participating scanners.
    // If differentiate_scanner_colors == true, each scanner's points are tinted with its unique CSlamColor.
    // Otherwise, native RGB / LiDAR reflectance is preserved.
    std::vector<schemas::PointXYZI> getMergedPointCloud(bool differentiate_scanner_colors = true) const;
    std::vector<schemas::PointXYZI> getScannerPointCloud(const std::string& node_uuid, bool differentiate_scanner_colors = true) const;
    std::vector<schemas::Pose3D> getMergedTrajectory() const;

    // Query scanner states (supports unlimited scanners)
    size_t getScannerCount() const;
    size_t scannerCount() const { return getScannerCount(); }
    size_t getSnappedScannerCount() const;
    size_t snappedScannerCount() const { return getSnappedScannerCount(); }
    std::vector<CSlamNodeSubmap> getAllNodeSubmaps() const;
    std::vector<CSlamNodeSubmap> getScannerNodes() const { return getAllNodeSubmaps(); }
    std::vector<CSlamNodeSubmap> getAllScannerStates() const { return getAllNodeSubmaps(); }
    bool getNodeSubmap(const std::string& node_uuid, CSlamNodeSubmap& out) const;

    // Network serialization for CoordinationService TCP streaming & alignment sync
    nlohmann::json serializeSubmapMessage(const std::string& node_uuid, size_t max_points = 8000) const;
    bool ingestSerializedSubmapMessage(const nlohmann::json& payload, bool auto_evaluate_overlap = true);

    nlohmann::json serializeAlignmentState() const;
    bool applySerializedAlignmentState(const nlohmann::json& payload);

private:
    mutable std::mutex mutex_;
    std::map<std::string, CSlamNodeSubmap> nodes_;
    std::vector<std::string> node_order_; // Preserves join order (index 0 = anchor)

    double separation_distance_m_{12.0};
    double overlap_snap_threshold_{0.28};
    double max_snap_rmse_m_{0.20};
    float submap_voxel_leaf_m_{0.04f};

    size_t evaluateAndSnapOverlapsUnlocked();
    bool tryAlignNodeToAnchoredCloud(CSlamNodeSubmap& unaligned_node,
                                     const std::vector<schemas::PointXYZI>& anchored_points,
                                     const std::string& primary_anchor_uuid);
};

} // namespace av::core::multi_node
