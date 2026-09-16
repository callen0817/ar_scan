#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/map_metadata.hpp"
#include "core/schemas/spatial_data.hpp"
#include <string>
#include <vector>
#include <memory>

namespace av::core::interfaces {

class ISlamBackend {
public:
    virtual ~ISlamBackend() = default;

    // Backend Lifecycle
    virtual bool initialize(const schemas::DeviceProfile& profile) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool reset() = 0;
    virtual bool isRunning() const = 0;

    // Spatial Telemetry & Estimates
    virtual schemas::Pose3D getPose() const = 0;
    virtual std::vector<schemas::Pose3D> getTrajectory() const = 0;
    virtual schemas::MapMetadata getMapMetadata() const = 0;
    virtual schemas::TrackingState getTrackingState() const = 0;

    // Observation Ingestion
    virtual void feedPointCloud(const schemas::PointCloudFrame& cloud) = 0;
    virtual void feedImu(const schemas::ImuSample& imu) = 0;
    virtual void feedImage(const schemas::ImageFrame& image) = 0;
};

} // namespace av::core::interfaces
