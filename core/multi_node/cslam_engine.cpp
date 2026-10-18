#include "core/multi_node/cslam_engine.hpp"
#include "core/logging/logger.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/registration/icp.h>
#include <pcl/registration/gicp.h>
#include <pcl/common/transforms.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <cmath>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <iomanip>

namespace av::core::multi_node {

static uint64_t getNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static Eigen::Matrix3f quaternionToRotationMatrix(const schemas::Quaternion& q) {
    double norm = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (norm < 1e-9) {
        return Eigen::Matrix3f::Identity();
    }
    Eigen::Quaternionf eq(
        static_cast<float>(q.w / norm),
        static_cast<float>(q.x / norm),
        static_cast<float>(q.y / norm),
        static_cast<float>(q.z / norm)
    );
    return eq.toRotationMatrix();
}

static std::string rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
    std::ostringstream oss;
    oss << "#" << std::uppercase << std::hex << std::setfill('0')
        << std::setw(2) << static_cast<int>(r)
        << std::setw(2) << static_cast<int>(g)
        << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

CSlamEngine::CSlamEngine() = default;

void CSlamEngine::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    nodes_.clear();
    node_order_.clear();
}

void CSlamEngine::setSeparationDistanceM(double dist_m) {
    std::lock_guard<std::mutex> lock(mutex_);
    separation_distance_m_ = std::max(2.0, dist_m);
    for (auto& kv : nodes_) {
        if (kv.second.scanner_index > 0) {
            kv.second.arbitrary_offset_m = computeSeparationOffset(kv.second.scanner_index, separation_distance_m_);
            syncWorldTransformMatrix(kv.second);
        }
    }
}

double CSlamEngine::getSeparationDistanceM() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return separation_distance_m_;
}

void CSlamEngine::setOverlapSnapThreshold(double min_overlap_ratio, double max_rmse_m) {
    std::lock_guard<std::mutex> lock(mutex_);
    overlap_snap_threshold_ = std::clamp(min_overlap_ratio, 0.05, 0.95);
    max_snap_rmse_m_ = std::max(0.02, max_rmse_m);
}

CSlamColor CSlamEngine::getScannerDistinctColor(size_t scanner_index) {
    // First 8 scanners use a curated high-contrast palette optimized for dark 3D viewports
    static const uint8_t kCuratedPalette[8][3] = {
        {  0, 210, 255}, // 0: Cyan-Blue (Anchor / Primary)
        { 46, 230, 115}, // 1: Emerald Green
        {255, 165,  30}, // 2: Amber-Orange
        {245,  70, 210}, // 3: Magenta-Pink
        {255, 230,  50}, // 4: Solar Gold
        {160, 100, 255}, // 5: Electric Violet
        {255,  80,  80}, // 6: Coral Red
        {  0, 240, 180}  // 7: Aquamarine
    };

    if (scanner_index < 8) {
        uint8_t r = kCuratedPalette[scanner_index][0];
        uint8_t g = kCuratedPalette[scanner_index][1];
        uint8_t b = kCuratedPalette[scanner_index][2];
        return {r, g, b, rgbToHex(r, g, b)};
    }

    // Unlimited scanners (k >= 8): Golden-Angle HSV distribution (137.50776405 deg step)
    double hue_deg = std::fmod(static_cast<double>(scanner_index) * 137.50776405003785, 360.0);
    double sat = 0.85;
    double val = 0.96;

    double c = val * sat;
    double x = c * (1.0 - std::abs(std::fmod(hue_deg / 60.0, 2.0) - 1.0));
    double m = val - c;

    double r1 = 0.0, g1 = 0.0, b1 = 0.0;
    if (hue_deg < 60.0)       { r1 = c; g1 = x; b1 = 0; }
    else if (hue_deg < 120.0) { r1 = x; g1 = c; b1 = 0; }
    else if (hue_deg < 180.0) { r1 = 0; g1 = c; b1 = x; }
    else if (hue_deg < 240.0) { r1 = 0; g1 = x; b1 = c; }
    else if (hue_deg < 300.0) { r1 = x; g1 = 0; b1 = c; }
    else                      { r1 = c; g1 = 0; b1 = x; }

    uint8_t r = static_cast<uint8_t>(std::clamp((r1 + m) * 255.0, 0.0, 255.0));
    uint8_t g = static_cast<uint8_t>(std::clamp((g1 + m) * 255.0, 0.0, 255.0));
    uint8_t b = static_cast<uint8_t>(std::clamp((b1 + m) * 255.0, 0.0, 255.0));
    return {r, g, b, rgbToHex(r, g, b)};
}

