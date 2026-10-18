#include "core/processing/point_cloud_processor.hpp"
#include "core/logging/logger.hpp"

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/kdtree/kdtree_flann.h>

#include <Eigen/Dense>
#include <Eigen/Geometry>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <unordered_map>

namespace av::core::processing {

namespace fs = std::filesystem;

namespace {

struct VoxelKey {
    int64_t x;
    int64_t y;
    int64_t z;

    bool operator==(const VoxelKey& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct VoxelKeyHash {
    std::size_t operator()(const VoxelKey& k) const noexcept {
        uint64_t h1 = static_cast<uint64_t>(k.x) * 73856093ULL;
        uint64_t h2 = static_cast<uint64_t>(k.y) * 19349663ULL;
        uint64_t h3 = static_cast<uint64_t>(k.z) * 83492791ULL;
        return static_cast<std::size_t>(h1 ^ h2 ^ h3);
    }
};

struct VoxelAccum {
    double sum_x{0.0};
    double sum_y{0.0};
    double sum_z{0.0};
    double sum_i{0.0};
    uint64_t sum_r{0};
    uint64_t sum_g{0};
    uint64_t sum_b{0};
    uint64_t count{0};
    uint64_t color_count{0};
};

inline bool isFinitePoint(const schemas::PointXYZI& pt) {
    return std::isfinite(pt.x) && std::isfinite(pt.y) && std::isfinite(pt.z);
}

} // namespace

bool PointCloudProcessor::loadPcd(const std::string& filepath, std::vector<schemas::PointXYZI>& out_points) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        AV_LOG_ERROR("PointCloudProcessor", "LoadError", "Cannot open PCD file: " + filepath);
        return false;
    }

    out_points.clear();
    std::string line;
    bool is_binary = false;
    bool has_rgb_field = false;
    bool has_intensity_field = false;
    size_t declared_points = 0;

    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.rfind("FIELDS", 0) == 0) {
            if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                has_rgb_field = true;
            }
            if (line.find("intensity") != std::string::npos) {
                has_intensity_field = true;
            }
        } else if (line.rfind("POINTS", 0) == 0) {
            try {
                declared_points = std::stoull(line.substr(7));
            } catch (...) {}
        } else if (line.rfind("DATA", 0) == 0) {
            if (line.find("binary") != std::string::npos) {
                is_binary = true;
            }
            break;
        }
    }

    if (declared_points > 0 && declared_points < 50000000) {
        out_points.reserve(declared_points);
    }

    if (is_binary) {
        for (size_t i = 0; i < declared_points && ifs.good(); ++i) {
            schemas::PointXYZI pt;
            ifs.read(reinterpret_cast<char*>(&pt.x), sizeof(float));
            ifs.read(reinterpret_cast<char*>(&pt.y), sizeof(float));
            ifs.read(reinterpret_cast<char*>(&pt.z), sizeof(float));
            if (has_rgb_field) {
                uint32_t rgb_packed = 0;
                ifs.read(reinterpret_cast<char*>(&rgb_packed), sizeof(uint32_t));
                pt.r = static_cast<uint8_t>((rgb_packed >> 16) & 0xFF);
                pt.g = static_cast<uint8_t>((rgb_packed >> 8) & 0xFF);
                pt.b = static_cast<uint8_t>(rgb_packed & 0xFF);
                pt.has_color = true;
            }
            if (has_intensity_field) {
                ifs.read(reinterpret_cast<char*>(&pt.intensity), sizeof(float));
            } else {
                pt.intensity = 1.0f;
            }
            if (ifs.good() && isFinitePoint(pt)) {
                out_points.push_back(pt);
            }
        }
        return !out_points.empty();
    }

    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        const char* ptr = line.c_str();
        char* end = nullptr;
        schemas::PointXYZI pt;
        pt.x = std::strtof(ptr, &end);
        if (end == ptr) continue;
        ptr = end;
        pt.y = std::strtof(ptr, &end);
        if (end == ptr) continue;
        ptr = end;
        pt.z = std::strtof(ptr, &end);
        if (end == ptr) continue;
        ptr = end;

        if (!isFinitePoint(pt)) continue;

        if (has_rgb_field) {
            while (*ptr == ' ' || *ptr == '\t') ++ptr;
            if (*ptr == '\0' || *ptr == '\r' || *ptr == '\n') continue;
            const char* tok_start = ptr;
            bool is_float_tok = false;
            while (*ptr != '\0' && *ptr != ' ' && *ptr != '\t' && *ptr != '\r' && *ptr != '\n') {
                if (*ptr == '.' || *ptr == 'e' || *ptr == 'E') {
                    is_float_tok = true;
                }
                ++ptr;
            }
            uint32_t rgb_packed = 0;
            if (is_float_tok) {
                float rgb_f = std::strtof(tok_start, nullptr);
                std::memcpy(&rgb_packed, &rgb_f, sizeof(uint32_t));
            } else {
                rgb_packed = static_cast<uint32_t>(std::strtoul(tok_start, nullptr, 10));
            }
            pt.r = static_cast<uint8_t>((rgb_packed >> 16) & 0xFF);
            pt.g = static_cast<uint8_t>((rgb_packed >> 8) & 0xFF);
            pt.b = static_cast<uint8_t>(rgb_packed & 0xFF);
            pt.has_color = true;

            float inten = std::strtof(ptr, &end);
            pt.intensity = (end != ptr && std::isfinite(inten)) ? inten : 1.0f;
            out_points.push_back(pt);
        } else {
            float inten = std::strtof(ptr, &end);
            pt.intensity = (end != ptr && std::isfinite(inten)) ? inten : 1.0f;
            uint8_t iv = static_cast<uint8_t>(std::clamp(
                pt.intensity <= 1.0f ? pt.intensity * 255.0f : pt.intensity, 50.0f, 255.0f));
            pt.r = iv;
            pt.g = iv;
            pt.b = iv;
            pt.has_color = false;
            out_points.push_back(pt);
        }
    }

    return !out_points.empty();
}

