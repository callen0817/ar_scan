#pragma once

#include "core/schemas/registration_result.hpp"
#include <string>
#include <memory>
#include <Eigen/Dense>

namespace av {
namespace core {
namespace registration {

struct RegistrationParams {
    float coarse_voxel_size = 0.25f;    // 25 cm for fast global SAC-IA
    float fine_voxel_size = 0.08f;      // 8 cm for high-fidelity GICP refinement
    float normal_radius = 0.40f;        // 40 cm search radius for normal estimation
    float fpfh_radius = 0.60f;          // 60 cm search radius for FPFH descriptor
    int sac_ia_iterations = 500;        // Maximum SAC-IA iterations
    float sac_ia_min_sample_dist = 0.25f;// 25 cm minimum sample distance
    float sac_ia_max_corr = 0.80f;      // 80 cm max correspondence distance
    int gicp_iterations = 80;           // GICP maximum iterations
    float gicp_max_corr = 0.35f;        // 35 cm max correspondence distance
    float inlier_threshold_m = 0.15f;   // 15 cm threshold for inlier evaluation
    ScalePolicy scale_policy = ScalePolicy::LOCKED_METRIC_SE3;
    bool enable_color_differentiation = true; // Tint Ref Cyan, Mov Amber
};

class RegistrationEngine {
public:
    RegistrationEngine() = default;
    ~RegistrationEngine() = default;

    /**
     * @brief Performs automated coarse-to-fine registration between two PCD files.
     * @param ref_pcd_path Path to reference (anchor) PCD file.
     * @param mov_pcd_path Path to moving PCD file to be transformed into reference frame.
     * @param params Configuration parameters (voxel sizes, search radii, scale policy).
     * @return RegistrationResult containing transformation matrix T_Ref<-Mov, inlier RMSE, overlap ratio, etc.
     */
    RegistrationResult registerClouds(
        const std::string& ref_pcd_path,
        const std::string& mov_pcd_path,
        const RegistrationParams& params = RegistrationParams()
    );

    /**
     * @brief Transforms moving cloud and writes combined true RGB PCD to disk.
     * output_combined_pcd_path: Preserves real sensor RGB and neutral reflectance.
     * Diagnostic differentiation (Cyan/Amber) is handled in-memory only during visual inspection.
     */
    bool generateColoredMergedPCD(
        const std::string& ref_pcd_path,
        const std::string& mov_pcd_path,
        const Eigen::Matrix4f& T_ref_from_mov,
        const std::string& output_combined_pcd_path,
        const std::string& output_moving_aligned_pcd_path,
        bool differentiate_colors = false,
        const std::string& output_combined_diff_pcd_path = ""
    );
};

} // namespace registration
} // namespace core
} // namespace av
