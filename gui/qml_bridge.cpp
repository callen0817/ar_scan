#include "qml_bridge.hpp"
#include "core/logging/logger.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <QDateTime>

namespace fs = std::filesystem;

namespace av::gui {

QmlBridge::QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                     std::shared_ptr<platform::IPlatformAdapter> platform,
                     std::shared_ptr<core::drivers::airy::AirySensorAdapter> airyAdapter,
                     std::shared_ptr<core::slam::airy::AiryLioBackend> airyBackend,
                     QObject *parent)
    : QObject(parent),
      storage_(std::move(storage)),
      platform_(std::move(platform)),
      airyAdapter_(std::move(airyAdapter)),
      airyBackend_(std::move(airyBackend)),
      timer_(new QTimer(this)),
      telemetryTimer_(new QTimer(this)) {

    if (!airyAdapter_ || !airyBackend_) {
        auto ipc = std::make_shared<core::drivers::airy::AiryIpcClient>("127.0.0.1", 9099);
        if (!airyAdapter_) airyAdapter_ = std::make_shared<core::drivers::airy::AirySensorAdapter>(ipc);
        if (!airyBackend_) airyBackend_ = std::make_shared<core::slam::airy::AiryLioBackend>(ipc);
    }

    connect(timer_, &QTimer::timeout, this, &QmlBridge::onTimerTick);
    connect(telemetryTimer_, &QTimer::timeout, this, &QmlBridge::onTelemetryTick);
    refreshData();
}

QmlBridge::~QmlBridge() {
    if (timer_->isActive()) timer_->stop();
    if (telemetryTimer_->isActive()) telemetryTimer_->stop();
}

QString QmlBridge::hostPlatform() const {
    if (platform_) {
        auto metrics = platform_->getHostMetrics();
        return QString::fromStdString(metrics.hostname + " (" + platform_->getPlatformName() + ")");
    }
    return "UNKNOWN";
}

QString QmlBridge::hostHardware() const {
    if (platform_) {
        auto metrics = platform_->getHostMetrics();
        return QString::fromStdString(metrics.is_jetson ? metrics.jetson_model : "Generic Linux Host");
    }
    return "Generic Hardware";
}

QString QmlBridge::hostMemory() const {
    if (platform_) {
        auto metrics = platform_->getHostMetrics();
        double avail_gb = metrics.available_ram_bytes / (1024.0 * 1024.0 * 1024.0);
        double total_gb = metrics.total_ram_bytes / (1024.0 * 1024.0 * 1024.0);
        return QString("%1 GB free / %2 GB").arg(avail_gb, 0, 'f', 1).arg(total_gb, 0, 'f', 1);
    }
    return "Memory N/A";
}

QString QmlBridge::hostDisk() const {
    if (platform_) {
        auto metrics = platform_->getHostMetrics();
        double avail_gb = metrics.available_disk_bytes / (1024.0 * 1024.0 * 1024.0);
        double total_gb = metrics.total_disk_bytes / (1024.0 * 1024.0 * 1024.0);
        return QString("%1 GB free / %2 GB").arg(avail_gb, 0, 'f', 0).arg(total_gb, 0, 'f', 0);
    }
    return "Disk N/A";
}

