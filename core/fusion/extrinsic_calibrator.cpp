#include "core/fusion/extrinsic_calibrator.hpp"
#include "core/logging/logger.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/fpfh_omp.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/registration/gicp.h>
#include <pcl/common/transforms.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace av::core::fusion {

schemas::Transform3D ExtrinsicCalibrator::matrixToTransform3D(
    const double mat[4][4],
    double* out_roll_deg,
    double* out_pitch_deg,
    double* out_yaw_deg) {
    schemas::Transform3D tf;
    tf.translation.x = mat[0][3];
    tf.translation.y = mat[1][3];
    tf.translation.z = mat[2][3];

    Eigen::Matrix3d R;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            R(r, c) = mat[r][c];
        }
    }

    // Orthonormalize R for quaternion extraction
    Eigen::JacobiSVD<Eigen::Matrix3d> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3d R_ortho = svd.matrixU() * svd.matrixV().transpose();
    if (R_ortho.determinant() < 0.0) {
        Eigen::Matrix3d V = svd.matrixV();
        V.col(2) *= -1.0;
        R_ortho = svd.matrixU() * V.transpose();
    }

    Eigen::Quaterniond q(R_ortho);
    q.normalize();
    tf.rotation.x = q.x();
    tf.rotation.y = q.y();
    tf.rotation.z = q.z();
    tf.rotation.w = q.w();

    tf.matrix4x4.resize(16);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            tf.matrix4x4[r * 4 + c] = mat[r][c];
        }
    }

    if (out_roll_deg || out_pitch_deg || out_yaw_deg) {
        double sy = std::sqrt(R_ortho(0, 0) * R_ortho(0, 0) + R_ortho(1, 0) * R_ortho(1, 0));
        bool singular = sy < 1e-6;
        double roll, pitch, yaw;
        if (!singular) {
            roll = std::atan2(R_ortho(2, 1), R_ortho(2, 2));
            pitch = std::atan2(-R_ortho(2, 0), sy);
            yaw = std::atan2(R_ortho(1, 0), R_ortho(0, 0));
        } else {
            roll = std::atan2(-R_ortho(1, 2), R_ortho(1, 1));
            pitch = std::atan2(-R_ortho(2, 0), sy);
            yaw = 0.0;
        }
        if (out_roll_deg) *out_roll_deg = roll * 180.0 / M_PI;
        if (out_pitch_deg) *out_pitch_deg = pitch * 180.0 / M_PI;
        if (out_yaw_deg) *out_yaw_deg = yaw * 180.0 / M_PI;
    }

    return tf;
}

