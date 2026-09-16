#include <cassert>
#include <iostream>
#include "core/drivers/viture/viture_sensor_adapter.hpp"
#include "core/slam/viture/viture_vio_backend.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/sensor_instance.hpp"

using namespace av::core;

int main() {
    std::cout << "[TEST] Starting VITURE Sensor Adapter & VIO Backend Unit Tests..." << std::endl;

    // 1. Adapter capabilities and metadata
    drivers::viture::VitureSensorAdapter adapter;
    auto caps = adapter.getCapabilities();
    assert(caps.size() >= 5);
    bool has_imu = false, has_rgb = false, has_stereo = false, has_vio = false;
    for (const auto& c : caps) {
        if (c == "HEAD_TRACKING_IMU") has_imu = true;
        if (c == "RGB_CAMERA") has_rgb = true;
        if (c == "STEREO_TRACKING_CAMERAS") has_stereo = true;
        if (c == "ONBOARD_6DOF_VIO") has_vio = true;
    }
    assert(has_imu);
    assert(has_rgb);
    assert(has_stereo);
    assert(has_vio);
    std::cout << "  ✓ VitureSensorAdapter capabilities verified (HEAD_TRACKING_IMU, RGB_CAMERA, STEREO_TRACKING_CAMERAS, ONBOARD_6DOF_VIO)." << std::endl;

    // 2. Streams verification
    auto streams = adapter.getStreams();
    assert(streams.size() == 4);
    assert(streams[0].stream_name == "head_pose_vio");
    assert(streams[1].stream_name == "raw_imu");
    assert(streams[2].stream_name == "rgb_camera");
    assert(streams[3].stream_name == "stereo_tracking");
    std::cout << "  ✓ VitureSensorAdapter stream modalities verified (pose, imu, rgb, stereo)." << std::endl;

    // 3. Calibration provenance
    auto cal = adapter.getCalibration();
    assert(!cal.empty());
    assert(cal[0].source() == schemas::CalibrationSource::DEVICE_FACTORY);
    assert(cal[0].device_serial() == "VITURE-35CA-1104");
    assert(cal[0].firmware() == "1.0.12");
    assert(cal[0].validated() == true);
    std::cout << "  ✓ VITURE factory calibration provenance verified (" << cal[0].device_serial() << ", FW: " << cal[0].firmware() << ")." << std::endl;

    // 4. Timing domain
    assert(adapter.getTiming().find("HARDWARE_CLOCK_SYNC") != std::string::npos);
    std::cout << "  ✓ Hardware timestamp domain verified." << std::endl;

    slam::viture::VitureVioBackend backend;
    schemas::DeviceProfile prof;
    prof.set_profile_id("viture_luma_ultra_01");
    prof.set_manufacturer("VITURE");
    prof.set_model("Luma Ultra XR Glasses");
    prof.set_device_family("XR_SMART_GLASSES");
    assert(!backend.isRunning());
    assert(backend.getTrackingState() == schemas::TrackingState::NO_TRACKING);
    assert(backend.getPointCount() == 0);
    std::cout << "  ✓ VitureVioBackend initial state verified." << std::endl;

    std::cout << "[TEST] All VITURE Adapter & Backend Unit Tests PASSED!" << std::endl;
    return 0;
}
