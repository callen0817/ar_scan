#include <cassert>
#include <iostream>
#include <memory>
#include "core/drivers/airy/airy_sensor_adapter.hpp"
#include "core/slam/airy/airy_lio_backend.hpp"

using namespace av::core;

int main() {
    std::cout << "Running RoboSense Airy Sensor Adapter & LIO Backend Unit Tests..." << std::endl;

    auto adapter = std::make_shared<drivers::airy::AirySensorAdapter>();
    auto backend = std::make_shared<slam::airy::AiryLioBackend>();

    // 1. Identification & Capabilities
    std::cout << "  Checking identity and capabilities..." << std::endl;
    assert(adapter->identify().find("RoboSense RS-Airy") != std::string::npos);

    auto caps = adapter->getCapabilities();
    bool has_pointcloud = false;
    bool has_imu = false;
    bool has_calib = false;
    for (const auto& c : caps) {
        if (c == "POINT_CLOUD_3D") has_pointcloud = true;
        if (c == "INTERNAL_IMU_6DOF") has_imu = true;
        if (c == "DIFOP_FACTORY_CALIBRATION") has_calib = true;
    }
    assert(has_pointcloud);
    assert(has_imu);
    assert(has_calib);

    // 2. Streams
    std::cout << "  Checking sensor streams..." << std::endl;
    auto streams = adapter->getStreams();
    assert(streams.size() == 2);
    assert(streams[0].name == "lidar_points");
    assert(streams[0].nominal_rate_hz == 10.0);
    assert(streams[1].name == "internal_imu");
    assert(streams[1].nominal_rate_hz == 200.0);

    // 3. Calibration Provenance
    std::cout << "  Checking factory calibration provenance..." << std::endl;
    auto calibs = adapter->getCalibration();
    assert(!calibs.empty());
    const auto& cal = calibs[0];
    assert(cal.source() == schemas::CalibrationSource::DEVICE_FACTORY);
    assert(cal.value() == "FACTORY_DIFOP_EEPROM");
    assert(cal.device_serial() == "AIRY-2024-99812");
    assert(cal.firmware() == "3.1.20");
    assert(cal.confidence() == 1.0);
    assert(cal.validated() == true);
    assert(cal.evidence().find("0.004250") != std::string::npos);

    // 4. Initial Status
    std::cout << "  Checking initial lifecycle status..." << std::endl;
    assert(!adapter->isStreaming());
    assert(backend->getTrackingState() == schemas::TrackingState::NO_TRACKING);
    assert(backend->getPointCount() == 0);
    assert(backend->getTrajectory().empty());

    std::cout << "All RoboSense Airy Unit Tests PASSED!" << std::endl;
    return 0;
}