schemas::Vector3D CSlamEngine::computeSeparationOffset(size_t scanner_index, double base_spacing_m) {
    if (scanner_index == 0) {
        return {0.0, 0.0, 0.0};
    }
    // Concentric ring placement so unlimited unaligned scanners never overlap each other:
    // Ring 1 (indices 1..4): (+d,0), (-d,0), (0,+d), (0,-d)
    // Ring 1 diagonals (indices 5..8): (+d,+d), (-d,+d), (-d,-d), (+d,-d)
    // Higher indices: spiral outward by ring radius
    static const double kDirections[8][2] = {
        { 1.0,  0.0},
        {-1.0,  0.0},
        { 0.0,  1.0},
        { 0.0, -1.0},
        { 1.0,  1.0},
        {-1.0,  1.0},
        {-1.0, -1.0},
        { 1.0, -1.0}
    };
    size_t ring = (scanner_index - 1) / 8 + 1;
    size_t slot = (scanner_index - 1) % 8;
    double radius = static_cast<double>(ring) * base_spacing_m;
    return {
        radius * kDirections[slot][0],
        radius * kDirections[slot][1],
        0.0
    };
}

void CSlamEngine::syncWorldTransformMatrix(CSlamNodeSubmap& node) {
    Eigen::Matrix4f T = getEffectiveWorldMatrix(node);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            node.T_world_from_scanner.matrix[r][c] = static_cast<double>(T(r, c));
        }
    }
}

size_t CSlamEngine::registerOrUpdateNode(const std::string& node_uuid,
                                         const std::string& display_name,
                                         const schemas::Quaternion& imu_orientation,
                                         bool is_anchor) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_uuid);
    if (it == nodes_.end()) {
        size_t idx = node_order_.size();
        node_order_.push_back(node_uuid);

        CSlamNodeSubmap sub;
        sub.node_uuid = node_uuid;
        sub.display_name = display_name.empty() ? ("Scanner-" + std::to_string(idx + 1)) : display_name;
        sub.scanner_index = idx;
        sub.distinct_color = getScannerDistinctColor(idx);
        sub.color_r = sub.distinct_color.r;
        sub.color_g = sub.distinct_color.g;
        sub.color_b = sub.distinct_color.b;
        sub.imu_orientation = imu_orientation;
        sub.last_update_ns = getNowNs();

        if (idx == 0 || is_anchor) {
            sub.alignment_state = CSlamAlignmentState::ANCHOR;
            sub.arbitrary_offset_m = {0.0, 0.0, 0.0};
            sub.overlap_ratio = 1.0;
            sub.inlier_rmse_m = 0.0;
        } else {
            sub.alignment_state = CSlamAlignmentState::UNALIGNED_IMU_OFFSET;
            sub.arbitrary_offset_m = computeSeparationOffset(idx, separation_distance_m_);
        }
        syncWorldTransformMatrix(sub);
        nodes_[node_uuid] = std::move(sub);
        return idx;
    } else {
        if (!display_name.empty()) {
            it->second.display_name = display_name;
        }
        it->second.imu_orientation = imu_orientation;
        it->second.last_update_ns = getNowNs();
        syncWorldTransformMatrix(it->second);
        return it->second.scanner_index;
    }
}

