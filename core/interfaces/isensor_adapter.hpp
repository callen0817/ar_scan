#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/sensor_instance.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/calibration_provenance.hpp"
#include "core/schemas/spatial_data.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace av::core::interfaces {

using PointCloudCallback = std::function<void(const schemas::PointCloudFrame&)>;
using ImuCallback = std::function<void(const schemas::ImuSample&)>;
using ImageCallback = std::function<void(const schemas::ImageFrame&)>;

class ISensorAdapter {
public:
    virtual ~ISensorAdapter() = default;

    // Lifecycle & Identification
    virtual std::string identify() const = 0;
    virtual bool connect(const schemas::SensorInstance& instance, const schemas::DeviceProfile& profile) = 0;
    virtual bool disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Capabilities & Introspection
    virtual std::vector<std::string> getCapabilities() const = 0;
    virtual std::vector<schemas::StreamInfo> getStreams() const = 0;
    virtual std::vector<schemas::CalibrationProvenance> getCalibration() const = 0;
    virtual std::string getTiming() const = 0;
    virtual schemas::SensorStatus getStatus() const = 0;

    // Acquisition Control
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool isStreaming() const = 0;

    // Data Delivery Callbacks
    virtual void registerPointCloudCallback(PointCloudCallback cb) = 0;
    virtual void registerImuCallback(ImuCallback cb) = 0;
    virtual void registerImageCallback(ImageCallback cb) = 0;
};

} // namespace av::core::interfaces