bool ExtrinsicCalibrator::alignTrajectoriesUmeyama(
    const std::vector<schemas::Pose3D>& ref_traj,
    const std::vector<schemas::Pose3D>& sec_traj,
    schemas::Transform3D& out_transform,
    size_t& out_matched_poses,
    double& out_traj_rmse_m) {
    out_matched_poses = 0;
    out_traj_rmse_m = 999.0;

    if (ref_traj.size() < 3 || sec_traj.size() < 3) {
        return false;
    }

    std::vector<Eigen::Vector3d> ref_pts;
    std::vector<Eigen::Vector3d> sec_pts;
    std::vector<Eigen::Quaterniond> ref_rots;
    std::vector<Eigen::Quaterniond> sec_rots;

    // Try timestamp association first (within 150ms tolerance)
    const uint64_t max_dt_ns = 150000000ULL;
    for (const auto& sp : sec_traj) {
        if (sp.timestamp_ns == 0) continue;
        uint64_t best_dt = UINT64_MAX;
        const schemas::Pose3D* best_rp = nullptr;
        for (const auto& rp : ref_traj) {
            if (rp.timestamp_ns == 0) continue;
            uint64_t dt = (rp.timestamp_ns > sp.timestamp_ns)
                              ? (rp.timestamp_ns - sp.timestamp_ns)
                              : (sp.timestamp_ns - rp.timestamp_ns);
            if (dt < best_dt) {
                best_dt = dt;
                best_rp = &rp;
            }
        }
        if (best_rp && best_dt <= max_dt_ns) {
            ref_pts.emplace_back(best_rp->position.x, best_rp->position.y, best_rp->position.z);
            sec_pts.emplace_back(sp.position.x, sp.position.y, sp.position.z);
            ref_rots.emplace_back(best_rp->orientation.w, best_rp->orientation.x, best_rp->orientation.y, best_rp->orientation.z);
            sec_rots.emplace_back(sp.orientation.w, sp.orientation.x, sp.orientation.y, sp.orientation.z);
        }
    }

    // Fallback to normalized index correspondence when trajectories come from unsynchronized epochs
    if (ref_pts.size() < 3) {
        ref_pts.clear();
        sec_pts.clear();
        ref_rots.clear();
        sec_rots.clear();
        size_t n_samples = std::min(ref_traj.size(), sec_traj.size());
        for (size_t i = 0; i < n_samples; ++i) {
            size_t r_idx = (i * (ref_traj.size() - 1)) / std::max<size_t>(1, n_samples - 1);
            size_t s_idx = (i * (sec_traj.size() - 1)) / std::max<size_t>(1, n_samples - 1);
            const auto& rp = ref_traj[r_idx];
            const auto& sp = sec_traj[s_idx];
            ref_pts.emplace_back(rp.position.x, rp.position.y, rp.position.z);
            sec_pts.emplace_back(sp.position.x, sp.position.y, sp.position.z);
            ref_rots.emplace_back(rp.orientation.w, rp.orientation.x, rp.orientation.y, rp.orientation.z);
            sec_rots.emplace_back(sp.orientation.w, sp.orientation.x, sp.orientation.y, sp.orientation.z);
        }
    }

    const size_t N = ref_pts.size();
    if (N < 3) return false;

    out_matched_poses = N;

    // Compute centroids
    Eigen::Vector3d mu_ref = Eigen::Vector3d::Zero();
    Eigen::Vector3d mu_sec = Eigen::Vector3d::Zero();
    for (size_t i = 0; i < N; ++i) {
        mu_ref += ref_pts[i];
        mu_sec += sec_pts[i];
    }
    mu_ref /= static_cast<double>(N);
    mu_sec /= static_cast<double>(N);

    // Check trajectory spatial excitation
    double var_sec = 0.0;
    Eigen::Matrix3d H = Eigen::Matrix3d::Zero();
    for (size_t i = 0; i < N; ++i) {
        Eigen::Vector3d d_ref = ref_pts[i] - mu_ref;
        Eigen::Vector3d d_sec = sec_pts[i] - mu_sec;
        var_sec += d_sec.squaredNorm();
        H += d_sec * d_ref.transpose();
    }

    Eigen::Matrix3d R = Eigen::Matrix3d::Identity();
    if (var_sec > 1e-4) {
        Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Matrix3d U = svd.matrixU();
        Eigen::Matrix3d V = svd.matrixV();
        R = V * U.transpose();
        if (R.determinant() < 0.0) {
            V.col(2) *= -1.0;
            R = V * U.transpose();
        }
    } else {
        // Fallback to rotational hand-eye averaging if translational excitation is tiny
        Eigen::Matrix3d R_accum = Eigen::Matrix3d::Zero();
        for (size_t i = 0; i < N; ++i) {
            Eigen::Matrix3d R_r = ref_rots[i].normalized().toRotationMatrix();
            Eigen::Matrix3d R_s = sec_rots[i].normalized().toRotationMatrix();
            R_accum += R_r * R_s.transpose();
        }
        Eigen::JacobiSVD<Eigen::Matrix3d> svd(R_accum, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Matrix3d U = svd.matrixU();
        Eigen::Matrix3d V = svd.matrixV();
        R = U * V.transpose();
        if (R.determinant() < 0.0) {
            U.col(2) *= -1.0;
            R = U * V.transpose();
        }
    }

    Eigen::Vector3d t = mu_ref - R * mu_sec;

    double sq_err = 0.0;
    for (size_t i = 0; i < N; ++i) {
        Eigen::Vector3d pred = R * sec_pts[i] + t;
        sq_err += (pred - ref_pts[i]).squaredNorm();
    }
    out_traj_rmse_m = std::sqrt(sq_err / static_cast<double>(N));

    double mat[4][4] = {
        {R(0, 0), R(0, 1), R(0, 2), t.x()},
        {R(1, 0), R(1, 1), R(1, 2), t.y()},
        {R(2, 0), R(2, 1), R(2, 2), t.z()},
        {0.0,     0.0,     0.0,     1.0}
    };
    out_transform = matrixToTransform3D(mat);
    return true;
}

ExtrinsicCalibrationResult ExtrinsicCalibrator::calibrateSensorPair(
    const std::string& ref_sensor_id,
    const std::string& sec_sensor_id,
    const std::vector<schemas::PointXYZI>& ref_cloud,
    const std::vector<schemas::PointXYZI>& sec_cloud,
    const std::vector<schemas::Pose3D>& ref_traj,
    const std::vector<schemas::Pose3D>& sec_traj,
    const ExtrinsicCalibrationParams& params) {
    auto start_time = std::chrono::steady_clock::now();
    ExtrinsicCalibrationResult result;
    result.reference_sensor_id = ref_sensor_id;
    result.secondary_sensor_id = sec_sensor_id;
    result.pair_key = ref_sensor_id + "<-" + sec_sensor_id;

    if (ref_cloud.size() < 20 || sec_cloud.size() < 20) {
        AV_LOG_ERROR("ExtrinsicCalibrator", "InsufficientPoints",
                     "Cannot calibrate sensor pair with <20 points: Ref=" + std::to_string(ref_cloud.size()) +
                     ", Sec=" + std::to_string(sec_cloud.size()));
        return result;
    }

    // Convert to PCL XYZ clouds
    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_raw(new pcl::PointCloud<pcl::PointXYZ>);
    ref_raw->points.reserve(ref_cloud.size());
    for (const auto& pt : ref_cloud) {
        if (std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z)) {
            pcl::PointXYZ p;
            p.x = pt.x; p.y = pt.y; p.z = pt.z;
            ref_raw->points.push_back(p);
        }
    }
    ref_raw->width = static_cast<uint32_t>(ref_raw->points.size());
    ref_raw->height = 1;

    pcl::PointCloud<pcl::PointXYZ>::Ptr sec_raw(new pcl::PointCloud<pcl::PointXYZ>);
    sec_raw->points.reserve(sec_cloud.size());
    for (const auto& pt : sec_cloud) {
        if (std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z)) {
            pcl::PointXYZ p;
            p.x = pt.x; p.y = pt.y; p.z = pt.z;
            sec_raw->points.push_back(p);
        }
    }
    sec_raw->width = static_cast<uint32_t>(sec_raw->points.size());
    sec_raw->height = 1;

    // 1. Coarse & Fine Voxel Downsampling
    pcl::VoxelGrid<pcl::PointXYZ> vox_coarse;
    vox_coarse.setLeafSize(params.coarse_voxel_m, params.coarse_voxel_m, params.coarse_voxel_m);
    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(ref_raw);
    vox_coarse.filter(*ref_coarse);
    if (ref_coarse->size() < 25) ref_coarse = ref_raw;

    pcl::PointCloud<pcl::PointXYZ>::Ptr sec_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(sec_raw);
    vox_coarse.filter(*sec_coarse);
    if (sec_coarse->size() < 25) sec_coarse = sec_raw;

    pcl::VoxelGrid<pcl::PointXYZ> vox_fine;
    vox_fine.setLeafSize(params.fine_voxel_m, params.fine_voxel_m, params.fine_voxel_m);
    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(ref_raw);
    vox_fine.filter(*ref_fine);
    if (ref_fine->size() < 30) ref_fine = ref_raw;

    pcl::PointCloud<pcl::PointXYZ>::Ptr sec_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(sec_raw);
    vox_fine.filter(*sec_fine);
    if (sec_fine->size() < 30) sec_fine = sec_raw;

    // Helper to evaluate mean nearest-neighbor distance of candidate transform on coarse clouds
    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree_coarse;
    kdtree_coarse.setInputCloud(ref_coarse);
    auto evaluateTransformScore = [&](const Eigen::Matrix4f& T) -> double {
        pcl::PointCloud<pcl::PointXYZ> transformed;
        pcl::transformPointCloud(*sec_coarse, transformed, T);
        double sum_sq = 0.0;
        size_t count = 0;
        for (const auto& pt : transformed.points) {
            std::vector<int> idx(1);
            std::vector<float> sq_dist(1);
            if (kdtree_coarse.nearestKSearch(pt, 1, idx, sq_dist) > 0) {
                sum_sq += std::min<double>(sq_dist[0], 4.0);
                count++;
            }
        }
        return (count > 0) ? (sum_sq / static_cast<double>(count)) : 999.0;
    };

    Eigen::Matrix4f T_init = Eigen::Matrix4f::Identity();
    double best_init_score = evaluateTransformScore(T_init);

    // 2. Stage 1: Trajectory Hand-Eye / Umeyama Prior
    if (params.use_trajectory_prior && ref_traj.size() >= 3 && sec_traj.size() >= 3) {
        schemas::Transform3D traj_tf;
        size_t matched_poses = 0;
        double traj_rmse = 999.0;
        if (alignTrajectoriesUmeyama(ref_traj, sec_traj, traj_tf, matched_poses, traj_rmse)) {
            result.matched_trajectory_poses = matched_poses;
            result.trajectory_rmse_m = traj_rmse;
            Eigen::Matrix4f T_traj = Eigen::Matrix4f::Identity();
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    T_traj(r, c) = static_cast<float>(traj_tf.matrix4x4[r * 4 + c]);
                }
            }
            double traj_score = evaluateTransformScore(T_traj);
            if (traj_score <= best_init_score * 1.15) {
                T_init = T_traj;
                best_init_score = traj_score;
                result.used_trajectory_prior = true;
            }
        }
    }

    // 3. Stage 2A: SAC-IA FPFH Coarse Global Alignment (competes with trajectory prior)
    if (ref_coarse->size() >= 25 && sec_coarse->size() >= 25) {
        pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::Normal> nest;
        nest.setNumberOfThreads(4);
        nest.setRadiusSearch(params.normal_radius_m);

        pcl::PointCloud<pcl::Normal>::Ptr ref_normals(new pcl::PointCloud<pcl::Normal>);
        nest.setInputCloud(ref_coarse);
        nest.compute(*ref_normals);

        pcl::PointCloud<pcl::Normal>::Ptr sec_normals(new pcl::PointCloud<pcl::Normal>);
        nest.setInputCloud(sec_coarse);
        nest.compute(*sec_normals);

        pcl::FPFHEstimationOMP<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fest;
        fest.setNumberOfThreads(4);
        fest.setRadiusSearch(params.fpfh_radius_m);

        pcl::PointCloud<pcl::FPFHSignature33>::Ptr ref_feat(new pcl::PointCloud<pcl::FPFHSignature33>);
        fest.setInputCloud(ref_coarse);
        fest.setInputNormals(ref_normals);
        fest.compute(*ref_feat);

        pcl::PointCloud<pcl::FPFHSignature33>::Ptr sec_feat(new pcl::PointCloud<pcl::FPFHSignature33>);
        fest.setInputCloud(sec_coarse);
        fest.setInputNormals(sec_normals);
        fest.compute(*sec_feat);

        std::srand(42);
        pcl::SampleConsensusInitialAlignment<pcl::PointXYZ, pcl::PointXYZ, pcl::FPFHSignature33> sac_ia;
        sac_ia.setMinSampleDistance(0.25f);
        sac_ia.setMaxCorrespondenceDistance(0.80f);
        sac_ia.setMaximumIterations(500);
        sac_ia.setInputTarget(ref_coarse);
        sac_ia.setTargetFeatures(ref_feat);
        sac_ia.setInputSource(sec_coarse);
        sac_ia.setSourceFeatures(sec_feat);

        pcl::PointCloud<pcl::PointXYZ> sac_aligned;
        sac_ia.align(sac_aligned);
        Eigen::Matrix4f T_sac = sac_ia.getFinalTransformation();
        double sac_score = evaluateTransformScore(T_sac);
        // Penalize non-upright SAC-IA hypotheses (> 25 deg tilt from +Z Up)
        if (T_sac(2, 2) < 0.9063f) {
            sac_score *= 1.35;
        }
        if (sac_score < best_init_score) {
            T_init = T_sac;
            best_init_score = sac_score;
        }

        // Also test gravity-constrained 4-DoF upright projection of T_sac
        float sac_yaw = std::atan2(T_sac(1, 0), T_sac(0, 0));
        Eigen::Matrix4f T_sac_4dof = Eigen::Matrix4f::Identity();
        T_sac_4dof(0, 0) = std::cos(sac_yaw); T_sac_4dof(0, 1) = -std::sin(sac_yaw);
        T_sac_4dof(1, 0) = std::sin(sac_yaw); T_sac_4dof(1, 1) =  std::cos(sac_yaw);
        T_sac_4dof.block<3, 1>(0, 3) = T_sac.block<3, 1>(0, 3);
        double sac_4dof_score = evaluateTransformScore(T_sac_4dof);
        if (sac_4dof_score < best_init_score) {
            T_init = T_sac_4dof;
            best_init_score = sac_4dof_score;
        }
    }

    // 4. Stage 2B: Generalized ICP (GICP) Fine Cloud-to-Cloud Refinement
    pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> gicp;
    gicp.setMaximumIterations(params.max_gicp_iterations);
    gicp.setMaxCorrespondenceDistance(params.max_correspondence_dist_m);
    gicp.setTransformationEpsilon(1e-6);
    gicp.setEuclideanFitnessEpsilon(1e-6);
    gicp.setInputTarget(ref_fine);
    gicp.setInputSource(sec_fine);

    pcl::PointCloud<pcl::PointXYZ> gicp_aligned;
    gicp.align(gicp_aligned, T_init);

    Eigen::Matrix4f T_final = gicp.hasConverged() ? gicp.getFinalTransformation() : T_init;
    result.fitness_score = gicp.hasConverged() ? gicp.getFitnessScore() : best_init_score;

    // 5. Enforce Rigid SE(3) Orthonormalization
    if (params.enforce_rigid_se3) {
        Eigen::Matrix3f R = T_final.block<3, 3>(0, 0);
        Eigen::JacobiSVD<Eigen::Matrix3f> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Matrix3f R_ortho = svd.matrixU() * svd.matrixV().transpose();
        if (R_ortho.determinant() < 0.0f) {
            Eigen::Matrix3f V = svd.matrixV();
            V.col(2) *= -1.0f;
            R_ortho = svd.matrixU() * V.transpose();
        }
        T_final.block<3, 3>(0, 0) = R_ortho;
        result.scale_factor = 1.0;
    } else {
        Eigen::Matrix3f R_scaled = T_final.block<3, 3>(0, 0);
        float det = R_scaled.determinant();
        result.scale_factor = (det > 0.0f) ? std::cbrt(det) : 1.0;
    }

    double mat4x4[4][4];
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            mat4x4[r][c] = static_cast<double>(T_final(r, c));
        }
    }
    result.transform = matrixToTransform3D(mat4x4, &result.roll_deg, &result.pitch_deg, &result.yaw_deg);

    // 6. Compute Inlier RMSE, Overlap Ratio, and Confidence
    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree_fine;
    kdtree_fine.setInputCloud(ref_fine);

    pcl::PointCloud<pcl::PointXYZ> sec_eval;
    pcl::transformPointCloud(*sec_fine, sec_eval, T_final);

    size_t inliers = 0;
    double sum_sq_dist = 0.0;
    const float max_inlier_sq = static_cast<float>(params.inlier_threshold_m * params.inlier_threshold_m);

    for (const auto& pt : sec_eval.points) {
        std::vector<int> idx(1);
        std::vector<float> sq_dist(1);
        if (kdtree_fine.nearestKSearch(pt, 1, idx, sq_dist) > 0) {
            if (sq_dist[0] <= max_inlier_sq) {
                inliers++;
                sum_sq_dist += sq_dist[0];
            }
        }
    }

    result.evaluated_points = sec_eval.size();
    result.inlier_count = inliers;
    result.overlap_ratio = (sec_eval.size() > 0) ? (static_cast<double>(inliers) / static_cast<double>(sec_eval.size())) : 0.0;
    result.inlier_rmse_m = (inliers > 0) ? std::sqrt(sum_sq_dist / static_cast<double>(inliers)) : 999.0;

    // Compute confidence in [0.0, 1.0]
    double overlap_score = std::clamp(result.overlap_ratio / 0.75, 0.0, 1.0);
    double rmse_score = (result.inlier_rmse_m < 0.25) ? std::clamp(1.0 - (result.inlier_rmse_m / 0.25), 0.0, 1.0) : 0.0;
    result.confidence = std::clamp(0.55 * overlap_score + 0.45 * rmse_score, 0.0, 1.0);

    result.converged = (gicp.hasConverged() || result.overlap_ratio >= 0.35) && (inliers >= 10);
    if (result.converged && result.overlap_ratio >= 0.45 && result.inlier_rmse_m <= 0.12) {
        result.status = schemas::ExtrinsicStatus::VALIDATED;
    } else if (result.converged) {
        result.status = schemas::ExtrinsicStatus::ESTIMATED;
    } else {
        result.status = schemas::ExtrinsicStatus::UNKNOWN;
    }

    result.method = result.used_trajectory_prior
                        ? "TRAJECTORY_HAND_EYE_GICP_SE3"
                        : "TARGETLESS_SAC_IA_FPFH_GICP_SE3";

    auto end_time = std::chrono::steady_clock::now();
    result.computation_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1000.0;

    AV_LOG_INFO("ExtrinsicCalibrator",
                "Calibrated pair " + result.pair_key +
                ": status=" + schemas::to_string(result.status) +
                ", overlap=" + std::to_string(result.overlap_ratio * 100.0) + "%" +
                ", RMSE=" + std::to_string(result.inlier_rmse_m * 1000.0) + "mm" +
                ", confidence=" + std::to_string(result.confidence) +
                ", time=" + std::to_string(result.computation_time_ms) + "ms");

    return result;
}