void CSlamEngine::ingestSubmap(const std::string& node_uuid,
                               const std::string& display_name,
                               const std::vector<schemas::PointXYZI>& points,
                               const schemas::Quaternion& imu_orientation,
                               const schemas::Pose3D& local_pose,
                               bool replace_cloud,
                               bool auto_evaluate_overlap,
                               const std::vector<schemas::Pose3D>& trajectory) {
    if (node_uuid.empty()) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodes_.find(node_uuid);
        if (it == nodes_.end()) {
            size_t idx = node_order_.size();
            node_order_.push_back(node_uuid);

            CSlamNodeSubmap sub;
            sub.node_uuid = node_uuid;
            sub.display_name = display_name.empty() ? ("Scanner-" + std::to_string(idx + 1)) : display_name;
            sub.scanner_index = idx;
            sub.distinct_color = getScannerDistinctColor(idx);
            sub.color_r = sub.distinct_color.r;
            sub.color_g = sub.distinct_color.g;
            sub.color_b = sub.distinct_color.b;
            sub.imu_orientation = imu_orientation;
            sub.current_local_pose = local_pose;
            sub.last_update_ns = getNowNs();

            if (idx == 0) {
                sub.alignment_state = CSlamAlignmentState::ANCHOR;
                sub.arbitrary_offset_m = {0.0, 0.0, 0.0};
                sub.overlap_ratio = 1.0;
            } else {
                sub.alignment_state = CSlamAlignmentState::UNALIGNED_IMU_OFFSET;
                sub.arbitrary_offset_m = computeSeparationOffset(idx, separation_distance_m_);
            }
            syncWorldTransformMatrix(sub);
            nodes_[node_uuid] = std::move(sub);
            it = nodes_.find(node_uuid);
        } else {
            if (!display_name.empty()) it->second.display_name = display_name;
            it->second.imu_orientation = imu_orientation;
            it->second.current_local_pose = local_pose;
            it->second.last_update_ns = getNowNs();
            syncWorldTransformMatrix(it->second);
        }

        if (!trajectory.empty()) {
            it->second.trajectory = trajectory;
        }

        if (replace_cloud) {
            it->second.raw_points = points;
        } else {
            // Fast incremental append + spatial hash voxel deduplication
            it->second.raw_points.insert(it->second.raw_points.end(), points.begin(), points.end());
            if (it->second.raw_points.size() > 25000) {
                const float inv_leaf = 1.0f / submap_voxel_leaf_m_;
                std::unordered_map<uint64_t, schemas::PointXYZI> voxel_map;
                voxel_map.reserve(it->second.raw_points.size() / 2);
                for (const auto& pt : it->second.raw_points) {
                    if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
                    int64_t ix = static_cast<int64_t>(std::floor(pt.x * inv_leaf));
                    int64_t iy = static_cast<int64_t>(std::floor(pt.y * inv_leaf));
                    int64_t iz = static_cast<int64_t>(std::floor(pt.z * inv_leaf));
                    uint64_t h = (static_cast<uint64_t>(ix) * 73856093ULL) ^
                                 (static_cast<uint64_t>(iy) * 19349663ULL) ^
                                 (static_cast<uint64_t>(iz) * 83492791ULL);
                    voxel_map[h] = pt;
                }
                it->second.raw_points.clear();
                it->second.raw_points.reserve(voxel_map.size());
                for (const auto& kv : voxel_map) {
                    it->second.raw_points.push_back(kv.second);
                }
            }
        }

        if (auto_evaluate_overlap && nodes_.size() >= 2) {
            evaluateAndSnapOverlapsUnlocked();
        }
    }
}

Eigen::Matrix4f CSlamEngine::getEffectiveWorldMatrix(const CSlamNodeSubmap& node) {
    if (node.alignment_state == CSlamAlignmentState::OVERLAP_LOCKED) {
        Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
        if (node.snapped_transform.matrix4x4.size() == 16) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    T(r, c) = static_cast<float>(node.snapped_transform.matrix4x4[r * 4 + c]);
                }
            }
            return T;
        }
        Eigen::Matrix3f R_snap = quaternionToRotationMatrix(node.snapped_transform.rotation);
        T.block<3, 3>(0, 0) = R_snap;
        T(0, 3) = static_cast<float>(node.snapped_transform.translation.x);
        T(1, 3) = static_cast<float>(node.snapped_transform.translation.y);
        T(2, 3) = static_cast<float>(node.snapped_transform.translation.z);
        return T;
    }

    // For ANCHOR and UNALIGNED_IMU_OFFSET:
    // Apply relative IMU orientation quaternion so the scan is properly gravity- and heading-oriented
    Eigen::Matrix4f T = Eigen::Matrix4f::Identity();
    Eigen::Matrix3f R_imu = quaternionToRotationMatrix(node.imu_orientation);
    T.block<3, 3>(0, 0) = R_imu;

    if (node.alignment_state == CSlamAlignmentState::UNALIGNED_IMU_OFFSET) {
        T(0, 3) = static_cast<float>(node.arbitrary_offset_m.x);
        T(1, 3) = static_cast<float>(node.arbitrary_offset_m.y);
        T(2, 3) = static_cast<float>(node.arbitrary_offset_m.z);
    }
    return T;
}

size_t CSlamEngine::evaluateAndSnapOverlaps() {
    std::lock_guard<std::mutex> lock(mutex_);
    return evaluateAndSnapOverlapsUnlocked();
}

