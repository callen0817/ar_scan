#pragma once

#include <string>
#include <vector>
#include <array>
#include <nlohmann/json.hpp>

namespace av {
namespace core {
namespace schemas {

enum class ScalePolicy {
    LOCKED_METRIC_SE3 = 0,   // Rigid SE(3): Rotation + Translation, Scale strictly locked at 1.0000
    ESTIMATED_SIM3 = 1       // Sim(3): Rotation + Translation + Estimated Scale (Monocular / Scale-Ambiguous)
};

inline std::string scalePolicyToString(ScalePolicy policy) {
    switch (policy) {
        case ScalePolicy::LOCKED_METRIC_SE3: return "LOCKED_METRIC_SE3";
        case ScalePolicy::ESTIMATED_SIM3: return "ESTIMATED_SIM3";
        default: return "LOCKED_METRIC_SE3";
    }
}

inline ScalePolicy scalePolicyFromString(const std::string& str) {
    if (str == "ESTIMATED_SIM3") return ScalePolicy::ESTIMATED_SIM3;
    return ScalePolicy::LOCKED_METRIC_SE3;
}

struct RegistrationResult {
    std::string registration_id;
    std::string project_id;
    std::string session_id;
    std::string reference_project_id;
    std::string reference_capture_id;
    std::string reference_sensor_id;
    std::string reference_node_uuid;
    std::string reference_pcd_path;
    std::string moving_project_id;
    std::string moving_capture_id;
    std::string moving_sensor_id;
    std::string moving_node_uuid;
    std::string moving_pcd_path;
    ScalePolicy scale_policy = ScalePolicy::LOCKED_METRIC_SE3;
    double scale_factor = 1.0;
    std::array<std::array<double, 4>, 4> transform_matrix = {{{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
    double overlap_ratio = 0.0;     // 0.0 to 1.0
    double inlier_rmse_m = 0.0;     // Inlier root-mean-square error in meters
    double fitness_score = 0.0;     // Convergence fitness score
    int64_t inlier_count = 0;
    int64_t total_points_evaluated = 0;
    double computation_time_ms = 0.0;
    std::string method = "SAC_IA_FPFH_GICP_SE3";
    std::string status = "CONVERGED"; // CONVERGED, FAILED, ACCEPTED, REJECTED
    int64_t created_at_ns = 0;
    std::string output_combined_pcd_path;
    std::string output_combined_diff_pcd_path;
    std::string output_moving_aligned_pcd_path;

    nlohmann::json to_json() const;
    static RegistrationResult from_json(const nlohmann::json& j);
};

} // namespace schemas

using ScalePolicy = schemas::ScalePolicy;
using RegistrationResult = schemas::RegistrationResult;

} // namespace core
} // namespace av
