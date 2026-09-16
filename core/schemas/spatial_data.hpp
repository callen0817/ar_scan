#pragma once

#include "types.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace av::core::schemas {

struct PointXYZI {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float intensity{1.0f};
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
    bool has_color{false};
};

using PointAV = PointXYZI;
using PointXYZRGB = PointXYZI;

struct PointCloudFrame {
    uint64_t timestamp_ns{0};
    uint64_t sequence_id{0};
    std::string frame_id;
    std::string sensor_instance_id;
    std::vector<PointXYZI> points;
    bool is_dense{true};
};

struct ImuSample {
    uint64_t timestamp_ns{0};
    std::string frame_id;
    Vector3D linear_acceleration; // m/s^2
    Vector3D angular_velocity;    // rad/s
    Quaternion orientation;       // optional orientation if filtered on-chip
    bool has_orientation{false};
};

struct ImageFrame {
    uint64_t timestamp_ns{0};
    uint64_t sequence_id{0};
    std::string frame_id;
    std::string sensor_instance_id;
    int width{0};
    int height{0};
    int channels{0};
    std::string encoding; // "rgb8", "bgr8", "16UC1" (depth)
    std::vector<uint8_t> data;
};

} // namespace av::core::schemas
