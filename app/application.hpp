#pragma once

#include <memory>
#include <string>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "core/storage/storage_engine.hpp"
#include "platform/platform_adapter.hpp"
#include "gui/qml_bridge.hpp"

namespace av::app {

struct AppConfig {
    std::string data_root;
    std::string log_file;
    bool headless{false};
    bool test_mode{false};
};

class Application {
public:
    Application(int& argc, char** argv);
    ~Application();

    int run();
    bool initialize();

    std::shared_ptr<core::storage::StorageEngine> storage() const { return storage_; }
    std::shared_ptr<platform::IPlatformAdapter> platform() const { return platform_; }

private:
    void parseCommandLineArgs();
    void seedDefaultData();

    int& argc_;
    char** argv_;
    AppConfig config_;

    std::shared_ptr<core::storage::StorageEngine> storage_;
    std::shared_ptr<platform::IPlatformAdapter> platform_;
    std::unique_ptr<QGuiApplication> qapp_;
    std::unique_ptr<gui::QmlBridge> qml_bridge_;
    std::unique_ptr<QQmlApplicationEngine> qml_engine_;
};

} // namespace av::app
