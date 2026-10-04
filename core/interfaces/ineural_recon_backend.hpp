#pragma once

#include "core/interfaces/islam_backend.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

namespace av::core::interfaces {

/**
 * Common minimal interface for neural monocular reconstruction backends
 * (e.g., LingBot, ABot-Recon).
 */
class INeuralReconstructionBackend : public ISlamBackend {
public:
    ~INeuralReconstructionBackend() override = default;

    virtual bool getNewMapPoints(std::vector<schemas::PointXYZRGB>& points) = 0;
    virtual bool getTelemetry(nlohmann::json& telemetry) = 0;
    virtual bool updateTelemetry() = 0;
    virtual bool fetchNewPoints() = 0;
    virtual std::vector<schemas::PointXYZRGB> getMapPoints() const = 0;
    virtual double getCurrentYaw() const = 0;
    virtual uint64_t getPointCount() const = 0;
    virtual double getAcquisitionRate() const = 0;
    virtual double getSlamRate() const = 0;
    virtual std::vector<schemas::CalibrationProvenance> getCalibration() const = 0;
    virtual std::string getBackendName() const = 0;
};

} // namespace av::core::interfaces