size_t CSlamEngine::evaluateAndSnapOverlapsUnlocked() {
    if (nodes_.size() < 2) return 0;

    size_t total_newly_snapped = 0;
    bool snapped_in_pass = true;

    // Multi-pass loop so multi-hop overlap chains (e.g. Node C -> Node B -> Node A) snap in one call
    while (snapped_in_pass) {
        snapped_in_pass = false;

        // 1. Collect the unified Anchored Reference Cloud from all ANCHOR and OVERLAP_LOCKED nodes
        std::vector<schemas::PointXYZI> anchored_points;
        std::string primary_anchor_uuid;
        for (const auto& uuid : node_order_) {
            const auto& node = nodes_.at(uuid);
            if ((node.alignment_state == CSlamAlignmentState::ANCHOR ||
                 node.alignment_state == CSlamAlignmentState::OVERLAP_LOCKED) &&
                !node.raw_points.empty()) {
                if (primary_anchor_uuid.empty()) primary_anchor_uuid = uuid;
                Eigen::Matrix4f T_world = getEffectiveWorldMatrix(node);
                anchored_points.reserve(anchored_points.size() + node.raw_points.size());
                for (const auto& pt : node.raw_points) {
                    if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
                    Eigen::Vector3f p(pt.x, pt.y, pt.z);
                    Eigen::Vector3f pw = T_world.block<3, 3>(0, 0) * p + T_world.block<3, 1>(0, 3);
                    schemas::PointXYZI out_pt = pt;
                    out_pt.x = pw.x();
                    out_pt.y = pw.y();
                    out_pt.z = pw.z();
                    anchored_points.push_back(out_pt);
                }
            }
        }

        if (anchored_points.size() < 30) break;

        // 2. Test each UNALIGNED_IMU_OFFSET node against the anchored cloud
        for (const auto& uuid : node_order_) {
            auto& node = nodes_.at(uuid);
            if (node.alignment_state != CSlamAlignmentState::UNALIGNED_IMU_OFFSET) continue;
            if (node.raw_points.size() < 30) continue;

            if (tryAlignNodeToAnchoredCloud(node, anchored_points, primary_anchor_uuid)) {
                snapped_in_pass = true;
                total_newly_snapped++;
                AV_LOG_INFO("CSlamEngine", "Scanner '" + node.display_name + "' (" + node.node_uuid +
                            ") SNAPPED to global C-SLAM map! Overlap=" +
                            std::to_string(node.overlap_ratio * 100.0) + "%, RMSE=" +
                            std::to_string(node.inlier_rmse_m) + "m");
            }
        }
    }

    return total_newly_snapped;
}

