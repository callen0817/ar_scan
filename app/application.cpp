#include "application.hpp"
#include "core/logging/logger.hpp"
#include "platform/linux/linux_platform.hpp"
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QImage>
#include <QPainter>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <iostream>
#include <filesystem>
#include <dlfcn.h>

namespace av::app {

namespace fs = std::filesystem;

Application::Application(int& argc, char** argv)
    : argc_(argc), argv_(argv) {
    parseCommandLineArgs();
}

Application::~Application() {
    core::logging::Logger::instance().close();
}

void Application::parseCommandLineArgs() {
    for (int i = 1; i < argc_; ++i) {
        std::string arg = argv_[i];
        if (arg == "--headless" || (arg == "-platform" && (i + 1 < argc_ && std::string(argv_[i+1]) == "offscreen"))) {
            config_.headless = true;
        } else if (arg == "--test-mode") {
            config_.test_mode = true;
        } else if (arg.rfind("--data-root=", 0) == 0) {
            config_.data_root = arg.substr(12);
        } else if (arg.rfind("--log-file=", 0) == 0) {
            config_.log_file = arg.substr(11);
        } else if (arg.rfind("--screenshot=", 0) == 0) {
            config_.screenshot_path = arg.substr(13);
        }
    }

    if (config_.data_root.empty()) {
#ifdef AV_SOURCE_DIR
        config_.data_root = AV_SOURCE_DIR;
#else
        config_.data_root = fs::current_path().string();
#endif
    }
    if (config_.log_file.empty()) {
        config_.log_file = (fs::path(config_.data_root) / "av_scan.log").string();
    }
}

void Application::seedDefaultData() {
    // If profiles/validated/airy_profile.json exists on disk, load and verify
    std::string profiles_dir = (fs::path(config_.data_root) / "profiles").string();
    std::string configs_dir = (fs::path(config_.data_root) / "scanner_configs").string();

    auto profiles = storage_->listDeviceProfiles();
    AV_LOG_INFO("Application", "Storage engine enumerated " + std::to_string(profiles.size()) + " device profile(s).");

    auto configs = storage_->listScannerConfigs();
    AV_LOG_INFO("Application", "Storage engine enumerated " + std::to_string(configs.size()) + " scanner configuration(s).");
}

bool Application::initialize() {
    // 1. Initialize Structured Logging
    core::logging::Logger::instance().init(config_.log_file, core::logging::LogLevel::DEBUG);
    AV_LOG_INFO("Application", "=== Artificial Vision — AV Scan Starting ===");
    AV_LOG_INFO("Application", "Mission: Universal Sensor Spatial Capture Platform");
    AV_LOG_INFO("Application", "Slogan: Find Your Sense.");

    // 2. Platform Detection
    platform_ = std::make_shared<platform::linux_os::LinuxPlatformAdapter>();
    auto metrics = platform_->getHostMetrics();
    AV_LOG_INFO("Application", "Host: " + metrics.hostname + " (" + platform_->getPlatformName() + ")");
    if (metrics.is_jetson) {
        AV_LOG_INFO("Application", "Detected Jetson Platform: " + metrics.jetson_model);
    }
    AV_LOG_INFO("Application", "Memory: " + std::to_string(metrics.available_ram_bytes / (1024*1024)) + 
                                " MB available / " + std::to_string(metrics.total_ram_bytes / (1024*1024)) + " MB total");
    AV_LOG_INFO("Application", "Disk Space: " + std::to_string(metrics.available_disk_bytes / (1024*1024*1024)) + 
                                " GB available / " + std::to_string(metrics.total_disk_bytes / (1024*1024*1024)) + " GB total");

    // 3. Storage Engine Initialization
    storage_ = std::make_shared<core::storage::StorageEngine>();
    if (!storage_->initialize(config_.data_root)) {
        AV_LOG_ERROR("Application", "Failed to initialize storage engine at " + config_.data_root, "");
        return false;
    }

    seedDefaultData();

    // 4. GUI & QML Engine Setup
    // Preload isolated third_party libraries if present
    fs::path lib_dir = fs::path(config_.data_root) / "third_party" / "lib";
    if (fs::exists(lib_dir / "libQt5QuickTemplates2.so.5")) {
        void* h1 = dlopen((lib_dir / "libQt5QuickTemplates2.so.5").c_str(), RTLD_LAZY | RTLD_GLOBAL);
        (void)h1;
    }
    if (fs::exists(lib_dir / "libQt5QuickControls2.so.5")) {
        void* h2 = dlopen((lib_dir / "libQt5QuickControls2.so.5").c_str(), RTLD_LAZY | RTLD_GLOBAL);
        (void)h2;
    }

    // If running in headless / test environment or no DISPLAY, automatically fallback to offscreen
    const char* display = std::getenv("DISPLAY");
    if (!display || config_.headless) {
        setenv("QT_QPA_PLATFORM", "offscreen", 0);
        setenv("QT_QUICK_BACKEND", "software", 0);
        AV_LOG_INFO("Application", "Running with QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software");
    }

    // Enable per-pixel alpha buffer so the ApplicationWindow is genuinely transparent
    // on desktop compositors and optical see-through AR glasses (matching scanarMini).
    QQuickWindow::setDefaultAlphaBuffer(true);

    qapp_ = std::make_unique<QGuiApplication>(argc_, argv_);
    qapp_->setApplicationName("AV Scan");
    qapp_->setOrganizationName("Artificial Vision, Inc.");
    qapp_->setApplicationVersion("1.0.0");

    fs::path icon_file = fs::path(config_.data_root) / "gui" / "qml" / "assets" / "av_logo.png";
    if (fs::exists(icon_file)) {
        qapp_->setWindowIcon(QIcon(QString::fromStdString(icon_file.string())));
    }

    qml_engine_ = std::make_unique<QQmlApplicationEngine>();

    // Add isolated third_party QML paths
    fs::path repo_qml = fs::path(config_.data_root) / "third_party" / "qml";
    if (fs::exists(repo_qml)) {
        qml_engine_->addImportPath(QString::fromStdString(repo_qml.string()));
        AV_LOG_INFO("Application", "Added isolated QML import path: " + repo_qml.string());
    }

    // 5. Instantiate and expose QmlBridge
    qml_bridge_ = std::make_unique<gui::QmlBridge>(storage_, platform_);
    qml_engine_->rootContext()->setContextProperty("bridge", qml_bridge_.get());

    // 6. Load QML Window
    fs::path qml_file = fs::path(config_.data_root) / "gui" / "qml" / "AppWindow.qml";
    if (!fs::exists(qml_file)) {
        AV_LOG_ERROR("Application", "QML interface file not found: " + qml_file.string(), "");
        return false;
    }

    qml_engine_->load(QUrl::fromLocalFile(QString::fromStdString(qml_file.string())));
    if (qml_engine_->rootObjects().isEmpty()) {
        AV_LOG_ERROR("Application", "Failed to load QML root objects", "");
        return false;
    }

    AV_LOG_INFO("Application", "AV Scan user interface successfully initialized.");
    return true;
}

int Application::run() {
    if (!config_.screenshot_path.empty()) {
        QObject* root_obj = qml_engine_->rootObjects().first();
        auto* window = qobject_cast<QQuickWindow*>(root_obj);
        if (window && window->contentItem()) {
            window->setVisibility(QWindow::Windowed);
            window->setGeometry(0, 0, 1600, 960);
            window->requestUpdate();
            QTimer::singleShot(450, [window, this]() {
                auto grab_result = window->contentItem()->grabToImage(QSize(1600, 960));
                if (grab_result) {
                    QObject::connect(grab_result.data(), &QQuickItemGrabResult::ready, [grab_result, this]() {
                        QImage raw = grab_result->image();
                        QImage composited(raw.size(), QImage::Format_RGB32);
                        composited.fill(Qt::black);
                        QPainter painter(&composited);
                        painter.drawImage(0, 0, raw);
                        painter.end();
                        bool ok = composited.save(QString::fromStdString(config_.screenshot_path));
                        AV_LOG_INFO("Application", "Saved UI screenshot (" +
                                    std::to_string(composited.width()) + "x" +
                                    std::to_string(composited.height()) + ", ok=" +
                                    std::to_string(ok) + ") to: " + config_.screenshot_path);
                        qapp_->quit();
                    });
                } else {
                    qapp_->quit();
                }
            });
            return qapp_->exec();
        }
    }
    if (config_.test_mode) {
        AV_LOG_INFO("Application", "Test mode active: verified initialization cleanly, exiting with status 0.");
        return 0;
    }
    return qapp_->exec();
}

} // namespace av::app

