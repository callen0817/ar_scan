#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <thread>
#include <chrono>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

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
    std::cout << "==========================================================" << std::endl;
    std::cout << "  AV SCAN — PHYSICAL HARDWARE QUALIFICATION (ROBOSENSE AIRY)  " << std::endl;
    std::cout << "==========================================================" << std::endl;

    fs::path repo_root = "/home/scanar/av_scan";
    fs::path qual_dir = repo_root / "projects" / "airy_qualification";
    fs::remove_all(qual_dir);
    fs::create_directories(qual_dir);

    auto storage = std::make_shared<storage::StorageEngine>();
    storage->initialize(repo_root.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto adapter = std::make_shared<drivers::airy::AirySensorAdapter>();
    auto backend = std::make_shared<slam::airy::AiryLioBackend>();

    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform, adapter, backend);

    // 1. Create Project
    std::cout << "[Step 1] Creating Qualification Project..." << std::endl;
    CHECK(bridge->createProject("Airy Hardware Qualification", "Real physical qualification on scanar-01 Jetson Orin NX"),
          "Failed to create project");
    std::string proj_id = bridge->projectId().toStdString();
    std::cout << "  Project ID: " << proj_id << std::endl;

    // 2. Select RoboSense Airy
    std::cout << "[Step 2] Selecting RoboSense Airy Scanner..." << std::endl;
    int airy_idx = -1;
    for (int i = 0; i < bridge->scannerList().size(); ++i) {
        QString sId = bridge->scannerList().at(i).toMap()["id"].toString();
        if (sId.contains("airy", Qt::CaseInsensitive) || sId.contains("robosense", Qt::CaseInsensitive)) {
            airy_idx = i;
            break;
        }
    }
    CHECK(airy_idx >= 0, "RoboSense Airy scanner not found");
    bridge->selectScannerByIndex(airy_idx);
    std::cout << "  Selected Scanner: " << bridge->selectedScannerName().toStdString() << std::endl;

    // 3. Connect & Verify Hardware EEPROM Provenance
    std::cout << "[Step 3] Verifying Hardware Calibration Provenance..." << std::endl;
    auto calib = adapter->getCalibration();
    CHECK(!calib.empty(), "Calibration provenance empty");
    std::cout << "  Hardware Serial: " << calib[0].device_serial() << std::endl;
    std::cout << "  Provenance: " << calib[0].value() << std::endl;
    std::cout << "  Evidence: " << calib[0].evidence() << std::endl;

    // 4. Start Real Physical Capture
    std::cout << "[Step 4] Starting Real Physical Capture (5 seconds acquisition)..." << std::endl;
    CHECK(bridge->startCapture(), "Failed to start capture");

    // Wait and process Qt events for 5.5 seconds while telemetry polls
    for (int i = 0; i < 55; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if ((i + 1) % 10 == 0) {
            std::cout << "  [" << (i + 1) * 100 << "ms] State: " << bridge->captureState().toStdString()
                      << " | Tracking: " << bridge->trackingStatus().toStdString()
                      << " | Points: " << bridge->pointsCaptured()
                      << " | Trajectory: " << bridge->trajectoryPointCount()
                      << " | Pose: (" << bridge->currentPoseX() << ", "
                      << bridge->currentPoseY() << ", " << bridge->currentPoseZ() << ")"
                      << std::endl;
        }
    }

    CHECK(bridge->pointsCaptured() > 1000, "Expected > 1000 real points from physical RoboSense Airy");
    std::cout << "  SUCCESS: Live physical Airy produced " << bridge->pointsCaptured() << " registered points!" << std::endl;

    // 5. Stop Capture
    std::cout << "[Step 5] Stopping Capture Cleanly..." << std::endl;
    CHECK(bridge->stopCapture(), "Failed to stop capture");
    CHECK(bridge->canSave(), "canSave must be true");

    // 6. Save Capture
    std::cout << "[Step 6] Persisting Real Physical Map to Disk..." << std::endl;
    CHECK(bridge->saveCapture(), "Failed to save capture");
    std::cout << "  Capture successfully persisted." << std::endl;

    // 7. Verify Files on Disk
    fs::path proj_dir = repo_root / "projects" / proj_id;
    CHECK(fs::exists(proj_dir), "Project directory does not exist");
    
    // Find capture directory
    bool found_pcd = false;
    for (const auto& entry : fs::recursive_directory_iterator(proj_dir)) {
        if (entry.is_regular_file() && entry.path().filename() == "map.pcd") {
            found_pcd = true;
            uintmax_t pcd_size = fs::file_size(entry.path());
            std::cout << "  PCD file: " << entry.path().string() << " (" << (pcd_size / 1024) << " KB)" << std::endl;
            CHECK(pcd_size > 10000, "PCD file size too small for real cloud");
            break;
        }
    }
    CHECK(found_pcd, "map.pcd not found in project captures");

    // 8. Cold Reopen Test
    std::cout << "[Step 8] Cold Project Reopening (viewing saved map without sensor scanning)..." << std::endl;
    auto bridge_cold = std::make_unique<av::gui::QmlBridge>(storage, platform);
    CHECK(bridge_cold->openProject(QString::fromStdString(proj_id)), "Failed to open project");
    CHECK(bridge_cold->hasMapData(), "Restored project must have hasMapData == true");
    CHECK(bridge_cold->pointsCaptured() > 1000, "Restored point count must match saved cloud");
    std::cout << "  Restored Points: " << bridge_cold->pointsCaptured() << std::endl;
    std::cout << "  Restored Trajectory Steps: " << bridge_cold->getTrajectoryPoints().size() << std::endl;

    std::cout << "==========================================================" << std::endl;
    std::cout << "PHYSICAL ROBOSENSE AIRY HARDWARE QUALIFICATION: PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;

    return 0;
}
