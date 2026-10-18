#include "core/registration/registration_engine.hpp"
#include "core/logging/logger.hpp"

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/fpfh_omp.h>
#include <pcl/registration/ia_ransac.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/gicp.h>
#include <pcl/common/transforms.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <chrono>
#include <cmath>
#include <filesystem>

namespace av {
namespace core {
namespace registration {

namespace fs = std::filesystem;

RegistrationResult RegistrationEngine::registerClouds(
    const std::string& ref_pcd_path,
    const std::string& mov_pcd_path,
    const RegistrationParams& params
) {
    RegistrationResult result;
    auto start_time = std::chrono::steady_clock::now();
    result.created_at_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    result.scale_policy = params.scale_policy;

    AV_LOG_INFO("RegistrationEngine", "Starting offline registration: Ref=" + ref_pcd_path + ", Mov=" + mov_pcd_path);

    if (!fs::exists(ref_pcd_path) || !fs::exists(mov_pcd_path)) {
        AV_LOG_ERROR("RegistrationEngine", "FileNotFound", "Source PCD file does not exist. Ref=" + ref_pcd_path + ", Mov=" + mov_pcd_path);
        result.status = "FAILED";
        return result;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_raw(new pcl::PointCloud<pcl::PointXYZ>);
    if (pcl::io::loadPCDFile<pcl::PointXYZ>(ref_pcd_path, *ref_raw) == -1) {
        AV_LOG_ERROR("RegistrationEngine", "LoadError", "Failed to load reference PCD: " + ref_pcd_path);
        result.status = "FAILED";
        return result;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_raw(new pcl::PointCloud<pcl::PointXYZ>);
    if (pcl::io::loadPCDFile<pcl::PointXYZ>(mov_pcd_path, *mov_raw) == -1) {
        AV_LOG_ERROR("RegistrationEngine", "LoadError", "Failed to load moving PCD: " + mov_pcd_path);
        result.status = "FAILED";
        return result;
    }

    AV_LOG_INFO("RegistrationEngine", "Raw points: Ref=" + std::to_string(ref_raw->size()) + ", Mov=" + std::to_string(mov_raw->size()));

    if (ref_raw->empty() || mov_raw->empty()) {
        AV_LOG_ERROR("RegistrationEngine", "EmptyCloud", "Cannot register empty point cloud");
        result.status = "FAILED";
        return result;
    }

    // 1. Coarse downsampling for fast global feature matching
    pcl::VoxelGrid<pcl::PointXYZ> vox_coarse;
    vox_coarse.setLeafSize(params.coarse_voxel_size, params.coarse_voxel_size, params.coarse_voxel_size);

    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(ref_raw);
    vox_coarse.filter(*ref_coarse);

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(mov_raw);
    vox_coarse.filter(*mov_coarse);

    AV_LOG_INFO("RegistrationEngine", "Coarse downsampled (" + std::to_string(params.coarse_voxel_size) + 
                "m): Ref=" + std::to_string(ref_coarse->size()) + ", Mov=" + std::to_string(mov_coarse->size()));

    // 2. Normals estimation on coarse cloud
    pcl::NormalEstimationOMP<pcl::PointXYZ, pcl::Normal> nest;
    nest.setNumberOfThreads(4);
    nest.setRadiusSearch(params.normal_radius);

    pcl::PointCloud<pcl::Normal>::Ptr ref_normals(new pcl::PointCloud<pcl::Normal>);
    nest.setInputCloud(ref_coarse);
    nest.compute(*ref_normals);

    pcl::PointCloud<pcl::Normal>::Ptr mov_normals(new pcl::PointCloud<pcl::Normal>);
    nest.setInputCloud(mov_coarse);
    nest.compute(*mov_normals);

    // 3. FPFH Features on coarse cloud
    pcl::FPFHEstimationOMP<pcl::PointXYZ, pcl::Normal, pcl::FPFHSignature33> fest;
    fest.setNumberOfThreads(4);
    fest.setRadiusSearch(params.fpfh_radius);

    pcl::PointCloud<pcl::FPFHSignature33>::Ptr ref_features(new pcl::PointCloud<pcl::FPFHSignature33>);
    fest.setInputCloud(ref_coarse);
    fest.setInputNormals(ref_normals);
    fest.compute(*ref_features);

    pcl::PointCloud<pcl::FPFHSignature33>::Ptr mov_features(new pcl::PointCloud<pcl::FPFHSignature33>);
    fest.setInputCloud(mov_coarse);
    fest.setInputNormals(mov_normals);
    fest.compute(*mov_features);

    // 4. SAC-IA Coarse Global Alignment (with Identity comparison for pre-aligned canonical clouds)
    std::srand(42);
    pcl::KdTreeFLANN<pcl::PointXYZ> coarse_tree;
    coarse_tree.setInputCloud(ref_coarse);
    auto eval_coarse_score = [&](const Eigen::Matrix4f& T_cand) -> double {
        pcl::PointCloud<pcl::PointXYZ> trans;
        pcl::transformPointCloud(*mov_coarse, trans, T_cand);
        double total_sq = 0.0;
        int valid = 0;
        std::vector<int> idx(1);
        std::vector<float> sq_dist(1);
        for (const auto& pt : trans.points) {
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
            if (coarse_tree.nearestKSearch(pt, 1, idx, sq_dist) > 0) {
                total_sq += std::min(sq_dist[0], params.sac_ia_max_corr * params.sac_ia_max_corr);
                valid++;
            }
        }
        return valid > 0 ? (total_sq / valid) : 1e9;
    };

    double identity_score = eval_coarse_score(Eigen::Matrix4f::Identity());

    pcl::SampleConsensusInitialAlignment<pcl::PointXYZ, pcl::PointXYZ, pcl::FPFHSignature33> sac_ia;
    sac_ia.setMinSampleDistance(params.sac_ia_min_sample_dist);
    sac_ia.setMaxCorrespondenceDistance(params.sac_ia_max_corr);
    sac_ia.setMaximumIterations(params.sac_ia_iterations);
    sac_ia.setInputTarget(ref_coarse);
    sac_ia.setTargetFeatures(ref_features);
    sac_ia.setInputSource(mov_coarse);
    sac_ia.setSourceFeatures(mov_features);

    pcl::PointCloud<pcl::PointXYZ>::Ptr sac_aligned(new pcl::PointCloud<pcl::PointXYZ>);
    sac_ia.align(*sac_aligned);

    Eigen::Matrix4f T_sac = sac_ia.getFinalTransformation();
    double sac_score = eval_coarse_score(T_sac);
    // Gravity-prior penalty: if SAC-IA tilts +Z Up by > 25 deg (cos(25 deg) = 0.9063), penalize raw 6-DoF score
    if (T_sac(2, 2) < 0.9063f) {
        sac_score *= 1.35;
    }

    Eigen::Matrix4f T_coarse = (sac_score < identity_score) ? T_sac : Eigen::Matrix4f::Identity();
    double best_coarse_score = std::min(sac_score, identity_score);

    // 4b. Gravity-Constrained 4-DoF (X, Y, Z, Yaw) Coarse Candidate Search
    // Since all canonical AV clouds share +Z Up, test upright 4-DoF projection of T_sac and 12 yaw hypotheses
    auto make_upright_4dof = [](float yaw_rad, const Eigen::Vector3f& c_ref, const Eigen::Vector3f& c_mov) {
        Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
        float cy = std::cos(yaw_rad);
        float sy = std::sin(yaw_rad);
        T(0, 0) = cy;  T(0, 1) = -sy;
        T(1, 0) = sy;  T(1, 1) =  cy;
        Eigen::Vector3f t = c_ref - T.block<3, 3>(0, 0) * c_mov;
        T(0, 3) = t.x();
        T(1, 3) = t.y();
        T(2, 3) = t.z();
        return T;
    };

    Eigen::Vector3f c_ref = Eigen::Vector3f::Zero();
    for (const auto& p : ref_coarse->points) c_ref += Eigen::Vector3f(p.x, p.y, p.z);
    if (!ref_coarse->empty()) c_ref /= static_cast<float>(ref_coarse->size());

    Eigen::Vector3f c_mov = Eigen::Vector3f::Zero();
    for (const auto& p : mov_coarse->points) c_mov += Eigen::Vector3f(p.x, p.y, p.z);
    if (!mov_coarse->empty()) c_mov /= static_cast<float>(mov_coarse->size());

    // Project SAC-IA onto gravity-upright 4-DoF
    float sac_yaw = std::atan2(T_sac(1, 0), T_sac(0, 0));
    Eigen::Matrix4f T_sac_4dof = Eigen::Matrix4f::Identity();
    T_sac_4dof(0, 0) = std::cos(sac_yaw); T_sac_4dof(0, 1) = -std::sin(sac_yaw);
    T_sac_4dof(1, 0) = std::sin(sac_yaw); T_sac_4dof(1, 1) =  std::cos(sac_yaw);
    T_sac_4dof.block<3, 1>(0, 3) = T_sac.block<3, 1>(0, 3);
    double sac_4dof_score = eval_coarse_score(T_sac_4dof);
    if (sac_4dof_score < best_coarse_score) {
        best_coarse_score = sac_4dof_score;
        T_coarse = T_sac_4dof;
    }

    for (int k = 0; k < 12; ++k) {
        float yaw = static_cast<float>(k) * static_cast<float>(M_PI / 6.0);
        Eigen::Matrix4f T_cand = make_upright_4dof(yaw, c_ref, c_mov);
        double score = eval_coarse_score(T_cand);
        if (score < best_coarse_score) {
            best_coarse_score = score;
            T_coarse = T_cand;
        }
    }

    AV_LOG_INFO("RegistrationEngine", "Coarse alignment complete, best_score=" + std::to_string(best_coarse_score) +
                ", sac_score=" + std::to_string(sac_score) + ", identity_score=" + std::to_string(identity_score));

    // 5. Fine downsampling for GICP
    pcl::VoxelGrid<pcl::PointXYZ> vox_fine;
    vox_fine.setLeafSize(params.fine_voxel_size, params.fine_voxel_size, params.fine_voxel_size);

    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(ref_raw);
    vox_fine.filter(*ref_fine);

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(mov_raw);
    vox_fine.filter(*mov_fine);

    AV_LOG_INFO("RegistrationEngine", "Fine downsampled (" + std::to_string(params.fine_voxel_size) + 
                "m): Ref=" + std::to_string(ref_fine->size()) + ", Mov=" + std::to_string(mov_fine->size()));

    // 6. GICP Fine Refinement
    pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> gicp;
    gicp.setMaximumIterations(params.gicp_iterations);
    gicp.setMaxCorrespondenceDistance(params.gicp_max_corr);
    gicp.setTransformationEpsilon(1e-6);
    gicp.setEuclideanFitnessEpsilon(1e-6);
    gicp.setInputTarget(ref_fine);
    gicp.setInputSource(mov_fine);

    pcl::PointCloud<pcl::PointXYZ>::Ptr gicp_aligned(new pcl::PointCloud<pcl::PointXYZ>);
    gicp.align(*gicp_aligned, T_coarse);

    Eigen::Matrix4f T_final = gicp.getFinalTransformation();
    bool converged = gicp.hasConverged();
    result.fitness_score = gicp.getFitnessScore();

    AV_LOG_INFO("RegistrationEngine", "GICP finished, converged=" + std::to_string(converged) + 
                ", fitness=" + std::to_string(result.fitness_score));

    // 7. Enforce Scale Policy (SE3 vs Sim3)
    if (params.scale_policy == ScalePolicy::LOCKED_METRIC_SE3) {
        // Enforce pure rigid SE(3): orthonormalize rotation block R
        Eigen::Matrix3f R = T_final.block<3, 3>(0, 0);
        Eigen::JacobiSVD<Eigen::Matrix3f> svd(R, Eigen::ComputeFullU | Eigen::ComputeFullV);
        Eigen::Matrix3f R_ortho = svd.matrixU() * svd.matrixV().transpose();
        if (R_ortho.determinant() < 0.0f) {
            Eigen::Matrix3f V = svd.matrixV();
            V.col(2) *= -1.0f;
            R_ortho = svd.matrixU() * V.transpose();
        }
        T_final.block<3, 3>(0, 0) = R_ortho;
        result.scale_factor = 1.000000;
    } else {
        // Sim(3) Scale estimation
        Eigen::Matrix3f R_scaled = T_final.block<3, 3>(0, 0);
        float det = R_scaled.determinant();
        float estimated_scale = (det > 0.0f) ? std::cbrt(det) : 1.0f;
        result.scale_factor = estimated_scale;
    }

    // Populate matrix array
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            result.transform_matrix[r][c] = static_cast<double>(T_final(r, c));
        }
    }

    // 8. Quality Assessment: Inlier RMSE and Overlap Ratio
    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;
    kdtree.setInputCloud(ref_fine);
    int64_t inliers = 0;
    double sum_sq_dist = 0.0;
    float max_dist = params.inlier_threshold_m;

    // Transform fine moving cloud by T_final for error calculation
    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_eval(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::transformPointCloud(*mov_fine, *mov_eval, T_final);

    for (const auto& pt : mov_eval->points) {
        std::vector<int> idx(1);
        std::vector<float> dist(1);
        if (kdtree.nearestKSearch(pt, 1, idx, dist) > 0) {
            float d = std::sqrt(dist[0]);
            if (d <= max_dist) {
                inliers++;
                sum_sq_dist += dist[0];
            }
        }
    }

    result.total_points_evaluated = mov_fine->size();
    result.inlier_count = inliers;
    result.overlap_ratio = (mov_fine->size() > 0) ? static_cast<double>(inliers) / mov_fine->size() : 0.0;
    result.inlier_rmse_m = (inliers > 0) ? std::sqrt(sum_sq_dist / inliers) : 999.0;

    auto end_time = std::chrono::steady_clock::now();
    result.computation_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    result.method = "SAC_IA_FPFH_GICP_SE3";
    result.status = (converged && result.overlap_ratio >= 0.50) ? "CONVERGED" : "FAILED";

    AV_LOG_INFO("RegistrationEngine", "Registration outcome: status=" + result.status + 
                ", overlap=" + std::to_string(result.overlap_ratio * 100.0) + "%" +
                ", RMSE=" + std::to_string(result.inlier_rmse_m * 1000.0) + "mm" +
                ", time=" + std::to_string(result.computation_time_ms) + "ms");

    return result;
}

bool RegistrationEngine::generateColoredMergedPCD(
    const std::string& ref_pcd_path,
    const std::string& mov_pcd_path,
    const Eigen::Matrix4f& T_ref_from_mov,
    const std::string& output_combined_pcd_path,
    const std::string& output_moving_aligned_pcd_path,
    bool differentiate_colors,
    const std::string& output_combined_diff_pcd_path
) {
    if (!fs::exists(ref_pcd_path) || !fs::exists(mov_pcd_path)) {
        AV_LOG_ERROR("RegistrationEngine", "FileNotFound", "Source PCD file does not exist for merging");
        return false;
    }

    // Helper to load PCD into PointXYZRGB preserving true RGB or intensity reflectance
    auto loadCloudAsRgb = [](const std::string& path, pcl::PointCloud<pcl::PointXYZRGB>::Ptr& cloud, uint8_t fallback_r, uint8_t fallback_g, uint8_t fallback_b) -> bool {
        bool has_rgb_in_header = false;
        bool has_intensity_in_header = false;
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.rfind("FIELDS", 0) == 0) {
                    if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                        has_rgb_in_header = true;
                    }
                    if (line.find("intensity") != std::string::npos) {
                        has_intensity_in_header = true;
                    }
                    break;
                }
            }
        }

