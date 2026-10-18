#include "core/fusion/multi_sensor_fusion_engine.hpp"
#include "core/logging/logger.hpp"

#include <chrono>
#include <cmath>
#include <unordered_map>
#include <algorithm>

#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/kdtree/kdtree_flann.h>

namespace av::core::fusion {

namespace {

struct VoxelCoord {
    int64_t ix;
    int64_t iy;
    int64_t iz;
    bool operator==(const VoxelCoord& o) const {
        return ix == o.ix && iy == o.iy && iz == o.iz;
    }
};

struct VoxelCoordHash {
    size_t operator()(const VoxelCoord& k) const {
        size_t h1 = std::hash<int64_t>{}(k.ix);
        size_t h2 = std::hash<int64_t>{}(k.iy);
        size_t h3 = std::hash<int64_t>{}(k.iz);
        return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2))
                  ^ (h3 * 0xc2b2ae3d27d4eb4fULL + (h2 << 6) + (h2 >> 2));
    }
};

struct VoxelAccumulator {
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_z = 0.0;
    double sum_weight = 0.0;

    double sum_r = 0.0;
    double sum_g = 0.0;
    double sum_b = 0.0;
    double rgb_weight = 0.0;
    bool has_color = false;

    double sum_intensity = 0.0;
    size_t count = 0;
};

void applyDiagnosticRoleColor(std::vector<schemas::PointXYZI>& cloud, const std::string& role) {
    uint8_t r = 200, g = 200, b = 200;
    if (role == "AIRY" || role.find("airy") != std::string::npos) {
        r = 0; g = 212; b = 255; // Cyan
    } else if (role == "GEMINI" || role.find("gemini") != std::string::npos) {
        r = 16; g = 185; b = 129; // Emerald Green
    } else if (role == "VITURE" || role.find("viture") != std::string::npos) {
        r = 168; g = 85; b = 247; // Purple
    } else if (role == "CAMERA" || role.find("camera") != std::string::npos) {
        r = 245; g = 158; b = 11; // Amber
    }
    for (auto& pt : cloud) {
        pt.r = r;
        pt.g = g;
        pt.b = b;
        pt.has_color = true;
    }
}

bool isValidTransform(const schemas::Transform3D& tf) {
    if (tf.matrix4x4.size() == 16) {
        bool non_zero = false;
        for (double v : tf.matrix4x4) {
            if (std::abs(v) > 1e-9) {
                non_zero = true;
                break;
            }
        }
        return non_zero;
    }
    double q_norm = tf.rotation.x * tf.rotation.x +
                    tf.rotation.y * tf.rotation.y +
                    tf.rotation.z * tf.rotation.z +
                    tf.rotation.w * tf.rotation.w;
    return q_norm > 1e-6;
}

} // namespace

double MultiSensorFusionEngine::computeNominalAccuracyScore(const std::string& sensor_id, bool is_metric_scale_verified) {
    SensorFusionInput in;
    in.sensor_id = sensor_id;
    in.sensor_role = sensor_id;
    in.is_metric_scale_verified = is_metric_scale_verified;
    return computeNominalAccuracyScore(in);
}

double MultiSensorFusionEngine::computeNominalAccuracyScore(const SensorFusionInput& input) {
    double base = input.base_accuracy_score;
    if (base <= 0.0) {
        std::string role = input.sensor_role;
        std::string id = input.sensor_id;
        if (role == "AIRY" || id.find("airy") != std::string::npos || id.find("lidar") != std::string::npos) {
            base = 100.0; // 3D Hemispherical LiDAR + Internal 6-Axis IMU (Metric LIO)
        } else if (role == "GEMINI" || id.find("gemini") != std::string::npos || id.find("rgbd") != std::string::npos) {
            base = 85.0;  // Active Stereo RGB-D + 6-Axis IMU (Metric VIO)
        } else if (role == "VITURE" || id.find("viture") != std::string::npos || id.find("vio") != std::string::npos) {
            base = 75.0;  // Stereo Fisheye + 1000Hz IMU (Metric XR VIO)
        } else if (role == "CAMERA" || id.find("camera") != std::string::npos || id.find("uvc") != std::string::npos) {
            base = 45.0;  // Monocular RGB VSLAM
        } else {
            base = 60.0;
        }
    }
    if (!input.is_metric_scale_verified) {
        base *= 0.45;
    }
    return base;
}