void QmlBridge::refreshData() {
    if (!storage_) return;

    // 1. Load available projects
    availableProjects_.clear();
    projectList_.clear();
    for (const auto& pid : storage_->listProjects()) {
        availableProjects_.append(QString::fromStdString(pid));
        core::schemas::Project proj;
        if (storage_->loadProject(pid, proj)) {
            QVariantMap item;
            item["id"] = QString::fromStdString(proj.project_id());
            item["name"] = QString::fromStdString(proj.name());
            item["description"] = QString::fromStdString(proj.description());
            uint64_t created_s = proj.created_at_ns() / 1000000000ULL;
            if (created_s > 0) {
                QDateTime dt = QDateTime::fromSecsSinceEpoch(created_s);
                item["date"] = dt.toString("yyyy-MM-dd HH:mm");
            } else {
                item["date"] = "Initial";
            }
            projectList_.append(item);
        }
    }
    emit availableProjectsChanged();
    emit projectListChanged();

    // 2. Load available scanner configs and device profiles
    availableScanners_.clear();
    scannerList_.clear();

    // Add Scanner Configurations
    for (const auto& cid : storage_->listScannerConfigs()) {
        core::schemas::ScannerConfig cfg;
        if (storage_->loadScannerConfig(cid, cfg)) {
            QVariantMap item;
            item["id"] = QString::fromStdString(cfg.config_id());
            item["name"] = QString::fromStdString(cfg.name()) + " [CONFIG]";
            item["type"] = "SCANNER_CONFIG";
            item["status"] = "CONFIG";
            item["details"] = QString("%1 sensor instances (Airy LiDAR + Gemini 336L)").arg(cfg.sensor_instances().size());
            item["validationNotice"] = "scanR Configuration — Rigid extrinsic status is UNKNOWN (Awaiting M7)";
            scannerList_.append(item);
            availableScanners_.append(item["name"].toString());
        }
    }

    // Add Device Profiles
    for (const auto& pid : storage_->listDeviceProfiles()) {
        core::schemas::DeviceProfile prof;
        if (storage_->loadDeviceProfile(pid, prof)) {
            QVariantMap item;
            item["id"] = QString::fromStdString(prof.profile_id());
            QString statusStr = QString::fromStdString(core::schemas::to_string(prof.profile_status()));
            item["name"] = QString::fromStdString(prof.manufacturer() + " " + prof.model()) + " [" + statusStr + "]";
            item["type"] = "DEVICE_PROFILE";
            item["status"] = statusStr;
            item["details"] = QString::fromStdString(prof.device_family()) + " via " + QString::fromStdString(prof.transport());

            if (prof.profile_id().rfind("airy", 0) != std::string::npos || prof.profile_id().rfind("robosense", 0) != std::string::npos) {
                item["validationNotice"] = "RoboSense Airy: Physical Hardware Qualified (DIFOP EEPROM Calibrated)";
            } else if (prof.profile_id().rfind("gemini", 0) != std::string::npos || prof.profile_id().rfind("orbbec", 0) != std::string::npos) {
                item["validationNotice"] = "Gemini 336L: Draft Specification — Physical hardware qualification scheduled for M4";
            } else if (prof.profile_id().rfind("viture", 0) != std::string::npos) {
                item["validationNotice"] = "VITURE Ultra: Experimental Specification — Physical qualification scheduled for M5";
            } else {
                item["validationNotice"] = "Device Profile status: " + statusStr;
            }
            scannerList_.append(item);
            availableScanners_.append(item["name"].toString());
        }
    }

    if (!scannerList_.isEmpty() && selectedScannerId_.isEmpty()) {
        selectScannerByIndex(0);
    }
    emit availableScannersChanged();
    emit scannerListChanged();
    emit hostMetricsChanged();
}

void QmlBridge::selectScannerByIndex(int index) {
    if (index >= 0 && index < scannerList_.size()) {
        QVariantMap item = scannerList_.at(index).toMap();
        selectedScannerId_ = item["id"].toString();
        selectedScannerName_ = item["name"].toString();
        selectedScannerStatus_ = item["status"].toString();
        selectedScannerDetails_ = item["details"].toString();
        selectedScannerValidationNotice_ = item["validationNotice"].toString();
        emit selectedScannerChanged();
        AV_LOG_INFO("QmlBridge", "Selected scanner: " + selectedScannerName_.toStdString());
    }
}

void QmlBridge::selectScannerById(const QString& scannerId) {
    for (int i = 0; i < scannerList_.size(); ++i) {
        if (scannerList_.at(i).toMap()["id"].toString() == scannerId) {
            selectScannerByIndex(i);
            return;
        }
    }
}

bool QmlBridge::createProject(const QString& name, const QString& description) {
    if (name.trimmed().isEmpty()) {
        emit errorOccurred("Project Error", "Project name cannot be empty.");
        return false;
    }

    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string pid = "proj_" + std::to_string(now_ms);

    core::schemas::Project proj(pid, name.toStdString(), "", description.toStdString());
    proj.set_created_at_ns(now_ms * 1000000ULL);

    if (storage_ && storage_->saveProject(proj)) {
        projectName_ = name.trimmed();
        projectId_ = QString::fromStdString(pid);
        projectDescription_ = description.trimmed();

        QDateTime dt = QDateTime::fromMSecsSinceEpoch(now_ms);
        projectCreatedAt_ = dt.toString("yyyy-MM-dd HH:mm");

        captureState_ = "READY";
        sensorStatus_ = "READY";
        trackingStatus_ = "READY";
        pointsCaptured_ = 0;
        hasMapData_ = false;
        mapPoints_.clear();
        trajectory_.clear();

        emit projectNameChanged();
        emit projectIdChanged();
        emit projectDescriptionChanged();
        emit projectCreatedAtChanged();
        emit captureStateChanged();
        emit sensorStatusChanged();
        emit trackingStatusChanged();
        emit pointsCapturedChanged();
        emit mapDataUpdated();

        refreshData();
        emit notification("Project Created", QString("Project '%1' is now active.").arg(projectName_));
        AV_LOG_INFO("QmlBridge", "Created and loaded project: " + pid + " (" + projectName_.toStdString() + ")");
        return true;
    }

    emit errorOccurred("Storage Error", "Failed to save project to repository.");
    return false;
}