        // 1. If header explicitly contains RGB, load as native PointXYZRGB
        if (has_rgb_in_header) {
            if (pcl::io::loadPCDFile<pcl::PointXYZRGB>(path, *cloud) == 0 && !cloud->empty()) {
                return true;
            }
        }

        // 2. If header contains intensity (LiDAR / grayscale SLAM), preserve intensity as neutral reflectance
        if (has_intensity_in_header) {
            pcl::PointCloud<pcl::PointXYZI>::Ptr xyzi(new pcl::PointCloud<pcl::PointXYZI>);
            if (pcl::io::loadPCDFile<pcl::PointXYZI>(path, *xyzi) == 0 && !xyzi->empty()) {
                cloud->resize(xyzi->size());
                for (size_t i = 0; i < xyzi->size(); ++i) {
                    cloud->points[i].x = xyzi->points[i].x;
                    cloud->points[i].y = xyzi->points[i].y;
                    cloud->points[i].z = xyzi->points[i].z;
                    float rawI = xyzi->points[i].intensity;
                    uint8_t iv = static_cast<uint8_t>(std::clamp(rawI <= 1.0f ? rawI * 255.0f : rawI, 50.0f, 255.0f));
                    cloud->points[i].r = iv;
                    cloud->points[i].g = iv;
                    cloud->points[i].b = iv;
                }
                return true;
            }
        }