double MultiSensorFusionEngine::computeSensorAccuracyScore(const SensorFusionInput& input) {
    double score = computeNominalAccuracyScore(input);

    // Health and tracking state modifiers
    if (!input.is_tracking_healthy) {
        score *= 0.08; // Severe penalty on tracking loss / dropout so healthy secondary takes lead
    } else if (input.is_degraded) {
        score *= 0.40; // Degraded tracking drops LiDAR (100 -> 40) below healthy Stereo Camera (85)
    }

    // Live update rate modifier
    if (input.slam_rate_hz <= 0.05) {
        score *= 0.15;
    } else if (input.slam_rate_hz < 2.0) {
        score *= 0.65;
    }

    // Point cloud availability modifier
    if (input.cloud.empty()) {
        score *= 0.20;
    } else if (input.cloud.size() < 50) {
        score *= 0.70;
    }

    return score;
}

size_t MultiSensorFusionEngine::selectDynamicAnchorIndex(
    const std::vector<SensorFusionInput>& inputs,
    std::string* out_rationale,
    bool* out_failover_active) {
    if (inputs.empty()) {
        if (out_rationale) *out_rationale = "No sensors configured";
        if (out_failover_active) *out_failover_active = false;
        return 0;
    }

    size_t nominal_best_idx = 0;
    double nominal_best_score = -1.0;

    size_t live_best_idx = 0;
    double live_best_score = -1.0;

    for (size_t i = 0; i < inputs.size(); ++i) {
        double nom = computeNominalAccuracyScore(inputs[i]);
        if (nom > nominal_best_score) {
            nominal_best_score = nom;
            nominal_best_idx = i;
        }

        double live = computeSensorAccuracyScore(inputs[i]);
        if (live > live_best_score) {
            live_best_score = live;
            live_best_idx = i;
        }
    }

    // If all sensors have 0 live score (e.g. before first points arrive), pick healthiest or nominal best
    if (live_best_score <= 0.0) {
        for (size_t i = 0; i < inputs.size(); ++i) {
            if (inputs[i].is_tracking_healthy && !inputs[i].is_degraded) {
                double nom = computeNominalAccuracyScore(inputs[i]);
                if (nom > live_best_score) {
                    live_best_score = nom;
                    live_best_idx = i;
                }
            }
        }
        if (live_best_score <= 0.0) {
            live_best_idx = nominal_best_idx;
            live_best_score = nominal_best_score;
        }
    }

    bool failover = (live_best_idx != nominal_best_idx);
    if (out_failover_active) {
        *out_failover_active = failover;
    }

    if (out_rationale) {
        const auto& chosen = inputs[live_best_idx];
        std::string chosen_label = !chosen.sensor_role.empty() ? chosen.sensor_role : chosen.sensor_id;
        if (failover) {
            const auto& primary = inputs[nominal_best_idx];
            std::string primary_label = !primary.sensor_role.empty() ? primary.sensor_role : primary.sensor_id;
            std::string reason = !primary.is_tracking_healthy ? "tracking lost/offline"
                               : (primary.is_degraded ? "tracking degraded"
                               : (primary.cloud.empty() ? "no geometry stream" : "low accuracy score"));
            *out_rationale = "Dynamic Failover: " + chosen_label + " (" + chosen.sensor_id +
                             ", score " + std::to_string(static_cast<int>(std::round(live_best_score))) +
                             ") leading because " + primary_label + " is " + reason;
        } else {
            *out_rationale = "Primary Anchor: " + chosen_label + " (" + chosen.sensor_id +
                             ", score " + std::to_string(static_cast<int>(std::round(live_best_score))) + ")";
        }
    }

    return live_best_idx;
}

schemas::Transform3D MultiSensorFusionEngine::invertTransform3D(const schemas::Transform3D& tf) {
    double mat[4][4];
    transform3DToMatrix(tf, mat);

    Eigen::Matrix3d R;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            R(r, c) = mat[r][c];
        }
    }
    Eigen::Vector3d t(mat[0][3], mat[1][3], mat[2][3]);

    Eigen::Matrix3d R_inv = R.transpose();
    Eigen::Vector3d t_inv = -R_inv * t;

    double inv_mat[4][4] = {
        {R_inv(0, 0), R_inv(0, 1), R_inv(0, 2), t_inv.x()},
        {R_inv(1, 0), R_inv(1, 1), R_inv(1, 2), t_inv.y()},
        {R_inv(2, 0), R_inv(2, 1), R_inv(2, 2), t_inv.z()},
        {0.0,         0.0,         0.0,         1.0}
    };
    return ExtrinsicCalibrator::matrixToTransform3D(inv_mat);
}