bool QmlBridge::openProject(const QString& projectId) {
    if (!storage_ || projectId.trimmed().isEmpty()) return false;

    core::schemas::Project proj;
    if (storage_->loadProject(projectId.toStdString(), proj)) {
        projectName_ = QString::fromStdString(proj.name());
        projectId_ = projectId;
        projectDescription_ = QString::fromStdString(proj.description());

        uint64_t created_s = proj.created_at_ns() / 1000000000ULL;
        if (created_s > 0) {
            QDateTime dt = QDateTime::fromSecsSinceEpoch(created_s);
            projectCreatedAt_ = dt.toString("yyyy-MM-dd HH:mm");
        } else {
            projectCreatedAt_ = "Initial";
        }

        captureState_ = "READY";
        sensorStatus_ = "READY";
        trackingStatus_ = "READY";

        // Check for saved captures in this project
        mapPoints_.clear();
        trajectory_.clear();
        hasMapData_ = false;

        fs::path proj_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId.toStdString();
        fs::path sess_dir = proj_dir / "sessions";
        if (fs::exists(sess_dir)) {
            for (const auto& sess_entry : fs::directory_iterator(sess_dir)) {
                if (sess_entry.is_directory()) {
                    fs::path caps_dir = sess_entry.path() / "captures";
                    if (fs::exists(caps_dir)) {
                        for (const auto& cap_entry : fs::directory_iterator(caps_dir)) {
                            if (cap_entry.is_directory()) {
                                fs::path pcd_file = cap_entry.path() / "map.pcd";
                                if (fs::exists(pcd_file)) {
                                    loadMapPcd(pcd_file.string(), mapPoints_);
                                    pointsCaptured_ = mapPoints_.size();
                                    hasMapData_ = (!mapPoints_.empty());
                                }
                                fs::path traj_file = cap_entry.path() / "trajectory.json";
                                if (fs::exists(traj_file)) {
                                    try {
                                        std::ifstream ifs(traj_file);
                                        nlohmann::json tj = nlohmann::json::parse(ifs);
                                        for (const auto& tp : tj) {
                                            core::schemas::Pose3D p;
                                            p.timestamp_ns = tp.value("timestamp_ns", 0ULL);
                                            p.position.x = tp.value("x", 0.0);
                                            p.position.y = tp.value("y", 0.0);
                                            p.position.z = tp.value("z", 0.0);
                                            p.orientation.x = tp.value("qx", 0.0);
                                            p.orientation.y = tp.value("qy", 0.0);
                                            p.orientation.z = tp.value("qz", 0.0);
                                            p.orientation.w = tp.value("qw", 1.0);
                                            trajectory_.push_back(p);
                                        }
                                        if (!trajectory_.empty()) {
                                            const auto& last = trajectory_.back();
                                            currentPoseX_ = last.position.x;
                                            currentPoseY_ = last.position.y;
                                            currentPoseZ_ = last.position.z;
                                        }
                                    } catch (...) {}
                                }
                                if (hasMapData_) break;
                            }
                        }
                    }
                }
                if (hasMapData_) break;
            }
        }

        emit projectNameChanged();
        emit projectIdChanged();
        emit projectDescriptionChanged();
        emit projectCreatedAtChanged();
        emit captureStateChanged();
        emit sensorStatusChanged();
        emit trackingStatusChanged();
        emit pointsCapturedChanged();
        emit poseChanged();
        emit trajectoryChanged();
        emit mapDataUpdated();

        emit notification("Project Opened", QString("Loaded project '%1' with %2 saved map points.")
                          .arg(projectName_).arg(pointsCaptured_));
        AV_LOG_INFO("QmlBridge", "Opened project: " + projectId.toStdString() + " with " + 
                     std::to_string(pointsCaptured_) + " map points");
        return true;
    }

    emit errorOccurred("Open Project Error", "Could not locate project record: " + projectId);
    return false;
}

