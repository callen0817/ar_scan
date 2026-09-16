#pragma once

#include "core/schemas/project.hpp"
#include "core/schemas/session.hpp"
#include "core/schemas/capture.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/scanner_config.hpp"
#include <string>
#include <vector>
#include <memory>

namespace av::core::interfaces {

class IStorageEngine {
public:
    virtual ~IStorageEngine() = default;

    // Initialization & Root Directory
    virtual bool initialize(const std::string& root_data_directory) = 0;
    virtual std::string getRootDirectory() const = 0;

    // Project Persistence
    virtual bool saveProject(const schemas::Project& project) = 0;
    virtual bool loadProject(const std::string& project_id, schemas::Project& out_project) = 0;
    virtual std::vector<std::string> listProjects() const = 0;

    // Session Persistence
    virtual bool saveSession(const schemas::Session& session) = 0;
    virtual bool loadSession(const std::string& session_id, schemas::Session& out_session) = 0;
    virtual std::vector<std::string> listSessions(const std::string& project_id) const = 0;

    // Capture Persistence
    virtual bool saveCapture(const schemas::Capture& capture) = 0;
    virtual bool loadCapture(const std::string& capture_id, schemas::Capture& out_capture) = 0;

    // Profile & Configuration Persistence
    virtual bool saveDeviceProfile(const schemas::DeviceProfile& profile) = 0;
    virtual bool loadDeviceProfile(const std::string& profile_id, schemas::DeviceProfile& out_profile) = 0;
    virtual std::vector<std::string> listDeviceProfiles() const = 0;

    virtual bool saveScannerConfig(const schemas::ScannerConfig& config) = 0;
    virtual bool loadScannerConfig(const std::string& config_id, schemas::ScannerConfig& out_config) = 0;
    virtual std::vector<std::string> listScannerConfigs() const = 0;
};

} // namespace av::core::interfaces
