#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <cassert>
#include <sstream>
#include <QCoreApplication>
#include <nlohmann/json.hpp>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/schemas/spatial_data.hpp"
#include "core/schemas/scanner_node.hpp"
#include "platform/linux/linux_platform.hpp"
#include "core/drivers/airy/airy_sensor_adapter.hpp"
#include "core/slam/airy/airy_lio_backend.hpp"
#include "core/drivers/gemini/gemini_sensor_adapter.hpp"
#include "core/slam/gemini/gemini_rgbd_backend.hpp"
#include "core/drivers/viture/viture_sensor_adapter.hpp"
#include "core/slam/viture/viture_vio_backend.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace av;
using namespace av::core;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[FAILED] " << message << " (at line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "==========================================================" << std::endl;
    std::cout << "Running AV Scan — Milestone 6 Multi-Sensor / One Node Qualification" << std::endl;
    std::cout << "==========================================================" << std::endl;

#ifdef AV_SOURCE_DIR
    fs::path repo_root = AV_SOURCE_DIR;
#else
    fs::path repo_root = "/home/scanar/av_scan";
#endif

    fs::path test_dir = fs::current_path() / "test_m6_sandbox";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);

    // Copy scanner configs and profiles to test sandbox
    fs::create_directories(test_dir / "scanner_configs");
    fs::create_directories(test_dir / "profiles");
    fs::create_directories(test_dir / "scanner_nodes");

    if (fs::exists(repo_root / "scanner_configs")) {
        fs::copy(repo_root / "scanner_configs", test_dir / "scanner_configs", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    if (fs::exists(repo_root / "profiles")) {
        fs::copy(repo_root / "profiles", test_dir / "profiles", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    if (fs::exists(repo_root / "scanner_nodes")) {
        fs::copy(repo_root / "scanner_nodes", test_dir / "scanner_nodes", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }

    auto storage = std::make_shared<storage::StorageEngine>();
    storage->initialize(test_dir.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto airy_adapter = std::make_shared<drivers::airy::AirySensorAdapter>();
    auto airy_backend = std::make_shared<slam::airy::AiryLioBackend>();
    auto gemini_adapter = std::make_shared<drivers::gemini::GeminiSensorAdapter>();
    auto gemini_backend = std::make_shared<slam::gemini::GeminiRgbdBackend>();
    auto viture_adapter = std::make_shared<drivers::viture::VitureSensorAdapter>();
    auto viture_backend = std::make_shared<slam::viture::VitureVioBackend>();

    // ------------------------------------------------------------------------
    // Step 1: ScannerNode Schema & Persistence Qualification
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 1] Verifying ScannerNode Model and Storage Engine Persistence..." << std::endl;
    {
        core::schemas::ScannerNode node("scanar-01", "orin-nano", "JETSON_LINUX", "127.0.0.1");
        node.set_role("PRIMARY_ORCHESTRATOR");
        node.set_status("ONLINE");

        core::schemas::SensorInstance airy("robosense_airy_01", "scanar-01", "airy_profile_validated");
        airy.set_serial_number("AIRY-2024-99812");
        airy.set_transport_endpoint("192.168.1.200:6699");
        airy.set_mount_label("TOP_MOUNT");
        airy.set_status(core::schemas::SensorStatus::CONNECTED);
        node.add_sensor(airy);

        core::schemas::SensorInstance gemini("orbbec_gemini_336l_01", "scanar-01", "gemini_336l_validated");
        gemini.set_serial_number("CPC6463000XW");
        gemini.set_transport_endpoint("/dev/video0");
        gemini.set_mount_label("FRONT_FACING");
        gemini.set_status(core::schemas::SensorStatus::CONNECTED);
        node.add_sensor(gemini);

        CHECK(storage->saveScannerNode(node), "Saving ScannerNode to storage");

        core::schemas::ScannerNode loaded_node;
        CHECK(storage->loadScannerNode("scanar-01", loaded_node), "Loading ScannerNode from storage");
        CHECK(loaded_node.node_id() == "scanar-01", "Node ID match");
        CHECK(loaded_node.platform() == "JETSON_LINUX", "Platform match");
        CHECK(loaded_node.attached_sensors().size() == 2, "Sensor instance count match (2 sensors attached)");
        CHECK(loaded_node.attached_sensors()[0].instance_id() == "robosense_airy_01", "Airy sensor instance present");
        CHECK(loaded_node.attached_sensors()[1].instance_id() == "orbbec_gemini_336l_01", "Gemini sensor instance present");

        auto nodeList = storage->listScannerNodes();
        CHECK(!nodeList.empty(), "Storage lists saved scanner nodes");
        std::cout << "  ✓ ScannerNode persisted and verified: 1 Node hosting 2 physical sensor pipelines." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 2: Extrinsics UNKNOWN Verification (No Premature Fusion)
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 2] Verifying Scanner Configuration Extrinsics Status..." << std::endl;
    {
        core::schemas::ScannerConfig cfg;
        CHECK(storage->loadScannerConfig("orin_dual_airy_gemini", cfg), "Loading default dual scanner config");
        CHECK(cfg.extrinsic_status() == core::schemas::ExtrinsicStatus::UNKNOWN, "Extrinsic status must strictly be UNKNOWN in M6");
        CHECK(cfg.relationship_type() == core::schemas::SensorRelationshipType::RIGID, "Relationship type is RIGID");
        CHECK(cfg.relative_transforms().empty(), "Relative transforms dictionary must be empty (no CAD transform allowed)");
        std::cout << "  ✓ Extrinsic status verified as UNKNOWN. Relative transform is empty. Spatial fusion prohibited." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 3: Platform Host Telemetry (CPU & GPU Load)
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 3] Verifying Host System Telemetry (CPU & GPU Load)..." << std::endl;
    {
        auto metrics = platform->getHostMetrics();
        std::cout << "  -> Total RAM: " << (metrics.total_ram_bytes / (1024*1024)) << " MB" << std::endl;
        std::cout << "  -> Available RAM: " << (metrics.available_ram_bytes / (1024*1024)) << " MB" << std::endl;
        std::cout << "  -> CPU Utilization: " << metrics.cpu_utilization_percent << "%" << std::endl;
        std::cout << "  -> GPU Utilization: " << metrics.gpu_utilization_percent << "%" << std::endl;
        CHECK(metrics.cpu_utilization_percent >= 0.0 && metrics.cpu_utilization_percent <= 100.0, "CPU percent within [0, 100]");
        CHECK(metrics.gpu_utilization_percent >= 0.0 && metrics.gpu_utilization_percent <= 100.0, "GPU percent within [0, 100]");
        std::cout << "  ✓ Host CPU & GPU metrics read successfully." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 4: QmlBridge Multi-Sensor Mode & Viewport Switching
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 4] Verifying QmlBridge Dual-Sensor Operations..." << std::endl;
    {
        auto bridge = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, nullptr, nullptr
        );

        // Select scanR dual configuration
        bridge->selectScannerById("orin_dual_airy_gemini");
        CHECK(bridge->isDualSensorMode(), "Bridge correctly flags isDualSensorMode for scanR dual config");
        CHECK(bridge->activeDisplaySensor() == "AIRY", "Default active display sensor is AIRY");

        // Create a test project
        CHECK(bridge->createProject("M6 Dual Test Project", "Testing M6 simultaneous capture"), "Project created");
        QString pid = bridge->projectId();
        CHECK(!pid.isEmpty(), "Project ID is populated");

        // Simulate multi-sensor buffers populated during capture
        // Inject Airy points (Map A)
        std::vector<core::schemas::PointXYZI> airy_pts;
        for (int i = 0; i < 1500; ++i) {
            core::schemas::PointXYZI pt;
            pt.x = static_cast<float>(i * 0.01);
            pt.y = 0.5f;
            pt.z = 0.2f;
            pt.intensity = 100.0f;
            airy_pts.push_back(pt);
        }

        // Inject Gemini points (Map B)
        std::vector<core::schemas::PointXYZI> gemini_pts;
        for (int i = 0; i < 800; ++i) {
            core::schemas::PointXYZI pt;
            pt.x = static_cast<float>(i * 0.005);
            pt.y = -0.3f;
            pt.z = 0.8f;
            pt.intensity = 50.0f;
            pt.r = 255; pt.g = 120; pt.b = 60;
            pt.has_color = true;
            gemini_pts.push_back(pt);
        }

        // Save PCD files directly to verify PCD helper
        fs::path temp_airy_pcd = test_dir / "temp_airy.pcd";
        fs::path temp_gem_pcd = test_dir / "temp_gemini.pcd";
        bridge->saveMapPcd(temp_airy_pcd.string(), airy_pts);
        bridge->saveMapPcd(temp_gem_pcd.string(), gemini_pts);

        std::vector<core::schemas::PointXYZI> verify_airy;
        std::vector<core::schemas::PointXYZI> verify_gem;
        CHECK(bridge->loadMapPcd(temp_airy_pcd.string(), verify_airy), "Airy PCD loaded");
        CHECK(bridge->loadMapPcd(temp_gem_pcd.string(), verify_gem), "Gemini PCD loaded");
        CHECK(verify_airy.size() == 1500, "Airy PCD size verified");
        CHECK(verify_gem.size() == 800, "Gemini PCD size verified");
        CHECK(verify_gem[0].has_color == true, "Gemini RGB attributes retained");
        std::cout << "  ✓ Dual point clouds generated and verified independently (Map A: 1,500 pts, Map B: 800 pts)." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 5: Dual Capture Persistence Under One Session
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 5] Verifying Dual Capture Storage Organization under One Session..." << std::endl;
    {
        std::string test_pid = "proj_m6_test";
        core::schemas::Project proj(test_pid, "M6 Session Structure Verification", "", "Qualification");
        storage->saveProject(proj);

        std::string sess_id = "session_dual_01";
        core::schemas::Session sess(sess_id, test_pid, "Dual Capture Session");
        sess.set_status(core::schemas::SessionStatus::SAVED);
        sess.add_scanner_node("scanar-01");

        fs::path sess_dir = test_dir / "projects" / test_pid / "sessions" / sess_id;
        fs::path caps_dir = sess_dir / "captures";
        fs::create_directories(caps_dir);

        // Scanner node snapshot in session
        core::schemas::ScannerNode node("scanar-01", "orin-nano", "JETSON_LINUX", "127.0.0.1");
        fs::path sess_node_file = sess_dir / "scanner_node.json";
        std::ofstream nof(sess_node_file);
        nof << node.to_json().dump(2);
        nof.close();

        // Capture A: Airy LiDAR
        std::string cap_a_id = "cap_1001_airy";
        fs::path cap_a_dir = caps_dir / cap_a_id;
        fs::create_directories(cap_a_dir);

        auto bridge_helper = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, nullptr, nullptr
        );

        std::vector<core::schemas::PointXYZI> airy_pts;
        for (int i = 0; i < 2000; ++i) {
            core::schemas::PointXYZI p;
            p.x = i * 0.01f; p.y = 1.0f; p.z = 0.0f; p.intensity = 120.0f;
            airy_pts.push_back(p);
        }
        bridge_helper->saveMapPcd((cap_a_dir / "map.pcd").string(), airy_pts);

        nlohmann::json tj_a = nlohmann::json::array();
        tj_a.push_back({{"timestamp_ns", 1000000000ULL}, {"x", 0.0}, {"y", 0.0}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        tj_a.push_back({{"timestamp_ns", 2000000000ULL}, {"x", 1.0}, {"y", 0.0}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        std::ofstream tof_a(cap_a_dir / "trajectory.json");
        tof_a << tj_a.dump(2);
        tof_a.close();

        core::schemas::Capture cap_a(cap_a_id, sess_id, "robosense_airy_01", 0);
        cap_a.set_observation_count(2000);
        cap_a.set_map_path((cap_a_dir / "map.pcd").string());
        cap_a.set_trajectory_path((cap_a_dir / "trajectory.json").string());
        cap_a.set_status("COMPLETED");
        storage->saveCapture(cap_a);
        sess.add_capture(cap_a);

        // Capture B: Gemini 336L
        std::string cap_b_id = "cap_1001_gemini";
        fs::path cap_b_dir = caps_dir / cap_b_id;
        fs::create_directories(cap_b_dir);

        std::vector<core::schemas::PointXYZI> gem_pts;
        for (int i = 0; i < 1200; ++i) {
            core::schemas::PointXYZI p;
            p.x = i * 0.005f; p.y = -0.5f; p.z = 0.3f; p.intensity = 45.0f;
            p.r = 200; p.g = 100; p.b = 50; p.has_color = true;
            gem_pts.push_back(p);
        }
        bridge_helper->saveMapPcd((cap_b_dir / "map.pcd").string(), gem_pts);

        nlohmann::json tj_b = nlohmann::json::array();
        tj_b.push_back({{"timestamp_ns", 1000000000ULL}, {"x", 0.0}, {"y", 0.0}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        tj_b.push_back({{"timestamp_ns", 2000000000ULL}, {"x", 0.5}, {"y", 0.1}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        std::ofstream tof_b(cap_b_dir / "trajectory.json");
        tof_b << tj_b.dump(2);
        tof_b.close();

        core::schemas::Capture cap_b(cap_b_id, sess_id, "orbbec_gemini_336l_01", 0);
        cap_b.set_observation_count(1200);
        cap_b.set_map_path((cap_b_dir / "map.pcd").string());
        cap_b.set_trajectory_path((cap_b_dir / "trajectory.json").string());
        cap_b.set_status("COMPLETED");
        storage->saveCapture(cap_b);
        sess.add_capture(cap_b);

        storage->saveSession(sess);

        CHECK(fs::exists(sess_node_file), "Session scanner_node.json exists");
        CHECK(fs::exists(cap_a_dir / "map.pcd"), "Airy map.pcd exists under session captures");
        CHECK(fs::exists(cap_b_dir / "map.pcd"), "Gemini map.pcd exists under session captures");
        std::cout << "  ✓ Session persisted with Scanner Node and 2 independent captures (Airy & Gemini)." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 6: Cold Reopen Qualification
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 6] Verifying Cold Reopen of Dual-Sensor Session..." << std::endl;
    {
        auto reopen_bridge = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, nullptr, nullptr
        );

        CHECK(reopen_bridge->openProject("proj_m6_test"), "Opening dual-sensor project from disk");
        CHECK(reopen_bridge->isDualSensorMode(), "Reopen recognizes dual sensor session mode");
        CHECK(reopen_bridge->pointsCaptured() == 3200, "Combined point count is 3200 (2000 Airy + 1200 Gemini)");
        CHECK(reopen_bridge->airyPointsCaptured() == 2000, "Airy independent buffer restored with 2000 points");
        CHECK(reopen_bridge->geminiPointsCaptured() == 1200, "Gemini independent buffer restored with 1200 points");

        // Default display is Airy
        CHECK(reopen_bridge->activeDisplaySensor() == "AIRY", "Default active display is AIRY");
        auto airyDisplay = reopen_bridge->getDisplayPoints(5000);
        CHECK(airyDisplay.size() == 2000, "Airy viewport returns 2000 points");

        // Toggle to Gemini
        reopen_bridge->setActiveDisplaySensor("GEMINI");
        CHECK(reopen_bridge->activeDisplaySensor() == "GEMINI", "Switched active display to GEMINI");
        auto gemDisplay = reopen_bridge->getDisplayPoints(5000);
        CHECK(gemDisplay.size() == 1200, "Gemini viewport returns 1200 points");

        // Verify independent trajectories
        reopen_bridge->setActiveDisplaySensor("AIRY");
        auto airyTraj = reopen_bridge->getTrajectoryPoints();
        CHECK(airyTraj.size() == 2, "Airy trajectory has 2 poses");

        reopen_bridge->setActiveDisplaySensor("GEMINI");
        auto gemTraj = reopen_bridge->getTrajectoryPoints();
        CHECK(gemTraj.size() == 2, "Gemini trajectory has 2 poses");

        std::cout << "  ✓ Cold reopen succeeded: Both independent maps and trajectories loaded cleanly." << std::endl;
        std::cout << "  ✓ Viewport toggle cleanly routes independent buffers without spatial fusion." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 7: Modular Composition Qualification (Arbitrary 3-Sensor Scanner Node)
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 7] Verifying Arbitrary Composition of Device Profiles into Scanner Configuration..." << std::endl;
    {
        auto comp_bridge = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, viture_adapter, viture_backend
        );

        auto availProfiles = comp_bridge->getAvailableDeviceProfiles();
        std::cout << "  -> Discovered " << availProfiles.size() << " available device profiles in storage." << std::endl;
        CHECK(availProfiles.size() >= 3, "At least 3 device profiles discovered (Airy, Gemini, Viture)");

        // Compose 3-sensor configuration: Airy + Gemini + Viture
        QStringList selectedProfiles = {
            "airy_profile_validated",
            "gemini_336l_validated",
            "viture_luma_ultra_01"
        };
        CHECK(comp_bridge->saveCustomScannerConfig("Triple Sensor Rig", selectedProfiles), "Custom 3-sensor scanner configuration created and persisted");
        QString customCfgId = comp_bridge->selectedScannerId();
        CHECK(!customCfgId.isEmpty(), "Custom 3-sensor scanner configuration is active");

        // Verify loaded config schema
        core::schemas::ScannerConfig loaded_cfg;
        CHECK(storage->loadScannerConfig(customCfgId.toStdString(), loaded_cfg), "Storage loads new 3-sensor scanner config");
        CHECK(loaded_cfg.sensor_instances().size() == 3, "Scanner config has exactly 3 attached sensors");
        CHECK(loaded_cfg.extrinsic_status() == core::schemas::ExtrinsicStatus::UNKNOWN, "Extrinsics strictly UNKNOWN in composed config");

        CHECK(comp_bridge->activeSensorCount() == 3, "Bridge reflects 3 active sensors");
        CHECK(comp_bridge->isMultiSensorMode(), "Bridge is in multi-sensor mode");

        auto activeList = comp_bridge->activeSensors();
        CHECK(activeList.size() == 3, "Active sensors QVariantList contains 3 items");
        bool hasAiry = false, hasGemini = false, hasViture = false;
        for (const auto& var : activeList) {
            auto m = var.toMap();
            if (m["id"].toString() == "AIRY") hasAiry = true;
            if (m["id"].toString() == "GEMINI") hasGemini = true;
            if (m["id"].toString() == "VITURE") hasViture = true;
        }
        CHECK(hasAiry && hasGemini && hasViture, "All three sensors present in active runtime context");
        std::cout << "  ✓ Arbitrary 3-sensor composition verified: RoboSense Airy + Gemini 336L + VITURE Ultra." << std::endl;
    }

    // ------------------------------------------------------------------------
    // Step 8: 3-Sensor Simultaneous Capture Persistence and Cold Reopen
    // ------------------------------------------------------------------------
    std::cout << "\n[Step 8] Verifying 3-Sensor Independent Persistence and Cold Reopen..." << std::endl;
    {
        std::string tri_pid = "proj_m6_tri_test";
        core::schemas::Project proj(tri_pid, "M6 Tri-Sensor Session Structure", "", "Qualification");
        storage->saveProject(proj);

        std::string tri_sess_id = "session_tri_01";
        core::schemas::Session sess(tri_sess_id, tri_pid, "Tri-Sensor Capture Session");
        sess.set_status(core::schemas::SessionStatus::SAVED);
        sess.add_scanner_node("scanar-01");

        fs::path sess_dir = test_dir / "projects" / tri_pid / "sessions" / tri_sess_id;
        fs::path caps_dir = sess_dir / "captures";
        fs::create_directories(caps_dir);

        // Scanner node snapshot with 3 sensors
        core::schemas::ScannerNode node("scanar-01", "orin-nano", "JETSON_LINUX", "127.0.0.1");
        core::schemas::SensorInstance airy("robosense_airy_01", "scanar-01", "airy_profile_validated");
        core::schemas::SensorInstance gemini("orbbec_gemini_336l_01", "scanar-01", "gemini_336l_validated");
        core::schemas::SensorInstance viture("viture_luma_ultra_01", "scanar-01", "viture_luma_ultra_01");
        node.add_sensor(airy);
        node.add_sensor(gemini);
        node.add_sensor(viture);

        std::ofstream nof(sess_dir / "scanner_node.json");
        nof << node.to_json().dump(2);
        nof.close();

        auto helper = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, viture_adapter, viture_backend
        );

        // Capture 1: Airy
        std::string cap1_id = "cap_2001_airy";
        fs::path cap1_dir = caps_dir / cap1_id;
        fs::create_directories(cap1_dir);
        std::vector<core::schemas::PointXYZI> airy_pts;
        for (int i = 0; i < 3000; ++i) {
            core::schemas::PointXYZI p; p.x = i * 0.01f; p.y = 1.0f; p.z = 0.0f; airy_pts.push_back(p);
        }
        helper->saveMapPcd((cap1_dir / "map.pcd").string(), airy_pts);
        nlohmann::json tj1 = nlohmann::json::array();
        tj1.push_back({{"timestamp_ns", 1000000000ULL}, {"x", 0.0}, {"y", 0.0}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        std::ofstream tof1(cap1_dir / "trajectory.json");
        tof1 << tj1.dump(2);
        tof1.close();
        core::schemas::Capture cap1(cap1_id, tri_sess_id, "robosense_airy_01", 0);
        cap1.set_observation_count(3000);
        cap1.set_map_path((cap1_dir / "map.pcd").string());
        cap1.set_trajectory_path((cap1_dir / "trajectory.json").string());
        cap1.set_status("COMPLETED");
        storage->saveCapture(cap1);
        sess.add_capture(cap1);

        // Capture 2: Gemini
        std::string cap2_id = "cap_2001_gemini";
        fs::path cap2_dir = caps_dir / cap2_id;
        fs::create_directories(cap2_dir);
        std::vector<core::schemas::PointXYZI> gem_pts;
        for (int i = 0; i < 2000; ++i) {
            core::schemas::PointXYZI p; p.x = i * 0.005f; p.y = -0.5f; p.z = 0.3f; p.has_color = true; gem_pts.push_back(p);
        }
        helper->saveMapPcd((cap2_dir / "map.pcd").string(), gem_pts);
        nlohmann::json tj2 = nlohmann::json::array();
        tj2.push_back({{"timestamp_ns", 1000000000ULL}, {"x", 0.5}, {"y", 0.1}, {"z", 0.0}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        std::ofstream tof2(cap2_dir / "trajectory.json");
        tof2 << tj2.dump(2);
        tof2.close();
        core::schemas::Capture cap2(cap2_id, tri_sess_id, "orbbec_gemini_336l_01", 0);
        cap2.set_observation_count(2000);
        cap2.set_map_path((cap2_dir / "map.pcd").string());
        cap2.set_trajectory_path((cap2_dir / "trajectory.json").string());
        cap2.set_status("COMPLETED");
        storage->saveCapture(cap2);
        sess.add_capture(cap2);

        // Capture 3: Viture
        std::string cap3_id = "cap_2001_viture";
        fs::path cap3_dir = caps_dir / cap3_id;
        fs::create_directories(cap3_dir);
        std::vector<core::schemas::PointXYZI> vit_pts;
        for (int i = 0; i < 1500; ++i) {
            core::schemas::PointXYZI p; p.x = i * 0.002f; p.y = 0.2f; p.z = 0.9f; p.has_color = true; vit_pts.push_back(p);
        }
        helper->saveMapPcd((cap3_dir / "map.pcd").string(), vit_pts);
        nlohmann::json tj3 = nlohmann::json::array();
        tj3.push_back({{"timestamp_ns", 1000000000ULL}, {"x", 0.2}, {"y", 0.2}, {"z", 0.9}, {"qx", 0.0}, {"qy", 0.0}, {"qz", 0.0}, {"qw", 1.0}});
        std::ofstream tof3(cap3_dir / "trajectory.json");
        tof3 << tj3.dump(2);
        tof3.close();
        core::schemas::Capture cap3(cap3_id, tri_sess_id, "viture_luma_ultra_01", 0);
        cap3.set_observation_count(1500);
        cap3.set_map_path((cap3_dir / "map.pcd").string());
        cap3.set_trajectory_path((cap3_dir / "trajectory.json").string());
        cap3.set_status("COMPLETED");
        storage->saveCapture(cap3);
        sess.add_capture(cap3);

        storage->saveSession(sess);

        // Cold Reopen of Tri-Sensor Session
        auto tri_reopen_bridge = std::make_unique<av::gui::QmlBridge>(
            storage, platform, airy_adapter, airy_backend, gemini_adapter, gemini_backend, viture_adapter, viture_backend
        );

        CHECK(tri_reopen_bridge->openProject(QString::fromStdString(tri_pid)), "Cold reopen of tri-sensor project");
        CHECK(tri_reopen_bridge->activeSensorCount() == 3, "Cold reopen recognized all 3 active sensors");
        CHECK(tri_reopen_bridge->pointsCaptured() == 6500, "Combined point count is 6500 (3000 + 2000 + 1500)");

        // Viewport switching between all 3 sensors
        tri_reopen_bridge->setActiveDisplaySensor("AIRY");
        CHECK(tri_reopen_bridge->getDisplayPoints(10000).size() == 3000, "Airy buffer returns 3000 points");

        tri_reopen_bridge->setActiveDisplaySensor("GEMINI");
        CHECK(tri_reopen_bridge->getDisplayPoints(10000).size() == 2000, "Gemini buffer returns 2000 points");

        tri_reopen_bridge->setActiveDisplaySensor("VITURE");
        CHECK(tri_reopen_bridge->getDisplayPoints(10000).size() == 1500, "Viture buffer returns 1500 points");

        std::cout << "  ✓ Tri-sensor cold reopen passed: 3 captures independently restored and rendered without fusion." << std::endl;
    }

    // Cleanup sandbox
    fs::remove_all(test_dir);

    std::cout << "\n==========================================================" << std::endl;
    std::cout << "Milestone 6 Multi-Sensor / One Scanner Node PASSED!" << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