bool PointCloudProcessor::savePcd(const std::string& filepath, const std::vector<schemas::PointXYZI>& points, bool binary) {
    fs::path p(filepath);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }

    std::ofstream ofs(filepath, binary ? (std::ios::out | std::ios::binary) : std::ios::out);
    if (!ofs.is_open()) {
        AV_LOG_ERROR("PointCloudProcessor", "SaveError", "Cannot open output PCD file: " + filepath);
        return false;
    }

    ofs << "# .PCD v0.7 - Point Cloud Data file format\n"
        << "VERSION 0.7\n"
        << "FIELDS x y z rgb intensity\n"
        << "SIZE 4 4 4 4 4\n"
        << "TYPE F F F U F\n"
        << "COUNT 1 1 1 1 1\n"
        << "WIDTH " << points.size() << "\n"
        << "HEIGHT 1\n"
        << "VIEWPOINT 0 0 0 1 0 0 0\n"
        << "POINTS " << points.size() << "\n"
        << "DATA " << (binary ? "binary\n" : "ascii\n");

    if (binary) {
        for (const auto& pt : points) {
            uint32_t rgb_packed = (static_cast<uint32_t>(pt.r) << 16) |
                                  (static_cast<uint32_t>(pt.g) << 8) |
                                  static_cast<uint32_t>(pt.b);
            ofs.write(reinterpret_cast<const char*>(&pt.x), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&pt.y), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&pt.z), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&rgb_packed), sizeof(uint32_t));
            ofs.write(reinterpret_cast<const char*>(&pt.intensity), sizeof(float));
        }
    } else {
        std::string chunk;
        chunk.reserve(65536);
        char line_buf[128];
        for (const auto& pt : points) {
            uint32_t rgb_packed = (static_cast<uint32_t>(pt.r) << 16) |
                                  (static_cast<uint32_t>(pt.g) << 8) |
                                  static_cast<uint32_t>(pt.b);
            int n = std::snprintf(line_buf, sizeof(line_buf), "%.5g %.5g %.5g %u %.4g\n",
                                  pt.x, pt.y, pt.z, rgb_packed, pt.intensity);
            if (n > 0) {
                chunk.append(line_buf, static_cast<size_t>(n));
                if (chunk.size() >= 60000) {
                    ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                    chunk.clear();
                }
            }
        }
        if (!chunk.empty()) {
            ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
    }

    ofs.close();
    return true;
}

std::vector<schemas::PointXYZI> PointCloudProcessor::voxelDownsample(
    const std::vector<schemas::PointXYZI>& input,
    float leaf_size_m) {
    if (input.empty() || leaf_size_m <= 1e-4f) {
        return input;
    }

    const double inv_leaf = 1.0 / static_cast<double>(leaf_size_m);
    std::unordered_map<VoxelKey, VoxelAccum, VoxelKeyHash> grid;
    grid.reserve(input.size() / 2 + 16);

    for (const auto& pt : input) {
        if (!isFinitePoint(pt)) continue;
        VoxelKey key{
            static_cast<int64_t>(std::floor(static_cast<double>(pt.x) * inv_leaf)),
            static_cast<int64_t>(std::floor(static_cast<double>(pt.y) * inv_leaf)),
            static_cast<int64_t>(std::floor(static_cast<double>(pt.z) * inv_leaf))
        };
        auto& acc = grid[key];
        acc.sum_x += pt.x;
        acc.sum_y += pt.y;
        acc.sum_z += pt.z;
        acc.sum_i += pt.intensity;
        acc.sum_r += pt.r;
        acc.sum_g += pt.g;
        acc.sum_b += pt.b;
        acc.count += 1;
        if (pt.has_color) {
            acc.color_count += 1;
        }
    }

    std::vector<schemas::PointXYZI> output;
    output.reserve(grid.size());
    for (const auto& kv : grid) {
        const auto& acc = kv.second;
        if (acc.count == 0) continue;
        const double inv_n = 1.0 / static_cast<double>(acc.count);
        schemas::PointXYZI pt;
        pt.x = static_cast<float>(acc.sum_x * inv_n);
        pt.y = static_cast<float>(acc.sum_y * inv_n);
        pt.z = static_cast<float>(acc.sum_z * inv_n);
        pt.intensity = static_cast<float>(acc.sum_i * inv_n);
        pt.r = static_cast<uint8_t>(std::clamp<uint64_t>(acc.sum_r / acc.count, 0, 255));
        pt.g = static_cast<uint8_t>(std::clamp<uint64_t>(acc.sum_g / acc.count, 0, 255));
        pt.b = static_cast<uint8_t>(std::clamp<uint64_t>(acc.sum_b / acc.count, 0, 255));
        pt.has_color = (acc.color_count > 0);
        output.push_back(pt);
    }

    return output;
}

