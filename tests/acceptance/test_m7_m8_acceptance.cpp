#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <cassert>
#include <QCoreApplication>

#include "core/fusion/extrinsic_calibrator.hpp"
#include "core/fusion/multi_sensor_fusion_engine.hpp"
#include "core/processing/point_cloud_processor.hpp"
#include "core/schemas/scanner_config.hpp"
#include "core/schemas/project.hpp"
#include "core/schemas/session.hpp"
#include "core/schemas/capture.hpp"
#include "core/storage/storage_engine.hpp"
#include "platform/linux/linux_platform.hpp"
#include "gui/qml_bridge.hpp"

namespace fs = std::filesystem;
using namespace av::core::fusion;
using namespace av::core::processing;
using namespace av::core::schemas;
using namespace av::core::storage;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[M7/M8 FAIL] " << message << " (line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "=================================================================" << std::endl;
    std::cout << "  AV SCAN — MILESTONES 7 & 8: AUTOMATIC EXTRINSIC CALIBRATION    " << std::endl;
    std::cout << "            & MULTI-SENSOR SPATIAL FUSION ACCEPTANCE SUITE       " << std::endl;
    std::cout << "=================================================================" << std::endl;

    fs::path repo_root = "/home/scanar/av_scan";
    if (!fs::exists(repo_root / "projects")) {
        repo_root = fs::current_path();
        while (repo_root.has_parent_path() && !fs::exists(repo_root / "projects")) {
            repo_root = repo_root.parent_path();
        }
    }

    fs::path sandbox_dir = fs::current_path() / "test_m7_m8_sandbox";
    if (fs::exists(sandbox_dir)) {
        fs::remove_all(sandbox_dir);
    }
    fs::create_directories(sandbox_dir);

    fs::path real_airy_pcd = repo_root / "projects" / "proj_1789675816776" / "sessions" / "session_1789675851740" / "captures" / "cap_1789676064983_airy" / "map.pcd";
    fs::path real_gemini_pcd = repo_root / "projects" / "proj_1789675816776" / "sessions" / "session_1789675851740" / "captures" / "cap_1789676064983_gemini" / "map.pcd";
    CHECK(fs::exists(real_airy_pcd), "Real Airy LiDAR PCD must exist");
    CHECK(fs::exists(real_gemini_pcd), "Real Gemini RGB-D PCD must exist");

    std::vector<PointXYZI> airy_cloud;
    std::vector<PointXYZI> gemini_cloud;
    CHECK(PointCloudProcessor::loadPcd(real_airy_pcd.string(), airy_cloud), "Failed to load Airy PCD");
    CHECK(PointCloudProcessor::loadPcd(real_gemini_pcd.string(), gemini_cloud), "Failed to load Gemini PCD");
    std::cout << "  Loaded Airy cloud:   " << airy_cloud.size() << " pts" << std::endl;
    std::cout << "  Loaded Gemini cloud: " << gemini_cloud.size() << " pts" << std::endl;

    // =========================================================================
    // GATE 1: ExtrinsicCalibrator — Trajectory Hand-Eye/Umeyama + Cloud GICP
    // =========================================================================
    std::cout << "\n[M7 GATE 1] Testing Automatic Rigid Extrinsic Calibration (Trajectory + Cloud GICP)..." << std::endl;

    // 1A: Trajectory calibration test with known rigid offset (tx=0.05m, ty=-0.03m, tz=0.08m)
    std::vector<Pose3D> ref_traj;
    std::vector<Pose3D> sec_traj;
    for (int i = 0; i < 40; ++i) {
        uint64_t t_ns = 1000000000ULL + static_cast<uint64_t>(i) * 100000000ULL;
        Pose3D p_ref;
        p_ref.timestamp_ns = t_ns;
        p_ref.position = {i * 0.08, std::sin(i * 0.2) * 0.3, std::cos(i * 0.15) * 0.1};
        p_ref.orientation = {0.0, 0.0, 0.0, 1.0};

        Pose3D p_sec;
        p_sec.timestamp_ns = t_ns + 5000000ULL; // 5ms offset
        p_sec.position = {p_ref.position.x - 0.05, p_ref.position.y + 0.03, p_ref.position.z - 0.08};
        p_sec.orientation = {0.0, 0.0, 0.0, 1.0};

        ref_traj.push_back(p_ref);
        sec_traj.push_back(p_sec);
    }

    Transform3D traj_tf;
    size_t matched_poses = 0;
    double traj_rmse_m = 0.0;
    bool traj_ok = ExtrinsicCalibrator::alignTrajectoriesUmeyama(ref_traj, sec_traj, traj_tf, matched_poses, traj_rmse_m);
    CHECK(traj_ok, "Trajectory Umeyama alignment should succeed");
    CHECK(matched_poses >= 35, "Should match at least 35 synchronized trajectory poses");
    CHECK(std::abs(traj_tf.translation.x - 0.05) < 0.02, "Recovered X offset within 2cm");
    CHECK(std::abs(traj_tf.translation.z - 0.08) < 0.02, "Recovered Z offset within 2cm");
    std::cout << "  Trajectory Alignment matched " << matched_poses << " poses, RMSE: " << (traj_rmse_m * 1000.0) << " mm" << std::endl;

    // 1B: Environmental Cloud Calibration on Real Airy + Gemini Captures
    ExtrinsicCalibrationResult cloud_cal = ExtrinsicCalibrator::calibrateSensorPair(
        "robosense_airy_01", "orbbec_gemini_336l_01", airy_cloud, gemini_cloud);

    std::cout << "  Cloud Calibration Pair:        " << cloud_cal.pair_key << std::endl;
    std::cout << "  Method:                        " << cloud_cal.method << std::endl;
    std::cout << "  RMSE:                          " << (cloud_cal.inlier_rmse_m * 1000.0) << " mm" << std::endl;
    std::cout << "  Inlier Overlap:                " << (cloud_cal.overlap_ratio * 100.0) << " %" << std::endl;
    std::cout << "  Confidence Score:              " << (cloud_cal.confidence * 100.0) << " %" << std::endl;

    CHECK(cloud_cal.converged, "Environmental cloud extrinsic calibration must converge on real dataset");
    CHECK(cloud_cal.overlap_ratio >= 0.80, "Overlap ratio must be >= 80%");
    CHECK(cloud_cal.inlier_rmse_m <= 0.065, "RMSE must be <= 65 mm");
    CHECK(std::abs(cloud_cal.scale_factor - 1.0) < 1e-6, "Extrinsic scale factor must be locked at 1.0");

    // Verify quaternion unit norm
    const auto& q = cloud_cal.transform.rotation;
    double q_norm = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    CHECK(std::abs(q_norm - 1.0) < 1e-4, "Extrinsic rotation quaternion must be unit norm");
    std::cout << "  [PASS] Gate 1: Trajectory & Environmental Cloud Extrinsic Calibration verified." << std::endl;

    // =========================================================================
    // GATE 2: ScannerConfig Persistence & CalibrationProvenance Verification
    // =========================================================================
    std::cout << "\n[M7 GATE 2] Testing ScannerConfig Persistence & CalibrationProvenance..." << std::endl;
    ScannerConfig cfg("scanner_m7_test", "Tri-Sensor Test Rig", "scanar-01");
    cfg.add_sensor_instance("robosense_airy_01");
    cfg.add_sensor_instance("orbbec_gemini_336l_01");

    bool persist_ok = ExtrinsicCalibrator::persistCalibrationToConfig(cfg, cloud_cal, "cap_1789676064983_gemini");
    CHECK(persist_ok, "persistCalibrationToConfig should succeed");
    CHECK(cfg.relative_transforms().count(cloud_cal.pair_key) == 1, "ScannerConfig should persist pair_key relative transform");
    CHECK(cfg.relative_transforms().count("orbbec_gemini_336l_01") == 1, "ScannerConfig should persist secondary_sensor_id relative transform");
    CHECK(cfg.extrinsic_status() == ExtrinsicStatus::VALIDATED || cfg.extrinsic_status() == ExtrinsicStatus::ESTIMATED,
          "Extrinsic status should be VALIDATED or ESTIMATED");
    CHECK(cfg.provenance().source() == CalibrationSource::AV_CALIBRATED,
          "Provenance source should be AV_CALIBRATED");
    std::cout << "  [PASS] Gate 2: ScannerConfig relative_transforms and provenance verified." << std::endl;

    // =========================================================================
    // GATE 3: MultiSensorFusionEngine — Spatial Alignment & RGB Color Projection
    // =========================================================================
    std::cout << "\n[M8 GATE 3] Testing Multi-Sensor Spatial Fusion & Optical RGB Color Transfer..." << std::endl;
    MultiSensorFusionEngine fusion_engine;
    MultiSensorFusionParams fparams;
    fparams.voxel_leaf_size_m = 0.025f;
    fparams.transfer_optical_rgb_to_lidar = true;
    fparams.rgb_transfer_radius_m = 0.10f;
    fparams.color_by_sensor_role = false;

    std::vector<SensorFusionInput> inputs = {
        {"robosense_airy_01", "AIRY", airy_cloud, {}, false, 1.0f},
        {"orbbec_gemini_336l_01", "GEMINI", gemini_cloud, {}, true, 0.8f}
    };

    MultiSensorFusionResult fresult = fusion_engine.fuseSensors(
        inputs, cfg, fparams, "robosense_airy_01");

    std::cout << "  Fusion Status:       " << fresult.status_message << std::endl;
    std::cout << "  Total Input Points:  " << fresult.total_input_points << std::endl;
    std::cout << "  Fused Output Points: " << fresult.fused_output_points << std::endl;
    std::cout << "  Colorized LiDAR Pts: " << fresult.colorized_lidar_points << std::endl;

    CHECK(fresult.success, "MultiSensorFusionEngine::fuseSensors must succeed");
    CHECK(fresult.per_sensor_point_counts.size() == 2, "Must fuse 2 sensor streams");
    CHECK(fresult.fused_output_points > 50000, "Fused cloud must contain > 50,000 voxelized points");
    CHECK(fresult.colorized_lidar_points > 10000, "Optical RGB projection must colorize > 10,000 LiDAR points");
    std::cout << "  [PASS] Gate 3: Multi-Sensor Spatial Fusion and RGB projection verified." << std::endl;

    // =========================================================================
    // GATE 4: QmlBridge End-to-End M7/M8 Workflow in Sandbox
    // =========================================================================
    std::cout << "\n[M7/M8 GATE 4] Testing QmlBridge End-to-End Calibration, Fusion & View Switcher..." << std::endl;
    if (fs::exists(repo_root / "profiles" / "validated")) {
        fs::create_directories(sandbox_dir / "profiles" / "validated");
        for (const auto& entry : fs::directory_iterator(repo_root / "profiles" / "validated")) {
            if (entry.is_regular_file()) {
                fs::copy_file(entry.path(), sandbox_dir / "profiles" / "validated" / entry.path().filename(),
                              fs::copy_options::overwrite_existing);
            }
        }
    }

    std::string test_proj_id = "proj_m7_m8_test";
    std::string test_sess_id = "session_m7_m8_01";
    fs::path sess_dir = sandbox_dir / "projects" / test_proj_id / "sessions" / test_sess_id;
    fs::path cap_airy_dir = sess_dir / "captures" / "cap_airy_01";
    fs::path cap_gemini_dir = sess_dir / "captures" / "cap_gemini_01";
    fs::create_directories(cap_airy_dir);
    fs::create_directories(cap_gemini_dir);
    fs::copy_file(real_airy_pcd, cap_airy_dir / "map.pcd", fs::copy_options::overwrite_existing);
    fs::copy_file(real_gemini_pcd, cap_gemini_dir / "map.pcd", fs::copy_options::overwrite_existing);

    auto storage = std::make_shared<StorageEngine>();
    CHECK(storage->initialize(sandbox_dir.string()), "Failed to initialize sandbox StorageEngine");
    CHECK(storage->saveScannerConfig(cfg), "Failed to save ScannerConfig to sandbox storage");

    Project proj(test_proj_id, "M7/M8 Fusion Project", "scanner_m7_test", "Sandbox multi-sensor project");
    proj.set_active_session_id(test_sess_id);
    CHECK(storage->saveProject(proj), "Failed to save M7/M8 project");

    Capture cap_airy("cap_airy_01", test_sess_id, "robosense_airy_01");
    cap_airy.set_map_path((cap_airy_dir / "map.pcd").string());
    CHECK(storage->saveCapture(cap_airy), "Failed to save Airy capture");

    Capture cap_gemini("cap_gemini_01", test_sess_id, "orbbec_gemini_336l_01");
    cap_gemini.set_map_path((cap_gemini_dir / "map.pcd").string());
    CHECK(storage->saveCapture(cap_gemini), "Failed to save Gemini capture");

    Session sess(test_sess_id, test_proj_id, "Multi-Sensor Session");
    sess.add_capture(cap_airy);
    sess.add_capture(cap_gemini);
    CHECK(storage->saveSession(sess), "Failed to save M7/M8 session");

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform);

    CHECK(bridge->openProject(QString::fromStdString(test_proj_id)), "QmlBridge failed to open M7/M8 project");

    // 1. Run Automatic Extrinsic Calibration via QmlBridge
    bool cal_ok = bridge->calibrateActiveScannerExtrinsics();
    CHECK(cal_ok, "bridge->calibrateActiveScannerExtrinsics() must succeed");
    CHECK(bridge->extrinsicCalibrationStatus().contains("VALIDATED") ||
          bridge->extrinsicCalibrationStatus().contains("ESTIMATED"), "Status must report VALIDATED or ESTIMATED");
    CHECK(bridge->extrinsicOverlapPercent() >= 80.0, "Bridge overlap must be >= 80%");
    CHECK(bridge->extrinsicRmseMm() <= 65.0, "Bridge RMSE must be <= 65 mm");
    CHECK(!bridge->extrinsicTransformSummary().isEmpty(), "Transform summary must be populated");

    // 2. Run Multi-Sensor Spatial Fusion via QmlBridge
    bool fuse_ok = bridge->fuseActiveMultiSensorMaps(0.025, true, false);
    CHECK(fuse_ok, "bridge->fuseActiveMultiSensorMaps() must succeed");
    CHECK(bridge->isFusionActive(), "isFusionActive() must be true");
    CHECK(bridge->fusedPointCount() > 50000, "fusedPointCount() must be > 50,000");
    CHECK(bridge->fusedColorizedPoints() > 10000, "fusedColorizedPoints() must be > 10,000");
    CHECK(bridge->activeDisplaySensor() == "FUSED", "Active display sensor should switch to FUSED");
    CHECK(bridge->activeAnchorSensor() == "AIRY", "Healthy multi-sensor rig must anchor to AIRY");
    CHECK(!bridge->isAnchorFailoverActive(), "Failover must be false when AIRY is healthy");

    // 2b. Verify Dynamic Anchor Failover when LiDAR degrades/fails and recovery when restored
    bridge->setSensorHealthOverride("AIRY", false, true);
    CHECK(bridge->activeAnchorSensor() == "GEMINI", "Anchor must dynamically fail over to GEMINI when AIRY fails");
    CHECK(bridge->isAnchorFailoverActive(), "isAnchorFailoverActive() must be true during LiDAR failover");
    CHECK(bridge->activeDisplaySensor() == "FUSED", "Viewport must remain overlapped (FUSED) during failover");

    bridge->clearSensorHealthOverrides();
    CHECK(bridge->activeAnchorSensor() == "AIRY", "Anchor must dynamically recover to AIRY when LiDAR recovers");
    CHECK(!bridge->isAnchorFailoverActive(), "isAnchorFailoverActive() must clear on recovery");

    // 3. Save Fused Map to Project
    CHECK(bridge->saveFusedMapToProject(), "saveFusedMapToProject() must succeed");
    CHECK(fs::exists(sess_dir / "fused_multi_sensor_map.pcd") || fs::exists(sess_dir / "map.pcd"),
          "Fused session map must exist on disk");
    std::cout << "  [PASS] Gate 4: QmlBridge M7 Calibration, M8 Fusion, View Switcher, and Persistence verified." << std::endl;

    // Clean up sandbox
    fs::remove_all(sandbox_dir);

    std::cout << "\n=================================================================" << std::endl;
    std::cout << "  ALL MILESTONE 7 & 8 ACCEPTANCE GATES PASSED!                   " << std::endl;
    std::cout << "=================================================================" << std::endl;
    return 0;
}
