#pragma once

#include "core/interfaces/ineural_recon_backend.hpp"
#include "abot_ipc_client.hpp"
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace av::core::reconstruction::abot {

/**
 * SLAM and 3D reconstruction backend implementing INeuralReconstructionBackend using the
 * upstream ABot-Recon foundation model.
 * 
 * QUALIFICATION STATE: RESEARCH / EXPERIMENTAL
 * COMMERCIAL USE: NOT CLEARED / NON-COMMERCIAL
 */
class ABotSlamBackend : public interfaces::INeuralReconstructionBackend {
public:
    explicit ABotSlamBackend(std::shared_ptr<ABotIpcClient> ipc_client = nullptr);
    ~ABotSlamBackend() override;

    // ISlamBackend Lifecycle
    bool initialize(const schemas::DeviceProfile& profile) override;
    bool start() override;
    bool stop() override;
    bool reset() override;
    bool isRunning() const override;

    // Spatial Telemetry & Estimates
    schemas::Pose3D getPose() const override;
    std::vector<schemas::Pose3D> getTrajectory() const override;
    schemas::MapMetadata getMapMetadata() const override;
    schemas::TrackingState getTrackingState() const override;

    // Observation Ingestion
    void feedPointCloud(const schemas::PointCloudFrame& cloud) override;
    void feedImu(const schemas::ImuSample& imu) override;
    void feedImage(const schemas::ImageFrame& image) override;

    // Streaming point cloud and telemetry access
    bool getNewMapPoints(std::vector<schemas::PointXYZRGB>& points) override;
    bool getTelemetry(nlohmann::json& telemetry) override;

    // Telemetry and Map synchronization
    bool updateTelemetry() override;
    bool fetchNewPoints() override;

    // Map geometry accessors
    std::vector<schemas::PointXYZRGB> getMapPoints() const override;
    double getCurrentYaw() const override;
    uint64_t getPointCount() const override;
    double getAcquisitionRate() const override;
    double getSlamRate() const override;
    std::vector<schemas::CalibrationProvenance> getCalibration() const override;
    std::string getBackendName() const override { return "ABotStreamingBackend"; }

private:
    std::shared_ptr<ABotIpcClient> ipc_client_;
    bool own_client_{false};
    std::atomic<bool> running_{false};
    std::string device_path_{""};

    schemas::DeviceProfile current_profile_;
    mutable std::mutex mutex_;
    mutable schemas::Pose3D current_pose_;
    std::vector<schemas::PointXYZRGB> map_points_;
    std::vector<schemas::Pose3D> trajectory_;
    double current_yaw_{0.0};
    uint64_t point_count_{0};
    double acquisition_rate_{0.0};
    double slam_rate_{0.0};
    schemas::TrackingState tracking_state_{schemas::TrackingState::NO_TRACKING};
};

} // namespace av::core::reconstruction::abot
