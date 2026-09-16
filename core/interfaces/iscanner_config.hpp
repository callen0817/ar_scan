#pragma once

#include "core/schemas/types.hpp"
#include "core/schemas/scanner_config.hpp"
#include "core/schemas/calibration_provenance.hpp"
#include <string>
#include <vector>
#include <memory>

namespace av::core::interfaces {

class IScannerConfigProvider {
public:
    virtual ~IScannerConfigProvider() = default;

    virtual schemas::ScannerConfig getScannerConfig() const = 0;
    virtual std::vector<std::string> getSensorInstances() const = 0;
    virtual schemas::SensorRelationshipType getRelationshipType() const = 0;
    virtual schemas::ExtrinsicStatus getExtrinsicStatus() const = 0;

    virtual bool getRelativeTransform(const std::string& from_sensor,
                                     const std::string& to_sensor,
                                     schemas::Transform3D& out_transform) const = 0;

    virtual schemas::CalibrationProvenance getCalibrationProvenance() const = 0;
};

} // namespace av::core::interfaces
