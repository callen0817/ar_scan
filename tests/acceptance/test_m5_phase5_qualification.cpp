#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <QCoreApplication>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/drivers/viture/viture_sensor_adapter.hpp"
#include "core/slam/viture/viture_vio_backend.hpp"
#include "core/drivers/viture/viture_ipc_client.hpp"
#include "platform/linux/linux_platform.hpp"

namespace fs = std::filesystem;
using namespace av::core;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FAIL] " message << " (at line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "==========================================================" << std::endl;
    std::cout << "AV Scan — Milestone 5 Phase 5 Live Acceptance Qualification" << std::endl;
    std::cout << "==========================================================" << std::endl;

#ifdef AV_SOURCE_DIR
    fs::path repo_root = AV_SOURCE_DIR;
#else
    fs::path repo_root = "/home/scanar/av_scan";
#endif

    fs::path test_dir = fs::current_path() / "test_m5_p5_sandbox";
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

    auto client = std::make_unique<drivers::viture::VitureIpcClient>();

    // ---------------------------------------------------------
    // Phase 5 Gate 1: Hardware Interrogation & Backend Verification
    // ---------------------------------------------------------
    std::cout << "[Step 1] Connecting to VITURE Luma Ultra and verifying backend..." << std::endl;
    CHECK(client->ensureDaemonRunning(), "Failed to connect to isolated daemon");

    nlohmann::json hw_info;
    CHECK(client->identify(hw_info), "Failed to interrogate hardware");
    std::cout << "  ✓ Hardware Identified: " << hw_info.value("model", "")
              << " (SN: " << hw_info.value("serial", "")
              << ", FW: " << hw_info.value("firmware", "") << ")" << std::endl;

    auto calib = adapter->getCalibration();
    CHECK(!calib.empty(), "Calibration provenance must not be empty");
    std::cout << "  ✓ Calibration Provenance: " << calib[0].device_serial()
              << " (" << calib[0].value() << ")" << std::endl;

    // ---------------------------------------------------------
    // Phase 5 Gate 2: GPU Foundation Model Pre-flight Readiness
    // ---------------------------------------------------------
    std::cout << "[Step 2] Waiting for LingBot foundation model to become resident on GPU..." << std::endl;
    CHECK(client->waitForModelReady(25000), "LingBot model failed to become ready on GPU");
    
    drivers::viture::VitureTelemetry pre_telem;
    client->getTelemetry(pre_telem);
    std::cout << "  ✓ Foundation Model Resident on GPU (VRAM: "
              << pre_telem.gpu_memory_gb << " GB allocated)." << std::endl;

    // ---------------------------------------------------------
    // Phase 5 Gate 3: Live Acquisition (Test A - Stationary Desk Scene)
    // ---------------------------------------------------------
    std::cout << "[Step 3] Initializing AV Scan GUI Bridge & Starting Live Capture..." << std::endl;
    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform, nullptr, nullptr, nullptr, nullptr, adapter, backend);

    CHECK(bridge->createProject("VITURE Phase 5 Acceptance", "Live Desk Qualification Scan"), "Failed to create project");
    std::string proj_id = bridge->projectId().toStdString();

    // Select VITURE profile
    int viture_idx = -1;
    for (int i = 0; i < bridge->scannerList().size(); ++i) {
        QString sId = bridge->scannerList().at(i).toMap()["id"].toString();
        if (sId.contains("viture", Qt::CaseInsensitive)) {
            viture_idx = i;
            break;
        }
    }
    CHECK(viture_idx >= 0, "VITURE profile not found");
    bridge->selectScannerByIndex(viture_idx);

    CHECK(bridge->startCapture(), "Failed to start capture");
    CHECK(bridge->isCapturing(), "Capture state must be active");
    std::cout << "  ✓ Capture started cleanly. Streaming spatial data (18 seconds)..." << std::endl;

    for (int s = 1; s <= 18; ++s) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        backend->updateTelemetry();
        backend->fetchNewPoints();

        auto pose = backend->getPose();
        auto pts = backend->getMapPoints();
        drivers::viture::VitureTelemetry telem;
        client->getTelemetry(telem);

        std::cout << "  [" << (s < 10 ? "0" : "") << s << "s] Pose: ("
                  << pose.position.x << ", " << pose.position.y << ", " << pose.position.z
                  << ") | Carina VIO: " << backend->getSlamRate() << " Hz | State: "
                  << schemas::to_string(backend->getTrackingState())
                  << " | 3D Points: " << pts.size()
                  << " | GPU VRAM: " << telem.gpu_memory_gb << " GB" << std::endl;
    }

    // ---------------------------------------------------------
    // Phase 5 Gate 4: Stopping Capture & Inspecting Live Map
    // ---------------------------------------------------------
    std::cout << "[Step 4] Stopping capture and evaluating reconstructed geometry..." << std::endl;
    CHECK(bridge->stopCapture(), "Failed to stop capture");

    auto final_pts = backend->getMapPoints();
    auto final_traj = backend->getTrajectory();

    CHECK(final_pts.size() > 500, "Point cloud count insufficient for physical scene");
    std::cout << "  ✓ Total 3D Map Points Reconstructed: " << final_pts.size() << std::endl;
    std::cout << "  ✓ Total Carina 6-DoF VIO Poses      : " << final_traj.size() << std::endl;

    // Verify 100% authentic RGB coloration
    size_t authentic_color_cnt = 0;
    float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
    float min_x = 1e6f, max_x = -1e6f;
    for (const auto& p : final_pts) {
        if (p.has_color) authentic_color_cnt++;
        sum_x += p.x;
        sum_y += p.y;
        sum_z += p.z;
        if (p.x < min_x) min_x = p.x;
        if (p.x > max_x) max_x = p.x;
    }

    float mean_x = sum_x / final_pts.size();
    float mean_y = sum_y / final_pts.size();
    float mean_z = sum_z / final_pts.size();

    std::cout << "  ✓ Authentic Camera RGB Count        : " << authentic_color_cnt
              << " / " << final_pts.size() << " (100.0%)" << std::endl;
    std::cout << "  ✓ Metric Forward Distance (X_av)    : Mean=" << mean_x
              << " m [Range: " << min_x << " m to " << max_x << " m]" << std::endl;
    std::cout << "  ✓ Lateral & Vertical Centroid       : Y=" << mean_y << " m, Z=" << mean_z << " m" << std::endl;

    // Verify realistic forward desk distance (0.4m to 1.2m)
    CHECK(mean_x >= 0.4f && mean_x <= 1.2f, "Forward desk distance out of realistic range");
    CHECK(authentic_color_cnt == final_pts.size(), "Points must have authentic RGB color");

    // ---------------------------------------------------------
    // Phase 5 Gate 5: Persistence & Cold Reopen (Test C)
    // ---------------------------------------------------------
    std::cout << "[Step 5] Persisting capture to disk..." << std::endl;
    CHECK(bridge->saveCapture(), "Failed to persist capture");
    std::cout << "  ✓ Capture successfully persisted to storage." << std::endl;

    // Locate persisted capture files
    fs::path sess_dir = test_dir / "projects" / proj_id / "sessions";
    CHECK(fs::exists(sess_dir), "Sessions directory missing");
    std::string sess_name;
    for (const auto& entry : fs::directory_iterator(sess_dir)) {
        if (entry.is_directory()) {
            sess_name = entry.path().filename().string();
            break;
        }
    }
    CHECK(!sess_name.empty(), "Session missing");
    fs::path cap_dir = sess_dir / sess_name / "captures" / ("cap_" + sess_name.substr(8) + "_viture");
    if (!fs::exists(cap_dir)) {
        // Fallback search
        for (const auto& entry : fs::directory_iterator(sess_dir / sess_name / "captures")) {
            if (entry.is_directory()) {
                cap_dir = entry.path();
                break;
            }
        }
    }
    CHECK(fs::exists(cap_dir / "map.pcd"), "map.pcd missing from storage");
    CHECK(fs::exists(cap_dir / "trajectory.json"), "trajectory.json missing from storage");
    CHECK(fs::exists(cap_dir / "map_metadata.json"), "map_metadata.json missing from storage");
    std::cout << "  ✓ Verified files: map.pcd, trajectory.json, map_metadata.json" << std::endl;

    // Reopen in completely fresh bridge instance
    std::cout << "  Simulating cold application reopen..." << std::endl;
    auto fresh_bridge = std::make_unique<av::gui::QmlBridge>(storage, platform);
    CHECK(fresh_bridge->openProject(QString::fromStdString(proj_id)), "Failed to reopen project");
    CHECK(fresh_bridge->pointsCaptured() > 0, "No points restored upon cold reopen");
    std::cout << "  ✓ Project Reopened: " << fresh_bridge->projectName().toStdString()
              << " with " << fresh_bridge->pointsCaptured() << " restored points." << std::endl;

    // Export PCD copy to scratch for visualization artifact generation
    fs::path scratch_pcd = "/home/scanar/av_scan/scratch/phase5_live_acceptance_map.pcd";
    fs::copy_file(cap_dir / "map.pcd", scratch_pcd, fs::copy_options::overwrite_existing);
    fs::path scratch_traj = "/home/scanar/av_scan/scratch/phase5_live_acceptance_trajectory.json";
    fs::copy_file(cap_dir / "trajectory.json", scratch_traj, fs::copy_options::overwrite_existing);
    std::cout << "  ✓ Exported acceptance data to scratch for artifact rendering." << std::endl;

    std::cout << "==========================================================" << std::endl;
    std::cout << "MILESTONE 5 PHASE 5 LIVE ACCEPTANCE QUALIFICATION SUCCESSFUL!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
