#include "storage_engine.hpp"
#include "core/logging/logger.hpp"
#include <fstream>

namespace av::core::storage {

bool StorageEngine::initialize(const std::string& root_data_directory) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        root_path_ = fs::absolute(root_data_directory);
        fs::create_directories(root_path_ / "projects");
        fs::create_directories(root_path_ / "profiles" / "validated");
        fs::create_directories(root_path_ / "profiles" / "experimental");
        fs::create_directories(root_path_ / "profiles" / "rejected");
        fs::create_directories(root_path_ / "scanner_configs");

        AV_LOG_INFO("StorageEngine", "Initialized storage repository at: " + root_path_.string());
        return true;
    } catch (const std::exception& e) {
        AV_LOG_ERROR("StorageEngine", "Failed to initialize storage", e.what());
        return false;
    }
}

std::string StorageEngine::getRootDirectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return root_path_.string();
}

bool StorageEngine::atomicWriteJson(const fs::path& target_file, const nlohmann::json& content) const {
    try {
        fs::create_directories(target_file.parent_path());
        fs::path tmp_file = target_file;
        tmp_file += ".tmp";

        {
            std::ofstream ofs(tmp_file, std::ios::out | std::ios::trunc);
            if (!ofs.is_open()) {
                return false;
            }
            ofs << content.dump(2) << "\n";
            ofs.flush();
        }

        fs::rename(tmp_file, target_file);
        return true;
    } catch (const std::exception& e) {
        AV_LOG_ERROR("StorageEngine", "Atomic write error for " + target_file.string(), e.what());
        return false;
    }
}

bool StorageEngine::readJson(const fs::path& source_file, nlohmann::json& out_content) const {
    try {
        if (!fs::exists(source_file)) {
            return false;
        }
        std::ifstream ifs(source_file);
        if (!ifs.is_open()) {
            return false;
        }
        ifs >> out_content;
        return true;
    } catch (const std::exception& e) {
        AV_LOG_ERROR("StorageEngine", "Read JSON error for " + source_file.string(), e.what());
        return false;
    }
}

bool StorageEngine::saveProject(const schemas::Project& project) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (project.project_id().empty()) {
        AV_LOG_ERROR("StorageEngine", "Cannot save project with empty ID", "");
        return false;
    }
    fs::path project_dir = root_path_ / "projects" / project.project_id();
    fs::path project_file = project_dir / "project.json";
    bool success = atomicWriteJson(project_file, project.to_json());
    if (success) {
        AV_LOG_INFO("StorageEngine", "Saved project: " + project.name() + " (" + project.project_id() + ")");
    }
    return success;
}

bool StorageEngine::loadProject(const std::string& project_id, schemas::Project& out_project) {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::path project_file = root_path_ / "projects" / project_id / "project.json";
    nlohmann::json j;
    if (!readJson(project_file, j)) {
        return false;
    }
    out_project = schemas::Project::from_json(j);
    return true;
}

std::vector<std::string> StorageEngine::listProjects() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> projects;
    fs::path projects_dir = root_path_ / "projects";
    if (!fs::exists(projects_dir)) return projects;

    for (const auto& entry : fs::directory_iterator(projects_dir)) {
        if (entry.is_directory() && fs::exists(entry.path() / "project.json")) {
            projects.push_back(entry.path().filename().string());
        }
    }
    return projects;
}

bool StorageEngine::saveSession(const schemas::Session& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (session.session_id().empty() || session.project_id().empty()) {
        return false;
    }
    fs::path session_dir = root_path_ / "projects" / session.project_id() / "sessions" / session.session_id();
    fs::path session_file = session_dir / "session.json";
    return atomicWriteJson(session_file, session.to_json());
}

