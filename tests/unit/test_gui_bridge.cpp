#include <cassert>
#include <iostream>
#include <filesystem>
#include <memory>
#include <QCoreApplication>
#include <cmath>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/schemas/project.hpp"
#include "platform/linux/linux_platform.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "Running AV Scan GUI Bridge Unit Tests..." << std::endl;

    fs::path test_dir = fs::current_path() / "test_gui_sandbox";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);

    auto storage = std::make_shared<av::core::storage::StorageEngine>();
    storage->initialize(test_dir.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform);

    // 1. Initial State Check
    std::cout << "  Checking initial state..." << std::endl;
    assert(bridge->captureState() == "NO_PROJECT");
    assert(!bridge->canStart());
    assert(!bridge->canStop());
    assert(!bridge->canSave());
    assert(!bridge->isCapturing());
    assert(bridge->pointsCaptured() == 0);

    // 2. Invalid Action Prevention
    std::cout << "  Checking invalid action prevention..." << std::endl;
    bool err_emitted = false;
    QObject::connect(bridge.get(), &av::gui::QmlBridge::errorOccurred, [&](const QString&, const QString&) {
        err_emitted = true;
    });

    // Cannot start without project
    assert(!bridge->startCapture());
    assert(err_emitted);
    err_emitted = false;

    // Cannot stop when not capturing
    assert(!bridge->stopCapture());
    assert(err_emitted);
    err_emitted = false;

    // Cannot save when not stopped
    assert(!bridge->saveCapture());
    assert(err_emitted);
    err_emitted = false;

    // Cannot create project with empty name
    assert(!bridge->createProject("", ""));
    assert(err_emitted);
    err_emitted = false;

    // 3. Project Creation
    std::cout << "  Testing project creation..." << std::endl;
    assert(bridge->createProject("Alpha Survey", "Test survey description"));
    assert(bridge->projectName() == "Alpha Survey");
    assert(!bridge->projectId().isEmpty());
    assert(bridge->captureState() == "READY");
    assert(bridge->canStart());
    assert(!bridge->canStop());
    assert(!bridge->canSave());

    // Verify persisted in storage
    av::core::schemas::Project p;
    assert(storage->loadProject(bridge->projectId().toStdString(), p));
    assert(p.name() == "Alpha Survey");

    // 4. Project Opening
    std::cout << "  Testing project loading..." << std::endl;
    // Create second project directly in storage
    av::core::schemas::Project p2("proj_manual_01", "Beta Facility", "/path", "Manual");
    storage->saveProject(p2);
    bridge->refreshData();

    assert(bridge->openProject("proj_manual_01"));
    assert(bridge->projectName() == "Beta Facility");
    assert(bridge->projectId() == "proj_manual_01");
    assert(bridge->captureState() == "READY");
    assert(bridge->canStart());

    // 5. 3D Camera Controls
    std::cout << "  Testing 3D camera controls..." << std::endl;
    bridge->recenter3D();
    assert(std::abs(bridge->camYaw3D() - 45.0) < 0.001);
    assert(std::abs(bridge->camPitch3D() - 30.0) < 0.001);
    assert(std::abs(bridge->camDistance3D() - 8.0) < 0.001);

    // Orbit
    bridge->orbit3D(10.0, -5.0);
    assert(std::abs(bridge->camYaw3D() - 49.0) < 0.001);
    assert(std::abs(bridge->camPitch3D() - 28.0) < 0.001);

    // Zoom
    qreal old_dist = bridge->camDistance3D();
    bridge->zoom3D(120); // zoom in
    assert(bridge->camDistance3D() < old_dist);
    (void)old_dist;

    // Preset
    bridge->setPreset3D("top");
    assert(std::abs(bridge->camPitch3D() - 89.9) < 0.001);

    // 6. 2D Camera Controls
    std::cout << "  Testing 2D camera controls..." << std::endl;
    bridge->recenter2D();
    assert(std::abs(bridge->pan2DX()) < 0.001);
    assert(std::abs(bridge->pan2DY()) < 0.001);
    assert(std::abs(bridge->zoom2D() - 40.0) < 0.001);

    bridge->pan2D(40.0, -80.0);
    assert(bridge->pan2DX() < 0.0);
    assert(bridge->pan2DY() < 0.0);

    bridge->zoom2DByFactor(1.5);
    assert(std::abs(bridge->zoom2D() - 60.0) < 0.001);

    // 7. Capture State Machine Workflow
    std::cout << "  Testing full capture state machine (READY -> START -> CAPTURING -> STOP -> STOPPED -> SAVE -> SAVED)..." << std::endl;
    // START
    assert(bridge->startCapture());
    assert(bridge->captureState() == "CAPTURING");
    assert(bridge->isCapturing());
    assert(!bridge->canStart());
    assert(bridge->canStop());
    assert(!bridge->canSave());
    assert(bridge->pointsCaptured() == 0); // Strictly 0: No fake synthetic points before M3

    // Double-start rejected
    assert(!bridge->startCapture());

    // STOP
    assert(bridge->stopCapture());
    assert(bridge->captureState() == "STOPPED");
    assert(!bridge->isCapturing());
    assert(!bridge->canStart());
    assert(!bridge->canStop());
    assert(bridge->canSave());

    // Double-stop rejected
    assert(!bridge->stopCapture());

    // SAVE
    assert(bridge->saveCapture());
    assert(bridge->captureState() == "SAVED");
    assert(!bridge->canSave());
    assert(bridge->canStart()); // Can begin next capture session

    // Double-save rejected
    assert(!bridge->saveCapture());

    // Clean up
    fs::remove_all(test_dir);
    std::cout << ">>> All GUI Bridge Unit Tests PASSED <<<" << std::endl;
    return 0;
}
