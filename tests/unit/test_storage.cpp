#include <cassert>
#include <iostream>
#include <filesystem>
#include "core/storage/storage_engine.hpp"

namespace fs = std::filesystem;
using namespace av::core;

int main() {
    std::cout << "=== Running Storage Engine Unit Tests ===" << std::endl;

    fs::path test_storage_dir = fs::current_path() / "test_storage_sandbox";
    if (fs::exists(test_storage_dir)) {
        fs::remove_all(test_storage_dir);
    }

    storage::StorageEngine engine;
    assert(engine.initialize(test_storage_dir.string()));

    // 1. Project Persistence
    schemas::Project proj("p1", "Storage Test Project", "", "Test description");
    assert(engine.saveProject(proj));

    schemas::Project loaded_proj;
    assert(engine.loadProject("p1", loaded_proj));
    assert(loaded_proj.project_id() == "p1");
    assert(loaded_proj.name() == "Storage Test Project");

    auto projects = engine.listProjects();
    assert(projects.size() == 1);
    assert(projects[0] == "p1");

    // 2. Session Persistence
    schemas::Session sess("s1", "p1", "Test Session");
    assert(engine.saveSession(sess));

    schemas::Session loaded_sess;
    assert(engine.loadSession("s1", loaded_sess));
    assert(loaded_sess.session_id() == "s1");
    assert(loaded_sess.project_id() == "p1");

    auto sessions = engine.listSessions("p1");
    assert(sessions.size() == 1);
    assert(sessions[0] == "s1");

    // 3. Capture Persistence
    schemas::Capture cap("c1", "s1", "sensor_01", 100);
    cap.set_observation_count(42);
    assert(engine.saveCapture(cap));

    schemas::Capture loaded_cap;
    assert(engine.loadCapture("c1", loaded_cap));
    assert(loaded_cap.capture_id() == "c1");
    assert(loaded_cap.observation_count() == 42);

    // 4. Device Profile Persistence
    schemas::DeviceProfile prof;
    prof.set_profile_id("test_prof_01");
    prof.set_manufacturer("ManufacturerX");
    prof.set_profile_status(schemas::ProfileStatus::VALIDATED);
    assert(engine.saveDeviceProfile(prof));

    schemas::DeviceProfile loaded_prof;
    assert(engine.loadDeviceProfile("test_prof_01", loaded_prof));
    assert(loaded_prof.profile_id() == "test_prof_01");
    assert(loaded_prof.manufacturer() == "ManufacturerX");

    // 5. Scanner Config Persistence
    schemas::ScannerConfig cfg("cfg_test", "Test Configuration", "node_01");
    assert(engine.saveScannerConfig(cfg));

    schemas::ScannerConfig loaded_cfg;
    assert(engine.loadScannerConfig("cfg_test", loaded_cfg));
    assert(loaded_cfg.config_id() == "cfg_test");
    assert(loaded_cfg.name() == "Test Configuration");

    // Clean up sandbox
    fs::remove_all(test_storage_dir);

    std::cout << "=== All Storage Engine Unit Tests PASSED ===" << std::endl;
    return 0;
}