std::vector<schemas::PointXYZI> PointCloudProcessor::removeStatisticalOutliers(
    const std::vector<schemas::PointXYZI>& input,
    int mean_k,
    double std_dev_mul_thresh) {
    if (input.size() <= static_cast<size_t>( std::max(2, mean_k) )) {
        return input;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr pcl_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl_cloud->points.reserve(input.size());
    std::vector<size_t> valid_indices;
    valid_indices.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (!isFinitePoint(input[i])) continue;
        pcl::PointXYZ p;
        p.x = input[i].x;
        p.y = input[i].y;
        p.z = input[i].z;
        pcl_cloud->points.push_back(p);
        valid_indices.push_back(i);
    }
    pcl_cloud->width = static_cast<uint32_t>(pcl_cloud->points.size());
    pcl_cloud->height = 1;
    pcl_cloud->is_dense = true;

    const size_t N = pcl_cloud->points.size();
    const int k_query = std::min<int>(mean_k + 1, static_cast<int>(N));
    if (k_query <= 1) {
        return input;
    }

    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;
    kdtree.setInputCloud(pcl_cloud);

    std::vector<double> mean_distances(N, 0.0);

    #pragma omp parallel for schedule(static)
    for (size_t i = 0; i < N; ++i) {
        std::vector<int> nn_indices(k_query);
        std::vector<float> nn_sq_dists(k_query);
        int found = kdtree.nearestKSearch(pcl_cloud->points[i], k_query, nn_indices, nn_sq_dists);
        if (found > 1) {
            double dist_sum = 0.0;
            int valid_k = 0;
            for (int j = 1; j < found; ++j) {
                dist_sum += std::sqrt(std::max(0.0f, nn_sq_dists[j]));
                valid_k++;
            }
            mean_distances[i] = (valid_k > 0) ? (dist_sum / valid_k) : 0.0;
        }
    }

    double sum = 0.0;
    double sq_sum = 0.0;
    for (double d : mean_distances) {
        sum += d;
        sq_sum += d * d;
    }
    const double mean = sum / static_cast<double>(N);
    const double variance = std::max(0.0, (sq_sum / static_cast<double>(N)) - (mean * mean));
    const double stddev = std::sqrt(variance);
    const double distance_threshold = mean + std_dev_mul_thresh * stddev;

    std::vector<schemas::PointXYZI> output;
    output.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        if (mean_distances[i] <= distance_threshold) {
            output.push_back(input[valid_indices[i]]);
        }
    }

    return output;
}

std::vector<schemas::PointXYZI> PointCloudProcessor::removeRadiusOutliers(
    const std::vector<schemas::PointXYZI>& input,
    double radius_m,
    int min_neighbors) {
    if (input.empty() || radius_m <= 1e-4 || min_neighbors <= 0) {
        return input;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr pcl_cloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl_cloud->points.reserve(input.size());
    std::vector<size_t> valid_indices;
    valid_indices.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (!isFinitePoint(input[i])) continue;
        pcl::PointXYZ p;
        p.x = input[i].x;
        p.y = input[i].y;
        p.z = input[i].z;
        pcl_cloud->points.push_back(p);
        valid_indices.push_back(i);
    }
    pcl_cloud->width = static_cast<uint32_t>(pcl_cloud->points.size());
    pcl_cloud->height = 1;
    pcl_cloud->is_dense = true;

    const size_t N = pcl_cloud->points.size();
    if (N == 0) return {};

    pcl::KdTreeFLANN<pcl::PointXYZ> kdtree;
    kdtree.setInputCloud(pcl_cloud);

    std::vector<uint8_t> keep(N, 0);
    const int required_found = min_neighbors + 1; // +1 for query point itself

    #pragma omp parallel for schedule(dynamic, 256)
    for (size_t i = 0; i < N; ++i) {
        std::vector<int> indices;
        std::vector<float> sq_dists;
        int found = kdtree.radiusSearch(pcl_cloud->points[i], radius_m, indices, sq_dists, required_found);
        if (found >= required_found) {
            keep[i] = 1;
        }
    }

    std::vector<schemas::PointXYZI> output;
    output.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        if (keep[i]) {
            output.push_back(input[valid_indices[i]]);
        }
    }

    return output;
}

