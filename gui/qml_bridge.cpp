#include "qml_bridge.hpp"
#include "core/logging/logger.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <QDateTime>

namespace av::gui {

QmlBridge::QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                     std::shared_ptr<platform::IPlatformAdapter> platform,
                     QObject *parent)
    : QObject(parent),
      storage_(std::move(storage)),
      platform_(std::move(platform)),
      timer_(new QTimer(this)) {

    connect(timer_, &QTimer::timeout, this, &QmlBridge::onTimerTick);
    refreshData();
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
            item["validationNotice"] = "Dual Scanner Configuration — Rigid extrinsic status is UNKNOWN (Awaiting M7)";
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
                item["validationNotice"] = "RoboSense Airy: Draft Specification — Physical hardware qualification scheduled for M3";
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

        emit projectNameChanged();
        emit projectIdChanged();
        emit projectDescriptionChanged();
        emit projectCreatedAtChanged();
        emit captureStateChanged();
        emit sensorStatusChanged();
        emit trackingStatusChanged();

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

        emit projectNameChanged();
        emit projectIdChanged();
        emit projectDescriptionChanged();
        emit projectCreatedAtChanged();
        emit captureStateChanged();
        emit sensorStatusChanged();
        emit trackingStatusChanged();

        emit notification("Project Opened", QString("Loaded active project: %1").arg(projectName_));
        AV_LOG_INFO("QmlBridge", "Opened existing project: " + projectId.toStdString() + " (" + projectName_.toStdString() + ")");
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
    pointsCaptured_ = 0; // Strictly 0: No fake synthetic points before M3 physical sensors
    isCapturing_ = true;
    canSave_ = false;
    captureState_ = "CAPTURING";
    sensorStatus_ = "STREAMING (STANDBY)";
    trackingStatus_ = "TRACKING_OK";

    timer_->start(1000);

    emit isCapturingChanged();
    emit canSaveChanged();
    emit captureStateChanged();
    emit sensorStatusChanged();
    emit trackingStatusChanged();
    emit elapsedTimeStringChanged();
    emit pointsCapturedChanged();

    AV_LOG_INFO("QmlBridge", "Capture started for session: " + currentSessionId_.toStdString());
    return true;
}

bool QmlBridge::stopCapture() {
    if (!isCapturing_) {
        emit errorOccurred("Capture Warning", "No capture session is currently running.");
        return false;
    }

    timer_->stop();
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

    AV_LOG_INFO("QmlBridge", "Capture stopped for session: " + currentSessionId_.toStdString());
    return true;
}

bool QmlBridge::saveCapture() {
    if (captureState_ != "STOPPED" || !canSave_) {
        emit errorOccurred("Save Error", "No completed capture session available to save.");
        return false;
    }

    if (storage_ && !currentSessionId_.isEmpty()) {
        // 1. Update Session status to SAVED
        core::schemas::Session session;
        if (storage_->loadSession(currentSessionId_.toStdString(), session)) {
            session.set_status(core::schemas::SessionStatus::SAVED);
            storage_->saveSession(session);

            // 2. Persist Capture schema record
            auto now = std::chrono::system_clock::now().time_since_epoch();
            uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
            std::string cap_id = "cap_" + std::to_string(now_ms);
            core::schemas::Capture capture(cap_id, currentSessionId_.toStdString(), selectedScannerId_.toStdString(), 0);
            capture.set_start_timestamp_ns(session.start_time_ns());
            capture.set_end_timestamp_ns(session.end_time_ns());
            capture.set_observation_count(0);
            storage_->saveCapture(capture);
        }
    }

    canSave_ = false;
    captureState_ = "SAVED";
    emit canSaveChanged();
    emit captureStateChanged();
    emit notification("Capture Saved", QString("Capture session %1 persisted successfully.").arg(currentSessionId_));
    AV_LOG_INFO("QmlBridge", "Capture session saved to storage: " + currentSessionId_.toStdString());
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
    pointsCaptured_ = 0; // Strictly real count: 0 before physical sensors in M3

    emit elapsedTimeStringChanged();
    emit pointsCapturedChanged();
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
    qreal yaw_rad = camYaw3D_ * M_PI / 180.0;
    qreal cos_yaw = std::cos(yaw_rad);
    qreal sin_yaw = std::sin(yaw_rad);

    qreal speed = (camDistance3D_ / 400.0);
    camTargetX3D_ += (-sin_yaw * deltaX - cos_yaw * deltaY) * speed;
    camTargetY3D_ += (cos_yaw * deltaX - sin_yaw * deltaY) * speed;

    emit cam3DChanged();
}

void QmlBridge::zoom3D(qreal deltaFactor) {
    if (deltaFactor > 0) {
        camDistance3D_ *= 0.88;
    } else if (deltaFactor < 0) {
        camDistance3D_ *= 1.14;
    }
    if (camDistance3D_ < 0.5) camDistance3D_ = 0.5;
    if (camDistance3D_ > 100.0) camDistance3D_ = 100.0;

    emit cam3DChanged();
}

void QmlBridge::recenter3D() {
    camYaw3D_ = 45.0;
    camPitch3D_ = 30.0;
    camDistance3D_ = 8.0;
    camTargetX3D_ = 0.0;
    camTargetY3D_ = 0.0;
    camTargetZ3D_ = 0.0;
    emit cam3DChanged();
}

void QmlBridge::setPreset3D(const QString& preset) {
    if (preset == "top") {
        camYaw3D_ = 0.0;
        camPitch3D_ = 89.9;
    } else if (preset == "front") {
        camYaw3D_ = 0.0;
        camPitch3D_ = 0.0;
    } else if (preset == "side") {
        camYaw3D_ = 90.0;
        camPitch3D_ = 0.0;
    } else { // "iso"
        camYaw3D_ = 45.0;
        camPitch3D_ = 30.0;
    }
    emit cam3DChanged();
}

// 2D Camera Controls
void QmlBridge::pan2D(qreal deltaX, qreal deltaY) {
    pan2DX_ -= deltaX / zoom2D_;
    pan2DY_ += deltaY / zoom2D_; // Y-up
    emit cam2DChanged();
}

void QmlBridge::zoom2DByFactor(qreal factor) {
    zoom2D_ *= factor;
    if (zoom2D_ < 5.0) zoom2D_ = 5.0;
    if (zoom2D_ > 300.0) zoom2D_ = 300.0;
    emit cam2DChanged();
}

void QmlBridge::recenter2D() {
    pan2DX_ = 0.0;
    pan2DY_ = 0.0;
    zoom2D_ = 40.0;
    emit cam2DChanged();
}

} // namespace av::gui
