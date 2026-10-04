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

    // 7. Milestone 14: Hardware Discovery and Autonomous Profile Onboarding
    std::cout << "  Testing hardware discovery and profiling methods..." << std::endl;
    bridge->refreshDiscovery();
    QVariantList devs = bridge->discoveredPhysicalDevices();
    std::cout << "    Discovered " << devs.size() << " physical hardware device(s)." << std::endl;
    assert(!devs.isEmpty());
    assert(!bridge->discoveryStatusText().isEmpty());

    bool found_cam = false;
    QString target_id;
    for (const auto& d : devs) {
        QVariantMap m = d.toMap();
        if (m["busType"].toString() == "USB" || m["transport"].toString().contains("USB") || m["interfaces"].toList().size() > 0) {
            found_cam = true;
            target_id = m["id"].toString();
            break;
        }
    }

    if (found_cam) {
        std::cout << "    Interrogating physical device and creating profile for: " << target_id.toStdString() << std::endl;
        QVariantMap res = bridge->createProfileForDevice(target_id);
        assert(res["success"].toBool());
        assert(res["profileStatus"].toString() == "EXPERIMENTAL");
        assert(res["compatibleReconstruction"].toString() == "Monocular Neural Reconstruction");
        assert(res["selectedBackend"].toString() == "LingBot-Map");
        assert(res["scaleStatus"].toString() == "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED");
        assert(res["nativeDepth"].toString() == "NO");
        assert(res["nativeImu"].toString() == "NO");
        assert(res["nativeGeometry"].toString() == "NO");

        // Now test selecting profile for capture
        QString pid = res["profileId"].toString();
        bridge->selectProfileForCapture(pid);
        assert(bridge->selectedScannerId() == pid);

        // Discovered devices should now mark it as PROFILED
        bridge->refreshDiscovery();
        QVariantList updated_devs = bridge->discoveredPhysicalDevices();
        for (const auto& d : updated_devs) {
            QVariantMap m = d.toMap();
            if (m["id"].toString() == target_id) {
                assert(m["profileStatus"].toString() == "PROFILED");
                assert(m["existingProfileId"].toString() == pid);
                break;
            }
        }

        // Also test profiling SenseCAP Watcher if discovered on host
        for (const auto& dev_entry : devs) {
            QVariantMap m = dev_entry.toMap();
            if (m["id"].toString().contains("1a86_55d2") || m["name"].toString().contains("Watcher")) {
                QString watcher_id = m["id"].toString();
                std::cout << "    Interrogating SenseCAP Watcher and creating profile for: " << watcher_id.toStdString() << std::endl;
                QVariantMap w_res = bridge->createProfileForDevice(watcher_id);
                assert(w_res["success"].toBool());
                assert(w_res["profileStatus"].toString() == "EXPERIMENTAL");
                assert(w_res["compatibleReconstruction"].toString() == "UNKNOWN");
                assert(w_res["selectedBackend"].toString() == "NONE");
                assert(w_res["scaleStatus"].toString() == "NO_RECONSTRUCTION_BACKEND_MATCHED");
                assert(w_res["nativeDepth"].toString() == "NO");
                assert(w_res["nativeImu"].toString() == "NO");
                assert(w_res["nativeGeometry"].toString() == "NO");
                assert(w_res["nativeCapabilities"].toString().contains("Serial Control"));
                std::cout << "    SenseCAP Watcher profile generated successfully: " << w_res["profileId"].toString().toStdString() << std::endl;
                break;
            }
        }
    }

    // 8. Universal Sensor Ecosystem Catalog & Template Profile Generation
    std::cout << "  Testing Ecosystem Catalog and Template Profile Generation..." << std::endl;
    assert(bridge->ecosystemModelCount() >= 80);
    assert(bridge->ecosystemVendorCount() >= 15);

    QStringList vendors = bridge->getEcosystemManufacturers();
    assert(vendors.contains("Ouster"));
    assert(vendors.contains("RoboSense"));
    assert(vendors.contains("Hesai"));
    assert(vendors.contains("Orbbec"));

    QStringList ouster_types = bridge->getSensorTypesForManufacturer("Ouster");
    assert(ouster_types.contains("LiDAR"));

    QVariantList hesai_lidars = bridge->getEcosystemModels("Hesai", "LiDAR", "");
    assert(!hesai_lidars.isEmpty());

    QVariantList search_res = bridge->getEcosystemModels("", "", "Pandar");
    assert(!search_res.isEmpty());

    // Check Model Details & Provenance
    QVariantMap details = bridge->getModelDetails("ouster_os1_rev8_rgb");
    assert(details["modelId"].toString() == "ouster_os1_rev8_rgb");
    assert(details["vendor"].toString() == "Ouster");
    assert(details["hasRgb"].toBool() == true);
    assert(details["hasLidar"].toBool() == true);
    assert(details["catKnowledge"].toBool() == true);
    assert(!details["docUrl"].toString().isEmpty());

    // Generate Template Profile
    QVariantMap t_res = bridge->createTemplateProfileFromModel("ouster_os1_rev8_rgb");
    assert(t_res["success"].toBool() == true);
    assert(t_res["profileId"].toString() == "ouster_os1_rev8_rgb_template");
    assert(t_res["profileStatus"].toString() == "EXPERIMENTAL");
    assert(t_res["calibrationSource"].toString() == "OFFICIAL_DOCUMENTATION");
    assert(!t_res["isMetricScaleValidated"].toBool());

    // Verify it appeared in project profiles
    QVariantList profiles = bridge->getAvailableDeviceProfiles();
    bool found_template = false;
    for (const auto& p : profiles) {
        if (p.toMap()["profileId"].toString() == "ouster_os1_rev8_rgb_template") {
            found_template = true;
            break;
        }
    }
    if (!found_template) {
        std::cerr << "Failed to find seeded template profile in available profiles!" << std::endl;
        return 1;
    }
    std::cout << "    Ecosystem catalog & template profile generation verified." << std::endl;

    // Clean up
    fs::remove_all(test_dir);
    std::cout << ">>> All GUI Bridge Unit Tests PASSED <<<" << std::endl;
    return 0;
}
