#include "core/schemas/registration_result.hpp"

namespace av {
namespace core {
namespace schemas {

nlohmann::json RegistrationResult::to_json() const {
    nlohmann::json mat_json = nlohmann::json::array();
    for (int r = 0; r < 4; ++r) {
        nlohmann::json row = nlohmann::json::array();
        for (int c = 0; c < 4; ++c) {
            row.push_back(transform_matrix[r][c]);
        }
        mat_json.push_back(row);
    }

    return nlohmann::json{
        {"registration_id", registration_id},
        {"project_id", project_id},
        {"session_id", session_id},
        {"reference_project_id", reference_project_id},
        {"reference_capture_id", reference_capture_id},
        {"reference_sensor_id", reference_sensor_id},
        {"reference_node_uuid", reference_node_uuid},
        {"reference_pcd_path", reference_pcd_path},
        {"moving_project_id", moving_project_id},
        {"moving_capture_id", moving_capture_id},
        {"moving_sensor_id", moving_sensor_id},
        {"moving_node_uuid", moving_node_uuid},
        {"moving_pcd_path", moving_pcd_path},
        {"scale_policy", scalePolicyToString(scale_policy)},
        {"scale_factor", scale_factor},
        {"transform_matrix", mat_json},
        {"overlap_ratio", overlap_ratio},
        {"inlier_rmse_m", inlier_rmse_m},
        {"fitness_score", fitness_score},
        {"inlier_count", inlier_count},
        {"total_points_evaluated", total_points_evaluated},
        {"computation_time_ms", computation_time_ms},
        {"method", method},
        {"status", status},
        {"created_at_ns", created_at_ns},
        {"output_combined_pcd_path", output_combined_pcd_path},
        {"output_combined_diff_pcd_path", output_combined_diff_pcd_path},
        {"output_moving_aligned_pcd_path", output_moving_aligned_pcd_path}
    };
}

RegistrationResult RegistrationResult::from_json(const nlohmann::json& j) {
    RegistrationResult r;
    r.registration_id = j.value("registration_id", "");
    r.project_id = j.value("project_id", "");
    r.session_id = j.value("session_id", "");
    r.reference_project_id = j.value("reference_project_id", r.project_id);
    r.reference_capture_id = j.value("reference_capture_id", "");
    r.reference_sensor_id = j.value("reference_sensor_id", "");
    r.reference_node_uuid = j.value("reference_node_uuid", "");
    r.reference_pcd_path = j.value("reference_pcd_path", "");
    r.moving_project_id = j.value("moving_project_id", r.project_id);
    r.moving_capture_id = j.value("moving_capture_id", "");
    r.moving_sensor_id = j.value("moving_sensor_id", "");
    r.moving_node_uuid = j.value("moving_node_uuid", "");
    r.moving_pcd_path = j.value("moving_pcd_path", "");
    r.scale_policy = scalePolicyFromString(j.value("scale_policy", "LOCKED_METRIC_SE3"));
    r.scale_factor = j.value("scale_factor", 1.0);

    if (j.contains("transform_matrix") && j["transform_matrix"].is_array()) {
        const auto& mat_json = j["transform_matrix"];
        for (size_t row = 0; row < 4 && row < mat_json.size(); ++row) {
            if (mat_json[row].is_array()) {
                for (size_t col = 0; col < 4 && col < mat_json[row].size(); ++col) {
                    r.transform_matrix[row][col] = mat_json[row][col].get<double>();
                }
            }
        }
    }

    r.overlap_ratio = j.value("overlap_ratio", 0.0);
    r.inlier_rmse_m = j.value("inlier_rmse_m", 0.0);
    r.fitness_score = j.value("fitness_score", 0.0);
    r.inlier_count = j.value("inlier_count", (int64_t)0);
    r.total_points_evaluated = j.value("total_points_evaluated", (int64_t)0);
    r.computation_time_ms = j.value("computation_time_ms", 0.0);
    r.method = j.value("method", "SAC_IA_FPFH_GICP_SE3");
    r.status = j.value("status", "CONVERGED");
    r.created_at_ns = j.value("created_at_ns", (int64_t)0);
    r.output_combined_pcd_path = j.value("output_combined_pcd_path", "");
    r.output_combined_diff_pcd_path = j.value("output_combined_diff_pcd_path", "");
    r.output_moving_aligned_pcd_path = j.value("output_moving_aligned_pcd_path", "");

    return r;
}

} // namespace schemas
} // namespace core
} // namespace av
