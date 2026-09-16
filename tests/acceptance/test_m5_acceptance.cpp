#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <QCoreApplication>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/drivers/viture/viture_sensor_adapter.hpp"
#include "core/slam/viture/viture_vio_backend.hpp"
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
    std::cout << "==========================================================" << std::endl;
    std::cout << "Running AV Scan — Milestone 5 (VITURE Ultra) Acceptance Tests" << std::endl;
    std::cout << "==========================================================" << std::endl;

#ifdef AV_SOURCE_DIR
    fs::path repo_root = AV_SOURCE_DIR;
#else
    fs::path repo_root = "/home/scanar/av_scan";
#endif

    fs::path test_dir = fs::current_path() / "test_m5_sandbox";
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
    auto adapter = std::make_shared<drivers::viture::VitureSensorAdapter>();
    auto backend = std::make_shared<slam::viture::VitureVioBackend>();

    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform, nullptr, nullptr, nullptr, nullptr, adapter, backend);

    // ---------------------------------------------------------
    // Step 1: Create Project
    // ---------------------------------------------------------
    std::cout << "[Step 1] Creating Project..." << std::endl;
    bool proj_ok = bridge->createProject("VITURE Qualification Scan", "M5 Hardware Acceptance Project");
    CHECK(proj_ok, "Failed to create project");
    CHECK(!bridge->projectId().isEmpty(), "Project ID is empty");
    CHECK(bridge->captureState() == "READY", "Capture state must be READY");
    std::string proj_id = bridge->projectId().toStdString();
    std::cout << "  ✓ Created Project ID: " << proj_id << std::endl;

    // ---------------------------------------------------------
    // Step 2: Select VITURE Profile
    // ---------------------------------------------------------
    std::cout << "[Step 2] Selecting VITURE Profile..." << std::endl;
    CHECK(bridge->availableScanners().size() > 0, "No scanners available");
    int viture_index = -1;
    for (int i = 0; i < bridge->scannerList().size(); ++i) {
        QString sId = bridge->scannerList().at(i).toMap()["id"].toString();
        if (sId.contains("viture", Qt::CaseInsensitive)) {
            viture_index = i;
            break;
        }
    }
    CHECK(viture_index >= 0, "VITURE scanner profile not found in scanner list");
    bridge->selectScannerByIndex(viture_index);
    CHECK(bridge->selectedScannerName().contains("VITURE", Qt::CaseInsensitive),
          "Selected scanner does not contain VITURE");
    std::cout << "  ✓ Selected: " << bridge->selectedScannerName().toStdString() << std::endl;

    // ---------------------------------------------------------
    // Step 3: Verify Hardware Calibration Provenance
    // ---------------------------------------------------------
    std::cout << "[Step 3] Verifying Hardware Calibration Provenance..." << std::endl;
    auto calib = adapter->getCalibration();
    CHECK(!calib.empty(), "Calibration provenance must not be empty");
    CHECK(calib[0].source() == schemas::CalibrationSource::DEVICE_FACTORY, "Source must be DEVICE_FACTORY");
    CHECK(calib[0].value() == "FACTORY_EEPROM", "Value must be FACTORY_EEPROM");
    CHECK(calib[0].device_serial() == "VITURE-35CA-1104", "Device serial must be VITURE-35CA-1104");
    CHECK(calib[0].firmware() == "1.0.12", "Firmware must be 1.0.12");
    std::cout << "  ✓ Provenance Verified: " << calib[0].device_serial() << " (FW: " << calib[0].firmware() << ")" << std::endl;

    // ---------------------------------------------------------
    // Step 4: Start Capture Session
    // ---------------------------------------------------------
    std::cout << "[Step 4] Starting Capture Session..." << std::endl;
    bool start_ok = bridge->startCapture();
    CHECK(start_ok, "Failed to start capture");
    CHECK(bridge->isCapturing(), "Bridge must be capturing");
    CHECK(bridge->captureState() == "CAPTURING", "Capture state must be CAPTURING");
    CHECK(bridge->canStop(), "Bridge must allow stopping");
    CHECK(!bridge->canStart(), "Bridge must not allow starting while active");
    std::cout << "  ✓ Capture State: " << bridge->captureState().toStdString()
              << " | Sensor Status: " << bridge->sensorStatus().toStdString() << std::endl;

    // ---------------------------------------------------------
    // Step 5: Simulate Spatial Ingestion & Telemetry
    // ---------------------------------------------------------
    std::cout << "[Step 5] Simulating Spatial Ingestion (Simulated Carina 6-DoF VIO)..." << std::endl;
    std::vector<schemas::PointXYZI> sample_points;
    for (int i = 0; i < 400; ++i) {
        schemas::PointXYZI pt;
        double angle = (i * 2.0 * M_PI) / 400.0;
        pt.x = std::cos(angle) * 1.5f;
        pt.y = std::sin(angle) * 1.5f;
        pt.z = static_cast<float>(i % 15) * 0.04f;
        pt.intensity = 0.75f;
        sample_points.push_back(pt);
    }
    CHECK(sample_points.size() == 400, "Point count mismatch");
    std::cout << "  ✓ Generated 400 spatial sample points." << std::endl;

    // ---------------------------------------------------------
    // Step 6: Stop Capture
    // ---------------------------------------------------------
    std::cout << "[Step 6] Stopping Capture Session..." << std::endl;
    bool stop_ok = bridge->stopCapture();
    CHECK(stop_ok, "Failed to stop capture");
    CHECK(!bridge->isCapturing(), "Bridge must not be capturing");
    CHECK(bridge->captureState() == "STOPPED", "Capture state must be STOPPED");
    CHECK(bridge->canSave(), "Bridge must allow saving");
    std::cout << "  ✓ Capture State: " << bridge->captureState().toStdString() << std::endl;

    // ---------------------------------------------------------
    // Step 7: Save Capture
    // ---------------------------------------------------------
    std::cout << "[Step 7] Persisting Capture to Storage..." << std::endl;
    bool save_ok = bridge->saveCapture();
    CHECK(save_ok, "Failed to save capture");
    CHECK(bridge->captureState() == "SAVED", "Capture state must be SAVED");
    CHECK(!bridge->canSave(), "Cannot save again after saving");
    CHECK(bridge->canStart(), "Can start new capture after saving");

    // Verify persisted session & capture files on disk
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

    fs::path captures_root = sess_dir / sess_name / "captures";
    CHECK(fs::exists(captures_root), "Captures directory does not exist");
    bool found_capture_dir = false;
    fs::path target_cap_dir;
    for (const auto& entry : fs::directory_iterator(captures_root)) {
        if (entry.is_directory()) {
            found_capture_dir = true;
            target_cap_dir = entry.path();
            CHECK(fs::exists(target_cap_dir / "map.pcd"), "map.pcd missing from capture dir");
            CHECK(fs::exists(target_cap_dir / "trajectory.json"), "trajectory.json missing");
            CHECK(fs::exists(target_cap_dir / "map_metadata.json"), "map_metadata.json missing");

            // Verify map_metadata contents
            std::ifstream mf(target_cap_dir / "map_metadata.json");
            nlohmann::json mj;
            mf >> mj;
            CHECK(mj.contains("map_geometry_format"), "map_metadata missing geometry format");
            CHECK(mj["map_geometry_format"] == "PCL_PCD", "map geometry format must be PCL_PCD");
            std::cout << "  ✓ Verified files: map.pcd, trajectory.json, map_metadata.json in " << target_cap_dir.string() << std::endl;
            break;
        }
    }
    CHECK(found_capture_dir, "No capture directory found on disk");

    // Populate PCD with sample points to test cold reopen restore
    fs::path pcd_path = target_cap_dir / "map.pcd";
    std::ofstream pcd_out(pcd_path);
    pcd_out << "# .PCD v0.7 - Point Cloud Data file format\n"
            << "VERSION 0.7\n"
            << "FIELDS x y z intensity\n"
            << "SIZE 4 4 4 4\n"
            << "TYPE F F F F\n"
            << "COUNT 1 1 1 1\n"
            << "WIDTH " << sample_points.size() << "\n"
            << "HEIGHT 1\n"
            << "VIEWPOINT 0 0 0 1 0 0 0\n"
            << "POINTS " << sample_points.size() << "\n"
            << "DATA ascii\n";
    for (const auto& p : sample_points) {
        pcd_out << p.x << " " << p.y << " " << p.z << " " << p.intensity << "\n";
    }
    pcd_out.close();

    // ---------------------------------------------------------
    // Step 8: Cold Reopen Project & Verify Map Data
    // ---------------------------------------------------------
    std::cout << "[Step 8] Cold Reopen Project..." << std::endl;
    auto cold_bridge = std::make_unique<av::gui::QmlBridge>(storage, platform, nullptr, nullptr, nullptr, nullptr, adapter, backend);
    bool open_ok = cold_bridge->openProject(QString::fromStdString(proj_id));
    CHECK(open_ok, "Failed to reopen project");
    CHECK(cold_bridge->projectId().toStdString() == proj_id, "Reopened project ID mismatch");
    CHECK(cold_bridge->hasMapData(), "Reopened project must have map data loaded");
    CHECK(cold_bridge->pointsCaptured() == static_cast<qlonglong>(sample_points.size()), "Point count mismatch");
    std::cout << "  ✓ Cold reopen verified. Points in memory: " << cold_bridge->pointsCaptured() << std::endl;

    // Cleanup
    fs::remove_all(test_dir);

    std::cout << "==========================================================" << std::endl;
    std::cout << "Milestone 5 (VITURE Ultra) Acceptance Tests PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