bool CSlamEngine::tryAlignNodeToAnchoredCloud(
    CSlamNodeSubmap& unaligned_node,
    const std::vector<schemas::PointXYZI>& anchored_points,
    const std::string& primary_anchor_uuid) {

    // Convert anchored points to PCL cloud
    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_raw(new pcl::PointCloud<pcl::PointXYZ>);
    ref_raw->points.reserve(anchored_points.size());
    for (const auto& pt : anchored_points) {
        ref_raw->points.emplace_back(pt.x, pt.y, pt.z);
    }

    // Apply the node's IMU orientation prior R_imu to its local points first
    Eigen::Matrix3f R_imu = quaternionToRotationMatrix(unaligned_node.imu_orientation);
    Eigen::Matrix4f T_imu = Eigen::Matrix4f::Identity();
    T_imu.block<3, 3>(0, 0) = R_imu;

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_imu(new pcl::PointCloud<pcl::PointXYZ>);
    mov_imu->points.reserve(unaligned_node.raw_points.size());
    for (const auto& pt : unaligned_node.raw_points) {
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
        Eigen::Vector3f p = R_imu * Eigen::Vector3f(pt.x, pt.y, pt.z);
        mov_imu->points.emplace_back(p.x(), p.y(), p.z());
    }

    if (ref_raw->points.size() < 30 || mov_imu->points.size() < 30) return false;

    // Downsample both clouds for fast real-time C-SLAM overlap evaluation
    pcl::VoxelGrid<pcl::PointXYZ> vox_fine;
    vox_fine.setLeafSize(0.08f, 0.08f, 0.08f);

    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(ref_raw);
    vox_fine.filter(*ref_fine);

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_fine(new pcl::PointCloud<pcl::PointXYZ>);
    vox_fine.setInputCloud(mov_imu);
    vox_fine.filter(*mov_fine);

    if (ref_fine->points.size() < 20 || mov_fine->points.size() < 20) return false;

    // Coarse 20cm clouds for rapid 4-DoF candidate evaluation
    pcl::VoxelGrid<pcl::PointXYZ> vox_coarse;
    vox_coarse.setLeafSize(0.20f, 0.20f, 0.20f);

    pcl::PointCloud<pcl::PointXYZ>::Ptr ref_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(ref_fine);
    vox_coarse.filter(*ref_coarse);

    pcl::PointCloud<pcl::PointXYZ>::Ptr mov_coarse(new pcl::PointCloud<pcl::PointXYZ>);
    vox_coarse.setInputCloud(mov_fine);
    vox_coarse.filter(*mov_coarse);

    if (ref_coarse->empty() || mov_coarse->empty()) return false;

    pcl::KdTreeFLANN<pcl::PointXYZ> coarse_tree;
    coarse_tree.setInputCloud(ref_coarse);

    // Compute centroids in IMU-oriented space
    Eigen::Vector3f c_ref = Eigen::Vector3f::Zero();
    for (const auto& p : ref_coarse->points) c_ref += Eigen::Vector3f(p.x, p.y, p.z);
    c_ref /= static_cast<float>(ref_coarse->size());

    Eigen::Vector3f c_mov = Eigen::Vector3f::Zero();
    for (const auto& p : mov_coarse->points) c_mov += Eigen::Vector3f(p.x, p.y, p.z);
    c_mov /= static_cast<float>(mov_coarse->size());

    // Check if the two clouds are within relocalization range in their IMU-oriented frames
    // (or have proximate points within 8.0m). If a scanner's local coordinates are placed > 10m away
    // with zero proximate points, it represents a disjoint region that hasn't connected yet.
    float centroid_dist = (c_ref - c_mov).norm();
    int proximate_coarse_pts = 0;
    {
        std::vector<int> idx(1);
        std::vector<float> sq_dist(1);
        for (const auto& pt : mov_coarse->points) {
            if (coarse_tree.nearestKSearch(pt, 1, idx, sq_dist) > 0 && sq_dist[0] <= 2.5f * 2.5f) {
                proximate_coarse_pts++;
            }
        }
    }
    double proximate_ratio = static_cast<double>(proximate_coarse_pts) / static_cast<double>(mov_coarse->size());
    if (centroid_dist > 8.5f && proximate_ratio < 0.15) {
        unaligned_node.overlap_ratio = proximate_ratio;
        return false;
    }

    auto eval_coarse_candidate = [&](const Eigen::Matrix4f& T_cand) -> double {
        pcl::PointCloud<pcl::PointXYZ> trans;
        pcl::transformPointCloud(*mov_coarse, trans, T_cand);
        double score = 0.0;
        std::vector<int> idx(1);
        std::vector<float> sq_dist(1);
        for (const auto& pt : trans.points) {
            if (coarse_tree.nearestKSearch(pt, 1, idx, sq_dist) > 0) {
                score += std::min(sq_dist[0], 0.64f); // cap at (0.8m)^2
            } else {
                score += 0.64;
            }
        }
        return score / static_cast<double>(trans.size());
    };

    // Test Identity in IMU-oriented space first (respects relative IMU heading directly!)
    Eigen::Matrix4f best_T_imu_space = Eigen::Matrix4f::Identity();
    double best_coarse_score = eval_coarse_candidate(best_T_imu_space);

    // Also test 4-DoF yaw + translation candidates around IMU heading (0 deg) and 24 yaw steps
    for (int k = 0; k < 24; ++k) {
        float yaw = static_cast<float>(k) * static_cast<float>(M_PI / 12.0);
        float cy = std::cos(yaw);
        float sy = std::sin(yaw);

        // Candidate 1: Pure yaw rotation around origin (when scanners share close origin)
        Eigen::Matrix4f T_origin = Eigen::Matrix4f::Identity();
        T_origin(0, 0) = cy; T_origin(0, 1) = -sy;
        T_origin(1, 0) = sy; T_origin(1, 1) =  cy;
        double s_orig = eval_coarse_candidate(T_origin);
        if (s_orig < best_coarse_score) {
            best_coarse_score = s_orig;
            best_T_imu_space = T_origin;
        }

        // Candidate 2: Yaw rotation + centroid alignment (when scanners started at offset positions in same room)
        Eigen::Matrix4f T_cent = T_origin;
        Eigen::Vector3f t = c_ref - T_cent.block<3, 3>(0, 0) * c_mov;
        T_cent(0, 3) = t.x();
        T_cent(1, 3) = t.y();
        T_cent(2, 3) = t.z();
        double s_cent = eval_coarse_candidate(T_cent);
        if (s_cent < best_coarse_score) {
            best_coarse_score = s_cent;
            best_T_imu_space = T_cent;
        }
    }

    // Fine GICP/ICP refinement on 8cm voxel clouds
    pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
    icp.setMaximumIterations(50);
    icp.setMaxCorrespondenceDistance(0.35f);
    icp.setTransformationEpsilon(1e-6);
    icp.setEuclideanFitnessEpsilon(1e-6);
    icp.setInputTarget(ref_fine);
    icp.setInputSource(mov_fine);

    pcl::PointCloud<pcl::PointXYZ>::Ptr aligned_fine(new pcl::PointCloud<pcl::PointXYZ>);
    icp.align(*aligned_fine, best_T_imu_space);

    Eigen::Matrix4f T_refined_imu = icp.getFinalTransformation();

    // Enforce rigid SE(3) orthonormal rotation and upright gravity constraint (+Z Up)
    Eigen::Matrix3f R_ref = T_refined_imu.block<3, 3>(0, 0);
    Eigen::JacobiSVD<Eigen::Matrix3f> svd(R_ref, Eigen::ComputeFullU | Eigen::ComputeFullV);
    Eigen::Matrix3f R_ortho = svd.matrixU() * svd.matrixV().transpose();
    if (R_ortho.determinant() < 0.0f) {
        Eigen::Matrix3f V = svd.matrixV();
        V.col(2) *= -1.0f;
        R_ortho = svd.matrixU() * V.transpose();
    }
    T_refined_imu.block<3, 3>(0, 0) = R_ortho;

    // Gravity check: since both clouds are already IMU-gravity-aligned, R_ortho(2,2) must remain upright (> 0.90)
    if (R_ortho(2, 2) < 0.90f) {
        return false;
    }

    // Evaluate true geometric inlier overlap ratio and RMSE on fine cloud
    pcl::KdTreeFLANN<pcl::PointXYZ> fine_tree;
    fine_tree.setInputCloud(ref_fine);

    const float inlier_thresh_sq = 0.20f * 0.20f; // 20 cm inlier threshold
    size_t inlier_count = 0;
    double sum_sq_err = 0.0;
    std::vector<int> nn_idx(1);
    std::vector<float> nn_sq(1);

    pcl::PointCloud<pcl::PointXYZ> transformed_mov;
    pcl::transformPointCloud(*mov_fine, transformed_mov, T_refined_imu);

    for (const auto& pt : transformed_mov.points) {
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
        if (fine_tree.nearestKSearch(pt, 1, nn_idx, nn_sq) > 0) {
            if (nn_sq[0] <= inlier_thresh_sq) {
                inlier_count++;
                sum_sq_err += nn_sq[0];
            }
        }
    }

    double overlap_ratio = static_cast<double>(inlier_count) / static_cast<double>(transformed_mov.size());
    double rmse_m = (inlier_count > 0) ? std::sqrt(sum_sq_err / static_cast<double>(inlier_count)) : 999.0;

    unaligned_node.overlap_ratio = overlap_ratio;
    unaligned_node.inlier_rmse_m = rmse_m;

    if (inlier_count >= 20 && overlap_ratio >= overlap_snap_threshold_ && rmse_m <= max_snap_rmse_m_) {
        // Total transform from raw node frame to world frame is T_world_from_node = T_refined_imu * T_imu
        Eigen::Matrix4f T_world_from_node = T_refined_imu * T_imu;

        schemas::Transform3D tf;
        tf.translation = {
            static_cast<double>(T_world_from_node(0, 3)),
            static_cast<double>(T_world_from_node(1, 3)),
            static_cast<double>(T_world_from_node(2, 3))
        };
        Eigen::Quaternionf q_total(T_world_from_node.block<3, 3>(0, 0));
        q_total.normalize();
        tf.rotation = {
            static_cast<double>(q_total.x()),
            static_cast<double>(q_total.y()),
            static_cast<double>(q_total.z()),
            static_cast<double>(q_total.w())
        };
        tf.matrix4x4.resize(16);
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                tf.matrix4x4[r * 4 + c] = static_cast<double>(T_world_from_node(r, c));
            }
        }
        unaligned_node.snapped_transform = tf;
        unaligned_node.alignment_state = CSlamAlignmentState::OVERLAP_LOCKED;
        unaligned_node.matched_anchor_node_uuid = primary_anchor_uuid;
        syncWorldTransformMatrix(unaligned_node);
        return true;
    }

    return false;
}