void MultiSensorFusionEngine::transform3DToMatrix(const schemas::Transform3D& tf, double out_matrix[4][4]) {
    if (tf.matrix4x4.size() == 16) {
        bool non_zero = false;
        for (double v : tf.matrix4x4) {
            if (std::abs(v) > 1e-9) {
                non_zero = true;
                break;
            }
        }
        if (non_zero) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    out_matrix[r][c] = tf.matrix4x4[r * 4 + c];
                }
            }
            return;
        }
    }

    Eigen::Quaterniond q(tf.rotation.w, tf.rotation.x, tf.rotation.y, tf.rotation.z);
    if (q.norm() > 1e-6) {
        q.normalize();
    } else {
        q = Eigen::Quaterniond::Identity();
    }
    Eigen::Matrix3d R = q.toRotationMatrix();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            out_matrix[r][c] = R(r, c);
        }
    }
    out_matrix[0][3] = tf.translation.x;
    out_matrix[1][3] = tf.translation.y;
    out_matrix[2][3] = tf.translation.z;
    out_matrix[3][0] = 0.0;
    out_matrix[3][1] = 0.0;
    out_matrix[3][2] = 0.0;
    out_matrix[3][3] = 1.0;
}

std::vector<schemas::PointXYZI> MultiSensorFusionEngine::transformPointCloud(
    const std::vector<schemas::PointXYZI>& cloud,
    const schemas::Transform3D& tf) {
    double mat[4][4];
    transform3DToMatrix(tf, mat);
    return transformPointCloud(cloud, mat);
}

std::vector<schemas::PointXYZI> MultiSensorFusionEngine::transformPointCloud(
    const std::vector<schemas::PointXYZI>& cloud,
    const double matrix[4][4]) {
    std::vector<schemas::PointXYZI> out;
    out.reserve(cloud.size());

    const float r00 = static_cast<float>(matrix[0][0]);
    const float r01 = static_cast<float>(matrix[0][1]);
    const float r02 = static_cast<float>(matrix[0][2]);
    const float tx  = static_cast<float>(matrix[0][3]);

    const float r10 = static_cast<float>(matrix[1][0]);
    const float r11 = static_cast<float>(matrix[1][1]);
    const float r12 = static_cast<float>(matrix[1][2]);
    const float ty  = static_cast<float>(matrix[1][3]);

    const float r20 = static_cast<float>(matrix[2][0]);
    const float r21 = static_cast<float>(matrix[2][1]);
    const float r22 = static_cast<float>(matrix[2][2]);
    const float tz  = static_cast<float>(matrix[2][3]);

    for (const auto& pt : cloud) {
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) {
            continue;
        }
        schemas::PointXYZI tpt = pt;
        tpt.x = r00 * pt.x + r01 * pt.y + r02 * pt.z + tx;
        tpt.y = r10 * pt.x + r11 * pt.y + r12 * pt.z + ty;
        tpt.z = r20 * pt.x + r21 * pt.y + r22 * pt.z + tz;
        out.push_back(tpt);
    }
    return out;
}

std::vector<schemas::Pose3D> MultiSensorFusionEngine::transformTrajectory(
    const std::vector<schemas::Pose3D>& trajectory,
    const schemas::Transform3D& tf) {
    double mat[4][4];
    transform3DToMatrix(tf, mat);
    return transformTrajectory(trajectory, mat);
}

std::vector<schemas::Pose3D> MultiSensorFusionEngine::transformTrajectory(
    const std::vector<schemas::Pose3D>& trajectory,
    const double matrix[4][4]) {
    std::vector<schemas::Pose3D> out;
    out.reserve(trajectory.size());

    Eigen::Matrix4d T_ref_sec = Eigen::Matrix4d::Identity();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            T_ref_sec(r, c) = matrix[r][c];
        }
    }
    Eigen::Matrix3d R_ref_sec = T_ref_sec.block<3, 3>(0, 0);

    for (const auto& pose : trajectory) {
        schemas::Pose3D tp = pose;
        Eigen::Vector3d p_sec(pose.position.x, pose.position.y, pose.position.z);
        Eigen::Vector3d p_ref = R_ref_sec * p_sec + T_ref_sec.block<3, 1>(0, 3);
        tp.position.x = p_ref.x();
        tp.position.y = p_ref.y();
        tp.position.z = p_ref.z();

        Eigen::Quaterniond q_sec(pose.orientation.w, pose.orientation.x, pose.orientation.y, pose.orientation.z);
        if (q_sec.norm() > 1e-6) {
            q_sec.normalize();
            Eigen::Matrix3d R_pose = R_ref_sec * q_sec.toRotationMatrix();
            Eigen::Quaterniond q_ref(R_pose);
            q_ref.normalize();
            tp.orientation.w = q_ref.w();
            tp.orientation.x = q_ref.x();
            tp.orientation.y = q_ref.y();
            tp.orientation.z = q_ref.z();
        }
        out.push_back(tp);
    }
    return out;
}

