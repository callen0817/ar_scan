#pragma once

#include "core/schemas/spatial_data.hpp"
#include "core/schemas/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace av::core::processing {

enum class ExportFormat {
    PLY_ASCII,
    PLY_BINARY,
    PCD_ASCII,
    PCD_BINARY,
    XYZ_ASCII,
    OBJ_MESH
};

struct GroundPlaneResult {
    bool found{false};
    double plane_coeffs[4]{0.0, 0.0, 1.0, 0.0}; // ax + by + cz + d = 0, unit normal with c > 0
    size_t inlier_count{0};
    double inlier_ratio{0.0};
    double tilt_deg{0.0};
    double floor_height_m{0.0};
    double alignment_transform[4][4]{
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
};

struct CleanupParams {
    bool enable_voxel_downsample{false};
    float voxel_leaf_size_m{0.02f};

    bool enable_sor{true};
    int sor_mean_k{20};
    double sor_std_mul{2.0};

    bool enable_ror{false};
    double ror_radius_m{0.10};
    int ror_min_neighbors{4};

    bool enable_ground_alignment{false};
    double ground_dist_threshold_m{0.04};
    int ground_max_iterations{400};
    double ground_max_tilt_deg{35.0};
    bool align_ground_to_z_zero{true};
    bool remove_ground_plane{false};
};

struct ProcessingStats {
    size_t input_point_count{0};
    size_t output_point_count{0};
    size_t voxel_removed_count{0};
    size_t sor_removed_count{0};
    size_t ror_removed_count{0};
    size_t ground_removed_count{0};
    double computation_time_ms{0.0};
    schemas::Vector3D bbox_min{0.0, 0.0, 0.0};
    schemas::Vector3D bbox_max{0.0, 0.0, 0.0};
    GroundPlaneResult ground_plane;
};

struct ExportResult {
    bool success{false};
    std::string file_path;
    std::string format_name;
    size_t exported_points{0};
    size_t exported_triangles{0};
    uint64_t file_size_bytes{0};
    double export_time_ms{0.0};
    std::string error_message;
};

class PointCloudProcessor {
public:
    PointCloudProcessor() = default;
    ~PointCloudProcessor() = default;

    // Load and save PointXYZI preserving RGB and intensity
    static bool loadPcd(const std::string& filepath, std::vector<schemas::PointXYZI>& out_points);
    static bool savePcd(const std::string& filepath, const std::vector<schemas::PointXYZI>& points, bool binary = false);

    // Individual filters preserving true RGB and intensity
    static std::vector<schemas::PointXYZI> voxelDownsample(
        const std::vector<schemas::PointXYZI>& input,
        float leaf_size_m);

    static std::vector<schemas::PointXYZI> removeStatisticalOutliers(
        const std::vector<schemas::PointXYZI>& input,
        int mean_k = 20,
        double std_dev_mul_thresh = 2.0);

    static std::vector<schemas::PointXYZI> removeRadiusOutliers(
        const std::vector<schemas::PointXYZI>& input,
        double radius_m = 0.10,
        int min_neighbors = 4);

    static GroundPlaneResult detectAndAlignGroundPlane(
        std::vector<schemas::PointXYZI>& in_out_points,
        double distance_threshold_m = 0.04,
        int max_iterations = 400,
        double max_tilt_deg = 35.0,
        bool align_to_z_zero = true,
        bool remove_ground_inliers = false);

    // Combined pipeline
    static ProcessingStats runCleanupPipeline(
        const std::vector<schemas::PointXYZI>& input,
        std::vector<schemas::PointXYZI>& output,
        const CleanupParams& params);

    // Real-time O(N) spatial-hash M12 cleanup pipeline (< 3 ms for live 10Hz telemetry)
    static ProcessingStats cleanRealtimeFast(
        const std::vector<schemas::PointXYZI>& input,
        std::vector<schemas::PointXYZI>& output,
        const CleanupParams& params);

    // Compute bounding box
    static void computeBoundingBox(
        const std::vector<schemas::PointXYZI>& points,
        schemas::Vector3D& out_min,
        schemas::Vector3D& out_max);

    // Multi-format export (.ply, .pcd, .xyz, .obj surface mesh)
    static ExportResult exportPointCloud(
        const std::vector<schemas::PointXYZI>& points,
        const std::string& output_path,
        ExportFormat format,
        float mesh_grid_resolution_m = 0.04f);

    static ExportFormat parseExportFormat(const std::string& format_str);
    static std::string exportFormatExtension(ExportFormat format);
    static std::string exportFormatName(ExportFormat format);
};

} // namespace av::core::processing