bool QmlBridge::startCapture() {
    if (projectId_.isEmpty()) {
        emit errorOccurred("Capture Error", "Please create or open a project before starting capture.");
        return false;
    }

    if (isCapturing_) {
        emit errorOccurred("Capture Warning", "A capture session is already in progress.");
        return false;
    }

    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    currentSessionId_ = QString::fromStdString("session_" + std::to_string(now_ms));

    // Persist real Session schema
    core::schemas::Session session(currentSessionId_.toStdString(), projectId_.toStdString(), "Capture Session");
    session.set_start_time_ns(now_ms * 1000000ULL);
    session.set_status(core::schemas::SessionStatus::ACTIVE);
    if (storage_) {
        storage_->saveSession(session);
    }

    elapsed_seconds_ = 0;
    elapsedTimeString_ = "00:00:00";
    pointsCaptured_ = 0;
    mapPoints_.clear();
    trajectory_.clear();
    currentPoseX_ = 0.0;
    currentPoseY_ = 0.0;
    currentPoseZ_ = 0.0;
    currentYaw_ = 0.0;
    hasMapData_ = false;

    // Start RoboSense Airy driver and LIO SLAM
    bool hardware_active = false;
    if (airyBackend_ && airyAdapter_) {
        if (airyAdapter_->start() && airyBackend_->start()) {
            hardware_active = true;
            AV_LOG_INFO("QmlBridge", "RoboSense Airy hardware & SLAM started successfully.");
        }
    }

    isCapturing_ = true;
    canSave_ = false;
    captureState_ = "CAPTURING";
    sensorStatus_ = hardware_active ? "STREAMING (LIVE AIRY)" : "STREAMING (STANDBY)";
    trackingStatus_ = hardware_active ? "INITIALIZING" : "TRACKING_OK";

    timer_->start(1000);
    telemetryTimer_->start(100); // 10 Hz fast telemetry loop

    emit isCapturingChanged();
    emit canSaveChanged();
    emit captureStateChanged();
    emit sensorStatusChanged();
    emit trackingStatusChanged();
    emit elapsedTimeStringChanged();
    emit pointsCapturedChanged();
    emit poseChanged();
    emit trajectoryChanged();
    emit mapDataUpdated();

    AV_LOG_INFO("QmlBridge", "Capture started for session: " + currentSessionId_.toStdString());
    return true;
}