bool ExtrinsicCalibrator::persistCalibrationToConfig(
    schemas::ScannerConfig& config,
    const ExtrinsicCalibrationResult& result,
    const std::string& supporting_capture_id,
    interfaces::IStorageEngine* storage) {
    if (!result.converged) {
        return false;
    }

    // Store under both explicit pair key "<ref><-<sec>" and "<sec>" for fast lookup
    config.set_relative_transform(result.pair_key, result.transform);
    config.set_relative_transform(result.secondary_sensor_id, result.transform);

    // Ensure reference sensor has identity transform recorded
    double id_mat[4][4] = {
        {1.0, 0.0, 0.0, 0.0},
        {0.0, 1.0, 0.0, 0.0},
        {0.0, 0.0, 1.0, 0.0},
        {0.0, 0.0, 0.0, 1.0}
    };
    schemas::Transform3D id_tf = matrixToTransform3D(id_mat);
    config.set_relative_transform(result.reference_sensor_id + "<-" + result.reference_sensor_id, id_tf);
    config.set_relative_transform(result.reference_sensor_id, id_tf);

    config.set_extrinsic_status(result.status);
    config.set_method(result.method);
    config.set_confidence(result.confidence);

    if (!supporting_capture_id.empty()) {
        auto caps = config.supporting_captures();
        if (std::find(caps.begin(), caps.end(), supporting_capture_id) == caps.end()) {
            caps.push_back(supporting_capture_id);
            config.set_supporting_captures(caps);
        }
    }

    // Format current ISO date
    std::time_t now_t = std::time(nullptr);
    char date_buf[64]{};
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now_t));
    std::string now_iso(date_buf);
    config.set_validation_date(now_iso);

    std::ostringstream hist;
    hist << std::fixed << std::setprecision(2)
         << "[" << now_iso << "] Calibrated " << result.pair_key
         << " via " << result.method
         << " (overlap=" << (result.overlap_ratio * 100.0) << "%, RMSE=" << (result.inlier_rmse_m * 1000.0)
         << "mm, status=" << schemas::to_string(result.status) << ")";
    config.add_validation_history(hist.str());

    nlohmann::json stats = config.residual_error_stats();
    if (!stats.is_object()) {
        stats = nlohmann::json::object();
    }
    stats[result.pair_key] = {
        {"inlier_rmse_m", result.inlier_rmse_m},
        {"inlier_rmse_mm", result.inlier_rmse_m * 1000.0},
        {"overlap_ratio", result.overlap_ratio},
        {"fitness_score", result.fitness_score},
        {"inlier_count", result.inlier_count},
        {"evaluated_points", result.evaluated_points},
        {"roll_deg", result.roll_deg},
        {"pitch_deg", result.pitch_deg},
        {"yaw_deg", result.yaw_deg},
        {"used_trajectory_prior", result.used_trajectory_prior},
        {"trajectory_rmse_m", result.trajectory_rmse_m},
        {"calibrated_at", now_iso}
    };
    config.set_residual_error_stats(stats);

    schemas::CalibrationProvenance prov(
        result.pair_key,
        schemas::CalibrationSource::AV_CALIBRATED,
        result.confidence,
        config.scanner_node_id(),
        "",
        hist.str(),
        result.status == schemas::ExtrinsicStatus::VALIDATED);
    config.set_provenance(prov);

    if (storage) {
        return storage->saveScannerConfig(config);
    }
    return true;
}

} // namespace av::core::fusion
