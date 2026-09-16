#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/spatial_data.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::drivers::viture {

struct VitureTelemetry {
    std::string status{"UNKNOWN"};
    std::string tracking_state{"NO_TRACKING"};
    bool is_capturing{false};
    schemas::Pose3D current_pose;
    double current_yaw{0.0};
    uint64_t point_count{0};
    uint64_t trajectory_count{0};
    int feature_count{0};
    double feature_quality{0.0};
};

class VitureIpcClient {
public:
    VitureIpcClient(const std::string& host = "127.0.0.1", int port = 9101);
    ~VitureIpcClient();

    bool ensureDaemonRunning();
    bool connect();
    void disconnect();
    bool isConnected() const;

    bool ping();
    bool identify(nlohmann::json& out_info);
    bool startAcquisition();
    bool stopAcquisition();
    bool reset();
    bool getTelemetry(VitureTelemetry& out_telem);
    bool getNewPoints(std::vector<schemas::PointXYZI>& out_points);
    bool getAllPoints(std::vector<schemas::PointXYZI>& out_points);
    bool getTrajectory(std::vector<schemas::Pose3D>& out_trajectory);
    bool getGridMap(nlohmann::json& out_grid);
    bool shutdownDaemon();

private:
    std::string host_;
    int port_;
    int sock_fd_{-1};
    mutable std::mutex mutex_;
    pid_t daemon_pid_{-1};

    bool sendCommand(const std::string& cmd);
    bool readLine(std::string& out_line, int timeout_ms = 3000);
    bool readExact(uint8_t* buffer, size_t size, int timeout_ms = 5000);
};

} // namespace av::core::drivers::viture