        // 3. Fallback to basic XYZ with neutral default reflectance
        pcl::PointCloud<pcl::PointXYZ>::Ptr xyz(new pcl::PointCloud<pcl::PointXYZ>);
        if (pcl::io::loadPCDFile<pcl::PointXYZ>(path, *xyz) == 0 && !xyz->empty()) {
            cloud->resize(xyz->size());
            for (size_t i = 0; i < xyz->size(); ++i) {
                cloud->points[i].x = xyz->points[i].x;
                cloud->points[i].y = xyz->points[i].y;
                cloud->points[i].z = xyz->points[i].z;
                cloud->points[i].r = fallback_r;
                cloud->points[i].g = fallback_g;
                cloud->points[i].b = fallback_b;
            }
            return true;
        }
        return false;
    };

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr ref_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    if (!loadCloudAsRgb(ref_pcd_path, ref_cloud, 210, 215, 225)) {
        return false;
    }

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr mov_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    if (!loadCloudAsRgb(mov_pcd_path, mov_cloud, 220, 220, 220)) {
        return false;
    }

    // Transform moving cloud into reference frame (True RGB preserved)
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr mov_aligned(new pcl::PointCloud<pcl::PointXYZRGB>);
    pcl::transformPointCloud(*mov_cloud, *mov_aligned, T_ref_from_mov);

    // Save moving aligned cloud (True RGB)
    if (!output_moving_aligned_pcd_path.empty()) {
        fs::create_directories(fs::path(output_moving_aligned_pcd_path).parent_path());
        pcl::io::savePCDFileASCII(output_moving_aligned_pcd_path, *mov_aligned);
    }

    // Generate combined true RGB cloud
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr combined_rgb(new pcl::PointCloud<pcl::PointXYZRGB>);
    *combined_rgb = *ref_cloud + *mov_aligned;

    if (!output_combined_pcd_path.empty()) {
        fs::create_directories(fs::path(output_combined_pcd_path).parent_path());
        pcl::io::savePCDFileASCII(output_combined_pcd_path, *combined_rgb);
        AV_LOG_INFO("RegistrationEngine", "Saved combined true RGB PCD: " + output_combined_pcd_path + 
                    " (" + std::to_string(combined_rgb->size()) + " points)");
    }

    return true;
}

} // namespace registration
} // namespace core
} // namespace av