std::vector<schemas::PointXYZI> CSlamEngine::getMergedPointCloud(bool differentiate_scanner_colors) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<schemas::PointXYZI> merged;

    size_t total_pts = 0;
    for (const auto& uuid : node_order_) {
        total_pts += nodes_.at(uuid).raw_points.size();
    }
    merged.reserve(total_pts);

    for (const auto& uuid : node_order_) {
        const auto& node = nodes_.at(uuid);
        if (node.raw_points.empty()) continue;

        Eigen::Matrix4f T_world = getEffectiveWorldMatrix(node);
        Eigen::Matrix3f R = T_world.block<3, 3>(0, 0);
        Eigen::Vector3f t = T_world.block<3, 1>(0, 3);

        for (const auto& pt : node.raw_points) {
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
            Eigen::Vector3f pw = R * Eigen::Vector3f(pt.x, pt.y, pt.z) + t;

            schemas::PointXYZI out_pt = pt;
            out_pt.x = pw.x();
            out_pt.y = pw.y();
            out_pt.z = pw.z();

            if (differentiate_scanner_colors) {
                // Modulate distinct scanner color subtly by height/intensity so 3D surface relief is crisp
                float shade = std::clamp(0.78f + 0.12f * std::sin(pw.z() * 1.5f) + 0.10f * (pt.intensity / 255.0f), 0.65f, 1.0f);
                out_pt.r = static_cast<uint8_t>(std::clamp(node.distinct_color.r * shade, 0.0f, 255.0f));
                out_pt.g = static_cast<uint8_t>(std::clamp(node.distinct_color.g * shade, 0.0f, 255.0f));
                out_pt.b = static_cast<uint8_t>(std::clamp(node.distinct_color.b * shade, 0.0f, 255.0f));
                out_pt.has_color = true;
            }
            merged.push_back(out_pt);
        }
    }

    return merged;
}