bool StorageEngine::loadSession(const std::string& session_id, schemas::Session& out_session) {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::path projects_dir = root_path_ / "projects";
    if (!fs::exists(projects_dir)) return false;

    for (const auto& proj_entry : fs::directory_iterator(projects_dir)) {
        if (proj_entry.is_directory()) {
            fs::path session_file = proj_entry.path() / "sessions" / session_id / "session.json";
            nlohmann::json j;
            if (readJson(session_file, j)) {
                out_session = schemas::Session::from_json(j);
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> StorageEngine::listSessions(const std::string& project_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> sessions;
    fs::path sessions_dir = root_path_ / "projects" / project_id / "sessions";
    if (!fs::exists(sessions_dir)) return sessions;

    for (const auto& entry : fs::directory_iterator(sessions_dir)) {
        if (entry.is_directory() && fs::exists(entry.path() / "session.json")) {
            sessions.push_back(entry.path().filename().string());
        }
    }
    return sessions;
}

bool StorageEngine::saveCapture(const schemas::Capture& capture) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capture.capture_id().empty() || capture.session_id().empty()) {
        return false;
    }
    // Search for project containing this session
    fs::path projects_dir = root_path_ / "projects";
    if (!fs::exists(projects_dir)) return false;

    for (const auto& proj_entry : fs::directory_iterator(projects_dir)) {
        if (proj_entry.is_directory()) {
            fs::path session_dir = proj_entry.path() / "sessions" / capture.session_id();
            if (fs::exists(session_dir)) {
                fs::path capture_dir = session_dir / "captures" / capture.capture_id();
                fs::path capture_file = capture_dir / "capture.json";
                return atomicWriteJson(capture_file, capture.to_json());
            }
        }
    }
    return false;
}

bool StorageEngine::loadCapture(const std::string& capture_id, schemas::Capture& out_capture) {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::path projects_dir = root_path_ / "projects";
    if (!fs::exists(projects_dir)) return false;

    for (const auto& proj_entry : fs::directory_iterator(projects_dir)) {
        if (proj_entry.is_directory()) {
            fs::path sessions_dir = proj_entry.path() / "sessions";
            if (fs::exists(sessions_dir)) {
                for (const auto& sess_entry : fs::directory_iterator(sessions_dir)) {
                    fs::path capture_file = sess_entry.path() / "captures" / capture_id / "capture.json";
                    nlohmann::json j;
                    if (readJson(capture_file, j)) {
                        out_capture = schemas::Capture::from_json(j);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool StorageEngine::saveDeviceProfile(const schemas::DeviceProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (profile.profile_id().empty()) return false;

    std::string subfolder = "draft";
    if (profile.profile_status() == schemas::ProfileStatus::VALIDATED) {
        subfolder = "validated";
    } else if (profile.profile_status() == schemas::ProfileStatus::EXPERIMENTAL) {
        subfolder = "experimental";
    } else if (profile.profile_status() == schemas::ProfileStatus::REJECTED) {
        subfolder = "rejected";
    }

    fs::path target_file = root_path_ / "profiles" / subfolder / (profile.profile_id() + ".json");
    return atomicWriteJson(target_file, profile.to_json());
}

bool StorageEngine::loadDeviceProfile(const std::string& profile_id, schemas::DeviceProfile& out_profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::vector<std::string> subfolders = {"draft", "experimental", "validated", "rejected"};
    for (const auto& sub : subfolders) {
        fs::path p = root_path_ / "profiles" / sub / (profile_id + ".json");
        nlohmann::json j;
        if (readJson(p, j)) {
            out_profile = schemas::DeviceProfile::from_json(j);
            return true;
        }
    }
    return false;
}

std::vector<std::string> StorageEngine::listDeviceProfiles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> profiles;
    const std::vector<std::string> subfolders = {"draft", "experimental", "validated", "rejected"};
    for (const auto& sub : subfolders) {
        fs::path dir = root_path_ / "profiles" / sub;
        if (fs::exists(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.path().extension() == ".json") {
                    profiles.push_back(entry.path().stem().string());
                }
            }
        }
    }
    return profiles;
}

bool StorageEngine::saveScannerConfig(const schemas::ScannerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (config.config_id().empty()) return false;
    fs::path target_file = root_path_ / "scanner_configs" / (config.config_id() + ".json");
    return atomicWriteJson(target_file, config.to_json());
}

bool StorageEngine::loadScannerConfig(const std::string& config_id, schemas::ScannerConfig& out_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    fs::path target_file = root_path_ / "scanner_configs" / (config_id + ".json");
    nlohmann::json j;
    if (readJson(target_file, j)) {
        out_config = schemas::ScannerConfig::from_json(j);
        return true;
    }
    return false;
}

std::vector<std::string> StorageEngine::listScannerConfigs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> configs;
    fs::path dir = root_path_ / "scanner_configs";
    if (fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                configs.push_back(entry.path().stem().string());
            }
        }
    }
    return configs;
}

} // namespace av::core::storage