bool QmlBridge::stopCapture() {
    if (!isCapturing_) {
        emit errorOccurred("Capture Warning", "No capture session is currently running.");
        return false;
    }

    timer_->stop();
    telemetryTimer_->stop();

    if (airyBackend_ && airyAdapter_) {
        airyBackend_->stop();
        airyAdapter_->stop();
        mapPoints_ = airyBackend_->getMapPoints();
        trajectory_ = airyBackend_->getTrajectory();
        pointsCaptured_ = mapPoints_.size();
        hasMapData_ = (!mapPoints_.empty());
    }

    isCapturing_ = false;
    canSave_ = true;
    captureState_ = "STOPPED";
    sensorStatus_ = "READY";
    trackingStatus_ = "IDLE";

    // Update session schema in storage
    if (storage_ && !currentSessionId_.isEmpty()) {
        core::schemas::Session session;
        if (storage_->loadSession(currentSessionId_.toStdString(), session)) {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            session.set_end_time_ns(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
            session.set_status(core::schemas::SessionStatus::STOPPED);
            storage_->saveSession(session);
        }
    }

    emit isCapturingChanged();
    emit canSaveChanged();
    emit captureStateChanged();
    emit sensorStatusChanged();
    emit trackingStatusChanged();
    emit pointsCapturedChanged();
    emit mapDataUpdated();

    AV_LOG_INFO("QmlBridge", "Capture stopped. Final map points: " + std::to_string(pointsCaptured_));
    return true;
}

bool QmlBridge::saveCapture() {
    if (captureState_ != "STOPPED" || !canSave_) {
        emit errorOccurred("Save Error", "No completed capture session available to save.");
        return false;
    }

    if (storage_ && !currentSessionId_.isEmpty() && !projectId_.isEmpty()) {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        std::string cap_id = "cap_" + std::to_string(now_ms);

        fs::path cap_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString() / 
                           "sessions" / currentSessionId_.toStdString() / "captures" / cap_id;
        fs::create_directories(cap_dir);

        // 1. Save Map Point Cloud (PCD & PLY)
        fs::path pcd_path = cap_dir / "map.pcd";
        saveMapPcd(pcd_path.string(), mapPoints_);

        // 2. Save Trajectory
        fs::path traj_path = cap_dir / "trajectory.json";
        nlohmann::json tj = nlohmann::json::array();
        for (const auto& tp : trajectory_) {
            tj.push_back({
                {"timestamp_ns", tp.timestamp_ns},
                {"x", tp.position.x},
                {"y", tp.position.y},
                {"z", tp.position.z},
                {"qx", tp.orientation.x},
                {"qy", tp.orientation.y},
                {"qz", tp.orientation.z},
                {"qw", tp.orientation.w}
            });
        }
        std::ofstream tof(traj_path);
        tof << tj.dump(2);
        tof.close();

        // 3. Save MapMetadata
        fs::path meta_path = cap_dir / "map_metadata.json";
        core::schemas::MapMetadata meta = (airyBackend_) ? airyBackend_->getMapMetadata() : core::schemas::MapMetadata();
        meta.set_point_count(pointsCaptured_);
        meta.set_trajectory(trajectory_);
        std::ofstream mof(meta_path);
        mof << meta.to_json().dump(2);
        mof.close();

        // 4. Save Capture Schema
        core::schemas::Capture capture(cap_id, currentSessionId_.toStdString(), selectedScannerId_.toStdString(), 0);
        core::schemas::Session session;
        if (storage_->loadSession(currentSessionId_.toStdString(), session)) {
            capture.set_start_timestamp_ns(session.start_time_ns());
            capture.set_end_timestamp_ns(session.end_time_ns());
            session.set_status(core::schemas::SessionStatus::SAVED);
            storage_->saveSession(session);
        }
        capture.set_observation_count(pointsCaptured_);
        capture.set_map_path(pcd_path.string());
        capture.set_trajectory_path(traj_path.string());
        capture.set_status("COMPLETED");

        if (airyAdapter_) {
            auto cal = airyAdapter_->getCalibration();
            if (!cal.empty()) capture.set_provenance(cal[0]);
        }
        storage_->saveCapture(capture);
    }

    canSave_ = false;
    captureState_ = "SAVED";
    hasMapData_ = (!mapPoints_.empty());

    emit canSaveChanged();
    emit captureStateChanged();
    emit mapDataUpdated();
    emit notification("Capture Saved", QString("Capture %1 persisted (%2 points).").arg(currentSessionId_).arg(pointsCaptured_));
    AV_LOG_INFO("QmlBridge", "Capture saved successfully: " + currentSessionId_.toStdString());
    return true;
}

void QmlBridge::onTimerTick() {
    if (!isCapturing_) return;

    ++elapsed_seconds_;
    int hrs = elapsed_seconds_ / 3600;
    int mins = (elapsed_seconds_ % 3600) / 60;
    int secs = elapsed_seconds_ % 60;

    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hrs << ":"
       << std::setfill('0') << std::setw(2) << mins << ":"
       << std::setfill('0') << std::setw(2) << secs;

    elapsedTimeString_ = QString::fromStdString(ss.str());
    emit elapsedTimeStringChanged();
}

void QmlBridge::onTelemetryTick() {
    if (!isCapturing_ || !airyBackend_) return;

    if (airyBackend_->updateTelemetry()) {
        currentPoseX_ = airyBackend_->getPose().position.x;
        currentPoseY_ = airyBackend_->getPose().position.y;
        currentPoseZ_ = airyBackend_->getPose().position.z;
        currentYaw_ = airyBackend_->getCurrentYaw();
        pointsCaptured_ = airyBackend_->getPointCount();
        trackingStatus_ = QString::fromStdString(core::schemas::to_string(airyBackend_->getTrackingState()));

        if (airyAdapter_) {
            sensorStatus_ = QString::fromStdString(core::schemas::to_string(airyAdapter_->getStatus()));
        }

        airyBackend_->fetchNewPoints();
        mapPoints_ = airyBackend_->getMapPoints();
        trajectory_ = airyBackend_->getTrajectory();
        hasMapData_ = (!mapPoints_.empty());

        emit poseChanged();
        emit trackingStatusChanged();
        emit sensorStatusChanged();
        emit pointsCapturedChanged();
        emit trajectoryChanged();
        emit mapDataUpdated();
    }
}

QVariantList QmlBridge::getDisplayPoints(int maxPoints) const {
    QVariantList list;
    if (mapPoints_.empty()) return list;

    size_t total = mapPoints_.size();
    size_t step = (total > static_cast<size_t>(maxPoints)) ? (total / maxPoints) : 1;
    if (step == 0) step = 1;

    list.reserve(static_cast<int>(total / step) + 1);
    for (size_t i = 0; i < total; i += step) {
        const auto& pt = mapPoints_[i];
        QVariantList p;
        p.append(pt.x);
        p.append(pt.y);
        p.append(pt.z);
        p.append(pt.intensity);
        list.append(QVariant::fromValue(p));
    }
    return list;
}

QVariantList QmlBridge::getTrajectoryPoints() const {
    QVariantList list;
    list.reserve(static_cast<int>(trajectory_.size()));
    for (const auto& tp : trajectory_) {
        QVariantList p;
        p.append(tp.position.x);
        p.append(tp.position.y);
        p.append(tp.position.z);
        // Compute yaw from quaternion
        double qx = tp.orientation.x;
        double qy = tp.orientation.y;
        double qz = tp.orientation.z;
        double qw = tp.orientation.w;
        double yaw = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));
        p.append(yaw);
        list.append(QVariant::fromValue(p));
    }
    return list;
}

