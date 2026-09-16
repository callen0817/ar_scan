#pragma once

#include "core/interfaces/isensor_adapter.hpp"
#include "viture_ipc_client.hpp"
#include <memory>
#include <atomic>

namespace av::core::drivers::viture {

class VitureSensorAdapter : public interfaces::ISensorAdapter {
public:
    explicit VitureSensorAdapter(std::shared_ptr<VitureIpcClient> ipc_client = nullptr);
    ~VitureSensorAdapter() override;

    // ISensorAdapter implementation
    std::string identify() const override;
    bool connect(const schemas::SensorInstance& instance, const schemas::DeviceProfile& profile) override;
    bool disconnect() override;
    bool isConnected() const override;

    std::vector<std::string> getCapabilities() const override;
    std::vector<schemas::StreamInfo> getStreams() const override;
    std::vector<schemas::CalibrationProvenance> getCalibration() const override;
    std::string getTiming() const override;
    schemas::SensorStatus getStatus() const override;

    bool start() override;
    bool stop() override;
    bool isStreaming() const override;

    void registerPointCloudCallback(interfaces::PointCloudCallback cb) override;
    void registerImuCallback(interfaces::ImuCallback cb) override;
    void registerImageCallback(interfaces::ImageCallback cb) override;

    std::shared_ptr<VitureIpcClient> getIpcClient() const { return ipc_client_; }

private:
    std::shared_ptr<VitureIpcClient> ipc_client_;
    bool own_client_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> streaming_{false};
    schemas::SensorInstance current_instance_;
    schemas::DeviceProfile current_profile_;

    interfaces::PointCloudCallback point_cloud_cb_;
    interfaces::ImuCallback imu_cb_;
    interfaces::ImageCallback image_cb_;
};

} // namespace av::core::drivers::viture