GroundPlaneResult PointCloudProcessor::detectAndAlignGroundPlane(
    std::vector<schemas::PointXYZI>& in_out_points,
    double distance_threshold_m,
    int max_iterations,
    double max_tilt_deg,
    bool align_to_z_zero,
    bool remove_ground_inliers) {
    GroundPlaneResult res;
    if (in_out_points.size() < 30) {
        return res;
    }

    // Focus candidate sampling on the lower 60% of the Z range to reliably lock onto floor rather than ceiling
    std::vector<float> z_vals;
    z_vals.reserve(in_out_points.size());
    for (const auto& pt : in_out_points) {
        if (isFinitePoint(pt)) z_vals.push_back(pt.z);
    }
    if (z_vals.size() < 30) return res;

    size_t z_cutoff_idx = static_cast<size_t>(z_vals.size() * 0.60);
    std::nth_element(z_vals.begin(), z_vals.begin() + z_cutoff_idx, z_vals.end());
    float z_cutoff = z_vals[z_cutoff_idx];

    std::vector<size_t> candidate_indices;
    candidate_indices.reserve(in_out_points.size());
    for (size_t i = 0; i < in_out_points.size(); ++i) {
        if (isFinitePoint(in_out_points[i]) && in_out_points[i].z <= z_cutoff) {
            candidate_indices.push_back(i);
        }
    }
    if (candidate_indices.size() < 10) {
        candidate_indices.clear();
        for (size_t i = 0; i < in_out_points.size(); ++i) {
            if (isFinitePoint(in_out_points[i])) candidate_indices.push_back(i);
        }
    }

    // Subsample evaluation cloud if large so RANSAC stays sub-15ms
    std::vector<size_t> eval_indices;
    const size_t max_eval = 15000;
    size_t step = std::max<size_t>(1, candidate_indices.size() / max_eval);
    for (size_t i = 0; i < candidate_indices.size(); i += step) {
        eval_indices.push_back(candidate_indices[i]);
    }

    const double min_cos_tilt = std::cos(max_tilt_deg * M_PI / 180.0);
    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> dist(0, candidate_indices.size() - 1);

    size_t best_inliers = 0;
    Eigen::Vector3d best_normal(0.0, 0.0, 1.0);
    double best_d = 0.0;

    for (int iter = 0; iter < max_iterations; ++iter) {
        size_t i1 = candidate_indices[dist(rng)];
        size_t i2 = candidate_indices[dist(rng)];
        size_t i3 = candidate_indices[dist(rng)];
        if (i1 == i2 || i1 == i3 || i2 == i3) continue;

        Eigen::Vector3d p1(in_out_points[i1].x, in_out_points[i1].y, in_out_points[i1].z);
        Eigen::Vector3d p2(in_out_points[i2].x, in_out_points[i2].y, in_out_points[i2].z);
        Eigen::Vector3d p3(in_out_points[i3].x, in_out_points[i3].y, in_out_points[i3].z);

        Eigen::Vector3d n = (p2 - p1).cross(p3 - p1);
        double norm = n.norm();
        if (norm < 1e-6) continue;
        n /= norm;
        if (n.z() < 0.0) n = -n; // Canonical +Z up

        // Check tilt constraint relative to +Z up
        if (n.z() < min_cos_tilt) continue;

        double d = -n.dot(p1);

        size_t inliers = 0;
        for (size_t idx : eval_indices) {
            const auto& pt = in_out_points[idx];
            double dist_plane = std::abs(n.x() * pt.x + n.y() * pt.y + n.z() * pt.z + d);
            if (dist_plane <= distance_threshold_m) {
                inliers++;
            }
        }

        if (inliers > best_inliers) {
            best_inliers = inliers;
            best_normal = n;
            best_d = d;
        }
    }

    if (best_inliers < 10) {
        return res;
    }

    // Refine plane normal & offset using PCA / SVD on all inliers in candidate_indices
    Eigen::Vector3d centroid(0.0, 0.0, 0.0);
    std::vector<size_t> refined_inliers;
    refined_inliers.reserve(candidate_indices.size());
    for (size_t idx : candidate_indices) {
        const auto& pt = in_out_points[idx];
        double dist_plane = std::abs(best_normal.x() * pt.x + best_normal.y() * pt.y + best_normal.z() * pt.z + best_d);
        if (dist_plane <= distance_threshold_m) {
            refined_inliers.push_back(idx);
            centroid += Eigen::Vector3d(pt.x, pt.y, pt.z);
        }
    }

    if (refined_inliers.size() >= 10) {
        centroid /= static_cast<double>(refined_inliers.size());
        Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
        for (size_t idx : refined_inliers) {
            Eigen::Vector3d diff(
                in_out_points[idx].x - centroid.x(),
                in_out_points[idx].y - centroid.y(),
                in_out_points[idx].z - centroid.z());
            cov += diff * diff.transpose();
        }
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(cov);
        Eigen::Vector3d pca_n = eig.eigenvectors().col(0); // smallest eigenvalue
        if (pca_n.z() < 0.0) pca_n = -pca_n;
        if (pca_n.z() >= min_cos_tilt) {
            best_normal = pca_n.normalized();
            best_d = -best_normal.dot(centroid);
        }
    }

    // Count full-cloud inliers
    std::vector<uint8_t> is_inlier(in_out_points.size(), 0);
    size_t total_inliers = 0;
    for (size_t i = 0; i < in_out_points.size(); ++i) {
        const auto& pt = in_out_points[i];
        double dist_plane = std::abs(best_normal.x() * pt.x + best_normal.y() * pt.y + best_normal.z() * pt.z + best_d);
        if (dist_plane <= distance_threshold_m) {
            is_inlier[i] = 1;
            total_inliers++;
        }
    }

    res.found = true;
    res.plane_coeffs[0] = best_normal.x();
    res.plane_coeffs[1] = best_normal.y();
    res.plane_coeffs[2] = best_normal.z();
    res.plane_coeffs[3] = best_d;
    res.inlier_count = total_inliers;
    res.inlier_ratio = static_cast<double>(total_inliers) / static_cast<double>(in_out_points.size());
    res.tilt_deg = std::acos(std::clamp(best_normal.z(), -1.0, 1.0)) * 180.0 / M_PI;
    res.floor_height_m = -best_d / std::max(1e-6, best_normal.z());

    // Compute rotation matrix that aligns best_normal -> [0, 0, 1]^T
    Eigen::Vector3d z_axis(0.0, 0.0, 1.0);
    Eigen::Quaterniond q_align = Eigen::Quaterniond::FromTwoVectors(best_normal, z_axis);
    Eigen::Matrix3d R_align = q_align.toRotationMatrix();

    // After rotating by R_align, a point p on the plane satisfies n^T p + d = 0 => (R n)^T (R p) + d = 0 => z_new + d = 0 => z_new = -d.
    // So shifting Z by +d places the ground plane at Z = 0.0!
    Eigen::Matrix4d T_align = Eigen::Matrix4d::Identity();
    T_align.block<3, 3>(0, 0) = R_align;
    T_align(2, 3) = best_d;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            res.alignment_transform[r][c] = T_align(r, c);
        }
    }

    if (remove_ground_inliers) {
        std::vector<schemas::PointXYZI> filtered;
        filtered.reserve(in_out_points.size() - total_inliers);
        for (size_t i = 0; i < in_out_points.size(); ++i) {
            if (!is_inlier[i]) {
                filtered.push_back(in_out_points[i]);
            }
        }
        in_out_points.swap(filtered);
    }

    if (align_to_z_zero) {
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < in_out_points.size(); ++i) {
            auto& pt = in_out_points[i];
            Eigen::Vector3d p(pt.x, pt.y, pt.z);
            Eigen::Vector3d p_aligned = R_align * p;
            pt.x = static_cast<float>(p_aligned.x());
            pt.y = static_cast<float>(p_aligned.y());
            pt.z = static_cast<float>(p_aligned.z() + best_d);
        }
    }

    return res;
}

