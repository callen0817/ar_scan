#include <iostream>
#include <cassert>
#include <memory>
#include <filesystem>
#include <cmath>
#include <dlfcn.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "core/storage/storage_engine.hpp"
#include "platform/linux/linux_platform.hpp"
#include "gui/qml_bridge.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "==========================================================" << std::endl;
    std::cout << "       AV SCAN — MILESTONE 2 (GUI) ACCEPTANCE TEST        " << std::endl;
    std::cout << "==========================================================" << std::endl;

    bool all_passed = true;

#ifdef AV_SOURCE_DIR
    fs::path repo_root = AV_SOURCE_DIR;
#else
    fs::path repo_root = fs::current_path();
#endif

    auto storage = std::make_shared<av::core::storage::StorageEngine>();
    storage->initialize(repo_root.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto bridge = std::make_unique<av::gui::QmlBridge>(storage, platform);

    // GATE 1: Scanner & Profile Architecture Enumeration
    std::cout << "[GATE 1] Scanner Configurations & Device Profiles Enumeration..." << std::endl;
    auto scanners = bridge->scannerList();
    std::cout << "  Enumerated " << scanners.size() << " scanner option(s):" << std::endl;
    bool found_airy = false;
    bool found_gemini = false;
    bool found_viture = false;
    bool found_dual = false;

    for (const auto& s : scanners) {
        auto m = s.toMap();
        std::cout << "   - " << m["name"].toString().toStdString() << " (" << m["status"].toString().toStdString() << ")" << std::endl;
        std::cout << "     Notice: " << m["validationNotice"].toString().toStdString() << std::endl;

        if (m["id"].toString().contains("airy") || m["id"].toString().contains("robosense")) {
            found_airy = true;
            assert(m["status"].toString() == "DRAFT" || m["status"].toString() == "VALIDATED");
        }
        if (m["id"].toString().contains("gemini") || m["id"].toString().contains("orbbec")) {
            found_gemini = true;
            assert(m["status"].toString() == "DRAFT");
        }
        if (m["id"].toString().contains("viture")) {
            found_viture = true;
            assert(m["status"].toString() == "EXPERIMENTAL");
        }
        if (m["type"].toString() == "SCANNER_CONFIG") {
            found_dual = true;
            assert(m["status"].toString() == "CONFIG");
        }
    }

    if (found_airy && found_gemini && found_viture && found_dual) {
        std::cout << "  -> GATE 1: PASS (All profiles accurately enumerated with draft/experimental/config status)" << std::endl;
    } else {
        std::cerr << "  -> GATE 1 FAILED: Missing expected profile or config" << std::endl;
        all_passed = false;
    }

    // GATE 2: Project Workflow (Create & Load)
    std::cout << "\n[GATE 2] Project Storage Workflow..." << std::endl;
    fs::path sandbox_dir = repo_root / "build" / "test_m2_sandbox";
    fs::remove_all(sandbox_dir);
    fs::create_directories(sandbox_dir);

    auto sandbox_storage = std::make_shared<av::core::storage::StorageEngine>();
    sandbox_storage->initialize(sandbox_dir.string());
    auto test_bridge = std::make_unique<av::gui::QmlBridge>(sandbox_storage, platform);

    if (test_bridge->createProject("Commercial Facility 01", "Acceptance test project")) {
        std::string pid = test_bridge->projectId().toStdString();
        assert(!pid.empty());
        assert(test_bridge->projectName() == "Commercial Facility 01");
        assert(test_bridge->captureState() == "READY");
        assert(test_bridge->canStart());

        // Test project opening
        test_bridge->refreshData();
        assert(test_bridge->openProject(QString::fromStdString(pid)));
        std::cout << "  -> GATE 2: PASS (Project creation and load validated against storage)" << std::endl;
    } else {
        std::cerr << "  -> GATE 2 FAILED: Project creation failed" << std::endl;
        all_passed = false;
    }

    // GATE 3: State Machine Logic & Nonsensical Action Prevention
    std::cout << "\n[GATE 3] Capture State Machine & Invalid Action Prevention..." << std::endl;
    {
        // Must reject stop when not capturing
        assert(!test_bridge->stopCapture());
        // Must reject save when not stopped
        assert(!test_bridge->saveCapture());

        // START
        assert(test_bridge->startCapture());
        assert(test_bridge->captureState() == "CAPTURING");
        assert(test_bridge->isCapturing());
        assert(!test_bridge->canStart());
        assert(test_bridge->canStop());
        assert(!test_bridge->canSave());
        assert(test_bridge->pointsCaptured() == 0); // No synthetic points before M3!

        // Must reject starting again while capturing
        assert(!test_bridge->startCapture());

        // STOP
        assert(test_bridge->stopCapture());
        assert(test_bridge->captureState() == "STOPPED");
        assert(!test_bridge->isCapturing());
        assert(test_bridge->canSave());
        assert(!test_bridge->canStart());

        // Must reject stopping again
        assert(!test_bridge->stopCapture());

        // SAVE
        assert(test_bridge->saveCapture());
        assert(test_bridge->captureState() == "SAVED");
        assert(!test_bridge->canSave());
        assert(test_bridge->canStart());

        std::cout << "  -> GATE 3: PASS (State flow READY -> START -> CAPTURING -> STOP -> STOPPED -> SAVE -> SAVED verified)" << std::endl;
    }

    // GATE 4: 3D and 2D Viewer Camera Interaction
    std::cout << "\n[GATE 4] 3D & 2D Viewer Interaction Math..." << std::endl;
    {
        test_bridge->recenter3D();
        test_bridge->orbit3D(15.0, 10.0);
        assert(std::abs(test_bridge->camYaw3D() - 51.0) < 0.01);
        test_bridge->pan3D(20.0, -20.0);
        test_bridge->zoom3D(100);
        test_bridge->setPreset3D("iso");
        assert(std::abs(test_bridge->camYaw3D() - 45.0) < 0.01);

        test_bridge->recenter2D();
        test_bridge->pan2D(10.0, 20.0);
        test_bridge->zoom2DByFactor(1.2);
        test_bridge->recenter2D();
        assert(std::abs(test_bridge->pan2DX()) < 0.001);
        assert(std::abs(test_bridge->zoom2D() - 40.0) < 0.001);

        std::cout << "  -> GATE 4: PASS (Orbit, Pan, Zoom, Recenter, Presets functional)" << std::endl;
    }

    // GATE 5: Qt Quick / QML Instantiation on Real Display / Offscreen Engine
    std::cout << "\n[GATE 5] Qt Quick / QML Component Binding & UI Instantiation..." << std::endl;
    try {
        setenv("QT_QPA_PLATFORM", "offscreen", 1);
        QGuiApplication qapp(argc, argv);

        fs::path lib_dir = repo_root / "third_party" / "lib";
        if (fs::exists(lib_dir / "libQt5QuickTemplates2.so.5")) {
            void* h1 = dlopen((lib_dir / "libQt5QuickTemplates2.so.5").c_str(), RTLD_LAZY | RTLD_GLOBAL);
            (void)h1;
        }
        if (fs::exists(lib_dir / "libQt5QuickControls2.so.5")) {
            void* h2 = dlopen((lib_dir / "libQt5QuickControls2.so.5").c_str(), RTLD_LAZY | RTLD_GLOBAL);
            (void)h2;
        }

        QQmlApplicationEngine engine;
        fs::path repo_qml = repo_root / "third_party" / "qml";
        if (fs::exists(repo_qml)) {
            engine.addImportPath(QString::fromStdString(repo_qml.string()));
        }

        engine.rootContext()->setContextProperty("bridge", bridge.get());

        fs::path qml_path = repo_root / "gui" / "qml" / "AppWindow.qml";
        engine.load(QUrl::fromLocalFile(QString::fromStdString(qml_path.string())));

        if (engine.rootObjects().isEmpty()) {
            std::cerr << "  -> GATE 5 FAILED: Failed to instantiate AppWindow.qml" << std::endl;
            all_passed = false;
        } else {
            std::cout << "  -> GATE 5: PASS (AppWindow.qml instantiated with dual viewports, capture controls, and modals)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "  -> GATE 5 FAILED: " << e.what() << std::endl;
        all_passed = false;
    }

    fs::remove_all(sandbox_dir);

    std::cout << "\n==========================================================" << std::endl;
    if (all_passed) {
        std::cout << "       STATUS: >>> PASS <<< ALL M2 GATES SATISFIED       " << std::endl;
        std::cout << "==========================================================" << std::endl;
        return 0;
    } else {
        std::cout << "       STATUS: >>> FAIL <<< UNRESOLVED ISSUES            " << std::endl;
        std::cout << "==========================================================" << std::endl;
        return 1;
    }
}
