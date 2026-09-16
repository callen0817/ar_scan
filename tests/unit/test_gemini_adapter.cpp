#include <cassert>
#include <iostream>
#include "core/drivers/gemini/gemini_sensor_adapter.hpp"
#include "core/slam/gemini/gemini_rgbd_backend.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/sensor_instance.hpp"

using namespace av::core;

int main() {
    std::cout << "[TEST] Starting Gemini Sensor Adapter & RGB-D Backend Unit Tests..." << std::endl;

    // 1. Adapter capabilities and metadata
    drivers::gemini::GeminiSensorAdapter adapter;
    auto caps = adapter.getCapabilities();
    assert(caps.size() >= 5);
    bool has_depth = false, has_rgb = false, has_slam = false;
    for (const auto& c : caps) {
        if (c == "DEPTH_STREAM") has_depth = true;
        if (c == "RGB_STREAM") has_rgb = true;
        if (c == "RGBD_SLAM") has_slam = true;
    }
    assert(has_depth);
    assert(has_rgb);
    assert(has_slam);
    std::cout << "  ✓ GeminiSensorAdapter capabilities verified (DEPTH, RGB, RGBD_SLAM)." << std::endl;

    // 2. Streams verification
    auto streams = adapter.getStreams();
    assert(streams.size() == 3);
    assert(streams[0].stream_name == "depth");
    assert(streams[1].stream_name == "color");
    assert(streams[2].stream_name == "imu");
    std::cout << "  ✓ GeminiSensorAdapter stream modalities verified (depth, color, imu)." << std::endl;

    // 3. Calibration provenance
    auto cal = adapter.getCalibration();
    assert(!cal.empty());
    assert(cal[0].source() == schemas::CalibrationSource::DEVICE_FACTORY);
    assert(cal[0].device_serial() == "CPC6463000XW");
    assert(cal[0].firmware() == "1.4.60");
    assert(cal[0].validated() == true);
    std::cout << "  ✓ Gemini factory calibration provenance verified (" << cal[0].device_serial() << ", FW: " << cal[0].firmware() << ")." << std::endl;

    // 4. Timing domain
    assert(adapter.getTiming().find("HARDWARE_CLOCK_SYNC") != std::string::npos);
    std::cout << "  ✓ Hardware timestamp domain verified." << std::endl;

    slam::gemini::GeminiRgbdBackend backend;
    schemas::DeviceProfile prof;
    prof.set_profile_id("orbbec_gemini_336l_01");
    prof.set_manufacturer("Orbbec");
    prof.set_model("Gemini 336L");
    prof.set_device_family("ACTIVE_STEREO_RGBD_CAMERA");
    assert(!backend.isRunning());
    assert(backend.getTrackingState() == schemas::TrackingState::NO_TRACKING);
    assert(backend.getPointCount() == 0);
    std::cout << "  ✓ GeminiRgbdBackend initial state verified." << std::endl;

    std::cout << "[TEST] All Gemini Adapter & Backend Unit Tests PASSED!" << std::endl;
    return 0;
}