std::vector<schemas::PointXYZI> CSlamEngine::getScannerPointCloud(const std::string& node_uuid, bool differentiate_scanner_colors) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_uuid);
    if (it == nodes_.end()) return {};

    const auto& node = it->second;
    std::vector<schemas::PointXYZI> out;
    out.reserve(node.raw_points.size());

    for (const auto& pt : node.raw_points) {
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
        schemas::PointXYZI out_pt = pt;
        if (differentiate_scanner_colors) {
            float shade = std::clamp(0.78f + 0.12f * std::sin(pt.z * 1.5f) + 0.10f * (pt.intensity / 255.0f), 0.65f, 1.0f);
            out_pt.r = static_cast<uint8_t>(std::clamp(node.distinct_color.r * shade, 0.0f, 255.0f));
            out_pt.g = static_cast<uint8_t>(std::clamp(node.distinct_color.g * shade, 0.0f, 255.0f));
            out_pt.b = static_cast<uint8_t>(std::clamp(node.distinct_color.b * shade, 0.0f, 255.0f));
            out_pt.has_color = true;
        }
        out.push_back(out_pt);
    }
    return out;
}

std::vector<schemas::Pose3D> CSlamEngine::getMergedTrajectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<schemas::Pose3D> merged;
    for (const auto& uuid : node_order_) {
        const auto& node = nodes_.at(uuid);
        if (node.trajectory.empty()) continue;

        Eigen::Matrix4f T_world = getEffectiveWorldMatrix(node);
        Eigen::Matrix3f R = T_world.block<3, 3>(0, 0);
        Eigen::Vector3f t = T_world.block<3, 1>(0, 3);

        for (const auto& pose : node.trajectory) {
            Eigen::Vector3f p(static_cast<float>(pose.position.x),
                              static_cast<float>(pose.position.y),
                              static_cast<float>(pose.position.z));
            Eigen::Vector3f pw = R * p + t;
            schemas::Pose3D out_pose = pose;
            out_pose.position.x = pw.x();
            out_pose.position.y = pw.y();
            out_pose.position.z = pw.z();
            merged.push_back(out_pose);
        }
    }
    return merged;
}

size_t CSlamEngine::getScannerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nodes_.size();
}

size_t CSlamEngine::getSnappedScannerCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& kv : nodes_) {
        if (kv.second.alignment_state == CSlamAlignmentState::ANCHOR ||
            kv.second.alignment_state == CSlamAlignmentState::OVERLAP_LOCKED) {
            count++;
        }
    }
    return count;
}

std::vector<CSlamNodeSubmap> CSlamEngine::getAllNodeSubmaps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CSlamNodeSubmap> list;
    list.reserve(node_order_.size());
    for (const auto& uuid : node_order_) {
        list.push_back(nodes_.at(uuid));
    }
    return list;
}

bool CSlamEngine::getNodeSubmap(const std::string& node_uuid, CSlamNodeSubmap& out) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_uuid);
    if (it == nodes_.end()) return false;
    out = it->second;
    return true;
}

nlohmann::json CSlamEngine::serializeSubmapMessage(const std::string& node_uuid, size_t max_points) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = nodes_.find(node_uuid);
    if (it == nodes_.end()) return nlohmann::json::object();

    const auto& node = it->second;
    nlohmann::json pts_json = nlohmann::json::array();
    size_t n = node.raw_points.size();
    size_t step = (max_points > 0 && n > max_points) ? (n / max_points) : 1;
    if (step == 0) step = 1;

    for (size_t i = 0; i < n; i += step) {
        const auto& p = node.raw_points[i];
        pts_json.push_back({p.x, p.y, p.z, p.intensity, p.r, p.g, p.b, p.has_color ? 1 : 0});
    }

    nlohmann::json msg = nlohmann::json::object();
    msg["node_uuid"] = node.node_uuid;
    msg["display_name"] = node.display_name;
    msg["scanner_index"] = node.scanner_index;
    msg["imu_orientation"] = nlohmann::json(node.imu_orientation);
    msg["current_local_pose"] = nlohmann::json(node.current_local_pose);
    msg["points"] = pts_json;
    return msg;
}