void PointCloudProcessor::computeBoundingBox(
    const std::vector<schemas::PointXYZI>& points,
    schemas::Vector3D& out_min,
    schemas::Vector3D& out_max) {
    if (points.empty()) {
        out_min = {0.0, 0.0, 0.0};
        out_max = {0.0, 0.0, 0.0};
        return;
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double min_z = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    double max_z = std::numeric_limits<double>::lowest();

    for (const auto& pt : points) {
        if (!isFinitePoint(pt)) continue;
        if (pt.x < min_x) min_x = pt.x;
        if (pt.y < min_y) min_y = pt.y;
        if (pt.z < min_z) min_z = pt.z;
        if (pt.x > max_x) max_x = pt.x;
        if (pt.y > max_y) max_y = pt.y;
        if (pt.z > max_z) max_z = pt.z;
    }

    out_min = {min_x, min_y, min_z};
    out_max = {max_x, max_y, max_z};
}

ProcessingStats PointCloudProcessor::runCleanupPipeline(
    const std::vector<schemas::PointXYZI>& input,
    std::vector<schemas::PointXYZI>& output,
    const CleanupParams& params) {
    auto start_time = std::chrono::steady_clock::now();
    ProcessingStats stats;
    stats.input_point_count = input.size();

    output = input;
    size_t prev_count = output.size();

    if (params.enable_voxel_downsample && params.voxel_leaf_size_m > 0.001f) {
        output = voxelDownsample(output, params.voxel_leaf_size_m);
        stats.voxel_removed_count = (prev_count >= output.size()) ? (prev_count - output.size()) : 0;
        prev_count = output.size();
    }

    if (params.enable_sor && params.sor_mean_k >= 2) {
        output = removeStatisticalOutliers(output, params.sor_mean_k, params.sor_std_mul);
        stats.sor_removed_count = (prev_count >= output.size()) ? (prev_count - output.size()) : 0;
        prev_count = output.size();
    }

    if (params.enable_ror && params.ror_radius_m > 0.001 && params.ror_min_neighbors >= 1) {
        output = removeRadiusOutliers(output, params.ror_radius_m, params.ror_min_neighbors);
        stats.ror_removed_count = (prev_count >= output.size()) ? (prev_count - output.size()) : 0;
        prev_count = output.size();
    }

    if (params.enable_ground_alignment) {
        stats.ground_plane = detectAndAlignGroundPlane(
            output,
            params.ground_dist_threshold_m,
            params.ground_max_iterations,
            params.ground_max_tilt_deg,
            params.align_ground_to_z_zero,
            params.remove_ground_plane);
        stats.ground_removed_count = (prev_count >= output.size()) ? (prev_count - output.size()) : 0;
    }

    stats.output_point_count = output.size();
    computeBoundingBox(output, stats.bbox_min, stats.bbox_max);

    auto end_time = std::chrono::steady_clock::now();
    stats.computation_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    AV_LOG_INFO("PointCloudProcessor",
                "Cleanup pipeline complete: in=" + std::to_string(stats.input_point_count) +
                ", out=" + std::to_string(stats.output_point_count) +
                ", sor_removed=" + std::to_string(stats.sor_removed_count) +
                ", ror_removed=" + std::to_string(stats.ror_removed_count) +
                ", vox_removed=" + std::to_string(stats.voxel_removed_count) +
                ", ground_removed=" + std::to_string(stats.ground_removed_count) +
                ", time=" + std::to_string(stats.computation_time_ms) + "ms");

    return stats;
}

ProcessingStats PointCloudProcessor::cleanRealtimeFast(
    const std::vector<schemas::PointXYZI>& input,
    std::vector<schemas::PointXYZI>& output,
    const CleanupParams& params) {
    auto start_time = std::chrono::steady_clock::now();
    ProcessingStats stats;
    stats.input_point_count = input.size();

    if (input.empty()) {
        output.clear();
        return stats;
    }

    std::vector<schemas::PointXYZI> working;
    if (params.enable_voxel_downsample && params.voxel_leaf_size_m > 0.001f) {
        working = voxelDownsample(input, params.voxel_leaf_size_m);
        stats.voxel_removed_count = (input.size() >= working.size()) ? (input.size() - working.size()) : 0;
    } else {
        working.reserve(input.size());
        for (const auto& pt : input) {
            if (isFinitePoint(pt)) working.push_back(pt);
        }
    }

    const size_t N = working.size();
    if (N > 8 && (params.enable_ror || params.enable_sor)) {
        const double ror_r = (params.enable_ror && params.ror_radius_m > 0.01) ? params.ror_radius_m : 0.12;
        const double cell_size = std::max(0.05, ror_r);
        const double inv_cell = 1.0 / cell_size;
        const float ror_r_sq = static_cast<float>(ror_r * ror_r);
        const float sor_r_sq = static_cast<float>((cell_size * 1.6) * (cell_size * 1.6));

        std::unordered_map<VoxelKey, int32_t, VoxelKeyHash> cell_head;
        cell_head.reserve(N / 2 + 16);
        std::vector<int32_t> next_pt(N, -1);
        std::vector<VoxelKey> pt_keys(N);

        for (size_t i = 0; i < N; ++i) {
            VoxelKey k{
                static_cast<int64_t>(std::floor(static_cast<double>(working[i].x) * inv_cell)),
                static_cast<int64_t>(std::floor(static_cast<double>(working[i].y) * inv_cell)),
                static_cast<int64_t>(std::floor(static_cast<double>(working[i].z) * inv_cell))
            };
            pt_keys[i] = k;
            auto it = cell_head.find(k);
            if (it == cell_head.end()) {
                cell_head.emplace(k, static_cast<int32_t>(i));
            } else {
                next_pt[i] = it->second;
                it->second = static_cast<int32_t>(i);
            }
        }

        std::vector<uint8_t> ror_keep(N, 1);
        std::vector<float> sor_mean_dist(N, 0.0f);
        const int min_nb = std::max(1, params.ror_min_neighbors);
        const int target_k = std::max(4, std::min(params.sor_mean_k, 16));
        const float isolated_penalty_dist = static_cast<float>(cell_size * 3.0);

        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < N; ++i) {
            const auto& pi = working[i];
            const VoxelKey base_k = pt_keys[i];
            int ror_count = 0;
            int sor_count = 0;
            float sor_dist_sum = 0.0f;

            for (int64_t dx = -1; dx <= 1; ++dx) {
                for (int64_t dy = -1; dy <= 1; ++dy) {
                    for (int64_t dz = -1; dz <= 1; ++dz) {
                        VoxelKey nk{base_k.x + dx, base_k.y + dy, base_k.z + dz};
                        auto it = cell_head.find(nk);
                        if (it == cell_head.end()) continue;
                        int32_t j = it->second;
                        int cell_visited = 0;
                        while (j != -1 && cell_visited < 24) {
                            if (static_cast<size_t>(j) != i) {
                                const auto& pj = working[j];
                                float ddx = pi.x - pj.x;
                                float ddy = pi.y - pj.y;
                                float ddz = pi.z - pj.z;
                                float sq_d = ddx * ddx + ddy * ddy + ddz * ddz;
                                if (sq_d <= ror_r_sq) {
                                    ++ror_count;
                                }
                                if (sq_d <= sor_r_sq && sor_count < target_k) {
                                    sor_dist_sum += std::sqrt(std::max(0.0f, sq_d));
                                    ++sor_count;
                                }
                            }
                            j = next_pt[j];
                            ++cell_visited;
                        }
                    }
                }
            }

            if (params.enable_ror && ror_count < min_nb) {
                ror_keep[i] = 0;
            }
            if (params.enable_sor) {
                sor_mean_dist[i] = (sor_count > 0)
                    ? (sor_dist_sum / static_cast<float>(sor_count))
                    : isolated_penalty_dist;
            }
        }

        float sor_thresh = std::numeric_limits<float>::max();
        if (params.enable_sor) {
            double sum = 0.0;
            double sq_sum = 0.0;
            size_t valid_n = 0;
            for (size_t i = 0; i < N; ++i) {
                if (!ror_keep[i]) continue;
                double d = sor_mean_dist[i];
                sum += d;
                sq_sum += d * d;
                ++valid_n;
            }
            if (valid_n > 4) {
                double mean = sum / static_cast<double>(valid_n);
                double var = std::max(0.0, (sq_sum / static_cast<double>(valid_n)) - (mean * mean));
                sor_thresh = static_cast<float>(mean + params.sor_std_mul * std::sqrt(var));
            }
        }

        std::vector<schemas::PointXYZI> filtered;
        filtered.reserve(N);
        size_t ror_rem = 0;
        size_t sor_rem = 0;
        for (size_t i = 0; i < N; ++i) {
            if (!ror_keep[i]) {
                ++ror_rem;
                continue;
            }
            if (params.enable_sor && sor_mean_dist[i] > sor_thresh) {
                ++sor_rem;
                continue;
            }
            filtered.push_back(working[i]);
        }
        stats.ror_removed_count = ror_rem;
        stats.sor_removed_count = sor_rem;
        working.swap(filtered);
    }

    if (params.enable_ground_alignment && working.size() >= 30) {
        size_t prev_count = working.size();
        stats.ground_plane = detectAndAlignGroundPlane(
            working,
            params.ground_dist_threshold_m,
            std::min(params.ground_max_iterations, 120),
            params.ground_max_tilt_deg,
            params.align_ground_to_z_zero,
            params.remove_ground_plane);
        stats.ground_removed_count = (prev_count >= working.size()) ? (prev_count - working.size()) : 0;
    }

    output.swap(working);
    stats.output_point_count = output.size();
    computeBoundingBox(output, stats.bbox_min, stats.bbox_max);

    auto end_time = std::chrono::steady_clock::now();
    stats.computation_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    return stats;
}

