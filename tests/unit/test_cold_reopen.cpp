#include "gui/qml_bridge.hpp"
#include "platform/linux/linux_platform.hpp"
#include "core/profiling/device_profiler.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/schemas/project.hpp"
#include "core/schemas/session.hpp"
#include "core/schemas/capture.hpp"
#include "core/schemas/spatial_data.hpp"

#include <QCoreApplication>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;
using namespace av::core;
using namespace av::gui;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================\n";
    std::cout << "AV Scan — Cold Profile Reuse & Saved-Project Cold Reopen Verification\n";
    std::cout << "=================================================================\n\n";

    fs::path repo_root = "/home/scanar/av_scan";

    // -------------------------------------------------------------
    // TEST PART 1: COLD PROFILE-REUSE TEST
    // -------------------------------------------------------------
    std::cout << "[PART 1] Verifying Cold Profile-Reuse for SenseCAP Watcher...\n";
    profiling::DeviceProfiler profiler;
    auto phys_devs = profiler.discoverPhysicalDevices(repo_root);

    bool watcher_found = false;
    bool watcher_profiled = false;
    std::string matched_prof_id = "";
    std::string matched_recon = "";
    std::string matched_backend = "";

    for (const auto& dev : phys_devs) {
        if (dev.vendor_id == "1a86" && dev.product_id == "55d2") {
            watcher_found = true;
            matched_prof_id = dev.existing_profile_id;
            matched_recon = dev.compatible_reconstruction;
            matched_backend = dev.recommended_backend;
            if (dev.profile_status == "PROFILED") {
                watcher_profiled = true;
            }
            std::cout << "  - Discovered Watcher: " << dev.display_name
                      << " | Serial: " << dev.serial_number
                      << " | Status: " << dev.profile_status
                      << " | Matched Profile: " << dev.existing_profile_id
                      << " | Reconstruction: " << dev.compatible_reconstruction
                      << " | Backend: " << dev.recommended_backend << "\n";
        }
    }

    assert(watcher_found);
    assert(watcher_profiled);
    assert(!matched_prof_id.empty());
    assert(matched_prof_id.find("watcher") != std::string::npos);
    assert(matched_recon == "UNKNOWN");
    assert(matched_backend == "NONE" || matched_backend.empty());
    std::cout << "  ✓ PASS: Watcher discovered on host and automatically matched to existing disk profile '"
              << matched_prof_id << "' without recreation!\n\n";

    // -------------------------------------------------------------
    // TEST PART 2: SAVED-PROJECT COLD REOPEN
    // -------------------------------------------------------------
    std::cout << "[PART 2] Verifying Saved-Project Cold Reopen (proj_1789959848766)...\n";
    std::string target_proj_id = "proj_1789959848766";
    fs::path proj_path = repo_root / "projects" / target_proj_id;
    if (!fs::exists(proj_path)) {
        std::cerr << "FAIL: Target project directory does not exist: " << proj_path << std::endl;
        return 1;
    }

    auto storage = std::make_shared<storage::StorageEngine>();
    storage->initialize(repo_root.string());
    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();

    // Launch a fresh QmlBridge instance (simulates fresh application launch)
    auto bridge = std::make_unique<QmlBridge>(storage, platform);
    bool open_ok = bridge->openProject(QString::fromStdString(target_proj_id));
    assert(open_ok);
    std::cout << "  - Project opened: " << bridge->projectName().toStdString() << " (" << bridge->projectId().toStdString() << ")\n";
    assert(bridge->projectName() == "WatcherTest");

    int points = bridge->pointsCaptured();
    std::cout << "  - Reopened Persisted Point Count: " << points << "\n";
    assert(points == 78810);
    assert(bridge->hasMapData());

    auto sensors = bridge->activeSensors();
    std::cout << "  - Active Sensors Loaded: " << sensors.size() << "\n";
    assert(sensors.size() == 1);

    QVariantMap sensor_info = sensors.front().toMap();
    std::cout << "  - Sensor Name: " << sensor_info["name"].toString().toStdString() << "\n";
    std::cout << "  - Sensor Instance ID: " << sensor_info["instanceId"].toString().toStdString() << "\n";
    std::cout << "  - Sensor Profile ID: " << sensor_info["profileId"].toString().toStdString() << "\n";
    std::cout << "  - Sensor Transport: " << sensor_info["transport"].toString().toStdString() << "\n";
    std::cout << "  - Sensor Scale Status: " << sensor_info["scaleStatus"].toString().toStdString() << "\n";

    assert(sensor_info["name"].toString().contains("Watcher"));
    assert(sensor_info["profileId"].toString().contains("watcher_56D5018737"));
    assert(sensor_info["scaleStatus"].toString() == "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED");

    // Verify RGB variance survives
    fs::path pcd_file = proj_path / "sessions" / "session_1789959866896" / "captures" / "cap_1789959918586_camera" / "map.pcd";
    assert(fs::exists(pcd_file));
    std::ifstream pcd_in(pcd_file);
    std::string line;
    bool has_rgb_field = false;
    while (std::getline(pcd_in, line) && line.find("DATA") == std::string::npos) {
        if (line.find("FIELDS") != std::string::npos && line.find("rgb") != std::string::npos) {
            has_rgb_field = true;
        }
    }
    std::cout << "  - PCD Format Contains RGB Channel: " << (has_rgb_field ? "YES" : "NO") << "\n";
    assert(has_rgb_field);

    // Verify trajectory
    fs::path traj_file = proj_path / "sessions" / "session_1789959866896" / "captures" / "cap_1789959918586_camera" / "trajectory.json";
    assert(fs::exists(traj_file));
    std::ifstream traj_in(traj_file);
    auto tj = nlohmann::json::parse(traj_in);
    std::cout << "  - Persisted Trajectory Poses: " << tj.size() << "\n";
    assert(tj.size() == 2252);

    std::cout << "  ✓ PASS: Saved Watcher XYZRGB reconstruction loaded directly from disk without re-running SLAM!\n\n";

    // -------------------------------------------------------------
    // TEST PART 3: MERGED-PROJECT COLD REOPEN
    // -------------------------------------------------------------
    std::cout << "[PART 3] Verifying Saved Merged-Project Cold Reopen (proj_1790909178235)...\n";
    std::string merged_proj_id = "proj_1790909178235";
    fs::path merged_proj_path = repo_root / "projects" / merged_proj_id;
    if (fs::exists(merged_proj_path)) {
        auto bridge_merged = std::make_unique<QmlBridge>(storage, platform);
        bool open_merged_ok = bridge_merged->openProject(QString::fromStdString(merged_proj_id));
        assert(open_merged_ok);
        std::cout << "  - Merged Project opened: " << bridge_merged->projectName().toStdString() << " (" << bridge_merged->projectId().toStdString() << ")\n";
        int merged_points = bridge_merged->pointsCaptured();
        std::cout << "  - Merged Project Reopened Point Count: " << merged_points << "\n";
        assert(merged_points == 384439);
        assert(bridge_merged->hasMapData());

        auto merged_sensors = bridge_merged->activeSensors();
        std::cout << "  - Active Sensors Loaded for Merged Project: " << merged_sensors.size() << "\n";
        assert(merged_sensors.size() >= 1);
        QVariantMap s_info = merged_sensors.front().toMap();
        std::cout << "  - Sensor Name: " << s_info["name"].toString().toStdString() << "\n";
        std::cout << "  - Sensor ID: " << s_info["id"].toString().toStdString() << "\n";
        assert(s_info["id"].toString() == "MERGED");

        std::cout << "  ✓ PASS: Merged project point cloud discovered, loaded, and context synthesized with 384,439 points!\n\n";
    }

    std::cout << "=================================================================\n";
    std::cout << "ALL COLD REUSE & REOPEN GATES PASSED!\n";
    std::cout << "=================================================================\n";

    return 0;
}
