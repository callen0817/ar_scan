#include <iostream>
#include <filesystem>
#include <memory>
#include <QCoreApplication>

#include "gui/qml_bridge.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/project.hpp"
#include "platform/linux/linux_platform.hpp"

namespace fs = std::filesystem;
using namespace av::core;
using namespace av::gui;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "\n[TEST FAILED] " << msg << " (" << #cond << ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return 1; \
        } \
    } while(0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================\n";
    std::cout << "AV Scan — Automated Runtime Device Isolation Verification\n";
    std::cout << "=================================================================\n\n";

    fs::path test_dir = fs::current_path() / "test_runtime_isolation_sandbox";
    fs::remove_all(test_dir);
    fs::create_directories(test_dir);

    auto storage = std::make_shared<storage::StorageEngine>();
    storage->initialize(test_dir.string());

    auto platform = std::make_shared<av::platform::linux_os::LinuxPlatformAdapter>();
    auto bridge = std::make_unique<QmlBridge>(storage, platform);

    // -------------------------------------------------------------
    // STEP 1: Supply Device A (V4L2 RGB Camera with LingBot Profile)
    // -------------------------------------------------------------
    std::cout << "[STEP 1] Provisioning Device A: V4L2 RGB Camera (LingBot)...\n";
    schemas::DeviceProfile dev_a;
    dev_a.set_profile_id("camera_48mp_rgb");
    dev_a.set_manufacturer("Generic");
    dev_a.set_model("48MP USB Camera");
    dev_a.set_profile_status(schemas::ProfileStatus::VALIDATED);
    dev_a.set_driver("uvcvideo");
    dev_a.set_transport("/dev/video2");
    dev_a.set_device_family("MONOCULAR_RGB_CAMERA");
    dev_a.set_slam_family("MONOCULAR_NEURAL_RECONSTRUCTION");
    dev_a.set_slam_backend("LingBotStreamingBackend");
    nlohmann::json int_a = {{"device_path", "/dev/video2"}};
    dev_a.set_intrinsics(int_a);
    schemas::CalibrationProvenance prov_a;
    prov_a.set_device_serial("2023081002");
    prov_a.set_source(schemas::CalibrationSource::DEVICE_FACTORY);
    dev_a.set_provenance(prov_a);
    bool saved_a = storage->saveDeviceProfile(dev_a);
    TEST_ASSERT(saved_a, "Failed to save Device A profile to storage");
    std::cout << "  Device A saved: profile_id=" << dev_a.profile_id()
              << ", transport=" << dev_a.transport()
              << ", backend=" << dev_a.slam_backend() << "\n";

    // -------------------------------------------------------------
    // STEP 2: Supply Device B (USB Serial CDC-ACM with UNKNOWN Profile)
    // -------------------------------------------------------------
    std::cout << "\n[STEP 2] Provisioning Device B: USB Serial CDC-ACM (Seeed SenseCAP Watcher)...\n";
    schemas::DeviceProfile dev_b;
    dev_b.set_profile_id("seeed_sensecap_watcher_56D5018737");
    dev_b.set_manufacturer("Seeed Studio");
    dev_b.set_model("SenseCAP Watcher");
    dev_b.set_profile_status(schemas::ProfileStatus::EXPERIMENTAL);
    dev_b.set_driver("cdc_acm");
    dev_b.set_transport("USB_SERIAL_CDC_ACM");
    dev_b.set_device_family("EDGE_AI_VISION_AGENT");
    dev_b.set_slam_family("UNKNOWN");
    dev_b.set_slam_backend("");
    std::map<std::string, std::string> frames_b = {{"body_frame", "base_link"}};
    dev_b.set_coordinate_frames(frames_b);
    schemas::CalibrationProvenance prov_b;
    prov_b.set_device_serial("56D5018737");
    prov_b.set_source(schemas::CalibrationSource::DEVICE_FACTORY);
    dev_b.set_provenance(prov_b);
    bool saved_b = storage->saveDeviceProfile(dev_b);
    TEST_ASSERT(saved_b, "Failed to save Device B profile to storage");
    std::cout << "  Device B saved: profile_id=" << dev_b.profile_id()
              << ", transport=" << dev_b.transport()
              << ", slam_family=" << dev_b.slam_family()
              << ", backend='" << dev_b.slam_backend() << "'\n";

    // Create an active project to test capture controls
    bool proj_ok = bridge->createProject("Isolation Test Project", "Unit testing device isolation");
    TEST_ASSERT(proj_ok, "Failed to create test project");
    TEST_ASSERT(bridge->captureState() == "READY", "Bridge captureState must be READY after project creation");

    // -------------------------------------------------------------
    // STEP 3: Select Device B and Verify Strict Runtime Isolation
    // -------------------------------------------------------------
    std::cout << "\n[STEP 3] Selecting Device B (Watcher) and asserting isolation...\n";
    bridge->selectProfileForCapture("seeed_sensecap_watcher_56D5018737");

    auto sensors_b = bridge->activeSensors();
    TEST_ASSERT(sensors_b.size() == 1, "Exactly 1 active sensor must be configured for Device B");
    QVariantMap b_map = sensors_b.first().toMap();

    std::cout << "  Active Sensor ID: " << b_map["id"].toString().toStdString() << "\n";
    std::cout << "  Profile ID: " << b_map["profileId"].toString().toStdString() << "\n";
    std::cout << "  Transport: " << b_map["transport"].toString().toStdString() << "\n";
    std::cout << "  Selected Adapter: " << b_map["adapterName"].toString().toStdString() << "\n";
    std::cout << "  Selected Backend: " << b_map["backendName"].toString().toStdString() << "\n";
    std::cout << "  Capturable: " << (b_map["isCapturable"].toBool() ? "TRUE" : "FALSE") << "\n";
    std::cout << "  bridge->canStart(): " << (bridge->canStart() ? "TRUE" : "FALSE") << "\n";

    // Assert: Device B is classified as UNSUPPORTED spatial capture
    TEST_ASSERT(b_map["id"].toString() == "UNSUPPORTED", "Device B ID must be UNSUPPORTED");
    TEST_ASSERT(b_map["profileId"].toString() == "seeed_sensecap_watcher_56D5018737", "Profile ID must match Watcher");

    // Assert: Does not resolve /dev/video* for Device B
    TEST_ASSERT(!b_map["transport"].toString().contains("/dev/video"), "Device B must NOT resolve any /dev/video* node");
    TEST_ASSERT(b_map["transport"].toString().contains("/dev/ttyACM"), "Device B transport must reflect serial CDC-ACM");

    // Assert: GenericUvcSensorAdapter is NOT instantiated/selected for Device B
    TEST_ASSERT(b_map["adapterName"].toString() == "NONE", "Device B adapter must be NONE");

    // Assert: LingBotStreamingBackend is NOT instantiated/selected for Device B
    TEST_ASSERT(b_map["backendName"].toString() == "NONE", "Device B backend must be NONE");

    // Assert: Capture availability is false
    TEST_ASSERT(b_map["isCapturable"].toBool() == false, "Device B isCapturable must be false");
    TEST_ASSERT(bridge->canStart() == false, "bridge->canStart() must be false when unsupported device is selected");

    // Assert: Attempting to start capture cleanly fails without touching backends
    bool err_emitted = false;
    QString err_title;
    QString err_msg;
    QObject::connect(bridge.get(), &QmlBridge::errorOccurred, [&](const QString& title, const QString& msg) {
        err_emitted = true;
        err_title = title;
        err_msg = msg;
    });

    bool started = bridge->startCapture();
    TEST_ASSERT(!started, "bridge->startCapture() must return false for Device B");
    TEST_ASSERT(err_emitted, "errorOccurred must be emitted on startCapture attempt");
    TEST_ASSERT(err_title == "Capture Unavailable", "Error title must be 'Capture Unavailable'");
    TEST_ASSERT(err_msg.contains("No compatible spatial capture backend has been established"), "Refusal notice must be accurate");
    std::cout << "  Clean capture refusal verified: [" << err_title.toStdString() << "] "
              << err_msg.toStdString() << "\n";

    // Assert: Application remains healthy
    TEST_ASSERT(bridge->captureState() == "READY", "Application captureState must remain READY");
    TEST_ASSERT(!bridge->isCapturing(), "Application must not be capturing");
    std::cout << "  Device B runtime isolation assertions PASSED.\n";

    // -------------------------------------------------------------
    // STEP 4: Select Device A and Verify Normal Capture Path
    // -------------------------------------------------------------
    std::cout << "\n[STEP 4] Selecting Device A (V4L2 Camera) and asserting capture resolution...\n";
    bridge->selectProfileForCapture("camera_48mp_rgb");

    auto sensors_a = bridge->activeSensors();
    TEST_ASSERT(sensors_a.size() == 1, "Exactly 1 active sensor must be configured for Device A");
    QVariantMap a_map = sensors_a.first().toMap();

    std::cout << "  Active Sensor ID: " << a_map["id"].toString().toStdString() << "\n";
    std::cout << "  Profile ID: " << a_map["profileId"].toString().toStdString() << "\n";
    std::cout << "  Transport: " << a_map["transport"].toString().toStdString() << "\n";
    std::cout << "  Selected Adapter: " << a_map["adapterName"].toString().toStdString() << "\n";
    std::cout << "  Selected Backend: " << a_map["backendName"].toString().toStdString() << "\n";
    std::cout << "  Reconstruction Family: " << a_map["reconstructionFamily"].toString().toStdString() << "\n";

    // Assert: Device A resolves properly to CAMERA
    TEST_ASSERT(a_map["id"].toString() == "CAMERA", "Device A ID must be CAMERA");
    TEST_ASSERT(a_map["profileId"].toString() == "camera_48mp_rgb", "Device A Profile ID must match");

    // Assert: Selected adapter is GenericUvcSensorAdapter
    TEST_ASSERT(a_map["adapterName"].toString() == "GenericUvcSensorAdapter", "Selected adapter must be GenericUvcSensorAdapter");

    // Assert: Selected backend is LingBotStreamingBackend
    TEST_ASSERT(a_map["backendName"].toString() == "LingBotStreamingBackend", "Selected backend must be LingBotStreamingBackend");
    TEST_ASSERT(a_map["reconstructionFamily"].toString() == "MONOCULAR_NEURAL_RECONSTRUCTION", "Reconstruction family must match");

    // Assert: Resolved endpoint is /dev/video2
    TEST_ASSERT(a_map["transport"].toString().contains("/dev/video2"), "Transport must resolve to /dev/video2");

    // Assert: No serial endpoints or Device B properties bleed into Device A
    TEST_ASSERT(!a_map["transport"].toString().contains("/dev/ttyACM"), "No Device B serial endpoints must bleed into Device A");

    std::cout << "  Device A capture resolution assertions PASSED.\n";

    std::cout << "\n=================================================================\n";
    std::cout << "ALL RUNTIME DEVICE ISOLATION TESTS PASSED SUCCESSFULLY!\n";
    std::cout << "=================================================================\n";

    return 0;
}
