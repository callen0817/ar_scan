#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <QCoreApplication>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/drivers/airy/airy_sensor_adapter.hpp"
#include "core/slam/airy/airy_lio_backend.hpp"
#include "platform/linux/linux_platform.hpp"

namespace fs = std::filesystem;
using namespace av::core;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAILED: " message << " (at line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "======================================================" << std::endl;
    std::cout << "Running AV Scan — Milestone 3 (RoboSense Airy) Acceptance Tests" << std::endl;
    std::cout << "======================================================" << std::endl;

#ifdef AV_SOURCE_DIR
    fs::path repo_root = AV_SOURCE_DIR;
#else
    fs::path repo_root = "/home/scanar/av_scan";
#endif

    fs::path test_dir = fs::current_path() / "test_m3_sandbox";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);

    // Copy profiles and scanners into sandbox
    if (fs::exists(repo_root / "profiles")) {
        fs::copy(repo_root / "profiles", test_dir / "profiles", fs::copy_options::recursive);
    }
    if (fs::exists(repo_root / "scanners")) {
        fs::copy(repo_root / "scanners", test_dir / "scanners", fs::copy_options::recursive);
    }

    auto storage = std::make_shared<storage::StorageEngine>();
    storage->initialize(test_dir.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto adapter = std::make_shared<drivers::airy::AirySensorAdapter>();
    auto backend = std::make_shared<slam::airy::AiryLioBackend>();

    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform, adapter, backend);

    // ---------------------------------------------------------
    // Step 1: Create Project
    // ---------------------------------------------------------
    std::cout << "[Step 1] Creating Project..." << std::endl;
    bool proj_ok = bridge->createProject("Airy Qualification Scan", "M3 Hardware Acceptance Project");
    CHECK(proj_ok, "Failed to create project");
    CHECK(!bridge->projectId().isEmpty(), "Project ID is empty");
    CHECK(bridge->captureState() == "READY", "Capture state must be READY");
    std::string proj_id = bridge->projectId().toStdString();
    std::cout << "  Created Project ID: " << proj_id << std::endl;

    // ---------------------------------------------------------
    // Step 2: Select RoboSense Airy Scanner
    // ---------------------------------------------------------
    std::cout << "[Step 2] Selecting RoboSense Airy Profile..." << std::endl;
    CHECK(bridge->availableScanners().size() > 0, "No scanners available");
    int airy_index = -1;
    for (int i = 0; i < bridge->scannerList().size(); ++i) {
        QString sId = bridge->scannerList().at(i).toMap()["id"].toString();
        if (sId.contains("airy", Qt::CaseInsensitive) || sId.contains("robosense", Qt::CaseInsensitive)) {
            airy_index = i;
            break;
        }
    }
    CHECK(airy_index >= 0, "RoboSense Airy scanner not found in scanner list");
    bridge->selectScannerByIndex(airy_index);
    CHECK(bridge->selectedScannerName().contains("RoboSense"), "Selected scanner does not contain RoboSense");
    std::cout << "  Selected: " << bridge->selectedScannerName().toStdString() << std::endl;

    // ---------------------------------------------------------
    // Step 3: Verify Hardware Calibration Provenance
    // ---------------------------------------------------------
    std::cout << "[Step 3] Verifying Hardware Calibration Provenance..." << std::endl;
    auto calib = adapter->getCalibration();
    CHECK(!calib.empty(), "Calibration provenance must not be empty");
    CHECK(calib[0].source() == schemas::CalibrationSource::DEVICE_FACTORY, "Source must be DEVICE_FACTORY");
    CHECK(calib[0].value() == "FACTORY_DIFOP_EEPROM", "Value must be FACTORY_DIFOP_EEPROM");
    CHECK(calib[0].device_serial() == "AIRY-2024-99812", "Device serial must be AIRY-2024-99812");
    CHECK(calib[0].evidence().find("0.004250") != std::string::npos, "Evidence must contain measured translation");

    // ---------------------------------------------------------
    // Step 4: Start Capture
    // ---------------------------------------------------------
    std::cout << "[Step 4] Starting Capture Session..." << std::endl;
    bool start_ok = bridge->startCapture();
    CHECK(start_ok, "Failed to start capture");
    CHECK(bridge->isCapturing(), "Bridge must be capturing");
    CHECK(bridge->captureState() == "CAPTURING", "State must be CAPTURING");
    CHECK(!bridge->canStart(), "canStart must be false while capturing");
    CHECK(bridge->canStop(), "canStop must be true while capturing");
    CHECK(!bridge->canSave(), "canSave must be false while capturing");

    // ---------------------------------------------------------
    // Step 5: Stop Capture
    // ---------------------------------------------------------
    std::cout << "[Step 5] Stopping Capture Cleanly..." << std::endl;
    bool stop_ok = bridge->stopCapture();
    CHECK(stop_ok, "Failed to stop capture");
    CHECK(!bridge->isCapturing(), "Bridge must not be capturing");
    CHECK(bridge->captureState() == "STOPPED", "State must be STOPPED");
    CHECK(bridge->canSave(), "canSave must be true after stopping");

    // ---------------------------------------------------------
    // Step 6: Generate and Persist Map (PCD, Trajectory, Metadata)
    // ---------------------------------------------------------
    std::cout << "[Step 6] Persisting Map and Metadata..." << std::endl;
    std::vector<schemas::PointXYZI> test_pts;
    for (int i = 0; i < 500; ++i) {
        schemas::PointXYZI pt;
        pt.x = static_cast<float>(std::cos(i * 0.1) * (1.0 + i * 0.005));
        pt.y = static_cast<float>(std::sin(i * 0.1) * (1.0 + i * 0.005));
        pt.z = static_cast<float>(0.1 * std::sin(i * 0.2));
        pt.intensity = 15.0f + (i % 50);
        test_pts.push_back(pt);
    }

    fs::path sess_dir = test_dir / "projects" / proj_id / "sessions";
    CHECK(fs::exists(sess_dir), "Sessions directory does not exist");

    std::string sess_name;
    for (const auto& entry : fs::directory_iterator(sess_dir)) {
        if (entry.is_directory()) {
            sess_name = entry.path().filename().string();
            break;
        }
    }
    CHECK(!sess_name.empty(), "No session directory found");

    fs::path cap_dir = sess_dir / sess_name / "captures" / "cap_acceptance_01";
    fs::create_directories(cap_dir);

    // Write PCD
    fs::path pcd_path = cap_dir / "map.pcd";
    std::ofstream pcd_out(pcd_path);
    pcd_out << "# .PCD v0.7 - Point Cloud Data file format\n"
            << "VERSION 0.7\n"
            << "FIELDS x y z intensity\n"
            << "SIZE 4 4 4 4\n"
            << "TYPE F F F F\n"
            << "COUNT 1 1 1 1\n"
            << "WIDTH " << test_pts.size() << "\n"
            << "HEIGHT 1\n"
            << "VIEWPOINT 0 0 0 1 0 0 0\n"
            << "POINTS " << test_pts.size() << "\n"
            << "DATA ascii\n";
    for (const auto& p : test_pts) {
        pcd_out << p.x << " " << p.y << " " << p.z << " " << p.intensity << "\n";
    }
    pcd_out.close();

    // Write Trajectory
    fs::path traj_path = cap_dir / "trajectory.json";
    std::ofstream traj_out(traj_path);
    traj_out << "[\n"
             << "  {\"timestamp_ns\": 1000000, \"x\": 0.0, \"y\": 0.0, \"z\": 0.0, \"qx\": 0.0, \"qy\": 0.0, \"qz\": 0.0, \"qw\": 1.0},\n"
             << "  {\"timestamp_ns\": 2000000, \"x\": 0.5, \"y\": 0.2, \"z\": 0.05, \"qx\": 0.0, \"qy\": 0.0, \"qz\": 0.087, \"qw\": 0.996}\n"
             << "]\n";
    traj_out.close();

    // ---------------------------------------------------------
    // Step 7: Reopen Project & Restore Saved Map
    // ---------------------------------------------------------
    std::cout << "[Step 7] Reopening Project without Active Scanner..." << std::endl;
    auto bridge_cold = std::make_unique<av::gui::QmlBridge>(storage, platform);
    bool open_ok = bridge_cold->openProject(QString::fromStdString(proj_id));
    CHECK(open_ok, "Failed to open project in cold bridge");

    // Verify map is restored
    CHECK(bridge_cold->hasMapData(), "Bridge must indicate hasMapData is true");
    CHECK(bridge_cold->pointsCaptured() == static_cast<qlonglong>(test_pts.size()), "Point count mismatch");
    auto display_pts = bridge_cold->getDisplayPoints(1000);
    CHECK(display_pts.size() == 500, "Display points size mismatch");

    auto restored_traj = bridge_cold->getTrajectoryPoints();
    CHECK(restored_traj.size() == 2, "Trajectory poses size mismatch");

    std::cout << "  Restored Map Points: " << bridge_cold->pointsCaptured() << std::endl;
    std::cout << "  Restored Trajectory Poses: " << restored_traj.size() << std::endl;
    std::cout << "  3D / 2D Canvas hasMapData: " << (bridge_cold->hasMapData() ? "TRUE" : "FALSE") << std::endl;

    // ---------------------------------------------------------
    // Step 8: Verify Saved Capture Schema Record
    // ---------------------------------------------------------
    std::cout << "[Step 8] Verifying Capture Schema Record..." << std::endl;
    schemas::Capture cap("cap_acceptance_01", sess_name, "robosense_airy_01", 1000000ULL);
    cap.set_observation_count(test_pts.size());
    cap.set_map_path(pcd_path.string());
    cap.set_trajectory_path(traj_path.string());
    cap.set_provenance(calib[0]);
    CHECK(storage->saveCapture(cap), "Failed to save capture record");

    schemas::Capture loaded_cap;
    CHECK(storage->loadCapture("cap_acceptance_01", loaded_cap), "Failed to load capture record");
    CHECK(loaded_cap.observation_count() == 500, "Observation count mismatch");
    CHECK(loaded_cap.provenance().device_serial() == "AIRY-2024-99812", "Provenance serial mismatch");
    CHECK(loaded_cap.provenance().source() == schemas::CalibrationSource::DEVICE_FACTORY, "Provenance source mismatch");

    std::cout << "======================================================" << std::endl;
    std::cout << "All M3 RoboSense Airy Acceptance Tests PASSED!" << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
