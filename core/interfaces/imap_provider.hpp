#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/map_metadata.hpp"
#include "core/schemas/spatial_data.hpp"
#include <memory>
#include <functional>

namespace av::core::interfaces {

using MapUpdateCallback = std::function<void(const schemas::MapMetadata&, const schemas::PointCloudFrame&)>;

class IMapProvider {
public:
    virtual ~IMapProvider() = default;

    virtual schemas::MapMetadata getMapMetadata() const = 0;
    virtual schemas::PointCloudFrame getLatestMapGeometry() const = 0;
    virtual schemas::TrackingState getTrackingState() const = 0;

    virtual void registerMapConsumer(MapUpdateCallback callback) = 0;
};

} // namespace av::core::interfaces
