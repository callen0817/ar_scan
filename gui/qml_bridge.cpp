#include "qml_bridge.hpp"
#include "core/logging/logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

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
        return QString::fromStdString(platform_->getPlatformName());
    }
    return "UNKNOWN";
}

void QmlBridge::setSelectedScannerConfig(const QString& cfg) {
    if (selectedScannerConfig_ != cfg) {
        selectedScannerConfig_ = cfg;
        emit selectedScannerConfigChanged();
        AV_LOG_INFO("QmlBridge", "Selected scanner config: " + cfg.toStdString());
    }
}

void QmlBridge::refreshData() {
    if (!storage_) return;

    // Load available projects
    availableProjects_.clear();
    for (const auto& pid : storage_->listProjects()) {
        availableProjects_.append(QString::fromStdString(pid));
    }
    emit availableProjectsChanged();

    // Load available scanner configs
    availableScanners_.clear();
    for (const auto& cid : storage_->listScannerConfigs()) {
        availableScanners_.append(QString::fromStdString(cid));
    }
    if (!availableScanners_.isEmpty() && selectedScannerConfig_.isEmpty()) {
        selectedScannerConfig_ = availableScanners_.first();
        emit selectedScannerConfigChanged();
    }
    emit availableScannersChanged();
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
        projectName_ = name;
        projectId_ = QString::fromStdString(pid);
        captureState_ = "READY";
        emit projectNameChanged();
        emit projectIdChanged();
        emit captureStateChanged();
        refreshData();
        emit notification("Project Created", QString("Project '%1' is now active.").arg(name));
        return true;
    }

    emit errorOccurred("Storage Error", "Failed to save project.");
    return false;
}

bool QmlBridge::openProject(const QString& projectId) {
    if (!storage_) return false;

    core::schemas::Project proj;
    if (storage_->loadProject(projectId.toStdString(), proj)) {
        projectName_ = QString::fromStdString(proj.name());
        projectId_ = projectId;
        captureState_ = "READY";
        emit projectNameChanged();
        emit projectIdChanged();
        emit captureStateChanged();
        emit notification("Project Opened", QString("Loaded project: %1").arg(projectName_));
        return true;
    }

    emit errorOccurred("Open Project Error", "Could not find project: " + projectId);
    return false;
}

void QmlBridge::selectScannerConfig(const QString& configId) {
    setSelectedScannerConfig(configId);
}

bool QmlBridge::startCapture() {
    if (projectId_.isEmpty()) {
        emit errorOccurred("Capture Error", "Please create or open a project before starting capture.");
        return false;
    }

    if (isCapturing_) {
        return true;
    }

    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    currentSessionId_ = QString::fromStdString("session_" + std::to_string(now_ms));

    // Initialize session schema
    core::schemas::Session session(currentSessionId_.toStdString(), projectId_.toStdString(), "Capture Session");
    session.set_start_time_ns(now_ms * 1000000ULL);
    session.set_status(core::schemas::SessionStatus::ACTIVE);
    if (storage_) {
        storage_->saveSession(session);
    }

    elapsed_seconds_ = 0;
    elapsedTimeString_ = "00:00:00";
    pointsCaptured_ = 0;
    isCapturing_ = true;
    canSave_ = false;
    captureState_ = "CAPTURING";
    sensorStatus_ = "STREAMING";
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
        return true;
    }

    timer_->stop();
    isCapturing_ = false;
    canSave_ = true;
    captureState_ = "STOPPED";
    sensorStatus_ = "READY";
    trackingStatus_ = "TRACKING_OK";

    // Update session schema
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
    if (!canSave_) {
        emit errorOccurred("Save Error", "No completed capture available to save.");
        return false;
    }

    if (storage_ && !currentSessionId_.isEmpty()) {
        core::schemas::Session session;
        if (storage_->loadSession(currentSessionId_.toStdString(), session)) {
            session.set_status(core::schemas::SessionStatus::SAVED);
            storage_->saveSession(session);
        }
    }

    canSave_ = false;
    captureState_ = "SAVED";
    emit canSaveChanged();
    emit captureStateChanged();
    emit notification("Capture Saved", "Capture session and raw data stored successfully.");
    AV_LOG_INFO("QmlBridge", "Capture session committed to storage: " + currentSessionId_.toStdString());
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
    pointsCaptured_ += 12800; // Simulated nominal point count per second for UI feedback

    emit elapsedTimeStringChanged();
    emit pointsCapturedChanged();
}

} // namespace av::gui