ExportFormat PointCloudProcessor::parseExportFormat(const std::string& format_str) {
    std::string s = format_str;
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    if (s.find("PLY") != std::string::npos && s.find("BIN") != std::string::npos) return ExportFormat::PLY_BINARY;
    if (s.find("PLY") != std::string::npos) return ExportFormat::PLY_ASCII;
    if (s.find("PCD") != std::string::npos && s.find("BIN") != std::string::npos) return ExportFormat::PCD_BINARY;
    if (s.find("PCD") != std::string::npos) return ExportFormat::PCD_ASCII;
    if (s.find("XYZ") != std::string::npos || s.find("PTS") != std::string::npos) return ExportFormat::XYZ_ASCII;
    if (s.find("OBJ") != std::string::npos || s.find("MESH") != std::string::npos) return ExportFormat::OBJ_MESH;
    return ExportFormat::PLY_ASCII;
}

std::string PointCloudProcessor::exportFormatExtension(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLY_ASCII:
        case ExportFormat::PLY_BINARY:
            return ".ply";
        case ExportFormat::PCD_ASCII:
        case ExportFormat::PCD_BINARY:
            return ".pcd";
        case ExportFormat::XYZ_ASCII:
            return ".xyz";
        case ExportFormat::OBJ_MESH:
            return ".obj";
    }
    return ".ply";
}

std::string PointCloudProcessor::exportFormatName(ExportFormat format) {
    switch (format) {
        case ExportFormat::PLY_ASCII: return "PLY (ASCII)";
        case ExportFormat::PLY_BINARY: return "PLY (Binary Little-Endian)";
        case ExportFormat::PCD_ASCII: return "PCD (ASCII)";
        case ExportFormat::PCD_BINARY: return "PCD (Binary)";
        case ExportFormat::XYZ_ASCII: return "XYZRGBI (ASCII)";
        case ExportFormat::OBJ_MESH: return "Wavefront OBJ (3D Surface Mesh)";
    }
    return "PLY (ASCII)";
}

