#pragma once

#include "core/interfaces/isensor_adapter.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>

namespace av::core::drivers::uvc {

/**
 * Generic Camera / UVC Sensor Adapter.
 * 
 * Provides standard V4L2/UVC video capture and exposes raw RGB ImageFrames
 * through the ISensorAdapter interface.
 * 
 * Contains STRICTLY NO LingBot or SLAM logic. Reusable for any compatible UVC camera.
 */
class GenericUvcSensorAdapter : public interfaces::ISensorAdapter {
public:
    explicit GenericUvcSensorAdapter(const std::string& device_path = "");
    ~GenericUvcSensorAdapter() override;

    // Lifecycle & Identification
    std::string identify() const override;
    bool connect(const schemas::SensorInstance& instance, const schemas::DeviceProfile& profile) override;
    bool disconnect() override;
    bool isConnected() const override;

    // Capabilities & Introspection
    std::vector<std::string> getCapabilities() const override;
    std::vector<schemas::StreamInfo> getStreams() const override;
    std::vector<schemas::CalibrationProvenance> getCalibration() const override;
    std::string getTiming() const override;
    schemas::SensorStatus getStatus() const override;

    // Acquisition Control
    bool start() override;
    bool stop() override;
    bool isStreaming() const override;

    // Data Delivery Callbacks
    void registerPointCloudCallback(interfaces::PointCloudCallback cb) override;
    void registerImuCallback(interfaces::ImuCallback cb) override;
    void registerImageCallback(interfaces::ImageCallback cb) override;

    // Device configuration
    void setDevicePath(const std::string& path);
    std::string devicePath() const;

private:
    void captureLoop();

    std::string device_path_{""};
    std::atomic<bool> connected_{false};
    std::atomic<bool> streaming_{false};
    std::thread capture_thread_;

    mutable std::mutex callback_mutex_;
    interfaces::ImageCallback image_callback_;
    interfaces::PointCloudCallback point_cloud_callback_;
    interfaces::ImuCallback imu_callback_;

    schemas::SensorInstance current_instance_;
    schemas::DeviceProfile current_profile_;

    int width_{1920};
    int height_{1080};
    double fps_{30.0};
    std::string model_name_{"Generic UVC Camera"};
};

} // namespace av::core::drivers::uvc
