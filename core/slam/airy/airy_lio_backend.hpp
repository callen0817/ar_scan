#pragma once

#include "core/interfaces/islam_backend.hpp"
#include "core/drivers/airy/airy_ipc_client.hpp"
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>

namespace av::core::slam::airy {

class AiryLioBackend : public interfaces::ISlamBackend {
public:
    explicit AiryLioBackend(std::shared_ptr<drivers::airy::AiryIpcClient> ipc_client = nullptr);
    ~AiryLioBackend() override;

    // ISlamBackend implementation
    bool initialize(const schemas::DeviceProfile& profile) override;
    bool start() override;
    bool stop() override;
    bool reset() override;
    bool isRunning() const override;

    schemas::Pose3D getPose() const override;
    std::vector<schemas::Pose3D> getTrajectory() const override;
    schemas::MapMetadata getMapMetadata() const override;
    schemas::TrackingState getTrackingState() const override;

    void feedPointCloud(const schemas::PointCloudFrame& cloud) override;
    void feedImu(const schemas::ImuSample& imu) override;
    void feedImage(const schemas::ImageFrame& image) override;

    // Telemetry and Map synchronization with isolated backend
    bool updateTelemetry();
    bool fetchNewPoints();
    bool fetchAllPoints();

    // Map geometry accessors
    std::vector<schemas::PointXYZI> getMapPoints() const;
    double getCurrentYaw() const;
    uint64_t getPointCount() const;

private:
    std::shared_ptr<drivers::airy::AiryIpcClient> ipc_client_;
    bool own_client_{false};
    schemas::DeviceProfile profile_;

    mutable std::mutex mutex_;
    std::atomic<bool> running_{false};
    schemas::TrackingState tracking_state_{schemas::TrackingState::NO_TRACKING};
    schemas::Pose3D current_pose_;
    double current_yaw_{0.0};
    std::vector<schemas::Pose3D> trajectory_;
    std::vector<schemas::PointXYZI> map_points_;
    uint64_t point_count_{0};
};

} // namespace av::core::slam::airy
