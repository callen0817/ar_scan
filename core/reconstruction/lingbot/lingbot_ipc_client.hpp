#pragma once

#include "core/schemas/spatial_data.hpp"
#include "core/schemas/types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <memory>

namespace av::core::reconstruction::lingbot {

class LingBotIpcClient {
public:
    explicit LingBotIpcClient(const std::string& host = "127.0.0.1", int port = 9102);
    ~LingBotIpcClient();

    bool connect();
    void disconnect();
    bool isConnected() const;
    bool ensureDaemonRunning();

    bool ping();
    bool identify(nlohmann::json& out_info);
    bool startAcquisition(const std::string& dev_path = "/dev/video2");
    bool stopAcquisition();
    bool getTelemetry(nlohmann::json& out_telemetry);
    bool getTrajectory(std::vector<schemas::Pose3D>& out_trajectory);
    bool fetchNewPoints(std::vector<schemas::PointXYZRGB>& out_points);

private:
    bool sendCommand(const std::string& cmd);
    bool readLine(std::string& out_line, int timeout_ms = 2000);
    bool readExact(uint8_t* buffer, size_t length, int timeout_ms = 2000);

    std::string host_;
    int port_;
    int sock_fd_{-1};
    int daemon_pid_{-1};
    mutable std::mutex mutex_;
};

} // namespace av::core::reconstruction::lingbot