MultiSensorFusionResult MultiSensorFusionEngine::fuseSensors(
    const std::vector<SensorFusionInput>& inputs,
    schemas::ScannerConfig& config,
    const MultiSensorFusionParams& params,
    const std::string& reference_sensor_id) {
    auto start_time = std::chrono::steady_clock::now();
    MultiSensorFusionResult result;

    for (const auto& in : inputs) {
        result.sensor_accuracy_scores[in.sensor_id] = computeSensorAccuracyScore(in);
    }

    if (!inputs.empty()) {
        size_t pre_dyn_idx = selectDynamicAnchorIndex(inputs, &result.anchor_rationale, &result.anchor_failover_active);
        const SensorFusionInput& pre_anchor = inputs[pre_dyn_idx];
        result.reference_sensor_id = pre_anchor.sensor_id;
        result.anchor_sensor_id = pre_anchor.sensor_id;
        result.anchor_sensor_role = pre_anchor.sensor_role;
        result.anchor_accuracy_score = computeSensorAccuracyScore(pre_anchor);
    }

    // Filter inputs that actually contain points or trajectory
    std::vector<const SensorFusionInput*> valid_inputs;
    for (const auto& in : inputs) {
        if (!in.cloud.empty() || !in.trajectory.empty()) {
            valid_inputs.push_back(&in);
        }
    }

    if (valid_inputs.empty()) {
        result.status_message = "No non-empty sensor inputs available for fusion.";
        return result;
    }

    // Evaluate dynamic accuracy-based anchor across all inputs
    size_t dyn_idx = selectDynamicAnchorIndex(inputs, &result.anchor_rationale, &result.anchor_failover_active);
    const SensorFusionInput* dyn_anchor = &inputs[dyn_idx];
    if (dyn_anchor->cloud.empty() && dyn_anchor->trajectory.empty()) {
        dyn_anchor = valid_inputs[0];
    }

    // Select reference sensor (dynamic by default, or explicit if healthy)
    const SensorFusionInput* ref_input = dyn_anchor;
    if (!reference_sensor_id.empty() && reference_sensor_id != "AUTO" && reference_sensor_id != "DYNAMIC") {
        const SensorFusionInput* explicit_ref = nullptr;
        for (const auto* in : valid_inputs) {
            if (in->sensor_id == reference_sensor_id || in->sensor_role == reference_sensor_id) {
                explicit_ref = in;
                break;
            }
        }
        if (explicit_ref != nullptr) {
            // Use explicit reference if it is healthy, or if dynamic anchor is the same sensor
            if (explicit_ref->is_tracking_healthy && !explicit_ref->is_degraded) {
                ref_input = explicit_ref;
            } else {
                // Explicit reference sensor is degraded or lost — dynamically fail over to strongest sensor
                ref_input = dyn_anchor;
            }
        }
    }

    result.reference_sensor_id = ref_input->sensor_id;
    result.anchor_sensor_id = ref_input->sensor_id;
    result.anchor_sensor_role = ref_input->sensor_role;
    result.anchor_accuracy_score = computeSensorAccuracyScore(*ref_input);

    // Identity transform for reference sensor
    double id_mat[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
    schemas::Transform3D id_tf = ExtrinsicCalibrator::matrixToTransform3D(id_mat);
    result.applied_transforms[ref_input->sensor_id] = id_tf;

    struct TransformedSensorData {
        const SensorFusionInput* source = nullptr;
        std::vector<schemas::PointXYZI> cloud;
        std::vector<schemas::Pose3D> trajectory;
    };
    std::vector<TransformedSensorData> aligned_sensors;
    aligned_sensors.reserve(valid_inputs.size());

    const auto& known_transforms = config.relative_transforms();

    for (const auto* in : valid_inputs) {
        TransformedSensorData tsd;
        tsd.source = in;
        result.per_sensor_point_counts[in->sensor_id] = in->cloud.size();
        result.total_input_points += in->cloud.size();

        if (in == ref_input) {
            tsd.cloud = in->cloud;
            tsd.trajectory = in->trajectory;
            if (params.color_by_sensor_role) {
                applyDiagnosticRoleColor(tsd.cloud, in->sensor_role);
            }
            aligned_sensors.push_back(std::move(tsd));
            continue;
        }

        // Resolve extrinsic transform T_{ref <- in} (supporting both direct and inverted transforms)
        schemas::Transform3D tf = id_tf;
        bool found_transform = false;

        std::string pair_key = ref_input->sensor_id + "<-" + in->sensor_id;
        std::string role_pair_key = ref_input->sensor_role + "<-" + in->sensor_role;
        std::string inv_pair_key = in->sensor_id + "<-" + ref_input->sensor_id;
        std::string inv_role_pair_key = in->sensor_role + "<-" + ref_input->sensor_role;

        auto it_pair = known_transforms.find(pair_key);
        auto it_role_pair = known_transforms.find(role_pair_key);
        auto it_inv_pair = known_transforms.find(inv_pair_key);
        auto it_inv_role_pair = known_transforms.find(inv_role_pair_key);
        auto it_sec = known_transforms.find(in->sensor_id);
        auto it_role = known_transforms.find(in->sensor_role);
        auto it_ref_as_sec = known_transforms.find(ref_input->sensor_id);
        auto it_ref_role_as_sec = known_transforms.find(ref_input->sensor_role);

        if (it_pair != known_transforms.end() && isValidTransform(it_pair->second)) {
            tf = it_pair->second;
            found_transform = true;
        } else if (it_role_pair != known_transforms.end() && isValidTransform(it_role_pair->second)) {
            tf = it_role_pair->second;
            found_transform = true;
        } else if (it_inv_pair != known_transforms.end() && isValidTransform(it_inv_pair->second)) {
            tf = invertTransform3D(it_inv_pair->second);
            found_transform = true;
        } else if (it_inv_role_pair != known_transforms.end() && isValidTransform(it_inv_role_pair->second)) {
            tf = invertTransform3D(it_inv_role_pair->second);
            found_transform = true;
        } else if (it_sec != known_transforms.end() && isValidTransform(it_sec->second)) {
            tf = it_sec->second;
            found_transform = true;
        } else if (it_role != known_transforms.end() && isValidTransform(it_role->second)) {
            tf = it_role->second;
            found_transform = true;
        } else if (it_ref_as_sec != known_transforms.end() && isValidTransform(it_ref_as_sec->second)) {
            tf = invertTransform3D(it_ref_as_sec->second);
            found_transform = true;
        } else if (it_ref_role_as_sec != known_transforms.end() && isValidTransform(it_ref_role_as_sec->second)) {
            tf = invertTransform3D(it_ref_role_as_sec->second);
            found_transform = true;
        }

        if (!found_transform && params.auto_calibrate_missing && !params.real_time_mode &&
            !ref_input->cloud.empty() && !in->cloud.empty()) {
            auto cal_res = ExtrinsicCalibrator::calibrateSensorPair(
                ref_input->sensor_id,
                in->sensor_id,
                ref_input->cloud,
                in->cloud,
                ref_input->trajectory,
                in->trajectory,
                params.calibration_params);

            result.calibration_results[pair_key] = cal_res;
            if (cal_res.converged) {
                tf = cal_res.transform;
                found_transform = true;
                ExtrinsicCalibrator::persistCalibrationToConfig(config, cal_res);
            }
        }

        result.applied_transforms[in->sensor_id] = tf;
        tsd.cloud = transformPointCloud(in->cloud, tf);
        tsd.trajectory = transformTrajectory(in->trajectory, tf);

        if (params.color_by_sensor_role) {
            applyDiagnosticRoleColor(tsd.cloud, in->sensor_role);
        }
        aligned_sensors.push_back(std::move(tsd));
    }

    // Optional: Project RGB texture from optical sensors onto non-optical (LiDAR) sensors
    if (!params.real_time_mode && params.transfer_optical_rgb_to_lidar && !params.color_by_sensor_role && params.rgb_transfer_radius_m > 0.0f) {
        pcl::PointCloud<pcl::PointXYZRGB>::Ptr optical_cloud(new pcl::PointCloud<pcl::PointXYZRGB>());
        for (const auto& tsd : aligned_sensors) {
            bool is_optical = tsd.source->is_optical_rgb_source ||
                              tsd.source->sensor_role == "GEMINI" ||
                              tsd.source->sensor_role == "CAMERA" ||
                              tsd.source->sensor_role == "VITURE";
            if (is_optical && !tsd.cloud.empty()) {
                for (const auto& pt : tsd.cloud) {
                    if (!pt.has_color && pt.r == 0 && pt.g == 0 && pt.b == 0) continue;
                    pcl::PointXYZRGB p;
                    p.x = pt.x;
                    p.y = pt.y;
                    p.z = pt.z;
                    p.r = pt.r;
                    p.g = pt.g;
                    p.b = pt.b;
                    optical_cloud->push_back(p);
                }
            }
        }

        if (!optical_cloud->empty()) {
            pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree;
            kdtree.setInputCloud(optical_cloud);
            const float radius = params.rgb_transfer_radius_m;

            for (auto& tsd : aligned_sensors) {
                bool is_optical = tsd.source->is_optical_rgb_source ||
                                  tsd.source->sensor_role == "GEMINI" ||
                                  tsd.source->sensor_role == "CAMERA" ||
                                  tsd.source->sensor_role == "VITURE";
                if (!is_optical) {
                    std::vector<int> indices(3);
                    std::vector<float> sq_dists(3);
                    for (auto& pt : tsd.cloud) {
                        pcl::PointXYZRGB search_pt;
                        search_pt.x = pt.x;
                        search_pt.y = pt.y;
                        search_pt.z = pt.z;
                        int found = kdtree.radiusSearch(search_pt, radius, indices, sq_dists, 3);
                        if (found > 0) {
                            double wr = 0.0, wg = 0.0, wb = 0.0, wsum = 0.0;
                            for (int k = 0; k < found; ++k) {
                                double w = 1.0 / (std::sqrt(static_cast<double>(sq_dists[k])) + 1e-4);
                                const auto& npt = optical_cloud->points[indices[k]];
                                wr += w * npt.r;
                                wg += w * npt.g;
                                wb += w * npt.b;
                                wsum += w;
                            }
                            if (wsum > 0.0) {
                                pt.r = static_cast<uint8_t>(std::clamp(std::round(wr / wsum), 0.0, 255.0));
                                pt.g = static_cast<uint8_t>(std::clamp(std::round(wg / wsum), 0.0, 255.0));
                                pt.b = static_cast<uint8_t>(std::clamp(std::round(wb / wsum), 0.0, 255.0));
                                pt.has_color = true;
                                result.colorized_lidar_points++;
                            }
                        }
                    }
                }
            }
        }
    }

    // Fuse trajectories
    if (!ref_input->trajectory.empty()) {
        result.fused_trajectory = ref_input->trajectory;
    } else {
        for (const auto& tsd : aligned_sensors) {
            if (!tsd.trajectory.empty()) {
                result.fused_trajectory = tsd.trajectory;
                break;
            }
        }
    }

    // Spatial Voxel Fusion or Direct Concatenation
    if (params.voxel_leaf_size_m <= 0.0f) {
        result.fused_cloud.reserve(result.total_input_points);
        for (const auto& tsd : aligned_sensors) {
            result.fused_cloud.insert(result.fused_cloud.end(), tsd.cloud.begin(), tsd.cloud.end());
        }
    } else {
        const double inv_leaf = 1.0 / static_cast<double>(params.voxel_leaf_size_m);
        std::unordered_map<VoxelCoord, VoxelAccumulator, VoxelCoordHash> grid;
        grid.reserve(result.total_input_points / 2 + 1);

        for (const auto& tsd : aligned_sensors) {
            bool is_optical = tsd.source->is_optical_rgb_source ||
                              tsd.source->sensor_role == "GEMINI" ||
                              tsd.source->sensor_role == "CAMERA" ||
                              tsd.source->sensor_role == "VITURE";
            double geom_w = std::max(0.1, static_cast<double>(tsd.source->geometry_weight));
            if (tsd.source->sensor_role == "AIRY") {
                geom_w *= 2.0; // LiDAR provides higher metric range accuracy
            }
            double color_w = (is_optical && !params.color_by_sensor_role) ? 3.0 : 1.0;

            for (const auto& pt : tsd.cloud) {
                VoxelCoord coord{
                    static_cast<int64_t>(std::floor(pt.x * inv_leaf)),
                    static_cast<int64_t>(std::floor(pt.y * inv_leaf)),
                    static_cast<int64_t>(std::floor(pt.z * inv_leaf))
                };
                auto& acc = grid[coord];
                acc.sum_x += geom_w * pt.x;
                acc.sum_y += geom_w * pt.y;
                acc.sum_z += geom_w * pt.z;
                acc.sum_weight += geom_w;

                if (pt.has_color) {
                    acc.sum_r += color_w * pt.r;
                    acc.sum_g += color_w * pt.g;
                    acc.sum_b += color_w * pt.b;
                    acc.rgb_weight += color_w;
                    acc.has_color = true;
                }

                acc.sum_intensity += pt.intensity;
                acc.count++;
            }
        }

        result.fused_cloud.reserve(grid.size());
        static const int64_t kNeighborOffsets[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
        };
        for (const auto& kv : grid) {
            const auto& acc = kv.second;
            if (acc.count == 0 || acc.sum_weight <= 0.0) continue;
            schemas::PointXYZI fpt;
            fpt.x = static_cast<float>(acc.sum_x / acc.sum_weight);
            fpt.y = static_cast<float>(acc.sum_y / acc.sum_weight);
            fpt.z = static_cast<float>(acc.sum_z / acc.sum_weight);
            if (acc.has_color && acc.rgb_weight > 0.0) {
                fpt.r = static_cast<uint8_t>(std::clamp(std::round(acc.sum_r / acc.rgb_weight), 0.0, 255.0));
                fpt.g = static_cast<uint8_t>(std::clamp(std::round(acc.sum_g / acc.rgb_weight), 0.0, 255.0));
                fpt.b = static_cast<uint8_t>(std::clamp(std::round(acc.sum_b / acc.rgb_weight), 0.0, 255.0));
                fpt.has_color = true;
                if (params.real_time_mode) {
                    result.colorized_lidar_points++;
                }
            } else if (params.real_time_mode && params.transfer_optical_rgb_to_lidar && !params.color_by_sensor_role) {
                // Fast O(1) 6-neighbor voxel RGB propagation in real-time mode
                double nr = 0.0, ng = 0.0, nb = 0.0, nw = 0.0;
                for (const auto& off : kNeighborOffsets) {
                    VoxelCoord nc{kv.first.ix + off[0], kv.first.iy + off[1], kv.first.iz + off[2]};
                    auto it_n = grid.find(nc);
                    if (it_n != grid.end() && it_n->second.has_color && it_n->second.rgb_weight > 0.0) {
                        nr += it_n->second.sum_r / it_n->second.rgb_weight;
                        ng += it_n->second.sum_g / it_n->second.rgb_weight;
                        nb += it_n->second.sum_b / it_n->second.rgb_weight;
                        nw += 1.0;
                    }
                }
                if (nw > 0.0) {
                    fpt.r = static_cast<uint8_t>(std::clamp(std::round(nr / nw), 0.0, 255.0));
                    fpt.g = static_cast<uint8_t>(std::clamp(std::round(ng / nw), 0.0, 255.0));
                    fpt.b = static_cast<uint8_t>(std::clamp(std::round(nb / nw), 0.0, 255.0));
                    fpt.has_color = true;
                    result.colorized_lidar_points++;
                } else {
                    fpt.has_color = false;
                }
            } else {
                fpt.has_color = false;
            }
            fpt.intensity = static_cast<float>(acc.sum_intensity / static_cast<double>(acc.count));
            result.fused_cloud.push_back(fpt);
        }
    }

    result.fused_output_points = result.fused_cloud.size();
    result.success = !result.fused_cloud.empty();

    auto end_time = std::chrono::steady_clock::now();
    result.computation_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1000.0;

    result.status_message = "Fused " + std::to_string(valid_inputs.size()) + " sensor stream(s): " +
                            std::to_string(result.total_input_points) + " -> " +
                            std::to_string(result.fused_output_points) + " pts (" +
                            std::to_string(result.colorized_lidar_points) + " RGB-projected) in " +
                            std::to_string(static_cast<int>(std::round(result.computation_time_ms))) + " ms";

    AV_LOG_INFO("MultiSensorFusionEngine", result.status_message);
    return result;
}

} // namespace av::core::fusion