ExportResult PointCloudProcessor::exportPointCloud(
    const std::vector<schemas::PointXYZI>& points,
    const std::string& output_path,
    ExportFormat format,
    float mesh_grid_resolution_m) {
    auto start_time = std::chrono::steady_clock::now();
    ExportResult res;
    res.file_path = output_path;
    res.format_name = exportFormatName(format);

    if (points.empty()) {
        res.error_message = "Cannot export empty point cloud.";
        return res;
    }

    fs::path p(output_path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }

    if (format == ExportFormat::PCD_ASCII || format == ExportFormat::PCD_BINARY) {
        bool ok = savePcd(output_path, points, format == ExportFormat::PCD_BINARY);
        if (!ok) {
            res.error_message = "Failed to write PCD file: " + output_path;
            return res;
        }
        res.success = true;
        res.exported_points = points.size();
    } else if (format == ExportFormat::PLY_ASCII) {
        std::ofstream ofs(output_path);
        if (!ofs.is_open()) {
            res.error_message = "Failed to open PLY file for writing: " + output_path;
            return res;
        }
        ofs << "ply\n"
            << "format ascii 1.0\n"
            << "comment Generated by AV Scan Milestone 12 PointCloudProcessor\n"
            << "element vertex " << points.size() << "\n"
            << "property float x\n"
            << "property float y\n"
            << "property float z\n"
            << "property uchar red\n"
            << "property uchar green\n"
            << "property uchar blue\n"
            << "property float intensity\n"
            << "end_header\n";

        for (const auto& pt : points) {
            ofs << pt.x << " " << pt.y << " " << pt.z << " "
                << static_cast<int>(pt.r) << " " << static_cast<int>(pt.g) << " " << static_cast<int>(pt.b) << " "
                << pt.intensity << "\n";
        }
        ofs.close();
        res.success = true;
        res.exported_points = points.size();
    } else if (format == ExportFormat::PLY_BINARY) {
        std::ofstream ofs(output_path, std::ios::out | std::ios::binary);
        if (!ofs.is_open()) {
            res.error_message = "Failed to open binary PLY file for writing: " + output_path;
            return res;
        }
        ofs << "ply\n"
            << "format binary_little_endian 1.0\n"
            << "comment Generated by AV Scan Milestone 12 PointCloudProcessor\n"
            << "element vertex " << points.size() << "\n"
            << "property float x\n"
            << "property float y\n"
            << "property float z\n"
            << "property uchar red\n"
            << "property uchar green\n"
            << "property uchar blue\n"
            << "property float intensity\n"
            << "end_header\n";

        for (const auto& pt : points) {
            ofs.write(reinterpret_cast<const char*>(&pt.x), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&pt.y), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&pt.z), sizeof(float));
            ofs.write(reinterpret_cast<const char*>(&pt.r), sizeof(uint8_t));
            ofs.write(reinterpret_cast<const char*>(&pt.g), sizeof(uint8_t));
            ofs.write(reinterpret_cast<const char*>(&pt.b), sizeof(uint8_t));
            ofs.write(reinterpret_cast<const char*>(&pt.intensity), sizeof(float));
        }
        ofs.close();
        res.success = true;
        res.exported_points = points.size();
    } else if (format == ExportFormat::XYZ_ASCII) {
        std::ofstream ofs(output_path);
        if (!ofs.is_open()) {
            res.error_message = "Failed to open XYZ file for writing: " + output_path;
            return res;
        }
        for (const auto& pt : points) {
            ofs << pt.x << " " << pt.y << " " << pt.z << " "
                << static_cast<int>(pt.r) << " " << static_cast<int>(pt.g) << " " << static_cast<int>(pt.b) << " "
                << pt.intensity << "\n";
        }
        ofs.close();
        res.success = true;
        res.exported_points = points.size();
    } else if (format == ExportFormat::OBJ_MESH) {
        // Reconstruct a clean 3D surface mesh via spatial voxel clustering + local neighborhood triangulation
        float cell = std::max(0.01f, mesh_grid_resolution_m);
        auto mesh_pts = voxelDownsample(points, cell);
        if (mesh_pts.empty()) mesh_pts = points;

        const double inv_cell = 1.0 / static_cast<double>(cell);
        std::unordered_map<VoxelKey, uint32_t, VoxelKeyHash> vox_to_idx;
        vox_to_idx.reserve(mesh_pts.size() * 2);

        for (uint32_t i = 0; i < mesh_pts.size(); ++i) {
            const auto& pt = mesh_pts[i];
            VoxelKey key{
                static_cast<int64_t>(std::floor(static_cast<double>(pt.x) * inv_cell)),
                static_cast<int64_t>(std::floor(static_cast<double>(pt.y) * inv_cell)),
                static_cast<int64_t>(std::floor(static_cast<double>(pt.z) * inv_cell))
            };
            vox_to_idx[key] = i; // 0-based vertex index
        }

        // Helper to find closest vertex in (kx, ky, kz) or (kx, ky, kz +/- 1) for surface continuity
        auto findSurfaceNeighbor = [&](int64_t kx, int64_t ky, int64_t kz, int axis, uint32_t& out_idx) -> bool {
            for (int delta = 0; delta <= 1; ++delta) {
                for (int sign = -1; sign <= 1; sign += 2) {
                    int d = delta * sign;
                    VoxelKey q{
                        kx + (axis == 0 ? d : 0),
                        ky + (axis == 1 ? d : 0),
                        kz + (axis == 2 ? d : 0)
                    };
                    auto it = vox_to_idx.find(q);
                    if (it != vox_to_idx.end()) {
                        out_idx = it->second;
                        return true;
                    }
                    if (delta == 0) break;
                }
            }
            return false;
        };

        struct Triangle {
            uint32_t a, b, c;
        };
        std::vector<Triangle> triangles;
        triangles.reserve(mesh_pts.size() * 2);

        const float max_edge_sq = (cell * 2.75f) * (cell * 2.75f);
        auto validTri = [&](uint32_t i0, uint32_t i1, uint32_t i2) -> bool {
            if (i0 == i1 || i1 == i2 || i0 == i2) return false;
            Eigen::Vector3f p0(mesh_pts[i0].x, mesh_pts[i0].y, mesh_pts[i0].z);
            Eigen::Vector3f p1(mesh_pts[i1].x, mesh_pts[i1].y, mesh_pts[i1].z);
            Eigen::Vector3f p2(mesh_pts[i2].x, mesh_pts[i2].y, mesh_pts[i2].z);
            if ((p1 - p0).squaredNorm() > max_edge_sq) return false;
            if ((p2 - p1).squaredNorm() > max_edge_sq) return false;
            if ((p0 - p2).squaredNorm() > max_edge_sq) return false;
            return (p1 - p0).cross(p2 - p0).squaredNorm() > 1e-10f;
        };

        for (const auto& kv : vox_to_idx) {
            const VoxelKey& k = kv.first;
            uint32_t v00 = kv.second;

            // XY plane quads (floors/ceilings)
            uint32_t v10 = 0, v01 = 0, v11 = 0;
            if (findSurfaceNeighbor(k.x + 1, k.y, k.z, 2, v10) &&
                findSurfaceNeighbor(k.x, k.y + 1, k.z, 2, v01)) {
                if (validTri(v00, v10, v01)) triangles.push_back({v00, v10, v01});
                if (findSurfaceNeighbor(k.x + 1, k.y + 1, k.z, 2, v11)) {
                    if (validTri(v10, v11, v01)) triangles.push_back({v10, v11, v01});
                }
            }

            // XZ plane quads (front/back walls)
            if (findSurfaceNeighbor(k.x + 1, k.y, k.z, 1, v10) &&
                findSurfaceNeighbor(k.x, k.y, k.z + 1, 1, v01)) {
                if (validTri(v00, v10, v01)) triangles.push_back({v00, v10, v01});
                if (findSurfaceNeighbor(k.x + 1, k.y, k.z + 1, 1, v11)) {
                    if (validTri(v10, v11, v01)) triangles.push_back({v10, v11, v01});
                }
            }

            // YZ plane quads (side walls)
            if (findSurfaceNeighbor(k.x, k.y + 1, k.z, 0, v10) &&
                findSurfaceNeighbor(k.x, k.y, k.z + 1, 0, v01)) {
                if (validTri(v00, v10, v01)) triangles.push_back({v00, v10, v01});
                if (findSurfaceNeighbor(k.x, k.y + 1, k.z + 1, 0, v11)) {
                    if (validTri(v10, v11, v01)) triangles.push_back({v10, v11, v01});
                }
            }
        }

        // Compute smooth vertex normals
        std::vector<Eigen::Vector3f> normals(mesh_pts.size(), Eigen::Vector3f(0.0f, 0.0f, 0.0f));
        for (const auto& tri : triangles) {
            Eigen::Vector3f p0(mesh_pts[tri.a].x, mesh_pts[tri.a].y, mesh_pts[tri.a].z);
            Eigen::Vector3f p1(mesh_pts[tri.b].x, mesh_pts[tri.b].y, mesh_pts[tri.b].z);
            Eigen::Vector3f p2(mesh_pts[tri.c].x, mesh_pts[tri.c].y, mesh_pts[tri.c].z);
            Eigen::Vector3f fn = (p1 - p0).cross(p2 - p0);
            normals[tri.a] += fn;
            normals[tri.b] += fn;
            normals[tri.c] += fn;
        }
        for (auto& n : normals) {
            float len = n.norm();
            if (len > 1e-6f) n /= len;
            else n = Eigen::Vector3f(0.0f, 0.0f, 1.0f);
        }

        std::ofstream ofs(output_path);
        if (!ofs.is_open()) {
            res.error_message = "Failed to open OBJ file for writing: " + output_path;
            return res;
        }

        ofs << "# Wavefront OBJ generated by AV Scan Milestone 12 PointCloudProcessor\n"
            << "# Vertices: " << mesh_pts.size() << " | Triangles: " << triangles.size() << "\n"
            << "o AV_Scan_Surface_Mesh\n";

        for (const auto& pt : mesh_pts) {
            float rf = static_cast<float>(pt.r) / 255.0f;
            float gf = static_cast<float>(pt.g) / 255.0f;
            float bf = static_cast<float>(pt.b) / 255.0f;
            ofs << "v " << pt.x << " " << pt.y << " " << pt.z << " "
                << rf << " " << gf << " " << bf << "\n";
        }
        for (const auto& n : normals) {
            ofs << "vn " << n.x() << " " << n.y() << " " << n.z() << "\n";
        }
        for (const auto& tri : triangles) {
            uint32_t i1 = tri.a + 1;
            uint32_t i2 = tri.b + 1;
            uint32_t i3 = tri.c + 1;
            ofs << "f " << i1 << "//" << i1 << " " << i2 << "//" << i2 << " " << i3 << "//" << i3 << "\n";
        }
        ofs.close();

        res.success = true;
        res.exported_points = mesh_pts.size();
        res.exported_triangles = triangles.size();
    }

    std::error_code ec;
    if (fs::exists(output_path, ec)) {
        res.file_size_bytes = fs::file_size(output_path, ec);
    }

    auto end_time = std::chrono::steady_clock::now();
    res.export_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    AV_LOG_INFO("PointCloudProcessor",
                "Exported " + std::to_string(res.exported_points) + " points (" +
                std::to_string(res.exported_triangles) + " triangles) to " + output_path +
                " [" + res.format_name + ", " + std::to_string(res.file_size_bytes) + " bytes, " +
                std::to_string(res.export_time_ms) + "ms]");

    return res;
}

} // namespace av::core::processing
