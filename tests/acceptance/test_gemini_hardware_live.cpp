#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include "core/drivers/gemini/gemini_ipc_client.hpp"
#include "core/drivers/gemini/gemini_sensor_adapter.hpp"
#include "core/slam/gemini/gemini_rgbd_backend.hpp"

using namespace av::core;

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "AV Scan — Orbbec Gemini 336L LIVE Physical Hardware Test" << std::endl;
    std::cout << "==========================================================" << std::endl;

    auto client = std::make_shared<drivers::gemini::GeminiIpcClient>("127.0.0.1", 9100);

    std::cout << "[Step 1] Ensuring isolated Gemini daemon is active..." << std::endl;
    if (!client->ensureDaemonRunning()) {
        std::cerr << "FAIL: Could not start or connect to av_gemini_daemon on 127.0.0.1:9100" << std::endl;
        return 1;
    }
    std::cout << "  ✓ Connected to daemon successfully." << std::endl;

    std::cout << "[Step 2] Interrogating physical device..." << std::endl;
    nlohmann::json info;
    if (!client->identify(info)) {
        std::cerr << "FAIL: Could not query device identity" << std::endl;
        return 1;
    }
    std::cout << "  Manufacturer: " << info.value("manufacturer", "Unknown") << std::endl;
    std::cout << "  Model:        " << info.value("model", "Unknown") << std::endl;
    std::cout << "  Serial:       " << info.value("serial", "Unknown") << std::endl;
    std::cout << "  Firmware:     " << info.value("firmware", "Unknown") << std::endl;
    std::cout << "  SLAM Backend: " << info.value("slam_backend", "Unknown") << " (" << info.value("slam_family", "") << ")" << std::endl;
    std::cout << "  Internal Extrinsics: " << info["internal_extrinsics"].dump() << std::endl;

    assert(info.value("serial", "") == "CPC6463000XW");
    std::cout << "  ✓ Physical serial CPC6463000XW matched." << std::endl;

    std::cout << "[Step 3] Initializing Gemini Sensor Adapter & RTAB-Map SLAM Backend..." << std::endl;
    drivers::gemini::GeminiSensorAdapter adapter(client);
    slam::gemini::GeminiRgbdBackend backend(client);

    schemas::DeviceProfile prof;
    prof.set_profile_id("orbbec_gemini_336l_01");
    prof.set_manufacturer("Orbbec");
    prof.set_model("Gemini 336L");
    prof.set_device_family("ACTIVE_STEREO_RGBD_CAMERA");
    schemas::SensorInstance inst("inst_gemini_01", "orbbec_gemini_336l_01", "Orbbec Gemini 336L");

    if (!adapter.connect(inst, prof)) {
        std::cerr << "FAIL: Adapter connect failed" << std::endl;
        return 1;
    }
    std::cout << "  ✓ Adapter connected: " << adapter.identify() << std::endl;

    std::cout << "[Step 4] Starting live acquisition and RTAB-Map RGB-D SLAM..." << std::endl;
    if (!backend.start()) {
        std::cerr << "FAIL: Could not start backend acquisition" << std::endl;
        return 1;
    }
    std::cout << "  ✓ Acquisition & RTAB-Map SLAM launched." << std::endl;

    std::cout << "[Step 5] Streaming live spatial data for 7 seconds..." << std::endl;
    for (int i = 0; i < 7; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        backend.updateTelemetry();
        backend.fetchNewPoints();

        auto pose = backend.getPose();
        auto state = schemas::to_string(backend.getTrackingState());
        auto count = backend.getPointCount();
        auto traj_count = backend.getTrajectory().size();

        std::cout << "  [" << (i + 1) << "s] Pose: ("
                  << pose.position.x << ", "
                  << pose.position.y << ", "
                  << pose.position.z << ") | Yaw: "
                  << backend.getCurrentYaw() << " rad | State: "
                  << state << " | 3D Points: "
                  << count << " | Trajectory Poses: "
                  << traj_count << std::endl;
    }

    std::cout << "[Step 6] Stopping acquisition and pulling final map..." << std::endl;
    backend.stop();
    adapter.stop();

    auto final_pts = backend.getMapPoints();
    std::cout << "  ✓ SLAM stopped cleanly." << std::endl;
    std::cout << "  Final 3D Map Point Count: " << final_pts.size() << std::endl;
    std::cout << "  Final Trajectory Count: " << backend.getTrajectory().size() << std::endl;

    client->shutdownDaemon();
    std::cout << "==========================================================" << std::endl;
    std::cout << "LIVE Gemini 336L Hardware Acceptance Test SUCCESSFUL!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