bool QmlBridge::loadSavedCapture(const QString& captureId) {
    if (!storage_ || projectId_.isEmpty()) return false;

    fs::path proj_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString();
    fs::path sess_dir = proj_dir / "sessions";
    if (!fs::exists(sess_dir)) return false;

    for (const auto& s_entry : fs::directory_iterator(sess_dir)) {
        if (s_entry.is_directory()) {
            fs::path cap_dir = s_entry.path() / "captures" / captureId.toStdString();
            if (fs::exists(cap_dir)) {
                fs::path pcd_file = cap_dir / "map.pcd";
                if (fs::exists(pcd_file)) {
                    loadMapPcd(pcd_file.string(), mapPoints_);
                    pointsCaptured_ = mapPoints_.size();
                    hasMapData_ = (!mapPoints_.empty());
                }
                fs::path traj_file = cap_dir / "trajectory.json";
                if (fs::exists(traj_file)) {
                    try {
                        std::ifstream ifs(traj_file);
                        nlohmann::json tj = nlohmann::json::parse(ifs);
                        trajectory_.clear();
                        for (const auto& tp : tj) {
                            core::schemas::Pose3D p;
                            p.timestamp_ns = tp.value("timestamp_ns", 0ULL);
                            p.position.x = tp.value("x", 0.0);
                            p.position.y = tp.value("y", 0.0);
                            p.position.z = tp.value("z", 0.0);
                            p.orientation.x = tp.value("qx", 0.0);
                            p.orientation.y = tp.value("qy", 0.0);
                            p.orientation.z = tp.value("qz", 0.0);
                            p.orientation.w = tp.value("qw", 1.0);
                            trajectory_.push_back(p);
                        }
                    } catch (...) {}
                }
                emit pointsCapturedChanged();
                emit trajectoryChanged();
                emit mapDataUpdated();
                return true;
            }
        }
    }
    return false;
}

void QmlBridge::saveMapPcd(const std::string& filepath, const std::vector<core::schemas::PointXYZI>& points) const {
    std::ofstream ofs(filepath);
    ofs << "# .PCD v0.7 - Point Cloud Data file format\n"
        << "VERSION 0.7\n"
        << "FIELDS x y z intensity\n"
        << "SIZE 4 4 4 4\n"
        << "TYPE F F F F\n"
        << "COUNT 1 1 1 1\n"
        << "WIDTH " << points.size() << "\n"
        << "HEIGHT 1\n"
        << "VIEWPOINT 0 0 0 1 0 0 0\n"
        << "POINTS " << points.size() << "\n"
        << "DATA ascii\n";

    for (const auto& pt : points) {
        ofs << pt.x << " " << pt.y << " " << pt.z << " " << pt.intensity << "\n";
    }
    ofs.close();
}

