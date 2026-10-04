#pragma once

#include "core/interfaces/istorage_engine.hpp"
#include <mutex>
#include <filesystem>

namespace av::core::storage {

namespace fs = std::filesystem;

class StorageEngine : public interfaces::IStorageEngine {
public:
    StorageEngine() = default;
    ~StorageEngine() override = default;

    bool initialize(const std::string& root_data_directory) override;
    std::string getRootDirectory() const override;

    // Project Persistence
    bool saveProject(const schemas::Project& project) override;
    bool loadProject(const std::string& project_id, schemas::Project& out_project) override;
    std::vector<std::string> listProjects() const override;

    // Session Persistence
    bool saveSession(const schemas::Session& session) override;
    bool loadSession(const std::string& session_id, schemas::Session& out_session) override;
    std::vector<std::string> listSessions(const std::string& project_id) const override;

    // Capture Persistence
    bool saveCapture(const schemas::Capture& capture) override;
    bool loadCapture(const std::string& capture_id, schemas::Capture& out_capture) override;

    // Profile & Configuration Persistence
    bool saveDeviceProfile(const schemas::DeviceProfile& profile) override;
    bool loadDeviceProfile(const std::string& profile_id, schemas::DeviceProfile& out_profile) override;
    std::vector<std::string> listDeviceProfiles() const override;

    bool saveScannerConfig(const schemas::ScannerConfig& config) override;
    bool loadScannerConfig(const std::string& config_id, schemas::ScannerConfig& out_config) override;
    std::vector<std::string> listScannerConfigs() const override;

    // Scanner Node Persistence
    bool saveScannerNode(const schemas::ScannerNode& node) override;
    bool loadScannerNode(const std::string& node_id, schemas::ScannerNode& out_node) override;
    std::vector<std::string> listScannerNodes() const override;

    // Session-Scoped Scanner Node Persistence (M10 Multi-Node Session)
    bool saveSessionScannerNode(const std::string& project_id, const std::string& session_id, const schemas::ScannerNode& node) override;
    bool loadSessionScannerNode(const std::string& project_id, const std::string& session_id, const std::string& node_id, schemas::ScannerNode& out_node) override;
    std::vector<schemas::ScannerNode> listSessionScannerNodes(const std::string& project_id, const std::string& session_id) override;

    // Registration Persistence (Milestone 9 Cross-Sensor Registration)
    bool saveRegistrationResult(const std::string& project_id, const std::string& session_id, const schemas::RegistrationResult& reg) override;
    bool loadRegistrationResult(const std::string& project_id, const std::string& session_id, const std::string& reg_id, schemas::RegistrationResult& out_reg) override;
    std::vector<schemas::RegistrationResult> listRegistrationResults(const std::string& project_id, const std::string& session_id) override;

private:
    mutable std::mutex mutex_;
    fs::path root_path_;

    bool atomicWriteJson(const fs::path& target_file, const nlohmann::json& content) const;
    bool readJson(const fs::path& source_file, nlohmann::json& out_content) const;
};

} // namespace av::core::storage
