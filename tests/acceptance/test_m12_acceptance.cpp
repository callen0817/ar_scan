#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <vector>
#include <cmath>
#include <cassert>
#include <QCoreApplication>

#include "core/processing/point_cloud_processor.hpp"
#include "core/schemas/project.hpp"
#include "core/schemas/session.hpp"
#include "core/storage/storage_engine.hpp"
#include "platform/linux/linux_platform.hpp"
#include "gui/qml_bridge.hpp"

namespace fs = std::filesystem;
using namespace av::core::processing;
using namespace av::core::schemas;
using namespace av::core::storage;

#define CHECK(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "[M12 FAIL] " << message << " (line " << __LINE__ << ")" << std::endl; \
            return 1; \
        } \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "=================================================================" << std::endl;
    std::cout << "  AV SCAN — MILESTONE 12: POINT CLOUD CLEANUP & EXPORT SUITE     " << std::endl;
    std::cout << "=================================================================" << std::endl;

    fs::path repo_root = "/home/scanar/av_scan";
    if (!fs::exists(repo_root / "projects")) {
        repo_root = fs::current_path();
        while (repo_root.has_parent_path() && !fs::exists(repo_root / "projects")) {
            repo_root = repo_root.parent_path();
        }
    }

    fs::path sandbox_dir = fs::current_path() / "test_m12_sandbox";
    if (fs::exists(sandbox_dir)) {
        fs::remove_all(sandbox_dir);
    }
    fs::create_directories(sandbox_dir / "exports");

    // =========================================================================
    // GATE 1: Synthetic Tilted Ground Plane + Outliers Cleanup Verification
    // =========================================================================
    std::cout << "\n[M12 GATE 1] Testing Voxel Downsampling, SOR, ROR, and RANSAC Ground Alignment..." << std::endl;
    std::vector<PointXYZI> synth_cloud;
    // Generate a 10 degree tilted floor (around X axis) + extreme statistical outliers
    const double tilt_rad = 10.0 * M_PI / 180.0;
    const double cos_t = std::cos(tilt_rad);
    const double sin_t = std::sin(tilt_rad);

    for (int ix = -30; ix <= 30; ++ix) {
        for (int iy = -30; iy <= 30; ++iy) {
            float x = ix * 0.05f;
            float y_flat = iy * 0.05f;
            float z_flat = 0.50f; // 0.5m offset above origin before tilt
            // Rotate around X axis by 10 deg
            float y = static_cast<float>(y_flat * cos_t - z_flat * sin_t);
            float z = static_cast<float>(y_flat * sin_t + z_flat * cos_t);
            // Add duplicate near-points to test voxel grid reduction
            synth_cloud.push_back({x, y, z, 1.0f, 120, 180, 220, true});
            synth_cloud.push_back({x + 0.002f, y + 0.002f, z + 0.001f, 1.0f, 122, 182, 218, true});
        }
    }
    // Add 25 isolated noise outliers far from the main structure
    for (int i = 0; i < 25; ++i) {
        synth_cloud.push_back({10.0f + i * 1.5f, -12.0f - i * 1.2f, 8.0f + i * 0.9f, 1.0f, 255, 0, 0, true});
    }

    CleanupParams params;
    params.enable_voxel_downsample = true;
    params.voxel_leaf_size_m = 0.04f;
    params.enable_sor = true;
    params.sor_mean_k = 16;
    params.sor_std_mul = 1.5;
    params.enable_ror = true;
    params.ror_radius_m = 0.15;
    params.ror_min_neighbors = 4;
    params.enable_ground_alignment = true;
    params.align_ground_to_z_zero = true;
    params.remove_ground_plane = false;

    std::vector<PointXYZI> cleaned_cloud;
    ProcessingStats stats = PointCloudProcessor::runCleanupPipeline(synth_cloud, cleaned_cloud, params);

    std::cout << "  Input points:          " << stats.input_point_count << std::endl;
    std::cout << "  Voxel removed:         " << stats.voxel_removed_count << std::endl;
    std::cout << "  SOR removed:           " << stats.sor_removed_count << std::endl;
    std::cout << "  ROR removed:           " << stats.ror_removed_count << std::endl;
    std::cout << "  Output points:         " << stats.output_point_count << std::endl;
    std::cout << "  Ground plane found:    " << (stats.ground_plane.found ? "YES" : "NO") << std::endl;
    std::cout << "  Ground tilt corrected: " << stats.ground_plane.tilt_deg << " deg" << std::endl;

    CHECK(stats.voxel_removed_count > 1000, "Voxel downsampling should merge duplicate points");
    CHECK((stats.sor_removed_count + stats.ror_removed_count) >= 25, "SOR/ROR should remove all 25 isolated noise outliers");
    CHECK(stats.ground_plane.found, "RANSAC must detect the dominant ground plane");
    CHECK(std::abs(stats.ground_plane.tilt_deg - 10.0) < 1.5, "Detected ground tilt should be ~10 degrees");

    // Verify all inlier floor points are now leveled near Z = 0
    double mean_abs_z = 0.0;
    for (const auto& pt : cleaned_cloud) {
        mean_abs_z += std::abs(pt.z);
    }
    mean_abs_z /= std::max<size_t>(1, cleaned_cloud.size());
    std::cout << "  Mean |Z| after Z=0 ground leveling: " << mean_abs_z << " m" << std::endl;
    CHECK(mean_abs_z < 0.03, "Leveled ground plane must lie at Z=0 within 3cm");
    std::cout << "  [PASS] Gate 1: Voxel, SOR, ROR, and RANSAC Ground Leveling verified." << std::endl;

    // =========================================================================
    // GATE 2: Multi-Format Export & Round-Trip Verification (.ply, .pcd, .xyz, .obj)
    // =========================================================================
    std::cout << "\n[M12 GATE 2] Testing Multi-Format Export (.ply, .pcd, .xyz, .obj)..." << std::endl;
    struct ExportCase {
        ExportFormat fmt;
        std::string filename;
    };
    std::vector<ExportCase> cases = {
        {ExportFormat::PLY_BINARY, "cloud_bin.ply"},
        {ExportFormat::PLY_ASCII,  "cloud_ascii.ply"},
        {ExportFormat::PCD_BINARY, "cloud_bin.pcd"},
        {ExportFormat::PCD_ASCII,  "cloud_ascii.pcd"},
        {ExportFormat::XYZ_ASCII,  "cloud.xyz"},
        {ExportFormat::OBJ_MESH,   "mesh_surface.obj"}
    };

    for (const auto& c : cases) {
        fs::path out_file = sandbox_dir / "exports" / c.filename;
        ExportResult exp_res = PointCloudProcessor::exportPointCloud(cleaned_cloud, out_file.string(), c.fmt, 0.08f);
        CHECK(exp_res.success, "Export failed for " + c.filename + ": " + exp_res.error_message);
        CHECK(fs::exists(out_file), "Exported file missing on disk: " + out_file.string());
        CHECK(exp_res.file_size_bytes > 1000, "Exported file suspiciously small: " + c.filename);
        std::cout << "  Exported " << c.filename << " (" << exp_res.file_size_bytes << " bytes, "
                  << exp_res.exported_points << " vertices, " << exp_res.exported_triangles << " triangles)" << std::endl;
        if (c.fmt == ExportFormat::OBJ_MESH) {
            CHECK(exp_res.exported_triangles > 100, "OBJ mesh export must reconstruct surface triangles");
        }
    }

    // Round-trip load verification on binary and ASCII PCD
    std::vector<PointXYZI> reload_pcd_bin;
    CHECK(PointCloudProcessor::loadPcd((sandbox_dir / "exports" / "cloud_bin.pcd").string(), reload_pcd_bin),
          "Failed to reload exported binary PCD");
    CHECK(reload_pcd_bin.size() == cleaned_cloud.size(), "Reloaded binary PCD point count mismatch");

    std::vector<PointXYZI> reload_pcd_ascii;
    CHECK(PointCloudProcessor::loadPcd((sandbox_dir / "exports" / "cloud_ascii.pcd").string(), reload_pcd_ascii),
          "Failed to reload exported ASCII PCD");
    CHECK(reload_pcd_ascii.size() == cleaned_cloud.size(), "Reloaded ASCII PCD point count mismatch");
    std::cout << "  [PASS] Gate 2: All 6 export formats and PCD round-trips verified." << std::endl;

    // =========================================================================
    // GATE 3: QmlBridge End-to-End M12 Integration on Real Project Data
    // =========================================================================
    std::cout << "\n[M12 GATE 3] Testing QmlBridge M12 Integration on Real Captured Point Cloud..." << std::endl;
    fs::path real_pcd = repo_root / "projects" / "proj_1789675816776" / "sessions" / "session_1789675851740" / "captures" / "cap_1789676064983_gemini" / "map.pcd";
    CHECK(fs::exists(real_pcd), "Real Gemini PCD dataset must exist");

    std::string test_proj_id = "proj_m12_test";
    std::string test_sess_id = "session_m12_01";
    fs::path sess_dir = sandbox_dir / "projects" / test_proj_id / "sessions" / test_sess_id;
    fs::create_directories(sess_dir);
    fs::copy_file(real_pcd, sess_dir / "map.pcd", fs::copy_options::overwrite_existing);

    auto storage = std::make_shared<StorageEngine>();
    CHECK(storage->initialize(sandbox_dir), "Failed to initialize sandbox StorageEngine");

    Project proj(test_proj_id, "M12 Cleanup Test Project", "", "Sandbox test project");
    proj.set_active_session_id(test_sess_id);
    CHECK(storage->saveProject(proj), "Failed to save sandbox project");

    Session sess(test_sess_id, test_proj_id, "M12 Session");
    CHECK(storage->saveSession(sess), "Failed to save sandbox session");

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform);

    CHECK(bridge->openProject(QString::fromStdString(test_proj_id)), "QmlBridge failed to open M12 project");
    CHECK(bridge->pointsCaptured() > 50000, "Initial real cloud should have > 50,000 points");

    // Run cleanup preview via QmlBridge
    bool preview_ok = bridge->runPointCloudCleanup(
        true, 0.03,   // Voxel 3cm
        true, 16, 1.5,// SOR
        true, 0.12, 4,// ROR
        true, false   // RANSAC ground leveling
    );
    CHECK(preview_ok, "bridge->runPointCloudCleanup() should succeed");
    CHECK(bridge->hasCleanupPreview(), "hasCleanupPreview should be true after running cleanup");
    CHECK(bridge->cleanupInputPoints() > bridge->cleanupOutputPoints(), "Cleanup should reduce point count");
    CHECK(bridge->cleanupRemovedPoints() > 0, "cleanupRemovedPoints should be > 0");

    // Reset preview and verify original point count returns
    int cleaned_count = bridge->cleanupOutputPoints();
    bridge->resetPointCloudCleanup();
    CHECK(!bridge->hasCleanupPreview(), "hasCleanupPreview should be false after reset");
    CHECK(bridge->pointsCaptured() == bridge->cleanupInputPoints(), "Point count should restore to original after reset");

    // Re-run and commit cleaned cloud
    CHECK(bridge->runPointCloudCleanup(true, 0.03, true, 16, 1.5, true, 0.12, 4, true, false),
          "Second runPointCloudCleanup should succeed");
    CHECK(bridge->commitCleanedPointCloud(), "commitCleanedPointCloud should succeed");
    CHECK(!bridge->hasCleanupPreview(), "hasCleanupPreview should be false after commit");
    CHECK(bridge->pointsCaptured() == cleaned_count, "Committed point count should match cleaned output");
    CHECK(fs::exists(sess_dir / "map_raw_backup.pcd"), "Non-destructive backup map_raw_backup.pcd must be created");

    // One-click export via QmlBridge
    QString ply_out = bridge->exportActivePointCloud("PLY_BINARY");
    CHECK(!ply_out.isEmpty() && fs::exists(ply_out.toStdString()), "QmlBridge PLY_BINARY export file should exist");

    QString obj_out = bridge->exportActivePointCloud("OBJ_MESH", "", 0.05);
    CHECK(!obj_out.isEmpty() && fs::exists(obj_out.toStdString()), "QmlBridge OBJ_MESH export file should exist");
    std::cout << "  QmlBridge exported PLY: " << ply_out.toStdString() << std::endl;
    std::cout << "  QmlBridge exported OBJ: " << obj_out.toStdString() << std::endl;
    std::cout << "  [PASS] Gate 3: QmlBridge M12 preview, reset, commit, and export verified." << std::endl;

    // =========================================================================
    // GATE 4: Real-Time Spatial-Hash M12 Cleanup & Live Stream UX Integration
    // =========================================================================
    std::cout << "\n[M12 GATE 4] Testing O(N) Real-Time M12 Cleanup (cleanRealtimeFast) & Live QmlBridge Controls..." << std::endl;
    std::vector<PointXYZI> rt_cleaned;
    CleanupParams rt_params;
    rt_params.enable_voxel_downsample = true;
    rt_params.voxel_leaf_size_m = 0.04f;
    rt_params.enable_ror = true;
    rt_params.ror_radius_m = 0.14;
    rt_params.ror_min_neighbors = 4;
    rt_params.enable_sor = true;
    rt_params.sor_std_mul = 1.6;
    rt_params.enable_ground_alignment = true;
    rt_params.align_ground_to_z_zero = true;

    ProcessingStats rt_stats = PointCloudProcessor::cleanRealtimeFast(synth_cloud, rt_cleaned, rt_params);
    std::cout << "  Real-time input:       " << rt_stats.input_point_count << std::endl;
    std::cout << "  Real-time output:      " << rt_stats.output_point_count << std::endl;
    std::cout << "  Real-time Voxel drops: " << rt_stats.voxel_removed_count << std::endl;
    std::cout << "  Real-time ROR+SOR:     " << (rt_stats.ror_removed_count + rt_stats.sor_removed_count) << std::endl;
    std::cout << "  Real-time latency:     " << rt_stats.computation_time_ms << " ms" << std::endl;

    CHECK(rt_stats.voxel_removed_count > 1000, "cleanRealtimeFast should merge duplicate voxel points");
    CHECK((rt_stats.ror_removed_count + rt_stats.sor_removed_count) >= 25, "cleanRealtimeFast should remove all 25 isolated noise outliers");
    CHECK(rt_stats.computation_time_ms < 50.0, "cleanRealtimeFast must execute within real-time frame budget (< 50 ms)");
    CHECK(!rt_cleaned.empty() && rt_cleaned.front().has_color, "cleanRealtimeFast must preserve RGB colors");

    // Verify QmlBridge live cleanup properties and UX helpers
    CHECK(bridge->liveCleanupEnabled(), "liveCleanupEnabled should default to true");
    bridge->setLiveCleanupEnabled(false);
    CHECK(!bridge->liveCleanupEnabled(), "setLiveCleanupEnabled(false) should toggle state");
    bridge->setLiveCleanupEnabled(true);
    CHECK(bridge->liveCleanupEnabled(), "setLiveCleanupEnabled(true) should restore state");

    bridge->setLiveGroundLockEnabled(true);
    CHECK(bridge->liveGroundLockEnabled(), "setLiveGroundLockEnabled(true) should toggle state");
    bridge->setLiveGroundLockEnabled(false);

    bridge->autoFitCameraToMapBounds();
    CHECK(bridge->camDistance3D() > 1.0, "autoFitCameraToMapBounds should compute a valid 3D camera distance");

    CHECK(bridge->autoLevelOrientationDiag(), "autoLevelOrientationDiag should level dominant floor plane via RANSAC");
    std::cout << "  [PASS] Gate 4: Real-Time O(N) M12 Cleanup and QmlBridge UX controls verified." << std::endl;

    // Clean up sandbox
    fs::remove_all(sandbox_dir);

    std::cout << "\n=================================================================" << std::endl;
    std::cout << "  ALL MILESTONE 12 ACCEPTANCE GATES PASSED!                      " << std::endl;
    std::cout << "=================================================================" << std::endl;
    return 0;
}