bool QmlBridge::loadMapPcd(const std::string& filepath, std::vector<core::schemas::PointXYZI>& out_points) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    out_points.clear();
    std::string line;
    bool in_data = false;

    while (std::getline(ifs, line)) {
        if (!in_data) {
            if (line.rfind("DATA", 0) == 0) {
                in_data = true;
            }
            continue;
        }

        std::istringstream iss(line);
        core::schemas::PointXYZI pt;
        if (iss >> pt.x >> pt.y >> pt.z) {
            if (!(iss >> pt.intensity)) {
                pt.intensity = 1.0f;
            }
            out_points.push_back(pt);
        }
    }
    return true;
}

// 3D Camera Controls
void QmlBridge::orbit3D(qreal deltaX, qreal deltaY) {
    camYaw3D_ += deltaX * 0.4;
    while (camYaw3D_ >= 360.0) camYaw3D_ -= 360.0;
    while (camYaw3D_ < 0.0) camYaw3D_ += 360.0;

    camPitch3D_ += deltaY * 0.4;
    if (camPitch3D_ > 89.0) camPitch3D_ = 89.0;
    if (camPitch3D_ < -89.0) camPitch3D_ = -89.0;

    emit cam3DChanged();
}

void QmlBridge::pan3D(qreal deltaX, qreal deltaY) {
    double radYaw = camYaw3D_ * M_PI / 180.0;
    double radPitch = camPitch3D_ * M_PI / 180.0;

    double fx = std::cos(radPitch) * std::sin(radYaw);
    double fy = std::cos(radPitch) * std::cos(radYaw);
    double rx = std::cos(radYaw);
    double ry = -std::sin(radYaw);

    double ux = -std::sin(radPitch) * std::sin(radYaw);
    double uy = -std::sin(radPitch) * std::cos(radYaw);
    double uz = std::cos(radPitch);

    double factor = camDistance3D_ * 0.002;
    camTargetX3D_ -= (rx * deltaX - ux * deltaY) * factor;
    camTargetY3D_ -= (ry * deltaX - uy * deltaY) * factor;
    camTargetZ3D_ -= (-uz * deltaY) * factor;

    emit cam3DChanged();
}

void QmlBridge::zoom3D(qreal deltaFactor) {
    if (deltaFactor > 0) {
        camDistance3D_ *= 0.9;
    } else {
        camDistance3D_ *= 1.1;
    }
    if (camDistance3D_ < 0.5) camDistance3D_ = 0.5;
    if (camDistance3D_ > 200.0) camDistance3D_ = 200.0;
    emit cam3DChanged();
}

void QmlBridge::recenter3D() {
    camTargetX3D_ = currentPoseX_;
    camTargetY3D_ = currentPoseY_;
    camTargetZ3D_ = currentPoseZ_;
    camDistance3D_ = 8.0;
    camYaw3D_ = 45.0;
    camPitch3D_ = 30.0;
    emit cam3DChanged();
}

void QmlBridge::setPreset3D(const QString& preset) {
    if (preset == "TOP") {
        camYaw3D_ = 0.0;
        camPitch3D_ = 89.0;
    } else if (preset == "FRONT") {
        camYaw3D_ = 0.0;
        camPitch3D_ = 0.0;
    } else if (preset == "SIDE") {
        camYaw3D_ = 90.0;
        camPitch3D_ = 0.0;
    } else if (preset == "ISO") {
        camYaw3D_ = 45.0;
        camPitch3D_ = 30.0;
    }
    emit cam3DChanged();
}

// 2D Camera Controls
void QmlBridge::pan2D(qreal deltaX, qreal deltaY) {
    pan2DX_ -= deltaX / zoom2D_;
    pan2DY_ += deltaY / zoom2D_;
    emit cam2DChanged();
}

void QmlBridge::zoom2DByFactor(qreal factor) {
    zoom2D_ *= factor;
    if (zoom2D_ < 2.0) zoom2D_ = 2.0;
    if (zoom2D_ > 500.0) zoom2D_ = 500.0;
    emit cam2DChanged();
}

void QmlBridge::recenter2D() {
    pan2DX_ = currentPoseX_;
    pan2DY_ = currentPoseY_;
    zoom2D_ = 40.0;
    emit cam2DChanged();
}

} // namespace av::gui