bool CSlamEngine::ingestSerializedSubmapMessage(const nlohmann::json& payload, bool auto_evaluate_overlap) {
    if (!payload.is_object() || !payload.contains("node_uuid")) return false;

    std::string uuid = payload.value("node_uuid", "");
    std::string name = payload.value("display_name", "");
    schemas::Quaternion imu_q{0.0, 0.0, 0.0, 1.0};
    if (payload.contains("imu_orientation") && payload["imu_orientation"].is_object()) {
        imu_q = payload["imu_orientation"].get<schemas::Quaternion>();
    }
    schemas::Pose3D pose;
    if (payload.contains("current_local_pose") && payload["current_local_pose"].is_object()) {
        pose = payload["current_local_pose"].get<schemas::Pose3D>();
    }

    std::vector<schemas::PointXYZI> pts;
    if (payload.contains("points") && payload["points"].is_array()) {
        const auto& arr = payload["points"];
        pts.reserve(arr.size());
        for (const auto& item : arr) {
            if (item.is_array() && item.size() >= 4) {
                schemas::PointXYZI pt;
                pt.x = item[0].get<float>();
                pt.y = item[1].get<float>();
                pt.z = item[2].get<float>();
                pt.intensity = item[3].get<float>();
                if (item.size() >= 8) {
                    pt.r = static_cast<uint8_t>(item[4].get<int>());
                    pt.g = static_cast<uint8_t>(item[5].get<int>());
                    pt.b = static_cast<uint8_t>(item[6].get<int>());
                    pt.has_color = (item[7].get<int>() != 0);
                }
                pts.push_back(pt);
            }
        }
    }

    ingestSubmap(uuid, name, pts, imu_q, pose, true, auto_evaluate_overlap);
    return true;
}

nlohmann::json CSlamEngine::serializeAlignmentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json nodes_arr = nlohmann::json::array();
    for (const auto& uuid : node_order_) {
        const auto& n = nodes_.at(uuid);
        nlohmann::json item = nlohmann::json::object();
        item["node_uuid"] = n.node_uuid;
        item["display_name"] = n.display_name;
        item["scanner_index"] = n.scanner_index;
        item["color_hex"] = n.distinct_color.hex;
        item["color_rgb"] = nlohmann::json::array({n.distinct_color.r, n.distinct_color.g, n.distinct_color.b});
        item["alignment_state"] = to_string(n.alignment_state);
        item["imu_orientation"] = nlohmann::json(n.imu_orientation);
        item["arbitrary_offset_m"] = nlohmann::json(n.arbitrary_offset_m);
        item["snapped_transform"] = n.snapped_transform.to_json();
        item["overlap_ratio"] = n.overlap_ratio;
        item["inlier_rmse_m"] = n.inlier_rmse_m;
        item["matched_anchor_node_uuid"] = n.matched_anchor_node_uuid;
        nodes_arr.push_back(item);
    }
    nlohmann::json out = nlohmann::json::object();
    out["separation_distance_m"] = separation_distance_m_;
    out["nodes"] = nodes_arr;
    return out;
}

bool CSlamEngine::applySerializedAlignmentState(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("nodes") || !payload["nodes"].is_array()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (payload.contains("separation_distance_m")) {
        separation_distance_m_ = payload.value("separation_distance_m", 12.0);
    }

    for (const auto& item : payload["nodes"]) {
        if (!item.is_object()) continue;
        std::string uuid = item.value("node_uuid", "");
        if (uuid.empty()) continue;

        auto it = nodes_.find(uuid);
        if (it == nodes_.end()) {
            size_t idx = item.value("scanner_index", node_order_.size());
            node_order_.push_back(uuid);
            CSlamNodeSubmap sub;
            sub.node_uuid = uuid;
            sub.display_name = item.value("display_name", "Scanner-" + std::to_string(idx + 1));
            sub.scanner_index = idx;
            sub.distinct_color = getScannerDistinctColor(idx);
            sub.color_r = sub.distinct_color.r;
            sub.color_g = sub.distinct_color.g;
            sub.color_b = sub.distinct_color.b;
            nodes_[uuid] = std::move(sub);
            it = nodes_.find(uuid);
        }

        auto& node = it->second;
        node.scanner_index = item.value("scanner_index", node.scanner_index);
        node.distinct_color = getScannerDistinctColor(node.scanner_index);
        node.color_r = node.distinct_color.r;
        node.color_g = node.distinct_color.g;
        node.color_b = node.distinct_color.b;
        node.alignment_state = cslam_alignment_state_from_string(item.value("alignment_state", "UNALIGNED_IMU_OFFSET"));
        if (item.contains("imu_orientation") && item["imu_orientation"].is_object()) {
            node.imu_orientation = item["imu_orientation"].get<schemas::Quaternion>();
        }
        if (item.contains("arbitrary_offset_m") && item["arbitrary_offset_m"].is_object()) {
            node.arbitrary_offset_m = item["arbitrary_offset_m"].get<schemas::Vector3D>();
        }
        if (item.contains("snapped_transform") && item["snapped_transform"].is_object()) {
            node.snapped_transform = schemas::Transform3D::from_json(item["snapped_transform"]);
        }
        node.overlap_ratio = item.value("overlap_ratio", node.overlap_ratio);
        node.inlier_rmse_m = item.value("inlier_rmse_m", node.inlier_rmse_m);
        node.matched_anchor_node_uuid = item.value("matched_anchor_node_uuid", node.matched_anchor_node_uuid);
        syncWorldTransformMatrix(node);
    }
    return true;
}

} // namespace av::core::multi_node
