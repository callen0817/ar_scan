#include "qml_bridge.hpp"
#include "core/logging/logger.hpp"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <QDateTime>
#include <QRegularExpression>

namespace fs = std::filesystem;

namespace av::gui {

QmlBridge::QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                     std::shared_ptr<platform::IPlatformAdapter> platform,
                     std::shared_ptr<core::drivers::airy::AirySensorAdapter> airyAdapter,
                     std::shared_ptr<core::slam::airy::AiryLioBackend> airyBackend,
                     std::shared_ptr<core::drivers::gemini::GeminiSensorAdapter> geminiAdapter,
                     std::shared_ptr<core::slam::gemini::GeminiRgbdBackend> geminiBackend,
                     std::shared_ptr<core::drivers::viture::VitureSensorAdapter> vitureAdapter,
                     std::shared_ptr<core::slam::viture::VitureVioBackend> vitureBackend,
                     QObject *parent)
    : QObject(parent),
      storage_(std::move(storage)),
      platform_(std::move(platform)),
      airyAdapter_(std::move(airyAdapter)),
      airyBackend_(std::move(airyBackend)),
      geminiAdapter_(std::move(geminiAdapter)),
      geminiBackend_(std::move(geminiBackend)),
      vitureAdapter_(std::move(vitureAdapter)),
      vitureBackend_(std::move(vitureBackend)),
      timer_(new QTimer(this)),
      telemetryTimer_(new QTimer(this)) {

    if (!airyAdapter_ || !airyBackend_) {
        auto ipc = std::make_shared<core::drivers::airy::AiryIpcClient>("127.0.0.1", 9099);
        if (!airyAdapter_) airyAdapter_ = std::make_shared<core::drivers::airy::AirySensorAdapter>(ipc);
        if (!airyBackend_) airyBackend_ = std::make_shared<core::slam::airy::AiryLioBackend>(ipc);
    }

    if (!geminiAdapter_ || !geminiBackend_) {
        auto g_ipc = std::make_shared<core::drivers::gemini::GeminiIpcClient>("127.0.0.1", 9100);
        if (!geminiAdapter_) geminiAdapter_ = std::make_shared<core::drivers::gemini::GeminiSensorAdapter>(g_ipc);
        if (!geminiBackend_) geminiBackend_ = std::make_shared<core::slam::gemini::GeminiRgbdBackend>(g_ipc);
    }

    if (!vitureAdapter_ || !vitureBackend_) {
        auto v_ipc = std::make_shared<core::drivers::viture::VitureIpcClient>("127.0.0.1", 9101);
        if (!vitureAdapter_) vitureAdapter_ = std::make_shared<core::drivers::viture::VitureSensorAdapter>(v_ipc);
        if (!vitureBackend_) vitureBackend_ = std::make_shared<core::slam::viture::VitureVioBackend>(v_ipc);
    }

    if (!cameraBackend_) {
        cameraBackend_ = std::make_shared<core::reconstruction::lingbot::LingBotSlamBackend>();
    }

    // Initialize MultiNodeSessionOrchestrator
    multiNodeOrchestrator_ = std::make_shared<core::multi_node::MultiNodeSessionOrchestrator>();
    if (multiNodeOrchestrator_->initialize(storage_)) {
        multiNodeOrchestrator_->setInviteHandler([this](const std::string& sid, const std::string& coord_uuid, const std::string& pid) {
            emit sessionInviteReceived(QString::fromStdString(sid), QString::fromStdString(coord_uuid), QString::fromStdString(pid));
            emit notification("Session Invite", QString("Received invitation to session %1 from %2").arg(QString::fromStdString(sid)).arg(QString::fromStdString(coord_uuid)));
        });
        multiNodeOrchestrator_->setCaptureStateChangedHandler([this](bool capturing, uint64_t ts) {
            (void)capturing;
            (void)ts;
            emit multiNodeSessionChanged();
            emit isCapturingChanged();
        });
        multiNodeOrchestrator_->setMemberListChangedHandler([this]() {
            emit sessionMembersChanged();
        });
    }

    connect(timer_, &QTimer::timeout, this, &QmlBridge::onTimerTick);
    connect(telemetryTimer_, &QTimer::timeout, this, &QmlBridge::onTelemetryTick);
    registrationEngine_ = std::make_unique<core::registration::RegistrationEngine>();
    refreshData();
    emit canStartChanged();
}

QmlBridge::~QmlBridge() {
    if (timer_->isActive()) timer_->stop();
    if (telemetryTimer_->isActive()) telemetryTimer_->stop();
    if (multiNodeOrchestrator_) {
        multiNodeOrchestrator_->shutdown();
    }
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

double QmlBridge::hostCpuUtil() const {
    if (platform_) {
        return platform_->getHostMetrics().cpu_utilization_percent;
    }
    return 0.0;
}

double QmlBridge::hostGpuUtil() const {
    if (platform_) {
        return platform_->getHostMetrics().gpu_utilization_percent;
    }
    return 0.0;
}

void QmlBridge::setProjectSortMode(int mode) {
    if (projectSortMode_ != mode) {
        projectSortMode_ = mode;
        emit projectSortModeChanged();
        sortProjectList();
    }
}

void QmlBridge::sortProjectList() {
    std::sort(projectList_.begin(), projectList_.end(), [this](const QVariant& a, const QVariant& b) {
        auto ma = a.toMap();
        auto mb = b.toMap();
        if (projectSortMode_ == 0) {
            // Date Newest First (default for operators)
            qulonglong ta = ma.value("created_at_ns", 0).toULongLong();
            qulonglong tb = mb.value("created_at_ns", 0).toULongLong();
            if (ta != tb) return ta > tb;
            return ma.value("name").toString().toLower() < mb.value("name").toString().toLower();
        } else if (projectSortMode_ == 1) {
            // Date Oldest First
            qulonglong ta = ma.value("created_at_ns", 0).toULongLong();
            qulonglong tb = mb.value("created_at_ns", 0).toULongLong();
            if (ta != tb) return ta < tb;
            return ma.value("name").toString().toLower() < mb.value("name").toString().toLower();
        } else if (projectSortMode_ == 2) {
            // Name A-Z
            int cmp = QString::compare(ma.value("name").toString(), mb.value("name").toString(), Qt::CaseInsensitive);
            if (cmp != 0) return cmp < 0;
            return ma.value("created_at_ns", 0).toULongLong() > mb.value("created_at_ns", 0).toULongLong();
        } else if (projectSortMode_ == 3) {
            // Name Z-A
            int cmp = QString::compare(ma.value("name").toString(), mb.value("name").toString(), Qt::CaseInsensitive);
            if (cmp != 0) return cmp > 0;
            return ma.value("created_at_ns", 0).toULongLong() > mb.value("created_at_ns", 0).toULongLong();
        }
        return false;
    });

    availableProjects_.clear();
    for (const auto& item : projectList_) {
        availableProjects_.append(item.toMap().value("id").toString());
    }
    emit availableProjectsChanged();
    emit projectListChanged();
}

void QmlBridge::refreshData() {
    if (!storage_) return;

    // 1. Load available projects
    availableProjects_.clear();
    projectList_.clear();
    for (const auto& pid : storage_->listProjects()) {
        core::schemas::Project proj;
        if (storage_->loadProject(pid, proj)) {
            QVariantMap item;
            item["id"] = QString::fromStdString(proj.project_id());
            item["name"] = QString::fromStdString(proj.name());
            item["description"] = QString::fromStdString(proj.description());
            item["created_at_ns"] = static_cast<qulonglong>(proj.created_at_ns());
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
    sortProjectList();

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

            QString instancesStr;
            for (size_t i = 0; i < cfg.sensor_instances().size(); ++i) {
                if (i > 0) instancesStr += " + ";
                QString inst = QString::fromStdString(cfg.sensor_instances()[i]);
                if (inst.contains("airy", Qt::CaseInsensitive)) instancesStr += "Airy LiDAR";
                else if (inst.contains("gemini", Qt::CaseInsensitive)) instancesStr += "Gemini 336L";
                else if (inst.contains("viture", Qt::CaseInsensitive) || inst.contains("luma", Qt::CaseInsensitive)) instancesStr += "Luma Ultra";
                else instancesStr += inst;
            }
            item["details"] = QString("%1 sensor(s) (%2)").arg(cfg.sensor_instances().size()).arg(instancesStr);
            item["validationNotice"] = "Modular Scanner Configuration — Extrinsics UNKNOWN (Independent SLAM Pipelines)";
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

            if (prof.slam_backend().empty() || prof.slam_family() == "UNKNOWN" || prof.slam_family() == "NONE") {
                item["validationNotice"] = "USB serial interfaces detected | Imaging stream not established | Mapping backend: None | Not currently available for spatial capture";
            } else if (prof.slam_family() == "MONOCULAR_NEURAL_RECONSTRUCTION" && prof.slam_backend() == "LingBotStreamingBackend") {
                item["validationNotice"] = "Monocular Neural Reconstruction (LingBot-Map) | Scale: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED";
            } else if (prof.slam_family() == "LIO" || prof.slam_backend() == "FAST-LIO2" || prof.profile_id().rfind("airy", 0) != std::string::npos || prof.profile_id().rfind("robosense", 0) != std::string::npos) {
                item["validationNotice"] = "RoboSense Airy: Physical Hardware Qualified (DIFOP EEPROM Calibrated)";
            } else if (prof.slam_family() == "RGBD" || prof.slam_backend() == "RTAB-Map" || prof.profile_id().rfind("gemini", 0) != std::string::npos || prof.profile_id().rfind("orbbec", 0) != std::string::npos) {
                item["validationNotice"] = "Orbbec Gemini 336L: Physical Hardware Qualified (Active Stereo RGB-D SLAM)";
            } else if (prof.slam_family() == "HYBRID_NEURAL_STREAMING_VIO" || prof.profile_id().rfind("viture", 0) != std::string::npos) {
                if (prof.profile_status() == core::schemas::ProfileStatus::VALIDATED) {
                    item["validationNotice"] = "VITURE Luma Ultra: Physical Hardware Qualified (Onboard 6-DoF VIO + Visual Point Mapping)";
                } else {
                    item["validationNotice"] = "VITURE Ultra: Experimental Specification — Physical qualification in progress";
                }
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

QVariantList QmlBridge::activeSensors() const {
    QVariantList list;
    for (const auto& s : activeSensors_) {
        QVariantMap m;
        m["id"] = QString::fromStdString(s.id);
        m["instanceId"] = QString::fromStdString(s.instance_id);
        m["profileId"] = QString::fromStdString(s.profile_id);
        m["serialNumber"] = QString::fromStdString(s.serial_number);
        m["transport"] = QString::fromStdString(s.transport);
        m["scaleStatus"] = QString::fromStdString(s.scale_status);
        m["name"] = QString::fromStdString(s.name);
        m["shortName"] = QString::fromStdString(s.short_name);
        m["icon"] = QString::fromStdString(s.icon);
        m["sensorType"] = (s.id == "AIRY") ? QString("LIDAR") : (s.id == "GEMINI") ? QString("RGBD") : (s.id == "CAMERA") ? QString("CAMERA") : (s.id == "VITURE") ? QString("XR") : QString("AGENT");
        m["deviceStatus"] = QString::fromStdString(s.device_status);
        m["acquisitionStatus"] = QString::fromStdString(s.acquisition_status);
        m["trackingStatus"] = QString::fromStdString(s.tracking_status);
        m["isCapturable"] = s.is_capturable;
        m["adapterName"] = QString::fromStdString(s.adapter_name);
        m["backendName"] = QString::fromStdString(s.backend_name);
        m["reconstructionFamily"] = QString::fromStdString(s.reconstruction_family);
        m["pointCount"] = static_cast<qlonglong>(s.point_count);
        m["acqRate"] = s.acquisition_rate;
        m["acquisitionRate"] = s.acquisition_rate;
        m["slamRate"] = s.slam_rate;
        m["active"] = (activeDisplaySensor_ == QString::fromStdString(s.id));
        list.append(m);
    }
    return list;
}

void QmlBridge::rebuildActiveSensorsForSelection() {
    activeSensors_.clear();

    std::vector<std::string> instance_ids;

    // Check if selected scanner is a ScannerConfig in storage
    core::schemas::ScannerConfig config;
    if (storage_ && storage_->loadScannerConfig(selectedScannerId_.toStdString(), config)) {
        instance_ids = config.sensor_instances();
    } else if (!selectedScannerId_.isEmpty()) {
        instance_ids.push_back(selectedScannerId_.toStdString());
    }

    for (const auto& raw_id : instance_ids) {
        // Step 1: Look up DeviceProfile from storage
        core::schemas::DeviceProfile prof;
        bool loaded = false;
        if (storage_) {
            loaded = storage_->loadDeviceProfile(raw_id, prof);
            if (!loaded) {
                // If raw_id is an alias or instance, search all loaded profiles
                for (const auto& pid : storage_->listDeviceProfiles()) {
                    core::schemas::DeviceProfile candidate;
                    if (storage_->loadDeviceProfile(pid, candidate)) {
                        if (candidate.profile_id() == raw_id || pid == raw_id) {
                            prof = candidate;
                            loaded = true;
                            break;
                        }
                    }
                }
            }
        }

        std::string profile_id = loaded ? prof.profile_id() : raw_id;
        std::string physical_device_id = (loaded && !prof.provenance().device_serial().empty()) ? prof.provenance().device_serial() : "N/A";
        std::string transport_desc = loaded ? prof.transport() : "";
        std::string slam_family = loaded ? prof.slam_family() : "UNKNOWN";
        std::string slam_backend = loaded ? prof.slam_backend() : "";
        std::string mfr = loaded ? prof.manufacturer() : "";
        std::string model = loaded ? prof.model() : "";

        // Standard simulation / fallback rig lookup if no profile exists on disk
        if (!loaded) {
            std::string lower_id = raw_id;
            std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
            if (lower_id.find("airy") != std::string::npos || lower_id.find("robosense") != std::string::npos) {
                mfr = "RoboSense"; model = "RS-Airy"; slam_family = "LIO"; slam_backend = "FAST-LIO2"; transport_desc = "192.168.1.200:6699";
            } else if (lower_id.find("gemini") != std::string::npos || lower_id.find("orbbec") != std::string::npos) {
                mfr = "Orbbec"; model = "Gemini 336L"; slam_family = "RGBD"; slam_backend = "RTAB-Map"; transport_desc = "/dev/video0";
            } else if (lower_id.find("viture") != std::string::npos || lower_id.find("luma") != std::string::npos) {
                mfr = "VITURE"; model = "Luma Ultra"; slam_family = "HYBRID_NEURAL_STREAMING_VIO"; slam_backend = "LingBotStreamingBackend + CarinaDeviceProvider"; transport_desc = "USB_3_UVC_HID";
            }
        }

        // Trace logging for device isolation audit
        AV_LOG_INFO("QmlBridge", "[Runtime Dispatch] Resolving sensor instance: " + raw_id);
        AV_LOG_INFO("QmlBridge", "  selected physical_device_id: " + physical_device_id);
        AV_LOG_INFO("QmlBridge", "  selected profile_id: " + profile_id);
        AV_LOG_INFO("QmlBridge", "  selected sensor_instance_id: " + raw_id);
        AV_LOG_INFO("QmlBridge", "  transport: " + transport_desc);

        // Determine compatible Adapter & Backend
        SensorRuntimeContext ctx;
        ctx.instance_id = raw_id;
        ctx.profile_id = profile_id;
        ctx.serial_number = physical_device_id;
        ctx.transport = transport_desc;
        ctx.name = mfr.empty() ? model : (mfr + " " + model);
        ctx.short_name = model.empty() ? raw_id : model;
        ctx.reconstruction_family = slam_family;
        ctx.backend_name = slam_backend.empty() ? "NONE" : slam_backend;

        // Check if device is Airy LiDAR
        if (slam_family == "LIO" || slam_backend == "FAST-LIO2" || (mfr == "RoboSense" && model.find("Airy") != std::string::npos)) {
            ctx.id = "AIRY";
            if (ctx.name.empty()) ctx.name = "RoboSense RS-Airy";
            if (ctx.short_name.empty()) ctx.short_name = "Airy LiDAR";
            ctx.icon = "📡";
            ctx.mount_label = "TOP_MOUNT";
            ctx.scale_status = "METRIC_VERIFIED";
            ctx.adapter_name = "AirySensorAdapter";
            ctx.backend_name = "FAST-LIO2";
            ctx.is_capturable = true;
            ctx.device_status = "STANDBY";
            ctx.acquisition_status = "STOPPED";
            ctx.tracking_status = "IDLE";
            ctx.map_points = airyMapPoints_;
            ctx.trajectory = airyTrajectory_;
            ctx.point_count = airyPointsCaptured_;
            AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + ctx.transport);
            AV_LOG_INFO("QmlBridge", "  selected adapter: " + ctx.adapter_name);
            AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: " + ctx.backend_name);
            AV_LOG_INFO("QmlBridge", "  capture available: true");
        }
        // Check if device is Gemini RGB-D
        else if (slam_family == "RGBD" || slam_family == "RGBD_SLAM" || slam_family == "RGBD_ODOMETRY_SLAM" || slam_backend == "RTAB-Map" || (mfr == "Orbbec" && model.find("Gemini") != std::string::npos)) {
            ctx.id = "GEMINI";
            if (ctx.name.empty()) ctx.name = "Orbbec Gemini 336L";
            if (ctx.short_name.empty()) ctx.short_name = "Gemini 336L";
            ctx.icon = "📷";
            ctx.mount_label = "FRONT_FACING";
            ctx.scale_status = "METRIC_VERIFIED";
            ctx.adapter_name = "GeminiSensorAdapter";
            ctx.backend_name = "RTAB-Map";
            ctx.is_capturable = true;
            ctx.device_status = "STANDBY";
            ctx.acquisition_status = "STOPPED";
            ctx.tracking_status = "IDLE";
            ctx.map_points = geminiMapPoints_;
            ctx.trajectory = geminiTrajectory_;
            ctx.point_count = geminiPointsCaptured_;
            AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + ctx.transport);
            AV_LOG_INFO("QmlBridge", "  selected adapter: " + ctx.adapter_name);
            AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: " + ctx.backend_name);
            AV_LOG_INFO("QmlBridge", "  capture available: true");
        }
        // Check if device is Viture XR
        else if (slam_family == "HYBRID_NEURAL_STREAMING_VIO" || slam_family == "VIO" || (mfr == "VITURE")) {
            ctx.id = "VITURE";
            if (ctx.name.empty()) ctx.name = "VITURE Luma Ultra";
            if (ctx.short_name.empty()) ctx.short_name = "Luma Ultra";
            ctx.icon = "🕶️";
            ctx.mount_label = "HEAD_MOUNT";
            ctx.scale_status = "METRIC_VERIFIED";
            ctx.adapter_name = "VitureSensorAdapter";
            ctx.backend_name = "CarinaDeviceProvider";
            ctx.is_capturable = true;
            ctx.device_status = "STANDBY";
            ctx.acquisition_status = "STOPPED";
            ctx.tracking_status = "IDLE";
            ctx.map_points = vitureMapPoints_;
            ctx.trajectory = vitureTrajectory_;
            ctx.point_count = viturePointsCaptured_;
            AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + ctx.transport);
            AV_LOG_INFO("QmlBridge", "  selected adapter: " + ctx.adapter_name);
            AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: " + ctx.backend_name);
            AV_LOG_INFO("QmlBridge", "  capture available: true");
        }
        // Check if device is a verified monocular camera with verified V4L2 RGB stream
        else if (slam_family == "MONOCULAR_NEURAL_RECONSTRUCTION" &&
                 (slam_backend == "LingBotStreamingBackend" || slam_backend == "ABotStreamingBackend") &&
                 (prof.driver() == "uvcvideo" || prof.transport().rfind("/dev/video", 0) == 0 || (prof.intrinsics().contains("device_path") && prof.intrinsics()["device_path"].get<std::string>().rfind("/dev/video", 0) == 0))) {
            ctx.id = "CAMERA";
            if (ctx.name.empty()) ctx.name = "48MP USB Camera";
            if (ctx.short_name.empty()) ctx.short_name = "USB Camera";
            ctx.icon = "📷";
            ctx.mount_label = "HANDHELD";
            ctx.scale_status = "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED";
            ctx.adapter_name = "GenericUvcSensorAdapter";
            ctx.backend_name = selectedCameraBackend_.toStdString();

            // Resolve physical V4L2 endpoint
            std::string vnode = "";
            if (prof.intrinsics().contains("device_path")) {
                vnode = prof.intrinsics()["device_path"].get<std::string>();
            } else if (prof.transport().rfind("/dev/video", 0) == 0) {
                vnode = prof.transport();
            }

            // Check if endpoint is physically present on host
            bool vnode_exists = (!vnode.empty() && fs::exists(vnode));
            if (vnode_exists) {
                ctx.transport = vnode;
                ctx.device_status = "STANDBY";
                ctx.acquisition_status = "STOPPED";
                ctx.tracking_status = "IDLE";
                ctx.is_capturable = true;
                AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + vnode + " [ONLINE]");
                AV_LOG_INFO("QmlBridge", "  selected adapter: GenericUvcSensorAdapter");
                AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: LingBotStreamingBackend");
                AV_LOG_INFO("QmlBridge", "  capture available: true");
            } else {
                ctx.transport = vnode.empty() ? "/dev/video* (Offline)" : (vnode + " (Disconnected)");
                ctx.device_status = "DISCONNECTED";
                ctx.acquisition_status = "DISCONNECTED";
                ctx.tracking_status = "OFFLINE";
                ctx.is_capturable = false;
                AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + ctx.transport);
                AV_LOG_INFO("QmlBridge", "  selected adapter: GenericUvcSensorAdapter");
                AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: LingBotStreamingBackend");
                AV_LOG_INFO("QmlBridge", "  capture available: false (Device physically disconnected)");
            }
            ctx.map_points = cameraMapPoints_;
            ctx.trajectory = cameraTrajectory_;
            ctx.point_count = cameraPointsCaptured_;
        }
        // Device without compatible spatial capture backend (e.g. Seeed SenseCAP Watcher)
        else {
            ctx.id = "UNSUPPORTED";
            if (ctx.name.empty()) ctx.name = "Unknown Physical Device";
            if (ctx.short_name.empty()) ctx.short_name = "Physical Device";
            ctx.icon = (prof.transport() == "USB_SERIAL_CDC_ACM" || prof.driver() == "cdc_acm" || model.find("Watcher") != std::string::npos) ? "🤖" : "⚙️";
            ctx.mount_label = "STANDALONE";
            ctx.scale_status = "NO_RECONSTRUCTION_BACKEND_MATCHED";
            ctx.device_status = "PROFILED";
            ctx.acquisition_status = "NOT ESTABLISHED";
            ctx.tracking_status = "NO BACKEND";
            ctx.adapter_name = "NONE";
            ctx.backend_name = "NONE";
            ctx.is_capturable = false;

            std::string endpoints = prof.transport();
            if (prof.transport() == "USB_SERIAL_CDC_ACM" || prof.driver() == "cdc_acm") {
                endpoints = "/dev/ttyACM0, /dev/ttyACM1 (Roles: UNKNOWN)";
            }
            ctx.transport = endpoints;

            AV_LOG_INFO("QmlBridge", "  resolved endpoint(s): " + endpoints);
            AV_LOG_INFO("QmlBridge", "  selected adapter: NONE (No compatible sensor adapter)");
            AV_LOG_INFO("QmlBridge", "  selected reconstruction/SLAM backend: NONE (No compatible reconstruction backend)");
            AV_LOG_INFO("QmlBridge", "  capture available: false");
        }

        activeSensors_.push_back(ctx);
    }

    if (activeSensors_.empty()) {
        SensorRuntimeContext ctx;
        ctx.id = "AIRY";
        ctx.instance_id = "robosense_airy_01";
        ctx.name = "RoboSense RS-Airy";
        ctx.short_name = "Airy LiDAR";
        ctx.icon = "📡";
        ctx.profile_id = "robosense_airy_01";
        ctx.serial_number = "AIRY-2024-99812";
        ctx.transport = "192.168.1.200:6699";
        ctx.mount_label = "TOP_MOUNT";
        ctx.scale_status = "METRIC_VERIFIED";
        ctx.adapter_name = "AirySensorAdapter";
        ctx.backend_name = "FAST-LIO2";
        ctx.is_capturable = true;
        ctx.map_points = airyMapPoints_;
        ctx.trajectory = airyTrajectory_;
        ctx.point_count = airyPointsCaptured_;
        activeSensors_.push_back(ctx);
    }

    activeDisplaySensor_ = QString::fromStdString(activeSensors_.front().id);
    isDualSensorMode_ = (activeSensors_.size() > 1);

    if (activeSensors_.size() == 1 && !activeSensors_.front().is_capturable) {
        selectedScannerValidationNotice_ = "USB serial interfaces detected | Imaging stream not established | Mapping backend: None | Not currently available for spatial capture";
        selectedScannerStatus_ = "PROFILED";
        selectedScannerDetails_ = QString::fromStdString(activeSensors_.front().transport + " (Mapping Backend: None)");
        sensorStatus_ = QString::fromStdString(activeSensors_.front().name + " [PROFILED — Backend: None]");
    }

    emit activeSensorsChanged();
    emit isDualSensorModeChanged();
    emit activeDisplaySensorChanged();
    emit selectedScannerChanged();
    emit canStartChanged();
}

void QmlBridge::selectScannerByIndex(int index) {
    if (index >= 0 && index < scannerList_.size()) {
        QVariantMap item = scannerList_.at(index).toMap();
        selectedScannerId_ = item["id"].toString();
        selectedScannerName_ = item["name"].toString();
        selectedScannerStatus_ = item["status"].toString();
        selectedScannerDetails_ = item["details"].toString();
        selectedScannerValidationNotice_ = item["validationNotice"].toString();

        rebuildActiveSensorsForSelection();

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

int QmlBridge::selectedScannerIndex() const {
    for (int i = 0; i < scannerList_.size(); ++i) {
        if (scannerList_.at(i).toMap()["id"].toString() == selectedScannerId_) {
            return i;
        }
    }
    return 0;
}

void QmlBridge::setSelectedCameraBackend(const QString& name) {
    selectCameraBackend(name);
}

QString QmlBridge::cameraBackendStatus() const {
    if (selectedCameraBackend_ == "ABotStreamingBackend") {
        return "RESEARCH / EXPERIMENTAL";
    }
    return "COMMERCIAL READY";
}

QString QmlBridge::cameraBackendNotice() const {
    if (selectedCameraBackend_ == "ABotStreamingBackend") {
        return "AMAP CVLab ABot-Recon weights licensed under CC BY-NC 4.0 / CC BY-NC-SA 4.0. COMMERCIAL USE NOT CLEARED.";
    }
    return "LingBot-Map monocular neural spatial reconstruction backend with full commercial clearance.";
}

void QmlBridge::selectCameraBackend(const QString& backendName) {
    if (selectedCameraBackend_ == backendName) return;
    selectedCameraBackend_ = backendName;

    // Update active camera in activeSensors_
    for (auto& s : activeSensors_) {
        if (s.id == "CAMERA") {
            s.backend_name = backendName.toStdString();
        }
    }
    emit selectedCameraBackendChanged();
    emit cameraBackendStatusChanged();
    emit cameraBackendNoticeChanged();
    emit activeSensorsChanged();
    emit canStartChanged();
    AV_LOG_INFO("QmlBridge", "Camera reconstruction backend selected: " + backendName.toStdString());
}

void QmlBridge::setActiveDisplaySensor(const QString& sensorName) {
    if (activeDisplaySensor_ == sensorName && !mapPoints_.empty()) return;
    activeDisplaySensor_ = sensorName;

    for (const auto& s : activeSensors_) {
        if (s.id == sensorName.toStdString()) {
            mapPoints_ = s.map_points;
            trajectory_ = s.trajectory;
            if (s.id == "AIRY" && airyBackend_) {
                currentPoseX_ = airyBackend_->getPose().position.x;
                currentPoseY_ = airyBackend_->getPose().position.y;
                currentPoseZ_ = airyBackend_->getPose().position.z;
                currentYaw_ = airyBackend_->getCurrentYaw();
            } else if (s.id == "GEMINI" && geminiBackend_) {
                currentPoseX_ = geminiBackend_->getPose().position.x;
                currentPoseY_ = geminiBackend_->getPose().position.y;
                currentPoseZ_ = geminiBackend_->getPose().position.z;
                currentYaw_ = geminiBackend_->getCurrentYaw();
            } else if (s.id == "VITURE" && vitureBackend_) {
                currentPoseX_ = vitureBackend_->getPose().position.x;
                currentPoseY_ = vitureBackend_->getPose().position.y;
                currentPoseZ_ = vitureBackend_->getPose().position.z;
                currentYaw_ = vitureBackend_->getCurrentYaw();
            }
            break;
        }
    }

    hasMapData_ = (!mapPoints_.empty());
    emit activeDisplaySensorChanged();
    emit mapDataUpdated();
    emit trajectoryChanged();
    emit poseChanged();
}

QVariantList QmlBridge::getAvailableDeviceProfiles() const {
    QVariantList list;
    if (!storage_) return list;

    for (const auto& pid : storage_->listDeviceProfiles()) {
        core::schemas::DeviceProfile prof;
        if (storage_->loadDeviceProfile(pid, prof)) {
            QVariantMap item;
            item["profileId"] = QString::fromStdString(prof.profile_id());
            item["name"] = QString::fromStdString(prof.model().empty() ? prof.profile_id() : prof.manufacturer() + " " + prof.model());
            item["manufacturer"] = QString::fromStdString(prof.manufacturer());
            item["model"] = QString::fromStdString(prof.model());
            item["family"] = QString::fromStdString(prof.device_family());
            item["transport"] = QString::fromStdString(prof.transport());
            item["driver"] = QString::fromStdString(prof.driver());
            item["status"] = QString::fromStdString(core::schemas::to_string(prof.profile_status()));

            std::string lower = pid;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find("airy") != std::string::npos || lower.find("robosense") != std::string::npos) {
                item["sensorType"] = "32-Beam Mechanical LiDAR + IMU";
                item["serialNumber"] = "AIRY-2024-99812";
                item["icon"] = "📡";
                item["modality"] = "LIDAR";
                item["tag"] = "LiDAR + IMU";
                item["tagColor"] = "#38bdf8";
            } else if (lower.find("gemini") != std::string::npos || lower.find("orbbec") != std::string::npos) {
                item["sensorType"] = "Active Stereo RGB-D + IMU";
                item["serialNumber"] = "CPC6463000XW";
                item["icon"] = "📷";
                item["modality"] = "RGBD";
                item["tag"] = "Stereo RGB-D + IMU";
                item["tagColor"] = "#c084fc";
            } else if (lower.find("viture") != std::string::npos || lower.find("luma") != std::string::npos) {
                item["sensorType"] = "XR Smart Glasses (6-DoF VIO)";
                item["serialNumber"] = "USB-35CA-1104";
                item["icon"] = "👓";
                item["modality"] = "XR";
                item["tag"] = "XR 6-DoF VIO";
                item["tagColor"] = "#2dd4bf";
            } else if (prof.transport() == "USB_SERIAL_CDC_ACM" || prof.driver() == "cdc_acm" || lower.find("watcher") != std::string::npos) {
                item["sensorType"] = "Edge AI Vision Agent (CDC-ACM Serial)";
                std::string ser = prof.provenance().device_serial().empty() ? prof.profile_id().substr(prof.profile_id().find_last_of('_') + 1) : prof.provenance().device_serial();
                item["serialNumber"] = QString::fromStdString(ser.empty() ? "N/A" : ser);
                item["icon"] = "🤖";
                item["modality"] = "VISION_AGENT";
                item["tag"] = "CDC-ACM Serial";
                item["tagColor"] = "#a855f7";
            } else if (lower.find("camera") != std::string::npos || prof.driver() == "uvcvideo" || prof.transport().rfind("/dev/video", 0) != std::string::npos || prof.device_family() == "MONOCULAR_RGB_CAMERA") {
                item["sensorType"] = "USB Camera (Monocular Optics)";
                std::string ser = prof.profile_id().substr(prof.profile_id().find_last_of('_') + 1);
                item["serialNumber"] = QString::fromStdString(ser.empty() ? "N/A" : ser);
                item["icon"] = "📹";
                item["modality"] = "CAMERA";
                item["tag"] = "Monocular Camera";
                item["tagColor"] = "#f59e0b";
            } else {
                item["sensorType"] = QString::fromStdString(prof.device_family());
                item["serialNumber"] = QString::fromStdString(prof.hardware_revision());
                item["icon"] = "⚙️";
                item["modality"] = "GENERIC";
                item["tag"] = QString::fromStdString(prof.device_family());
                item["tagColor"] = "#94a3b8";
            }

            list.append(item);
        }
    }
    return list;
}

void QmlBridge::refreshDiscovery() {
    isDiscovering_ = true;
    emit isDiscoveringChanged();

    QVariantList list;
    core::profiling::DeviceProfiler profiler;
    fs::path repo_root = storage_ ? fs::path(storage_->getRootDirectory()) : fs::current_path();

    auto phys_devs = profiler.discoverPhysicalDevices(repo_root);
    int unprofiled_count = 0;
    int online_count = 0;

    for (const auto& dev : phys_devs) {
        QVariantMap m;
        m["id"] = QString::fromStdString(dev.device_id);
        m["name"] = QString::fromStdString(dev.display_name.empty() ? dev.primary_endpoint : dev.display_name);
        m["manufacturer"] = QString::fromStdString(dev.manufacturer.empty() ? "Generic" : dev.manufacturer);
        m["productName"] = QString::fromStdString(dev.product_name);
        m["driver"] = QString::fromStdString(dev.driver_name);
        m["busType"] = QString::fromStdString(dev.bus_type);
        m["busInfo"] = QString::fromStdString(dev.bus_info);
        m["vendorId"] = QString::fromStdString(dev.vendor_id);
        m["productId"] = QString::fromStdString(dev.product_id);
        m["serial"] = QString::fromStdString(dev.serial_number.empty() ? "N/A" : dev.serial_number);
        m["sysfsPath"] = QString::fromStdString(dev.sysfs_path);
        m["primaryEndpoint"] = QString::fromStdString(dev.primary_endpoint);
        m["profileStatus"] = QString::fromStdString(dev.profile_status);
        m["existingProfileId"] = QString::fromStdString(dev.existing_profile_id);
        m["compatibleReconstruction"] = QString::fromStdString(dev.compatible_reconstruction);
        m["recommendedBackend"] = QString::fromStdString(dev.recommended_backend);
        m["statusRationale"] = QString::fromStdString(dev.status_rationale);

        if (dev.bus_type == "USB") {
            if (dev.driver_name == "cdc_acm" || dev.display_name.find("Watcher") != std::string::npos || dev.product_name.find("Dual_Serial") != std::string::npos) {
                m["icon"] = "🤖";
                m["modality"] = "AI VISION AGENT";
                m["transport"] = QString::fromStdString("USB " + dev.bus_info + " (Dual Serial ACM)");
            } else {
                m["icon"] = "📹";
                m["modality"] = "CAMERA";
                m["transport"] = QString::fromStdString("USB " + dev.bus_info);
            }
        } else if (dev.bus_type == "ETHERNET" || dev.display_name.find("LiDAR") != std::string::npos) {
            m["icon"] = "📡";
            m["modality"] = "LIDAR";
            m["transport"] = QString::fromStdString("Ethernet UDP");
        } else {
            m["icon"] = "⚙️";
            m["modality"] = "SENSOR";
            m["transport"] = QString::fromStdString(dev.bus_type);
        }

        if (dev.profile_status == "UNPROFILED") {
            unprofiled_count++;
        }
        if (dev.profile_status != "DISCONNECTED") {
            online_count++;
        }

        // Grouped interfaces
        QVariantList ifaces;
        for (const auto& iface : dev.interfaces) {
            QVariantMap im;
            im["node"] = QString::fromStdString(iface.node_path);
            im["type"] = QString::fromStdString(iface.interface_type);
            im["summary"] = QString::fromStdString(iface.format_summary);
            im["isPrimary"] = iface.is_primary;
            ifaces.append(im);
        }
        m["interfaces"] = ifaces;
        m["interfaceCount"] = static_cast<int>(dev.interfaces.size());

        // Formats summary
        QStringList fmt_list;
        for (const auto& f : dev.formats) {
            fmt_list.append(QString("%1 %2x%3@%4fps")
                .arg(QString::fromStdString(f.pixel_format))
                .arg(f.width)
                .arg(f.height)
                .arg(static_cast<int>(f.max_fps)));
        }
        m["formats"] = fmt_list;

        // Native capabilities
        QStringList caps;
        for (const auto& c : dev.native_capabilities) {
            caps.append(QString::fromStdString(c));
        }
        m["nativeCapabilities"] = caps;
        m["nativeCapsDisplay"] = caps.join(", ");
        m["hasNativeDepth"] = false;
        m["hasNativeImu"] = false;
        m["hasNativeGeometry"] = false;

        list.append(m);
    }

    std::cout << "\n================================================================================" << std::endl;
    std::cout << ">>> [STAGE 4 — QML BRIDGE] Exporting " << list.size() << " device(s) to QML discoveredPhysicalDevices model:" << std::endl;
    std::cout << "================================================================================" << std::endl;
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap m = list[i].toMap();
        std::cout << "  Item " << i << ": id='" << m["id"].toString().toStdString()
                  << "' | name='" << m["name"].toString().toStdString()
                  << "' | status='" << m["profileStatus"].toString().toStdString()
                  << "' | primaryEndpoint='" << m["primaryEndpoint"].toString().toStdString()
                  << "' | interfaces=" << m["interfaceCount"].toInt()
                  << " | formats=" << m["formats"].toStringList().size() << std::endl;
    }
    std::cout << "  Summary: " << online_count << " online sensor(s) detected (" << unprofiled_count << " unprofiled)" << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    discoveredPhysicalDevices_ = list;
    discoveredDeviceCount_ = online_count;
    unprofiledDeviceCount_ = unprofiled_count;

    QDateTime now = QDateTime::currentDateTime();
    discoveryStatusText_ = QString("Scan complete at %1: %2 physical sensor(s) detected (%3 unprofiled)")
        .arg(now.toString("hh:mm:ss"))
        .arg(online_count)
        .arg(unprofiled_count);

    isDiscovering_ = false;
    emit isDiscoveringChanged();
    emit discoveryStatusTextChanged();
    emit discoveredDevicesChanged();
}

QVariantList QmlBridge::discoverHardwareDevices() {
    refreshDiscovery();
    return discoveredPhysicalDevices_;
}

QVariantMap QmlBridge::createProfileForDevice(const QString& deviceId) {
    QVariantMap res;
    res["success"] = false;

    core::profiling::DeviceProfiler profiler;
    fs::path repo_root = storage_ ? fs::path(storage_->getRootDirectory()) : fs::current_path();

    auto phys_devs = profiler.discoverPhysicalDevices(repo_root);
    const core::profiling::DeviceProfiler::DiscoveredPhysicalDevice* target_dev = nullptr;
    for (const auto& d : phys_devs) {
        if (QString::fromStdString(d.device_id) == deviceId ||
            QString::fromStdString(d.primary_endpoint) == deviceId ||
            (!d.vendor_id.empty() && deviceId.contains(QString::fromStdString(d.vendor_id)) &&
             !d.product_id.empty() && deviceId.contains(QString::fromStdString(d.product_id)))) {
            target_dev = &d;
            break;
        }
    }

    if (!target_dev) {
        for (const auto& d : phys_devs) {
            for (const auto& iface : d.interfaces) {
                if (QString::fromStdString(iface.node_path) == deviceId) {
                    target_dev = &d;
                    break;
                }
            }
            if (target_dev) break;
        }
    }

    if (!target_dev) {
        res["error"] = "Physical device not found on host: " + deviceId;
        AV_LOG_ERROR("QmlBridge", "Create Profile Failed", res["error"].toString().toStdString());
        return res;
    }

    AV_LOG_INFO("QmlBridge", "Interrogating physical hardware and generating profile for: " + target_dev->display_name + " (" + target_dev->primary_endpoint + ")");

    // 1. Hardware interrogation and experimental profile generation
    core::schemas::DeviceProfile prof = profiler.generateProfileFromPhysicalDevice(*target_dev);

    // 2. Persist profile to disk (profiles/experimental/<profile_id>.json)
    bool save_ok = profiler.saveExperimentalProfile(prof, repo_root);
    if (!save_ok) {
        res["error"] = "Failed to write experimental profile to disk";
        AV_LOG_ERROR("QmlBridge", "Profile Save Failed", res["error"].toString().toStdString());
        return res;
    }

    // 3. Immediately refresh storage and scanners
    refreshData();

    // 4. Re-run discovery so this device immediately becomes PROFILED in the GUI!
    refreshDiscovery();

    // 5. Construct verbatim concise summary required by user
    res["success"] = true;
    res["profileId"] = QString::fromStdString(prof.profile_id());
    res["deviceName"] = QString::fromStdString(prof.manufacturer() + " " + prof.model());
    res["manufacturer"] = QString::fromStdString(prof.manufacturer());
    res["profileStatus"] = "EXPERIMENTAL";
    if (prof.transport() == "USB_SERIAL_CDC_ACM" || target_dev->driver_name == "cdc_acm") {
        res["nativeCapabilities"] = "Serial Control\nCDC-ACM Endpoint";
    } else {
        res["nativeCapabilities"] = "RGB\nMonocular Camera\nUVC Streaming";
    }
    res["nativeDepth"] = "NO";
    res["nativeImu"] = "NO";
    res["nativeGeometry"] = "NO";
    res["compatibleReconstruction"] = QString::fromStdString(prof.slam_family());
    res["selectedBackend"] = QString::fromStdString(prof.slam_backend());
    res["geometrySource"] = (prof.slam_backend() != "NONE" && !prof.slam_backend().empty()) ? "BACKEND DERIVED" : "NONE";
    std::string scale_str = "NO_RECONSTRUCTION_BACKEND_MATCHED";
    if (prof.output_contract().contains("scale_classification") && prof.output_contract()["scale_classification"].contains("scale_status")) {
        scale_str = prof.output_contract()["scale_classification"]["scale_status"].get<std::string>();
    }
    res["scaleStatus"] = QString::fromStdString(scale_str);
    res["filePath"] = QString::fromStdString((repo_root / "profiles" / "experimental" / (prof.profile_id() + ".json")).string());

    AV_LOG_INFO("QmlBridge", "Successfully generated and onboarded profile: " + prof.profile_id());
    return res;
}

void QmlBridge::selectProfileForCapture(const QString& profileId) {
    if (profileId.isEmpty()) return;
    refreshData();
    for (int i = 0; i < scannerList_.size(); ++i) {
        QVariantMap m = scannerList_.at(i).toMap();
        QString sId = m["id"].toString();
        if (sId == profileId || sId.contains(profileId)) {
            selectScannerByIndex(i);
            AV_LOG_INFO("QmlBridge", "Active scanner selected for capture: " + profileId.toStdString());
            return;
        }
    }
    // Direct profile selection fallback
    selectedScannerId_ = profileId;
    selectedScannerName_ = profileId;
    rebuildActiveSensorsForSelection();
    emit selectedScannerChanged();
    AV_LOG_INFO("QmlBridge", "Direct profile selected for capture: " + profileId.toStdString());
}

bool QmlBridge::saveCustomScannerConfig(const QString& name, const QStringList& profileIds) {
    if (name.trimmed().isEmpty() || profileIds.isEmpty()) {
        emit errorOccurred("Configuration Error", "Scanner configuration must have a name and at least one sensor selected.");
        return false;
    }

    QString slug = name.toLower().trimmed();
    slug.replace(QRegularExpression("[^a-z0-9_]"), "_");
    while (slug.contains("__")) slug.replace("__", "_");
    std::string config_id = "cfg_" + slug.toStdString();

    core::schemas::ScannerConfig cfg(config_id, name.trimmed().toStdString(), "scanar-01", core::schemas::SensorRelationshipType::RIGID);
    cfg.set_extrinsic_status(core::schemas::ExtrinsicStatus::UNKNOWN);
    cfg.set_method("PENDING_ENVIRONMENTAL_OBSERVATION");
    cfg.set_confidence(0.0);

    std::vector<std::string> instances;
    for (const auto& pid : profileIds) {
        instances.push_back(pid.toStdString());
    }
    cfg.set_sensor_instances(instances);

    core::schemas::CalibrationProvenance prov;
    prov.set_source(core::schemas::CalibrationSource::INFERRED_FROM_GEOMETRY);
    prov.set_evidence("User composed modular scanner configuration");
    prov.set_confidence(1.0);
    prov.set_validated(false);
    cfg.set_provenance(prov);

    if (storage_ && storage_->saveScannerConfig(cfg)) {
        refreshData();
        selectScannerById(QString::fromStdString(config_id));
        AV_LOG_INFO("QmlBridge", "Saved and selected custom scanner configuration: " + name.toStdString() + " with " + std::to_string(profileIds.size()) + " sensor(s).");
        emit notification("Scanner Configured", QString("Scanner Configuration '%1' created with %2 sensor(s).").arg(name).arg(profileIds.size()));
        return true;
    }

    emit errorOccurred("Storage Error", "Failed to save scanner configuration to disk.");
    return false;
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
        emit canStartChanged();

        refreshData();
        emit canStartChanged();
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
        airyMapPoints_.clear();
        airyTrajectory_.clear();
        geminiMapPoints_.clear();
        geminiTrajectory_.clear();
        vitureMapPoints_.clear();
        vitureTrajectory_.clear();
        airyPointsCaptured_ = 0;
        geminiPointsCaptured_ = 0;
        viturePointsCaptured_ = 0;
        cameraMapPoints_.clear();
        cameraTrajectory_.clear();
        cameraPointsCaptured_ = 0;
        pointsCaptured_ = 0;
        hasMapData_ = false;

        std::string camera_inst_id = "camera_32e4_4802_2023081002";
        std::string camera_prof_id = "camera_32e4_4802_2023081002";
        std::string camera_name = "48MP USB Camera";

        fs::path proj_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId.toStdString();
        fs::path sess_dir = proj_dir / "sessions";
        if (fs::exists(sess_dir)) {
            for (const auto& sess_entry : fs::directory_iterator(sess_dir)) {
                if (sess_entry.is_directory()) {
                    currentSessionId_ = QString::fromStdString(sess_entry.path().filename().string());
                    fs::path caps_dir = sess_entry.path() / "captures";
                    if (fs::exists(caps_dir)) {
                        for (const auto& cap_entry : fs::directory_iterator(caps_dir)) {
                            if (cap_entry.is_directory()) {
                                std::string cap_dirname = cap_entry.path().filename().string();
                                fs::path pcd_file = cap_entry.path() / "map.pcd";
                                fs::path traj_file = cap_entry.path() / "trajectory.json";
                                fs::path cap_meta = cap_entry.path() / "capture.json";

                                std::string sensor_id = "";
                                if (fs::exists(cap_meta)) {
                                    try {
                                        std::ifstream c_ifs(cap_meta);
                                        auto cj = nlohmann::json::parse(c_ifs);
                                        sensor_id = cj.value("sensor_instance_id", "");
                                    } catch (...) {}
                                }

                                std::vector<core::schemas::PointXYZI> pts;
                                if (fs::exists(pcd_file)) {
                                    loadMapPcd(pcd_file.string(), pts);
                                }
                                std::vector<core::schemas::Pose3D> trj;
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
                                            trj.push_back(p);
                                        }
                                    } catch (...) {}
                                }

                                bool is_airy_cap = (sensor_id == "robosense_airy_01" || cap_dirname.find("airy") != std::string::npos);
                                bool is_gemini_cap = (sensor_id == "orbbec_gemini_336l_01" || cap_dirname.find("gemini") != std::string::npos);
                                bool is_viture_cap = (sensor_id == "viture_luma_ultra_01" || cap_dirname.find("viture") != std::string::npos || cap_dirname.find("luma") != std::string::npos);
                                bool is_camera_cap = (sensor_id.find("camera") != std::string::npos || cap_dirname.find("camera") != std::string::npos);
                                bool is_merged_cap = (sensor_id.find("merged") != std::string::npos || cap_dirname.find("merged") != std::string::npos);

                                if (is_airy_cap) {
                                    airyMapPoints_ = pts;
                                    airyTrajectory_ = trj;
                                    airyPointsCaptured_ = pts.size();
                                } else if (is_gemini_cap) {
                                    geminiMapPoints_ = pts;
                                    geminiTrajectory_ = trj;
                                    geminiPointsCaptured_ = pts.size();
                                } else if (is_viture_cap) {
                                    vitureMapPoints_ = pts;
                                    vitureTrajectory_ = trj;
                                    viturePointsCaptured_ = pts.size();
                                } else if (is_camera_cap) {
                                    cameraMapPoints_ = pts;
                                    cameraTrajectory_ = trj;
                                    cameraPointsCaptured_ = pts.size();
                                    if (!sensor_id.empty()) {
                                        camera_inst_id = sensor_id;
                                        camera_prof_id = sensor_id;
                                        if (sensor_id.find("watcher") != std::string::npos) {
                                            camera_name = "Seeed SenseCAP Watcher";
                                        }
                                    }
                                } else if (is_merged_cap) {
                                    mapPoints_ = pts;
                                    trajectory_ = trj;
                                    pointsCaptured_ = pts.size();
                                } else if (mapPoints_.empty() && !pts.empty()) {
                                    mapPoints_ = pts;
                                    trajectory_ = trj;
                                }
                            }
                        }
                    }
                    if (!airyMapPoints_.empty() || !geminiMapPoints_.empty() || !vitureMapPoints_.empty() || !cameraMapPoints_.empty() || !mapPoints_.empty()) {
                        break;
                    }
                }
            }
        }

        // Rebuild activeSensors_ dynamically from discovered capture data
        activeSensors_.clear();
        if (!airyMapPoints_.empty()) {
            SensorRuntimeContext ctx;
            ctx.id = "AIRY";
            ctx.instance_id = "robosense_airy_01";
            ctx.name = "RoboSense RS-Airy";
            ctx.short_name = "Airy LiDAR";
            ctx.icon = "📡";
            ctx.profile_id = "robosense_airy_01";
            ctx.serial_number = "AIRY-2024-99812";
            ctx.transport = "192.168.1.200:6699";
            ctx.mount_label = "TOP_MOUNT";
            ctx.map_points = airyMapPoints_;
            ctx.trajectory = airyTrajectory_;
            ctx.point_count = airyPointsCaptured_;
            activeSensors_.push_back(ctx);
        }
        if (!geminiMapPoints_.empty()) {
            SensorRuntimeContext ctx;
            ctx.id = "GEMINI";
            ctx.instance_id = "orbbec_gemini_336l_01";
            ctx.name = "Orbbec Gemini 336L";
            ctx.short_name = "Gemini 336L";
            ctx.icon = "📷";
            ctx.profile_id = "orbbec_gemini_336l_01";
            ctx.serial_number = "CPC6463000XW";
            ctx.transport = "/dev/video0";
            ctx.mount_label = "FRONT_FACING";
            ctx.map_points = geminiMapPoints_;
            ctx.trajectory = geminiTrajectory_;
            ctx.point_count = geminiPointsCaptured_;
            activeSensors_.push_back(ctx);
        }
        if (!vitureMapPoints_.empty()) {
            SensorRuntimeContext ctx;
            ctx.id = "VITURE";
            ctx.instance_id = "viture_luma_ultra_01";
            ctx.name = "VITURE Luma Ultra";
            ctx.short_name = "Luma Ultra";
            ctx.icon = "🕶️";
            ctx.profile_id = "viture_luma_ultra_01";
            ctx.serial_number = "USB-35CA-1104";
            ctx.transport = "USB_3_UVC_HID";
            ctx.mount_label = "HEAD_MOUNT";
            ctx.map_points = vitureMapPoints_;
            ctx.trajectory = vitureTrajectory_;
            ctx.point_count = viturePointsCaptured_;
            activeSensors_.push_back(ctx);
        }
        if (!cameraMapPoints_.empty()) {
            SensorRuntimeContext ctx;
            ctx.id = "CAMERA";
            ctx.instance_id = camera_inst_id;
            ctx.name = camera_name;
            ctx.short_name = (camera_name.find("Watcher") != std::string::npos) ? "SenseCAP Watcher" : "USB Camera";
            ctx.icon = (camera_name.find("Watcher") != std::string::npos) ? "🤖" : "📷";
            ctx.profile_id = camera_prof_id;
            ctx.serial_number = (camera_name.find("Watcher") != std::string::npos) ? "56D5018737" : "2023081002";
            ctx.transport = (camera_name.find("Watcher") != std::string::npos) ? "/dev/ttyACM0" : "/dev/video0";
            ctx.mount_label = "HANDHELD";
            ctx.scale_status = "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED";
            ctx.map_points = cameraMapPoints_;
            ctx.trajectory = cameraTrajectory_;
            ctx.point_count = cameraPointsCaptured_;
            activeSensors_.push_back(ctx);
        }

        // If no named sensors found but mapPoints_ has data, add synthetic Combined Merged sensor
        if (!mapPoints_.empty() && activeSensors_.empty()) {
            SensorRuntimeContext ctx;
            ctx.id = "MERGED";
            ctx.instance_id = "merged_multisensor_01";
            ctx.name = "Combined Merged Cloud";
            ctx.short_name = "Merged Cloud";
            ctx.icon = "🌐";
            ctx.profile_id = "merged_multisensor_01";
            ctx.serial_number = "MERGED-REG-01";
            ctx.transport = "Offline Registration Engine";
            ctx.mount_label = "COMBINED";
            ctx.map_points = mapPoints_;
            ctx.trajectory = trajectory_;
            ctx.point_count = mapPoints_.size();
            activeSensors_.push_back(ctx);
        }

        // Check for session map.pcd or project root map.pcd if no capture data was found
        if (activeSensors_.empty() && mapPoints_.empty()) {
            fs::path s_dir = proj_dir / "sessions";
            if (fs::exists(s_dir)) {
                for (const auto& se : fs::directory_iterator(s_dir)) {
                    if (se.is_directory()) {
                        fs::path smap = se.path() / "map.pcd";
                        if (fs::exists(smap)) {
                            loadMapPcd(smap.string(), mapPoints_);
                            if (!mapPoints_.empty()) {
                                currentSessionId_ = QString::fromStdString(se.path().filename().string());
                                break;
                            }
                        }
                    }
                }
            }
            if (mapPoints_.empty()) {
                fs::path direct_pcd = proj_dir / "spatial_map.pcd";
                if (!fs::exists(direct_pcd)) direct_pcd = proj_dir / "map.pcd";
                if (fs::exists(direct_pcd)) {
                    loadMapPcd(direct_pcd.string(), mapPoints_);
                }
            }
            if (!mapPoints_.empty()) {
                SensorRuntimeContext ctx;
                ctx.id = "MERGED";
                ctx.instance_id = "merged_multisensor_01";
                ctx.name = "Combined Merged Cloud";
                ctx.short_name = "Merged Cloud";
                ctx.icon = "🌐";
                ctx.profile_id = "merged_multisensor_01";
                ctx.serial_number = "MERGED-REG-01";
                ctx.transport = "Offline Registration Engine";
                ctx.mount_label = "COMBINED";
                ctx.map_points = mapPoints_;
                ctx.trajectory = trajectory_;
                ctx.point_count = mapPoints_.size();
                activeSensors_.push_back(ctx);
            }
        }

        if (!activeSensors_.empty()) {
            activeDisplaySensor_ = QString::fromStdString(activeSensors_.front().id);
            mapPoints_ = activeSensors_.front().map_points;
            trajectory_ = activeSensors_.front().trajectory;
            pointsCaptured_ = 0;
            for (const auto& s : activeSensors_) pointsCaptured_ += s.point_count;
            hasMapData_ = (!mapPoints_.empty());
            isDualSensorMode_ = (activeSensors_.size() > 1);

            if (!trajectory_.empty()) {
                const auto& last = trajectory_.back();
                currentPoseX_ = last.position.x;
                currentPoseY_ = last.position.y;
                currentPoseZ_ = last.position.z;
            }
            emit activeSensorsChanged();
            emit isDualSensorModeChanged();
            emit activeDisplaySensorChanged();
        } else if (!mapPoints_.empty()) {
            pointsCaptured_ = mapPoints_.size();
            hasMapData_ = true;
        }

        // Reset temporary registration inspection overlay state on opening a regular project
        hasActiveRegistration_ = false;
        emit activeRegistrationChanged();

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
        emit canStartChanged();

        emit notification("Project Opened", QString("Loaded project '%1' with %2 saved map points.")
                          .arg(projectName_).arg(pointsCaptured_));
        AV_LOG_INFO("QmlBridge", "Opened project: " + projectId.toStdString() + " with " + 
                     std::to_string(pointsCaptured_) + " map points");

        refreshSessionCaptures();
        refreshRegistrations();
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

    // Ensure activeSensors_ is populated
    if (activeSensors_.empty()) {
        rebuildActiveSensorsForSelection();
    }

    if (activeSensors_.empty()) {
        emit errorOccurred("Capture Error", "No active sensors configured for capture.");
        return false;
    }

    // Strict Device Isolation: verify all active sensors have a compatible spatial capture backend
    for (const auto& s : activeSensors_) {
        if (!s.is_capturable) {
            emit errorOccurred("Capture Unavailable", QString::fromStdString("No compatible spatial capture backend has been established for " + s.name + "."));
            AV_LOG_WARN("QmlBridge", "Start capture rejected: No compatible spatial capture backend established for " + s.name);
            return false;
        }
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

    // Ensure activeSensors_ is populated
    if (activeSensors_.empty()) {
        rebuildActiveSensorsForSelection();
    }

    airyMapPoints_.clear();
    airyTrajectory_.clear();
    airyPointsCaptured_ = 0;

    geminiMapPoints_.clear();
    geminiTrajectory_.clear();
    geminiPointsCaptured_ = 0;

    vitureMapPoints_.clear();
    vitureTrajectory_.clear();
    viturePointsCaptured_ = 0;

    cameraMapPoints_.clear();
    cameraTrajectory_.clear();
    cameraPointsCaptured_ = 0;

    for (auto& s : activeSensors_) {
        s.map_points.clear();
        s.trajectory.clear();
        s.point_count = 0;
    }

    // Start all active sensors with failure isolation
    std::vector<std::string> started_sensors;
    bool all_succeeded = true;
    std::string failed_sensor;

    for (auto& s : activeSensors_) {
        bool ok = false;
        if (s.id == "AIRY") {
            if (airyAdapter_ && airyBackend_) {
                ok = (airyAdapter_->start() && airyBackend_->start());
            }
        } else if (s.id == "GEMINI") {
            if (geminiAdapter_ && geminiBackend_) {
                ok = (geminiAdapter_->start() && geminiBackend_->start());
            }
        } else if (s.id == "VITURE") {
            if (vitureAdapter_ && vitureBackend_) {
                ok = (vitureAdapter_->start() && vitureBackend_->start());
            }
        } else if (s.id == "CAMERA") {
            core::schemas::DeviceProfile prof;
            if (storage_) storage_->loadDeviceProfile(s.profile_id, prof);

            std::string backendType = selectedCameraBackend_.toStdString();
            if (backendType.empty()) backendType = s.backend_name;
            if (backendType.empty()) backendType = prof.slam_backend();

            if (backendType == "ABotStreamingBackend") {
                cameraBackend_ = std::make_shared<core::reconstruction::abot::ABotSlamBackend>();
                AV_LOG_INFO("QmlBridge", "Instantiated neural reconstruction backend: ABotSlamBackend [EXPERIMENTAL]");
            } else {
                cameraBackend_ = std::make_shared<core::reconstruction::lingbot::LingBotSlamBackend>();
                AV_LOG_INFO("QmlBridge", "Instantiated neural reconstruction backend: LingBotSlamBackend [COMMERCIAL]");
            }

            if (!prof.intrinsics().contains("device_path") && s.transport.rfind("/dev/video", 0) == 0) {
                auto intr = prof.intrinsics();
                intr["device_path"] = s.transport;
                prof.set_intrinsics(intr);
            }

            cameraBackend_->initialize(prof);
            ok = cameraBackend_->start();
        }

        if (ok) {
            started_sensors.push_back(s.id);
            s.device_status = "CONNECTED";
            s.acquisition_status = "RUNNING";
            s.tracking_status = "INITIALIZING";

            if (s.id == "AIRY") {
                airyDeviceStatus_ = "CONNECTED";
                airyAcquisitionStatus_ = "RUNNING";
                airyTrackingStatus_ = "INITIALIZING";
            } else if (s.id == "GEMINI") {
                geminiDeviceStatus_ = "CONNECTED";
                geminiAcquisitionStatus_ = "RUNNING";
                geminiTrackingStatus_ = "INITIALIZING";
            } else if (s.id == "VITURE") {
                vitureDeviceStatus_ = "CONNECTED";
                vitureAcquisitionStatus_ = "RUNNING";
                vitureTrackingStatus_ = "INITIALIZING";
            } else if (s.id == "CAMERA") {
                cameraDeviceStatus_ = "CONNECTED";
                cameraAcquisitionStatus_ = "RUNNING";
                cameraTrackingStatus_ = "INITIALIZING";
            }
        } else {
            all_succeeded = false;
            failed_sensor = s.name;
            break;
        }
    }

    if (!all_succeeded) {
        for (const auto& sid : started_sensors) {
            if (sid == "AIRY") {
                if (airyBackend_) airyBackend_->stop();
                if (airyAdapter_) airyAdapter_->stop();
            } else if (sid == "GEMINI") {
                if (geminiBackend_) geminiBackend_->stop();
                if (geminiAdapter_) geminiAdapter_->stop();
            } else if (sid == "VITURE") {
                if (vitureBackend_) vitureBackend_->stop();
                if (vitureAdapter_) vitureAdapter_->stop();
            } else if (sid == "CAMERA") {
                if (cameraBackend_) cameraBackend_->stop();
            }
        }
        emit errorOccurred("Multi-Sensor Startup Failure", QString::fromStdString("Sensor failed to start: " + failed_sensor + ". All active pipelines stopped to prevent non-compliant partial capture."));
        return false;
    }

    isCapturing_ = true;
    canSave_ = false;
    captureState_ = "CAPTURING";
    if (activeSensors_.size() > 1) {
        sensorStatus_ = QString("SCANNER NODE: scanar-01 (%1 Active Sensors)").arg(activeSensors_.size());
    } else if (!activeSensors_.empty()) {
        sensorStatus_ = QString::fromStdString(activeSensors_.front().name);
    } else {
        sensorStatus_ = "STREAMING";
    }
    trackingStatus_ = "INITIALIZING";

    timer_->start(1000);
    telemetryTimer_->start(100);

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
    emit activeSensorsChanged();
    emit airyTelemetryChanged();
    emit geminiTelemetryChanged();
    emit vitureTelemetryChanged();
    emit cameraTelemetryChanged();
    emit canStartChanged();

    AV_LOG_INFO("QmlBridge", "Modular multi-sensor capture started with " + std::to_string(activeSensors_.size()) + " sensor(s).");
    return true;
}

bool QmlBridge::stopCapture() {
    if (!isCapturing_) {
        emit errorOccurred("Capture Warning", "No capture session is currently running.");
        return false;
    }

    timer_->stop();
    telemetryTimer_->stop();

    uint64_t total_points = 0;
    for (auto& s : activeSensors_) {
        if (s.id == "AIRY") {
            if (airyBackend_) {
                airyBackend_->stop();
                s.map_points = airyBackend_->getMapPoints();
                s.trajectory = airyBackend_->getTrajectory();
            }
            if (airyAdapter_) airyAdapter_->stop();
            airyMapPoints_ = s.map_points;
            airyTrajectory_ = s.trajectory;
            airyPointsCaptured_ = s.map_points.size();
            airyDeviceStatus_ = "STANDBY";
            airyAcquisitionStatus_ = "STOPPED";
            airyTrackingStatus_ = "IDLE";
        } else if (s.id == "GEMINI") {
            if (geminiBackend_) {
                geminiBackend_->stop();
                s.map_points = geminiBackend_->getMapPoints();
                s.trajectory = geminiBackend_->getTrajectory();
            }
            if (geminiAdapter_) geminiAdapter_->stop();
            geminiMapPoints_ = s.map_points;
            geminiTrajectory_ = s.trajectory;
            geminiPointsCaptured_ = s.map_points.size();
            geminiDeviceStatus_ = "STANDBY";
            geminiAcquisitionStatus_ = "STOPPED";
            geminiTrackingStatus_ = "IDLE";
        } else if (s.id == "VITURE") {
            if (vitureBackend_) {
                vitureBackend_->stop();
                s.map_points = vitureBackend_->getMapPoints();
                s.trajectory = vitureBackend_->getTrajectory();
            }
            if (vitureAdapter_) vitureAdapter_->stop();
            vitureMapPoints_ = s.map_points;
            vitureTrajectory_ = s.trajectory;
            viturePointsCaptured_ = s.map_points.size();
            vitureDeviceStatus_ = "STANDBY";
            vitureAcquisitionStatus_ = "STOPPED";
            vitureTrackingStatus_ = "IDLE";
        } else if (s.id == "CAMERA") {
            if (cameraBackend_) {
                cameraBackend_->stop();
                s.map_points = cameraBackend_->getMapPoints();
                s.trajectory = cameraBackend_->getTrajectory();
            }
            cameraMapPoints_ = s.map_points;
            cameraTrajectory_ = s.trajectory;
            cameraPointsCaptured_ = s.map_points.size();
            cameraDeviceStatus_ = "STANDBY";
            cameraAcquisitionStatus_ = "STOPPED";
            cameraTrackingStatus_ = "IDLE";
        }

        s.point_count = s.map_points.size();
        total_points += s.point_count;
        s.device_status = "STANDBY";
        s.acquisition_status = "STOPPED";
        s.tracking_status = "IDLE";
    }

    pointsCaptured_ = total_points;
    hasMapData_ = (pointsCaptured_ > 0);

    for (const auto& s : activeSensors_) {
        if (s.id == activeDisplaySensor_.toStdString()) {
            mapPoints_ = s.map_points;
            trajectory_ = s.trajectory;
            break;
        }
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
    emit trajectoryChanged();
    emit activeSensorsChanged();
    emit airyTelemetryChanged();
    emit geminiTelemetryChanged();
    emit vitureTelemetryChanged();
    emit cameraTelemetryChanged();
    emit canStartChanged();

    AV_LOG_INFO("QmlBridge", "Modular multi-sensor capture stopped. Final map points: " + std::to_string(pointsCaptured_));
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

        fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString() / 
                            "sessions" / currentSessionId_.toStdString();
        fs::path caps_base_dir = sess_dir / "captures";
        fs::create_directories(caps_base_dir);

        // 1. Save ScannerNode definition using persistent node identity
        core::schemas::ScannerNode node = multiNodeOrchestrator_ ? multiNodeOrchestrator_->getLocalNode() 
                                                                 : core::schemas::ScannerNode("scanar-01", "orin-nano", "JETSON_LINUX", "127.0.0.1");
        std::string local_node_uuid = node.node_uuid().empty() ? "node_local_jetson" : node.node_uuid();
        node.set_node_uuid(local_node_uuid);
        node.set_role("PRIMARY_ORCHESTRATOR");
        node.set_status("ONLINE");

        for (const auto& s : activeSensors_) {
            core::schemas::SensorInstance inst(s.instance_id, local_node_uuid, s.profile_id);
            inst.set_serial_number(s.serial_number);
            inst.set_transport_endpoint(s.transport);
            inst.set_mount_label(s.mount_label);
            inst.set_status(core::schemas::SensorStatus::CONNECTED);
            node.add_sensor(inst);
        }

        storage_->saveScannerNode(node);
        storage_->saveSessionScannerNode(projectId_.toStdString(), currentSessionId_.toStdString(), node);
        fs::path sess_node_file = sess_dir / "scanner_node.json";
        std::ofstream nof(sess_node_file);
        nof << node.to_json().dump(2);
        nof.close();

        // 2. Load session
        core::schemas::Session session;
        storage_->loadSession(currentSessionId_.toStdString(), session);
        session.set_coordinator_node_uuid(local_node_uuid);
        session.add_scanner_node(local_node_uuid, "SESSION_COORDINATOR");
        session.set_status(core::schemas::SessionStatus::SAVED);

        // 3. Persist each sensor's capture independently with owner_node_uuid
        for (const auto& s : activeSensors_) {
            std::string s_lower = s.id;
            std::transform(s_lower.begin(), s_lower.end(), s_lower.begin(), ::tolower);
            std::string cap_id = "cap_" + std::to_string(now_ms) + "_" + s_lower;
            fs::path cap_dir = caps_base_dir / cap_id;
            fs::create_directories(cap_dir);

            // Save map.pcd
            fs::path pcd_path = cap_dir / "map.pcd";
            saveMapPcd(pcd_path.string(), s.map_points);

            // Save trajectory.json
            fs::path traj_path = cap_dir / "trajectory.json";
            nlohmann::json taj = nlohmann::json::array();
            for (const auto& tp : s.trajectory) {
                taj.push_back({
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
            tof << taj.dump(2);
            tof.close();

            // Save map_metadata.json
            fs::path meta_path = cap_dir / "map_metadata.json";
            core::schemas::MapMetadata meta(cap_id, currentSessionId_.toStdString(), s.instance_id, s_lower + "_slam");
            meta.set_point_count(s.map_points.size());
            meta.set_tracking_state(core::schemas::TrackingState::TRACKING_OK);
            meta.set_map_geometry_format("PCL_PCD");
            nlohmann::json mj = meta.to_json();
            if (s.id == "CAMERA") {
                mj["geometry_source"] = "MONOCULAR_NEURAL_RECONSTRUCTION";
                mj["metric_constraint"] = "NONE";
                mj["scale_status"] = "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED";
            }
            std::ofstream mof(meta_path);
            mof << mj.dump(2);
            mof.close();

            // Save capture.json with explicit owner_node_uuid
            core::schemas::Capture cap(cap_id, currentSessionId_.toStdString(), s.instance_id, session.start_time_ns(), local_node_uuid);
            cap.set_owner_node_uuid(local_node_uuid);
            cap.set_start_timestamp_ns(session.start_time_ns());
            cap.set_end_timestamp_ns(session.end_time_ns());
            cap.set_observation_count(s.map_points.size());
            cap.set_map_path(pcd_path.string());
            cap.set_trajectory_path(traj_path.string());
            cap.set_status("COMPLETED");

            if (s.id == "AIRY" && airyAdapter_) {
                auto cal = airyAdapter_->getCalibration();
                if (!cal.empty()) cap.set_provenance(cal[0]);
            } else if (s.id == "GEMINI" && geminiAdapter_) {
                auto cal = geminiAdapter_->getCalibration();
                if (!cal.empty()) cap.set_provenance(cal[0]);
            } else if (s.id == "VITURE" && vitureAdapter_) {
                auto cal = vitureAdapter_->getCalibration();
                if (!cal.empty()) cap.set_provenance(cal[0]);
            } else if (s.id == "CAMERA" && cameraBackend_) {
                auto cal = cameraBackend_->getCalibration();
                if (!cal.empty()) cap.set_provenance(cal[0]);
            }

            storage_->saveCapture(cap);
            session.add_capture(cap);
        }

        storage_->saveSession(session);

        canSave_ = false;
        captureState_ = "SAVED";
        hasMapData_ = (!mapPoints_.empty());

        emit canSaveChanged();
        emit captureStateChanged();
        emit mapDataUpdated();
        emit canStartChanged();
        emit notification("Capture Saved", QString("Capture %1 persisted (%2 points across %3 sensors).")
                          .arg(currentSessionId_).arg(pointsCaptured_).arg(activeSensors_.size()));
        AV_LOG_INFO("QmlBridge", "Modular multi-sensor capture saved successfully for session: " + currentSessionId_.toStdString());
        return true;
    }

    emit errorOccurred("Save Error", "Failed to save capture session.");
    return false;
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
    if (!isCapturing_) return;

    bool any_updated = false;
    uint64_t total_points = 0;
    QStringList trackSummaries;

    for (auto& s : activeSensors_) {
        if (s.id == "AIRY" && airyBackend_) {
            if (airyBackend_->updateTelemetry()) {
                any_updated = true;
                s.point_count = airyBackend_->getPointCount();
                s.tracking_status = core::schemas::to_string(airyBackend_->getTrackingState());
                s.acquisition_rate = airyBackend_->getAcquisitionRate();
                s.slam_rate = airyBackend_->getSlamRate();
                if (airyAdapter_) {
                    s.device_status = core::schemas::to_string(airyAdapter_->getStatus());
                }
                airyBackend_->fetchNewPoints();
                s.map_points = airyBackend_->getMapPoints();
                s.trajectory = airyBackend_->getTrajectory();

                airyPointsCaptured_ = s.point_count;
                airyTrackingStatus_ = QString::fromStdString(s.tracking_status);
                airyAcquisitionRate_ = s.acquisition_rate;
                airySlamRate_ = s.slam_rate;
                airyDeviceStatus_ = QString::fromStdString(s.device_status);
                airyMapPoints_ = s.map_points;
                airyTrajectory_ = s.trajectory;
                emit airyTelemetryChanged();
            }
        } else if (s.id == "GEMINI" && geminiBackend_) {
            if (geminiBackend_->updateTelemetry()) {
                any_updated = true;
                s.point_count = geminiBackend_->getPointCount();
                s.tracking_status = core::schemas::to_string(geminiBackend_->getTrackingState());
                s.acquisition_rate = geminiBackend_->getAcquisitionRate();
                s.slam_rate = geminiBackend_->getSlamRate();
                if (geminiAdapter_) {
                    s.device_status = core::schemas::to_string(geminiAdapter_->getStatus());
                }
                geminiBackend_->fetchNewPoints();
                s.map_points = geminiBackend_->getMapPoints();
                s.trajectory = geminiBackend_->getTrajectory();

                geminiPointsCaptured_ = s.point_count;
                geminiTrackingStatus_ = QString::fromStdString(s.tracking_status);
                geminiAcquisitionRate_ = s.acquisition_rate;
                geminiSlamRate_ = s.slam_rate;
                geminiDeviceStatus_ = QString::fromStdString(s.device_status);
                geminiMapPoints_ = s.map_points;
                geminiTrajectory_ = s.trajectory;
                emit geminiTelemetryChanged();
            }
        } else if (s.id == "VITURE" && vitureBackend_) {
            if (vitureBackend_->updateTelemetry()) {
                any_updated = true;
                s.point_count = vitureBackend_->getPointCount();
                s.tracking_status = core::schemas::to_string(vitureBackend_->getTrackingState());
                s.acquisition_rate = vitureBackend_->getAcquisitionRate();
                s.slam_rate = vitureBackend_->getSlamRate();
                if (vitureAdapter_) {
                    s.device_status = core::schemas::to_string(vitureAdapter_->getStatus());
                }
                vitureBackend_->fetchNewPoints();
                s.map_points = vitureBackend_->getMapPoints();
                s.trajectory = vitureBackend_->getTrajectory();

                viturePointsCaptured_ = s.point_count;
                vitureTrackingStatus_ = QString::fromStdString(s.tracking_status);
                vitureAcquisitionRate_ = s.acquisition_rate;
                vitureSlamRate_ = s.slam_rate;
                vitureDeviceStatus_ = QString::fromStdString(s.device_status);
                vitureMapPoints_ = s.map_points;
                vitureTrajectory_ = s.trajectory;
                emit vitureTelemetryChanged();
            }
        } else if (s.id == "CAMERA" && cameraBackend_) {
            if (cameraBackend_->updateTelemetry()) {
                any_updated = true;
                s.point_count = cameraBackend_->getPointCount();
                s.tracking_status = core::schemas::to_string(cameraBackend_->getTrackingState());
                s.acquisition_rate = cameraBackend_->getAcquisitionRate();
                s.slam_rate = cameraBackend_->getSlamRate();
                s.device_status = "CONNECTED";
                cameraBackend_->fetchNewPoints();
                s.map_points = cameraBackend_->getMapPoints();
                s.trajectory = cameraBackend_->getTrajectory();

                cameraPointsCaptured_ = s.point_count;
                cameraTrackingStatus_ = QString::fromStdString(s.tracking_status);
                cameraAcquisitionRate_ = s.acquisition_rate;
                cameraSlamRate_ = s.slam_rate;
                cameraDeviceStatus_ = QString::fromStdString(s.device_status);
                cameraMapPoints_ = s.map_points;
                cameraTrajectory_ = s.trajectory;
                emit cameraTelemetryChanged();
            }
        }

        total_points += s.point_count;
        trackSummaries.append(QString("%1: %2").arg(QString::fromStdString(s.short_name)).arg(QString::fromStdString(s.tracking_status)));
    }

    pointsCaptured_ = total_points;
    if (activeSensors_.size() > 1) {
        sensorStatus_ = QString("SCANNER NODE: scanar-01 (%1 Active Sensors)").arg(activeSensors_.size());
        trackingStatus_ = trackSummaries.join(" | ");
    } else if (!activeSensors_.empty()) {
        sensorStatus_ = QString::fromStdString(activeSensors_.front().name);
        trackingStatus_ = QString::fromStdString(activeSensors_.front().tracking_status);
    }

    // Viewport routing based on activeDisplaySensor_
    for (const auto& s : activeSensors_) {
        if (s.id == activeDisplaySensor_.toStdString()) {
            mapPoints_ = s.map_points;
            trajectory_ = s.trajectory;
            if (s.id == "AIRY" && airyBackend_) {
                currentPoseX_ = airyBackend_->getPose().position.x;
                currentPoseY_ = airyBackend_->getPose().position.y;
                currentPoseZ_ = airyBackend_->getPose().position.z;
                currentYaw_ = airyBackend_->getCurrentYaw();
            } else if (s.id == "GEMINI" && geminiBackend_) {
                currentPoseX_ = geminiBackend_->getPose().position.x;
                currentPoseY_ = geminiBackend_->getPose().position.y;
                currentPoseZ_ = geminiBackend_->getPose().position.z;
                currentYaw_ = geminiBackend_->getCurrentYaw();
            } else if (s.id == "VITURE" && vitureBackend_) {
                currentPoseX_ = vitureBackend_->getPose().position.x;
                currentPoseY_ = vitureBackend_->getPose().position.y;
                currentPoseZ_ = vitureBackend_->getPose().position.z;
                currentYaw_ = vitureBackend_->getCurrentYaw();
            } else if (s.id == "CAMERA" && cameraBackend_) {
                currentPoseX_ = cameraBackend_->getPose().position.x;
                currentPoseY_ = cameraBackend_->getPose().position.y;
                currentPoseZ_ = cameraBackend_->getPose().position.z;
                currentYaw_ = cameraBackend_->getCurrentYaw();
            }
            break;
        }
    }
    hasMapData_ = (!mapPoints_.empty());

    emit hostMetricsChanged();
    emit poseChanged();
    emit trackingStatusChanged();
    emit sensorStatusChanged();
    emit pointsCapturedChanged();
    emit trajectoryChanged();
    emit mapDataUpdated();
}

QVariantList QmlBridge::getDisplayPoints(int maxPoints) const {
    QVariantList list;
    if (mapPoints_.empty()) return list;

    size_t total = mapPoints_.size();
    size_t step = (total > static_cast<size_t>(maxPoints)) ? (total / maxPoints) : 1;
    if (step == 0) step = 1;

    bool applyDiag = orientationDiagOpen_ && (orientationDiagSensor_ == activeDisplaySensor_ || orientationDiagSensor_ == "ALL");
    double r00 = orientationDiagR_[0][0];
    double r01 = orientationDiagR_[0][1];
    double r02 = orientationDiagR_[0][2];
    double r10 = orientationDiagR_[1][0];
    double r11 = orientationDiagR_[1][1];
    double r12 = orientationDiagR_[1][2];
    double r20 = orientationDiagR_[2][0];
    double r21 = orientationDiagR_[2][1];
    double r22 = orientationDiagR_[2][2];

    list.reserve(static_cast<int>(total / step) + 1);
    for (size_t i = 0; i < total; i += step) {
        const auto& pt = mapPoints_[i];
        double px = pt.x;
        double py = pt.y;
        double pz = pt.z;
        if (applyDiag) {
            double rx = r00 * px + r01 * py + r02 * pz;
            double ry = r10 * px + r11 * py + r12 * pz;
            double rz = r20 * px + r21 * py + r22 * pz;
            px = rx; py = ry; pz = rz;
        }
        QVariantList p;
        p.append(px);
        p.append(py);
        p.append(pz);
        p.append(pt.intensity);
        p.append(static_cast<int>(pt.r));
        p.append(static_cast<int>(pt.g));
        p.append(static_cast<int>(pt.b));
        p.append(pt.has_color);
        list.append(QVariant::fromValue(p));
    }
    return list;
}

QVariantList QmlBridge::getTrajectoryPoints() const {
    QVariantList list;
    list.reserve(static_cast<int>(trajectory_.size()));

    bool applyDiag = orientationDiagOpen_ && (orientationDiagSensor_ == activeDisplaySensor_ || orientationDiagSensor_ == "ALL");
    double r00 = orientationDiagR_[0][0];
    double r01 = orientationDiagR_[0][1];
    double r02 = orientationDiagR_[0][2];
    double r10 = orientationDiagR_[1][0];
    double r11 = orientationDiagR_[1][1];
    double r12 = orientationDiagR_[1][2];
    double r20 = orientationDiagR_[2][0];
    double r21 = orientationDiagR_[2][1];
    double r22 = orientationDiagR_[2][2];

    for (const auto& tp : trajectory_) {
        double px = tp.position.x;
        double py = tp.position.y;
        double pz = tp.position.z;
        if (applyDiag) {
            double rx = r00 * px + r01 * py + r02 * pz;
            double ry = r10 * px + r11 * py + r12 * pz;
            double rz = r20 * px + r21 * py + r22 * pz;
            px = rx; py = ry; pz = rz;
        }
        QVariantList p;
        p.append(px);
        p.append(py);
        p.append(pz);
        // Compute yaw from quaternion
        double qx = tp.orientation.x;
        double qy = tp.orientation.y;
        double qz = tp.orientation.z;
        double qw = tp.orientation.w;
        double yaw = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));
        if (applyDiag) {
            yaw += orientationDiagYaw_ * M_PI / 180.0;
        }
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
    if (!ofs.is_open()) return;

    bool has_color = false;
    for (const auto& pt : points) {
        if (pt.has_color) {
            has_color = true;
            break;
        }
    }

    if (has_color) {
        ofs << "# .PCD v0.7 - Point Cloud Data file format\n"
            << "VERSION 0.7\n"
            << "FIELDS x y z rgb intensity\n"
            << "SIZE 4 4 4 4 4\n"
            << "TYPE F F F U F\n"
            << "COUNT 1 1 1 1 1\n"
            << "WIDTH " << points.size() << "\n"
            << "HEIGHT 1\n"
            << "VIEWPOINT 0 0 0 1 0 0 0\n"
            << "POINTS " << points.size() << "\n"
            << "DATA ascii\n";

        for (const auto& pt : points) {
            uint32_t rgb_packed = ((uint32_t)pt.r << 16) | ((uint32_t)pt.g << 8) | ((uint32_t)pt.b);
            ofs << pt.x << " " << pt.y << " " << pt.z << " " << rgb_packed << " " << pt.intensity << "\n";
        }
    } else {
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
    }
    ofs.close();
}

bool QmlBridge::loadMapPcd(const std::string& filepath, std::vector<core::schemas::PointXYZI>& out_points) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return false;

    out_points.clear();
    std::string line;
    bool in_data = false;
    bool has_rgb_field = false;

    while (std::getline(ifs, line)) {
        if (!in_data) {
            if (line.rfind("FIELDS", 0) == 0) {
                if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                    has_rgb_field = true;
                }
            } else if (line.rfind("DATA", 0) == 0) {
                in_data = true;
            }
            continue;
        }

        std::istringstream iss(line);
        core::schemas::PointXYZI pt;
        if (has_rgb_field) {
            std::string rgb_token;
            if (iss >> pt.x >> pt.y >> pt.z >> rgb_token) {
                uint32_t rgb_packed = 0;
                if (rgb_token.find('.') != std::string::npos || 
                    rgb_token.find('e') != std::string::npos || 
                    rgb_token.find('E') != std::string::npos) {
                    double d = std::strtod(rgb_token.c_str(), nullptr);
                    float rgb_f = static_cast<float>(d);
                    std::memcpy(&rgb_packed, &rgb_f, sizeof(uint32_t));
                } else {
                    rgb_packed = static_cast<uint32_t>(std::strtoul(rgb_token.c_str(), nullptr, 10));
                }
                pt.r = static_cast<uint8_t>((rgb_packed >> 16) & 0xFF);
                pt.g = static_cast<uint8_t>((rgb_packed >> 8) & 0xFF);
                pt.b = static_cast<uint8_t>(rgb_packed & 0xFF);
                pt.has_color = true;
                if (!(iss >> pt.intensity)) {
                    pt.intensity = 1.0f;
                }
                out_points.push_back(pt);
            }
        } else {
            if (iss >> pt.x >> pt.y >> pt.z) {
                if (!(iss >> pt.intensity)) {
                    pt.intensity = 1.0f;
                }
                pt.r = 255;
                pt.g = 255;
                pt.b = 255;
                pt.has_color = false;
                out_points.push_back(pt);
            }
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
    pan2DX_ += deltaY / zoom2D_;
    pan2DY_ += deltaX / zoom2D_;
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

// Milestone 5.5: Orientation Diagnostic Tool Implementation
void QmlBridge::openOrientationDiagnostic(const QString& sensorId) {
    orientationDiagOpen_ = true;
    if (!sensorId.isEmpty()) {
        orientationDiagSensor_ = sensorId.toUpper();
    } else if (!activeDisplaySensor_.isEmpty()) {
        orientationDiagSensor_ = activeDisplaySensor_.toUpper();
    } else {
        orientationDiagSensor_ = "GEMINI";
    }

    // If mapPoints_ is empty, auto-load the real physical diagnostic capture if available
    if (mapPoints_.empty() && orientationDiagSensor_ == "GEMINI") {
        fs::path sample_pcd = "/home/scanar/av_scan/diagnostics/gemini_real_rgb.pcd";
        if (fs::exists(sample_pcd)) {
            loadMapPcd(sample_pcd.string(), mapPoints_);
            pointsCaptured_ = mapPoints_.size();
            hasMapData_ = (!mapPoints_.empty());
            emit pointsCapturedChanged();
        }
    }

    if (orientationDiagSensor_ == "AIRY") {
        orientationDiagRoll_ = 0.0;
        orientationDiagPitch_ = 0.0;
        orientationDiagYaw_ = 0.0;
        orientationDiagStatusMsg_ = "Airy is FROZEN Ground Truth Reference (Identity FLU). Controls locked.";
    } else {
        orientationDiagStatusMsg_ = QString("Adjust %1 Roll/Pitch/Yaw until floor is horizontal and +Z is UP.").arg(orientationDiagSensor_);
    }

    updateOrientationDiagMatrix();
}

void QmlBridge::closeOrientationDiagnostic() {
    orientationDiagOpen_ = false;
    orientationDiagRoll_ = 0.0;
    orientationDiagPitch_ = 0.0;
    orientationDiagYaw_ = 0.0;
    updateOrientationDiagMatrix();
    emit orientationDiagChanged();
    emit mapDataUpdated();
    emit trajectoryChanged();
}

void QmlBridge::setOrientationDiagSensor(const QString& sensorId) {
    if (sensorId.isEmpty()) return;
    orientationDiagSensor_ = sensorId.toUpper();
    setActiveDisplaySensor(orientationDiagSensor_);
    resetOrientationDiagRotation();
}

void QmlBridge::setOrientationDiagRotation(qreal rollDeg, qreal pitchDeg, qreal yawDeg) {
    if (orientationDiagIsFrozen()) return;
    orientationDiagRoll_ = rollDeg;
    orientationDiagPitch_ = pitchDeg;
    orientationDiagYaw_ = yawDeg;
    updateOrientationDiagMatrix();
}

void QmlBridge::stepOrientationDiagRotation(const QString& axis, qreal deltaDeg) {
    if (orientationDiagIsFrozen()) return;
    if (axis == "ROLL" || axis == "X") {
        orientationDiagRoll_ = std::fmod(orientationDiagRoll_ + deltaDeg + 180.0, 360.0) - 180.0;
    } else if (axis == "PITCH" || axis == "Y") {
        orientationDiagPitch_ = std::fmod(orientationDiagPitch_ + deltaDeg + 180.0, 360.0) - 180.0;
    } else if (axis == "YAW" || axis == "Z") {
        orientationDiagYaw_ = std::fmod(orientationDiagYaw_ + deltaDeg + 180.0, 360.0) - 180.0;
    }
    updateOrientationDiagMatrix();
}

void QmlBridge::resetOrientationDiagRotation() {
    orientationDiagRoll_ = 0.0;
    orientationDiagPitch_ = 0.0;
    orientationDiagYaw_ = 0.0;
    if (orientationDiagSensor_ == "AIRY") {
        orientationDiagStatusMsg_ = "Airy is FROZEN Ground Truth Reference (Identity FLU).";
    } else {
        orientationDiagStatusMsg_ = QString("Rotation reset for %1. Adjust sliders or step buttons.").arg(orientationDiagSensor_);
    }
    updateOrientationDiagMatrix();
}

void QmlBridge::updateOrientationDiagMatrix() {
    double radRoll = orientationDiagRoll_ * M_PI / 180.0;
    double radPitch = orientationDiagPitch_ * M_PI / 180.0;
    double radYaw = orientationDiagYaw_ * M_PI / 180.0;

    double cr = std::cos(radRoll), sr = std::sin(radRoll);
    double cp = std::cos(radPitch), sp = std::sin(radPitch);
    double cy = std::cos(radYaw), sy = std::sin(radYaw);

    // R = Rz(yaw) * Ry(pitch) * Rx(roll)
    orientationDiagR_[0][0] = cp * cy;
    orientationDiagR_[0][1] = sp * sr * cy - sy * cr;
    orientationDiagR_[0][2] = sp * cr * cy + sr * sy;

    orientationDiagR_[1][0] = sy * cp;
    orientationDiagR_[1][1] = sp * sr * sy + cr * cy;
    orientationDiagR_[1][2] = sp * sy * cr - sr * cy;

    orientationDiagR_[2][0] = -sp;
    orientationDiagR_[2][1] = sr * cp;
    orientationDiagR_[2][2] = cp * cr;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "[ %+.3f, %+.3f, %+.3f ]\n[ %+.3f, %+.3f, %+.3f ]\n[ %+.3f, %+.3f, %+.3f ]",
             orientationDiagR_[0][0], orientationDiagR_[0][1], orientationDiagR_[0][2],
             orientationDiagR_[1][0], orientationDiagR_[1][1], orientationDiagR_[1][2],
             orientationDiagR_[2][0], orientationDiagR_[2][1], orientationDiagR_[2][2]);
    orientationDiagMatrixStr_ = QString::fromUtf8(buf);

    char tbuf[512];
    snprintf(tbuf, sizeof(tbuf),
             "X_av = (%+.3f)*X + (%+.3f)*Y + (%+.3f)*Z\n"
             "Y_av = (%+.3f)*X + (%+.3f)*Y + (%+.3f)*Z\n"
             "Z_av = (%+.3f)*X + (%+.3f)*Y + (%+.3f)*Z",
             orientationDiagR_[0][0], orientationDiagR_[0][1], orientationDiagR_[0][2],
             orientationDiagR_[1][0], orientationDiagR_[1][1], orientationDiagR_[1][2],
             orientationDiagR_[2][0], orientationDiagR_[2][1], orientationDiagR_[2][2]);
    orientationDiagTransformStr_ = QString::fromUtf8(tbuf);

    emit orientationDiagChanged();
    emit mapDataUpdated();
    emit trajectoryChanged();
}

bool QmlBridge::acceptOrientationDiagRotation() {
    if (orientationDiagSensor_ == "AIRY") {
        orientationDiagStatusMsg_ = "Airy is FROZEN Ground Truth reference; orientation cannot be modified.";
        emit orientationDiagChanged();
        return false;
    }

    std::string s_id = orientationDiagSensor_.toStdString();

    // 1. Permanently rotate current in-memory map points and trajectory
    for (auto& s : activeSensors_) {
        if (s.id == s_id) {
            for (auto& pt : s.map_points) {
                double rx = orientationDiagR_[0][0] * pt.x + orientationDiagR_[0][1] * pt.y + orientationDiagR_[0][2] * pt.z;
                double ry = orientationDiagR_[1][0] * pt.x + orientationDiagR_[1][1] * pt.y + orientationDiagR_[1][2] * pt.z;
                double rz = orientationDiagR_[2][0] * pt.x + orientationDiagR_[2][1] * pt.y + orientationDiagR_[2][2] * pt.z;
                pt.x = static_cast<float>(rx);
                pt.y = static_cast<float>(ry);
                pt.z = static_cast<float>(rz);
            }
            for (auto& tp : s.trajectory) {
                double rx = orientationDiagR_[0][0] * tp.position.x + orientationDiagR_[0][1] * tp.position.y + orientationDiagR_[0][2] * tp.position.z;
                double ry = orientationDiagR_[1][0] * tp.position.x + orientationDiagR_[1][1] * tp.position.y + orientationDiagR_[1][2] * tp.position.z;
                double rz = orientationDiagR_[2][0] * tp.position.x + orientationDiagR_[2][1] * tp.position.y + orientationDiagR_[2][2] * tp.position.z;
                tp.position.x = rx;
                tp.position.y = ry;
                tp.position.z = rz;
            }
            if (activeDisplaySensor_.toStdString() == s_id) {
                mapPoints_ = s.map_points;
                trajectory_ = s.trajectory;
            }
            break;
        }
    }

    // Also update global mapPoints_ if matching
    if (activeDisplaySensor_.toStdString() == s_id) {
        for (auto& pt : mapPoints_) {
            double rx = orientationDiagR_[0][0] * pt.x + orientationDiagR_[0][1] * pt.y + orientationDiagR_[0][2] * pt.z;
            double ry = orientationDiagR_[1][0] * pt.x + orientationDiagR_[1][1] * pt.y + orientationDiagR_[1][2] * pt.z;
            double rz = orientationDiagR_[2][0] * pt.x + orientationDiagR_[2][1] * pt.y + orientationDiagR_[2][2] * pt.z;
            pt.x = static_cast<float>(rx);
            pt.y = static_cast<float>(ry);
            pt.z = static_cast<float>(rz);
        }
        for (auto& tp : trajectory_) {
            double rx = orientationDiagR_[0][0] * tp.position.x + orientationDiagR_[0][1] * tp.position.y + orientationDiagR_[0][2] * tp.position.z;
            double ry = orientationDiagR_[1][0] * tp.position.x + orientationDiagR_[1][1] * tp.position.y + orientationDiagR_[1][2] * tp.position.z;
            double rz = orientationDiagR_[2][0] * tp.position.x + orientationDiagR_[2][1] * tp.position.y + orientationDiagR_[2][2] * tp.position.z;
            tp.position.x = rx;
            tp.position.y = ry;
            tp.position.z = rz;
        }
    }

    // 2. Save candidate transform to profiles/validated/<sensor>_profile.json
    fs::path prof_path;
    if (s_id == "GEMINI") {
        prof_path = fs::path("/home/scanar/av_scan/profiles/validated/gemini_336l_profile.json");
    } else if (s_id == "VITURE") {
        prof_path = fs::path("/home/scanar/av_scan/profiles/validated/viture_ultra_profile.json");
    }

    if (!prof_path.empty() && fs::exists(prof_path)) {
        try {
            std::ifstream pf(prof_path);
            nlohmann::json pj = nlohmann::json::parse(pf);
            pf.close();

            pj["canonical_av_transform"] = {
                {"target_convention", "CANONICAL_AV (+X Fwd, +Y Left, +Z Up, Ground XY)"},
                {"source_frame", s_id == "GEMINI" ? "camera_link" : "carina_gl_origin"},
                {"status", "VALIDATED"},
                {"roll_deg", orientationDiagRoll_},
                {"pitch_deg", orientationDiagPitch_},
                {"yaw_deg", orientationDiagYaw_},
                {"matrix_3x3", {
                    {orientationDiagR_[0][0], orientationDiagR_[0][1], orientationDiagR_[0][2]},
                    {orientationDiagR_[1][0], orientationDiagR_[1][1], orientationDiagR_[1][2]},
                    {orientationDiagR_[2][0], orientationDiagR_[2][1], orientationDiagR_[2][2]}
                }},
                {"transform_4x4", {
                    {orientationDiagR_[0][0], orientationDiagR_[0][1], orientationDiagR_[0][2], 0.0},
                    {orientationDiagR_[1][0], orientationDiagR_[1][1], orientationDiagR_[1][2], 0.0},
                    {orientationDiagR_[2][0], orientationDiagR_[2][1], orientationDiagR_[2][2], 0.0},
                    {0.0, 0.0, 0.0, 1.0}
                }},
                {"mapping", orientationDiagTransformStr_.toStdString()}
            };

            std::ofstream pof(prof_path);
            pof << pj.dump(2);
            pof.close();
        } catch (const std::exception& e) {
            orientationDiagStatusMsg_ = QString("Error saving profile: %1").arg(e.what());
            emit orientationDiagChanged();
            return false;
        }
    }

    // 3. Save daemon runtime transform file: core/drivers/<sensor>/canonical_transform.json
    fs::path daemon_tf_path;
    if (s_id == "GEMINI") {
        daemon_tf_path = "/home/scanar/av_scan/core/drivers/gemini/canonical_transform.json";
    } else if (s_id == "VITURE") {
        daemon_tf_path = "/home/scanar/av_scan/core/drivers/viture/canonical_transform.json";
    }
    if (!daemon_tf_path.empty()) {
        try {
            nlohmann::json dj = {
                {"sensor", s_id},
                {"roll_deg", orientationDiagRoll_},
                {"pitch_deg", orientationDiagPitch_},
                {"yaw_deg", orientationDiagYaw_},
                {"matrix_3x3", {
                    {orientationDiagR_[0][0], orientationDiagR_[0][1], orientationDiagR_[0][2]},
                    {orientationDiagR_[1][0], orientationDiagR_[1][1], orientationDiagR_[1][2]},
                    {orientationDiagR_[2][0], orientationDiagR_[2][1], orientationDiagR_[2][2]}
                }}
            };
            std::ofstream dof(daemon_tf_path);
            dof << dj.dump(2);
            dof.close();
        } catch (...) {}
    }

    orientationDiagStatusMsg_ = QString("Orientation ACCEPTED and SAVED for %1! Canonical pipeline updated.").arg(orientationDiagSensor_);
    // Reset active preview rotation since points are baked
    orientationDiagRoll_ = 0.0;
    orientationDiagPitch_ = 0.0;
    orientationDiagYaw_ = 0.0;
    updateOrientationDiagMatrix();

    AV_LOG_INFO("QmlBridge", "Orientation accepted and canonical transform persisted for " + s_id);
    return true;
}

// ============================================================================
// Milestone 14.5: Universal Sensor Ecosystem & Catalog Bridge Implementation
// ============================================================================

int QmlBridge::ecosystemModelCount() const {
    return static_cast<int>(core::ecosystem::VendorEcosystemRegistry::instance().getAllModels().size());
}

int QmlBridge::ecosystemVendorCount() const {
    return static_cast<int>(core::ecosystem::VendorEcosystemRegistry::instance().getManufacturers().size());
}

QStringList QmlBridge::getEcosystemManufacturers() const {
    QStringList res;
    auto vendors = core::ecosystem::VendorEcosystemRegistry::instance().getManufacturers();
    for (const auto& v : vendors) {
        res.append(QString::fromStdString(v));
    }
    return res;
}

QStringList QmlBridge::getSensorTypesForManufacturer(const QString& vendor) const {
    QStringList res;
    std::vector<std::string> types;
    if (vendor.isEmpty() || vendor == "All" || vendor == "ALL") {
        types = core::ecosystem::VendorEcosystemRegistry::instance().getSensorTypes();
    } else {
        types = core::ecosystem::VendorEcosystemRegistry::instance().getSensorTypesForManufacturer(vendor.toStdString());
    }
    for (const auto& t : types) {
        res.append(QString::fromStdString(t));
    }
    return res;
}

QVariantList QmlBridge::getEcosystemModels(const QString& vendor, const QString& sensorType, const QString& searchQuery) const {
    QVariantList list;
    auto& reg = core::ecosystem::VendorEcosystemRegistry::instance();

    std::vector<core::schemas::DeviceModelDefinition> models;
    if (!searchQuery.trimmed().isEmpty()) {
        auto query_results = reg.searchModels(searchQuery.trimmed().toStdString());
        std::string v_filter = (vendor == "All" || vendor == "ALL") ? "" : vendor.toLower().toStdString();
        std::string t_filter = (sensorType == "All" || sensorType == "ALL") ? "" : sensorType.toLower().toStdString();
        for (const auto& m : query_results) {
            std::string mv = m.vendor();
            std::transform(mv.begin(), mv.end(), mv.begin(), ::tolower);
            std::string mt = m.sensor_type();
            std::transform(mt.begin(), mt.end(), mt.begin(), ::tolower);

            bool v_match = v_filter.empty() || (mv.find(v_filter) != std::string::npos);
            bool t_match = t_filter.empty() || (mt.find(t_filter) != std::string::npos);
            if (v_match && t_match) {
                models.push_back(m);
            }
        }
    } else {
        std::string v_arg = (vendor == "All" || vendor == "ALL") ? "" : vendor.toStdString();
        std::string t_arg = (sensorType == "All" || sensorType == "ALL") ? "" : sensorType.toStdString();
        models = reg.getModels(v_arg, t_arg);
    }

    for (const auto& m : models) {
        QVariantMap item;
        item["id"] = QString::fromStdString(m.model_id());
        item["vendor"] = QString::fromStdString(m.vendor());
        item["family"] = QString::fromStdString(m.family());
        item["model"] = QString::fromStdString(m.model());
        item["hardwareRevision"] = QString::fromStdString(m.hardware_revision());
        item["productionStatus"] = QString::fromStdString(core::schemas::to_string(m.production_status()));
        item["sensorType"] = QString::fromStdString(m.sensor_type());
        item["transportType"] = QString::fromStdString(m.transport_type());
        item["discoveryMechanism"] = QString::fromStdString(m.discovery_mechanism());

        item["validationTier"] = QString::fromStdString(core::schemas::to_string(m.av_support().validation_status));
        item["sourceProvenance"] = QString::fromStdString(core::schemas::to_string(m.source_provenance()));
        item["slamBackend"] = QString::fromStdString(m.av_support().backend);
        item["adapter"] = QString::fromStdString(m.av_support().adapter_class);

        item["hasLidar"] = m.features().has_lidar;
        item["hasRgb"] = m.features().has_rgb;
        item["hasDepth"] = m.features().has_depth;
        item["hasImu"] = m.features().has_imu;

        std::string st = m.sensor_type();
        if (st == "LiDAR") {
            item["icon"] = "📡";
            item["typeColor"] = "#38bdf8";
        } else if (st == "RGB-D") {
            item["icon"] = "📷";
            item["typeColor"] = "#c084fc";
        } else if (st == "Stereo") {
            item["icon"] = "👓";
            item["typeColor"] = "#2dd4bf";
        } else if (st == "RGB Camera") {
            item["icon"] = "📹";
            item["typeColor"] = "#f59e0b";
        } else if (st == "ToF") {
            item["icon"] = "⚡";
            item["typeColor"] = "#ec4899";
        } else if (st == "IMU") {
            item["icon"] = "🧭";
            item["typeColor"] = "#eab308";
        } else if (st == "Multi-Sensor") {
            item["icon"] = "🥽";
            item["typeColor"] = "#10b981";
        } else {
            item["icon"] = "⚙️";
            item["typeColor"] = "#94a3b8";
        }

        list.append(item);
    }
    return list;
}

QVariantMap QmlBridge::getModelDetails(const QString& modelId) const {
    QVariantMap res;
    const auto* m = core::ecosystem::VendorEcosystemRegistry::instance().findModelById(modelId.toStdString());
    if (!m) return res;

    res["id"] = QString::fromStdString(m->model_id());
    res["modelId"] = QString::fromStdString(m->model_id());
    res["vendor"] = QString::fromStdString(m->vendor());
    res["family"] = QString::fromStdString(m->family());
    res["model"] = QString::fromStdString(m->model());
    res["hardwareRevision"] = QString::fromStdString(m->hardware_revision());
    res["productionStatus"] = QString::fromStdString(core::schemas::to_string(m->production_status()));
    res["sensorType"] = QString::fromStdString(m->sensor_type());
    res["transportType"] = QString::fromStdString(m->transport_type());
    res["discoveryMechanism"] = QString::fromStdString(m->discovery_mechanism());

    QStringList vids, pids;
    for (const auto& v : m->vendor_ids()) vids.append(QString::fromStdString(v));
    for (const auto& p : m->product_ids()) pids.append(QString::fromStdString(p));
    res["vendorIds"] = vids;
    res["productIds"] = pids;
    res["defaultIp"] = QString::fromStdString(m->default_ip());

    QVariantList ports;
    for (uint16_t port : m->default_ports()) ports.append(port);
    res["defaultPorts"] = ports;

    // Vendor SDK
    const auto& sdk = m->vendor_sdk();
    res["sdkVendor"] = QString::fromStdString(sdk.vendor_name);
    res["sdkFamily"] = QString::fromStdString(sdk.family_name);
    res["currentSdk"] = QString::fromStdString(sdk.current_sdk);
    res["currentDriver"] = QString::fromStdString(sdk.current_driver);
    res["rosDriver"] = QString::fromStdString(sdk.current_ros_driver);
    res["license"] = QString::fromStdString(sdk.sdk_license);
    QStringList os_list;
    for (const auto& o : sdk.supported_os) os_list.append(QString::fromStdString(o));
    res["supportedOs"] = os_list;

    // Compatibility
    const auto& compat = m->compatibility();
    res["minFw"] = QString::fromStdString(compat.minimum_firmware);
    res["maxFw"] = QString::fromStdString(compat.maximum_firmware);
    QStringList sdks;
    for (const auto& s : compat.supported_sdk_versions) sdks.append(QString::fromStdString(s));
    res["supportedSdks"] = sdks;
    res["preferredSdk"] = QString::fromStdString(compat.preferred_sdk_version);
    res["legacySdk"] = QString::fromStdString(compat.legacy_sdk_version);
    res["protocolSpec"] = QString::fromStdString(compat.protocol_version);
    res["legacyRequired"] = compat.legacy_adapter_required;

    // Features
    const auto& feat = m->features();
    res["hasLidar"] = feat.has_lidar;
    res["hasRgb"] = feat.has_rgb;
    res["hasDepth"] = feat.has_depth;
    res["hasImu"] = feat.has_imu;
    res["hasPtp"] = feat.has_ptp;
    res["hasPps"] = feat.has_pps;
    res["hasHardwareSync"] = feat.has_hardware_sync;
    res["nativeCoordinateSystem"] = QString::fromStdString(feat.native_coordinate_system);
    QStringList returns, fields;
    for (const auto& r : feat.return_modes) returns.append(QString::fromStdString(r));
    for (const auto& f : feat.point_cloud_fields) fields.append(QString::fromStdString(f));
    res["returnModes"] = returns;
    res["pointCloudFields"] = fields;
    QVariantList rates;
    for (double r : feat.scan_rates_hz) rates.append(r);
    res["scanRates"] = rates;
    res["factoryCalibrationApi"] = QString::fromStdString(feat.factory_calibration_api);
    res["timestampSource"] = QString::fromStdString(feat.timestamp_source);

    // AV Support
    const auto& av = m->av_support();
    res["runtimeAdapter"] = QString::fromStdString(av.adapter_class);
    res["streamingBackend"] = QString::fromStdString(av.backend);
    res["recommendedSlamBackend"] = QString::fromStdString(av.slam_family);
    res["sdkVersion"] = QString::fromStdString(av.tested_sdk_version);
    res["driverVersion"] = QString::fromStdString(av.tested_driver_version);
    res["firmwareVersion"] = QString::fromStdString(av.tested_firmware);
    res["validationTier"] = QString::fromStdString(core::schemas::to_string(av.validation_status));
    res["sourceProvenance"] = QString::fromStdString(core::schemas::to_string(m->source_provenance()));

    // Provenance Record
    const auto& prov = m->provenance_record();
    res["docUrl"] = QString::fromStdString(prov.documentation_url);
    res["manualRef"] = QString::fromStdString(prov.manual_reference);
    res["provSdkVersion"] = QString::fromStdString(prov.official_sdk_version);
    res["protoSpec"] = QString::fromStdString(prov.protocol_specification);
    res["lastVerified"] = QString::fromStdString(prov.last_verified_date);

    // 8-Point Runtime Integration Matrix (A-H)
    const auto& rim = m->runtime_integration();
    res["catKnowledge"] = rim.catalog_knowledge_exists;
    res["officialSdkId"] = QString::fromStdString(rim.official_sdk_identified);
    res["sdkIntegrated"] = rim.sdk_integrated;
    res["hwInterrogated"] = rim.hardware_interrogation_implemented;
    res["adapterImplemented"] = rim.runtime_adapter_implemented;
    res["slamImplemented"] = rim.slam_backend_implemented;
    res["physTested"] = rim.physically_tested;
    res["guiValidated"] = rim.gui_validated;

    return res;
}

QVariantMap QmlBridge::createTemplateProfileFromModel(const QString& modelId) {
    QVariantMap res;
    res["success"] = false;

    const auto* m = core::ecosystem::VendorEcosystemRegistry::instance().findModelById(modelId.toStdString());
    if (!m) {
        res["error"] = "Model not found in ecosystem registry: " + modelId;
        AV_LOG_ERROR("QmlBridge", "Template Profile Generation Failed", res["error"].toString().toStdString());
        return res;
    }

    AV_LOG_INFO("QmlBridge", "Generating authoritative template profile from model definition: " + m->vendor() + " " + m->model());

    // 1. Seed template device profile (is_template = true)
    core::schemas::DeviceProfile prof = m->seedDeviceProfile(true);

    // 2. Persist to disk using DeviceProfiler
    core::profiling::DeviceProfiler profiler;
    fs::path repo_root = storage_ ? fs::path(storage_->getRootDirectory()) : fs::current_path();
    bool save_ok = profiler.saveExperimentalProfile(prof, repo_root);

    if (!save_ok) {
        res["error"] = "Failed to write template profile to disk";
        AV_LOG_ERROR("QmlBridge", "Template Profile Save Failed", res["error"].toString().toStdString());
        return res;
    }

    // 3. Immediately refresh storage engine and profile lists
    refreshData();
    refreshDiscovery();

    res["success"] = true;
    res["profileId"] = QString::fromStdString(prof.profile_id());
    res["modelId"] = QString::fromStdString(m->model_id());
    res["modelName"] = QString::fromStdString(prof.manufacturer() + " " + prof.model());
    res["manufacturer"] = QString::fromStdString(prof.manufacturer());
    res["sensorType"] = QString::fromStdString(m->sensor_type());
    res["profileStatus"] = "EXPERIMENTAL (TEMPLATE)";
    res["calibrationSource"] = "DEFAULT_NOMINAL (DOCUMENTED)";
    res["filePath"] = QString::fromStdString((repo_root / "profiles" / "experimental" / (prof.profile_id() + ".json")).string());

    AV_LOG_INFO("QmlBridge", "Template profile successfully created and persisted: " + prof.profile_id());
    return res;
}

// Milestone 10: Multi-Node Session Infrastructure Implementation

QString QmlBridge::nodeUuid() const {
    if (multiNodeOrchestrator_) {
        return QString::fromStdString(multiNodeOrchestrator_->getLocalNodeUuid());
    }
    return "UNKNOWN";
}

QString QmlBridge::nodeDisplayName() const {
    if (multiNodeOrchestrator_) {
        return QString::fromStdString(multiNodeOrchestrator_->getLocalNode().display_name());
    }
    return "AV Scan Node";
}

QString QmlBridge::nodePlatform() const {
    if (multiNodeOrchestrator_) {
        return QString::fromStdString(multiNodeOrchestrator_->getLocalNode().platform());
    }
    return "LINUX_ARM64";
}

QString QmlBridge::nodeHardware() const {
    return hostHardware();
}

QString QmlBridge::nodeRole() const {
    if (multiNodeOrchestrator_) {
        return QString::fromStdString(core::schemas::to_string(multiNodeOrchestrator_->getSessionRole()));
    }
    return "UNASSIGNED";
}

QString QmlBridge::nodeStatus() const {
    if (isCapturing_) return "CAPTURING";
    if (multiNodeOrchestrator_ && !multiNodeOrchestrator_->getActiveSessionId().empty()) return "IN_SESSION";
    return "AVAILABLE";
}

QString QmlBridge::activeMultiNodeSessionId() const {
    if (multiNodeOrchestrator_) {
        return QString::fromStdString(multiNodeOrchestrator_->getActiveSessionId());
    }
    return "";
}

bool QmlBridge::isMultiNodeSessionActive() const {
    return multiNodeOrchestrator_ && !multiNodeOrchestrator_->getActiveSessionId().empty();
}

bool QmlBridge::isCoordinator() const {
    return multiNodeOrchestrator_ && multiNodeOrchestrator_->getSessionRole() == core::schemas::NodeSessionRole::SESSION_COORDINATOR;
}

QVariantList QmlBridge::discoveredNodes() const {
    QVariantList list;
    if (!multiNodeOrchestrator_) return list;

    auto nodes = multiNodeOrchestrator_->getDiscoveredNodes();
    std::string my_uuid = multiNodeOrchestrator_->getLocalNodeUuid();

    for (const auto& n : nodes) {
        QVariantMap m;
        m["uuid"] = QString::fromStdString(n.beacon.node_uuid);
        m["shortUuid"] = QString::fromStdString(n.beacon.node_uuid.substr(0, 8));
        m["displayName"] = QString::fromStdString(n.beacon.display_name.empty() ? n.beacon.hostname : n.beacon.display_name);
        m["hostname"] = QString::fromStdString(n.beacon.hostname);
        m["platform"] = QString::fromStdString(n.beacon.platform);
        m["ip"] = QString::fromStdString(n.ip_address);
        m["port"] = n.coordination_port;
        m["endpoint"] = QString::fromStdString(n.ip_address + ":" + std::to_string(n.coordination_port));
        m["version"] = QString::fromStdString(n.beacon.av_scan_version);
        m["sessionState"] = QString::fromStdString(n.beacon.session_state);
        m["currentSessionId"] = QString::fromStdString(n.beacon.current_session_id);
        m["isSelf"] = (n.beacon.node_uuid == my_uuid);
        m["isAvailable"] = (n.beacon.session_state == "AVAILABLE" || n.beacon.session_state == "IDLE");
        
        QStringList caps;
        for (const auto& c : n.beacon.capabilities) {
            caps.append(QString::fromStdString(c));
        }
        m["capabilities"] = caps.join(", ");
        list.append(m);
    }
    return list;
}

QVariantList QmlBridge::sessionMembers() const {
    QVariantList list;
    if (!multiNodeOrchestrator_) return list;

    auto members = multiNodeOrchestrator_->getSessionMembers();
    for (const auto& mem : members) {
        QVariantMap m;
        m["uuid"] = QString::fromStdString(mem.node_uuid);
        m["shortUuid"] = QString::fromStdString(mem.node_uuid.substr(0, 8));
        m["displayName"] = QString::fromStdString(mem.display_name);
        m["platform"] = QString::fromStdString(mem.platform);
        m["ip"] = QString::fromStdString(mem.ip_address);
        m["role"] = QString::fromStdString(core::schemas::to_string(mem.role));
        m["isCoordinator"] = (mem.role == core::schemas::NodeSessionRole::SESSION_COORDINATOR);
        m["connected"] = mem.is_connected;
        m["ready"] = mem.is_ready;
        m["captureState"] = QString::fromStdString(mem.capture_state);
        m["capturing"] = (mem.capture_state == "CAPTURING");
        m["clockOffsetNs"] = static_cast<qlonglong>(mem.clock_offset_ns);
        m["clockOffsetMs"] = QString::number(mem.clock_offset_ns / 1000000.0, 'f', 2) + " ms";
        list.append(m);
    }
    return list;
}

void QmlBridge::setSelectedNodeViewIndex(int index) {
    if (selectedNodeViewIndex_ != index) {
        selectedNodeViewIndex_ = index;
        emit selectedNodeViewChanged();
    }
}

void QmlBridge::selectNodeView(int index) {
    setSelectedNodeViewIndex(index);
}

QString QmlBridge::selectedNodeViewName() const {
    if (selectedNodeViewIndex_ == 0 || !multiNodeOrchestrator_) {
        return QString("Local Device (%1)").arg(nodeDisplayName());
    }
    auto members = multiNodeOrchestrator_->getSessionMembers();
    int idx = 1;
    for (const auto& mem : members) {
        if (mem.node_uuid != multiNodeOrchestrator_->getLocalNodeUuid()) {
            if (idx == selectedNodeViewIndex_) {
                return QString("Remote Node: %1 [%2]").arg(QString::fromStdString(mem.display_name)).arg(QString::fromStdString(mem.platform));
            }
            idx++;
        }
    }
    return QString("Local Device (%1)").arg(nodeDisplayName());
}

void QmlBridge::setNodeDisplayName(const QString& name) {
    if (multiNodeOrchestrator_) {
        multiNodeOrchestrator_->setDisplayName(name.toStdString());
        emit nodeIdentityChanged();
    }
}

void QmlBridge::refreshMultiNodeDiscovery() {
    emit discoveredNodesChanged();
    emit sessionMembersChanged();
}

void QmlBridge::addManualPeer(const QString& ip, int port) {
    if (multiNodeOrchestrator_) {
        multiNodeOrchestrator_->addKnownPeer(ip.trimmed().toStdString(), port);
        emit notification("Peer Added", QString("Directed beacon sent to %1:%2").arg(ip.trimmed()).arg(port));
        emit discoveredNodesChanged();
    }
}

bool QmlBridge::createMultiNodeSession(const QString& sessionName) {
    if (projectId_.isEmpty()) {
        emit errorOccurred("Multi-Node Error", "Please create or open a project first.");
        return false;
    }
    if (!multiNodeOrchestrator_) return false;

    QString sName = sessionName.isEmpty() ? QString("Session %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm")) : sessionName;
    bool ok = multiNodeOrchestrator_->createSession(projectId_.toStdString(), sName.toStdString());
    if (ok) {
        currentSessionId_ = QString::fromStdString(multiNodeOrchestrator_->getActiveSessionId());
        multiNodeNotice_ = QString("Session %1 active as Coordinator.").arg(currentSessionId_);
        emit multiNodeSessionChanged();
        emit sessionMembersChanged();
        emit multiNodeNoticeChanged();
        emit notification("Multi-Node Session", "Created multi-node session as Coordinator.");
    }
    return ok;
}

bool QmlBridge::inviteNodeToSession(const QString& nodeUuid) {
    if (!multiNodeOrchestrator_) return false;
    bool ok = multiNodeOrchestrator_->inviteNode(nodeUuid.toStdString());
    if (ok) {
        emit notification("Invitation Sent", QString("Invited node %1 to active session.").arg(nodeUuid.left(8)));
    } else {
        emit errorOccurred("Invite Failed", QString("Could not send session invite to node %1.").arg(nodeUuid));
    }
    return ok;
}

bool QmlBridge::joinMultiNodeSession(const QString& sessionId, const QString& coordIp, int coordPort) {
    if (!multiNodeOrchestrator_) return false;
    bool ok = multiNodeOrchestrator_->joinSession(sessionId.toStdString(), coordIp.toStdString(), coordPort);
    if (ok) {
        currentSessionId_ = sessionId;
        multiNodeNotice_ = QString("Connected to session %1 as Participant.").arg(sessionId);
        emit multiNodeSessionChanged();
        emit sessionMembersChanged();
        emit multiNodeNoticeChanged();
        emit notification("Joined Session", QString("Connected to session %1 at %2:%3").arg(sessionId).arg(coordIp).arg(coordPort));
    } else {
        emit errorOccurred("Join Failed", QString("Failed to join session %1 at %2:%3").arg(sessionId).arg(coordIp).arg(coordPort));
    }
    return ok;
}

bool QmlBridge::leaveMultiNodeSession() {
    if (!multiNodeOrchestrator_) return false;
    bool ok = multiNodeOrchestrator_->leaveSession();
    multiNodeNotice_ = "Single-node standalone mode.";
    emit multiNodeSessionChanged();
    emit sessionMembersChanged();
    emit multiNodeNoticeChanged();
    return ok;
}

bool QmlBridge::startCoordinatedCapture() {
    if (!isCoordinator()) {
        emit errorOccurred("Coordinated Capture Error", "Only the SESSION_COORDINATOR can initiate coordinated capture.");
        return false;
    }
    if (!multiNodeOrchestrator_->prepareCapture()) {
        emit errorOccurred("Coordinated Capture Error", "One or more session nodes are not ready for capture.");
        return false;
    }
    if (!startCapture()) {
        return false;
    }
    return multiNodeOrchestrator_->startCapture();
}

bool QmlBridge::stopCoordinatedCapture() {
    bool ok = stopCapture();
    if (multiNodeOrchestrator_ && isCoordinator()) {
        multiNodeOrchestrator_->stopCapture();
    }
    return ok;
}

// =========================================================================
// Milestone 9: Cross-Sensor Offline Registration Implementations
// =========================================================================

QString QmlBridge::registrationTransformStr() const {
    const auto& m = activeRegistration_.transform_matrix;
    QString str;
    for (int r = 0; r < 4; ++r) {
        str += QString("[ %1, %2, %3, %4 ]\n")
            .arg(m[r][0], 8, 'f', 4)
            .arg(m[r][1], 8, 'f', 4)
            .arg(m[r][2], 8, 'f', 4)
            .arg(m[r][3], 8, 'f', 4);
    }
    return str.trimmed();
}

void QmlBridge::setRegistrationViewMode(int mode) {
    if (registrationViewMode_ != mode) {
        registrationViewMode_ = mode;
        updateRegistrationDisplayPoints();
        emit registrationViewModeChanged();
    }
}

void QmlBridge::setRegistrationColorMode(int mode) {
    if (registrationColorMode_ != mode) {
        registrationColorMode_ = mode;
        updateRegistrationDisplayPoints();
        emit registrationColorModeChanged();
    }
}

void QmlBridge::refreshSessionCaptures() {
    availableSessionCaptures_.clear();
    if (!storage_ || projectId_.isEmpty() || currentSessionId_.isEmpty()) {
        emit availableSessionCapturesChanged();
        return;
    }

    fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString() / "sessions" / currentSessionId_.toStdString();
    fs::path caps_dir = sess_dir / "captures";
    if (fs::exists(caps_dir)) {
        for (const auto& entry : fs::directory_iterator(caps_dir)) {
            if (entry.is_directory()) {
                std::string cap_id = entry.path().filename().string();
                fs::path pcd_path = entry.path() / "map.pcd";
                fs::path meta_path = entry.path() / "capture.json";
                if (fs::exists(pcd_path)) {
                    std::string sensor_id = "";
                    uint64_t point_count = 0;
                    if (fs::exists(meta_path)) {
                        try {
                            std::ifstream ifs(meta_path);
                            nlohmann::json mj = nlohmann::json::parse(ifs);
                            sensor_id = mj.value("sensor_instance_id", "");
                            point_count = mj.value("point_count", 0ULL);
                        } catch (...) {}
                    }
                    if (sensor_id.empty()) {
                        if (cap_id.find("airy") != std::string::npos) sensor_id = "robosense_airy_01";
                        else if (cap_id.find("gemini") != std::string::npos) sensor_id = "orbbec_gemini_336l_01";
                        else if (cap_id.find("viture") != std::string::npos) sensor_id = "viture_luma_ultra_01";
                        else if (cap_id.find("camera") != std::string::npos) sensor_id = "elp_48mp_camera";
                        else sensor_id = cap_id;
                    }
                    
                    QVariantMap item;
                    item["capture_id"] = QString::fromStdString(cap_id);
                    item["sensor_id"] = QString::fromStdString(sensor_id);
                    QString sensorName = QString::fromStdString(sensor_id);
                    if (sensor_id == "robosense_airy_01") sensorName = "RoboSense RS-Airy (LiDAR)";
                    else if (sensor_id == "orbbec_gemini_336l_01") sensorName = "Orbbec Gemini 336L (RGB-D)";
                    else if (sensor_id == "viture_luma_ultra_01") sensorName = "VITURE Luma Ultra (VIO)";
                    else if (sensor_id.find("camera") != std::string::npos) sensorName = "48MP USB Camera";
                    
                    item["sensor_name"] = sensorName;
                    item["point_count"] = static_cast<qlonglong>(point_count);
                    item["pcd_path"] = QString::fromStdString(pcd_path.string());
                    item["display_text"] = QString("%1 (%2 pts) - %3")
                        .arg(sensorName)
                        .arg(point_count)
                        .arg(QString::fromStdString(cap_id));
                    availableSessionCaptures_.append(item);
                }
            }
        }
    }
    emit availableSessionCapturesChanged();
}

QVariantList QmlBridge::getProjectClouds(const QString& projectId) {
    QVariantList clouds;
    if (!storage_) return clouds;

    std::string proj_id = projectId.trimmed().toStdString();
    if (proj_id.empty()) proj_id = projectId_.toStdString();
    if (proj_id.empty()) return clouds;

    fs::path repo_root = storage_->getRootDirectory();
    fs::path proj_dir = repo_root / "projects" / proj_id;
    if (!fs::exists(proj_dir)) return clouds;

    fs::path sess_base = proj_dir / "sessions";
    if (fs::exists(sess_base)) {
        for (const auto& sess_entry : fs::directory_iterator(sess_base)) {
            if (!sess_entry.is_directory()) continue;
            std::string sess_id = sess_entry.path().filename().string();

            // 1. Session map
            fs::path sess_map = sess_entry.path() / "map.pcd";
            if (fs::exists(sess_map)) {
                uint64_t count = 0;
                try {
                    std::ifstream ifs(sess_map);
                    std::string line;
                    while (std::getline(ifs, line)) {
                        if (line.rfind("POINTS ", 0) == 0) {
                            count = std::stoull(line.substr(7));
                            break;
                        }
                    }
                } catch (...) {}

                bool sess_has_rgb = false;
                try {
                    std::ifstream ifs(sess_map);
                    std::string line;
                    while (std::getline(ifs, line)) {
                        if (line.rfind("FIELDS ", 0) == 0) {
                            if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                                sess_has_rgb = true;
                            }
                            break;
                        }
                    }
                } catch (...) {}

                QVariantMap item;
                item["cloud_id"] = QString("session_map_%1").arg(QString::fromStdString(sess_id));
                item["project_id"] = QString::fromStdString(proj_id);
                item["session_id"] = QString::fromStdString(sess_id);
                item["capture_id"] = "";
                item["name"] = QString("Merged Session Map");
                item["type"] = "SESSION_MAP";
                item["point_count"] = static_cast<qlonglong>(count);
                item["pcd_path"] = QString::fromStdString(sess_map.string());
                item["has_rgb"] = sess_has_rgb;
                QString sessBadge = sess_has_rgb ? "[📷 True RGB]" : "[📡 Reflectance]";
                item["display_text"] = QString("🗺️ Merged Session Map (%1 pts) %2")
                    .arg(count)
                    .arg(sessBadge);
                clouds.append(item);
            }

            // 2. Individual sensor captures
            fs::path caps_dir = sess_entry.path() / "captures";
            if (fs::exists(caps_dir)) {
                for (const auto& cap_entry : fs::directory_iterator(caps_dir)) {
                    if (!cap_entry.is_directory()) continue;
                    std::string cap_id = cap_entry.path().filename().string();
                    fs::path cap_map = cap_entry.path() / "map.pcd";
                    fs::path meta_path = cap_entry.path() / "capture.json";
                    if (fs::exists(cap_map)) {
                        std::string sensor_id = "";
                        uint64_t count = 0;
                        if (fs::exists(meta_path)) {
                            try {
                                std::ifstream ifs(meta_path);
                                nlohmann::json mj = nlohmann::json::parse(ifs);
                                sensor_id = mj.value("sensor_instance_id", "");
                                count = mj.value("point_count", 0ULL);
                            } catch (...) {}
                        }
                        if (count == 0) {
                            try {
                                std::ifstream ifs(cap_map);
                                std::string line;
                                while (std::getline(ifs, line)) {
                                    if (line.rfind("POINTS ", 0) == 0) {
                                        count = std::stoull(line.substr(7));
                                        break;
                                    }
                                }
                            } catch (...) {}
                        }
                        if (sensor_id.empty()) {
                            if (cap_id.find("airy") != std::string::npos) sensor_id = "robosense_airy_01";
                            else if (cap_id.find("gemini") != std::string::npos) sensor_id = "orbbec_gemini_336l_01";
                            else if (cap_id.find("viture") != std::string::npos) sensor_id = "viture_luma_ultra_01";
                            else if (cap_id.find("camera") != std::string::npos) sensor_id = "elp_48mp_camera";
                            else sensor_id = cap_id;
                        }

                        bool cap_has_rgb = false;
                        try {
                            std::ifstream ifs(cap_map);
                            std::string line;
                            while (std::getline(ifs, line)) {
                                if (line.rfind("FIELDS ", 0) == 0) {
                                    if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                                        cap_has_rgb = true;
                                    }
                                    break;
                                }
                            }
                        } catch (...) {}

                        QString sensorName = QString::fromStdString(sensor_id);
                        QString icon = "📷 ";
                        if (sensor_id == "robosense_airy_01") { sensorName = "RoboSense RS-Airy (LiDAR)"; icon = "📡 "; }
                        else if (sensor_id == "orbbec_gemini_336l_01") { sensorName = "Orbbec Gemini 336L (RGB-D)"; icon = "📷 "; }
                        else if (sensor_id == "viture_luma_ultra_01") { sensorName = "VITURE Luma Ultra (VIO)"; icon = "👓 "; }
                        else if (sensor_id.find("camera") != std::string::npos) { sensorName = "48MP USB Camera"; icon = "📸 "; }

                        QString capBadge = cap_has_rgb ? "[📷 True RGB]" : "[📡 Reflectance]";

                        QVariantMap item;
                        item["cloud_id"] = QString::fromStdString(cap_id);
                        item["project_id"] = QString::fromStdString(proj_id);
                        item["session_id"] = QString::fromStdString(sess_id);
                        item["capture_id"] = QString::fromStdString(cap_id);
                        item["sensor_id"] = QString::fromStdString(sensor_id);
                        item["name"] = sensorName;
                        item["type"] = "CAPTURE";
                        item["point_count"] = static_cast<qlonglong>(count);
                        item["pcd_path"] = QString::fromStdString(cap_map.string());
                        item["has_rgb"] = cap_has_rgb;
                        item["display_text"] = QString("%1%2 (%3 pts) %4 - %5")
                            .arg(icon)
                            .arg(sensorName)
                            .arg(count)
                            .arg(capBadge)
                            .arg(QString::fromStdString(cap_id));
                        clouds.append(item);
                    }
                }
            }
        }
    }

    return clouds;
}

void QmlBridge::refreshRegistrations() {
    registrationList_.clear();
    if (!storage_) {
        emit registrationListChanged();
        return;
    }

    std::vector<core::schemas::RegistrationResult> list;
    if (!projectId_.isEmpty() && !currentSessionId_.isEmpty()) {
        list = storage_->listRegistrationResults(projectId_.toStdString(), currentSessionId_.toStdString());
    }

    // If active session list is empty, search across all projects/sessions to show recent history
    if (list.empty()) {
        fs::path repo_root = storage_->getRootDirectory();
        fs::path pdir = repo_root / "projects";
        if (fs::exists(pdir)) {
            for (const auto& pe : fs::directory_iterator(pdir)) {
                if (!pe.is_directory()) continue;
                fs::path sdir = pe.path() / "sessions";
                if (!fs::exists(sdir)) continue;
                for (const auto& se : fs::directory_iterator(sdir)) {
                    if (!se.is_directory()) continue;
                    auto sub = storage_->listRegistrationResults(pe.path().filename().string(), se.path().filename().string());
                    list.insert(list.end(), sub.begin(), sub.end());
                }
            }
        }
    }

    for (const auto& r : list) {
        QVariantMap item;
        item["registration_id"] = QString::fromStdString(r.registration_id);
        item["reference_capture_id"] = QString::fromStdString(r.reference_capture_id);
        item["moving_capture_id"] = QString::fromStdString(r.moving_capture_id);
        item["reference_project_id"] = QString::fromStdString(r.reference_project_id);
        item["moving_project_id"] = QString::fromStdString(r.moving_project_id);
        item["scale_policy"] = QString::fromStdString(core::schemas::scalePolicyToString(r.scale_policy));
        item["scale_factor"] = r.scale_factor;
        item["overlap_ratio"] = r.overlap_ratio;
        item["overlap_percent"] = r.overlap_ratio * 100.0;
        item["inlier_rmse_mm"] = r.inlier_rmse_m * 1000.0;
        item["fitness_score"] = r.fitness_score;
        item["status"] = QString::fromStdString(r.status);
        item["method"] = QString::fromStdString(r.method);
        item["created_at_ns"] = static_cast<qlonglong>(r.created_at_ns);
        item["display_text"] = QString("[%1] %2 <- %3 (%4% overlap, %5 mm RMSE)")
            .arg(QString::fromStdString(r.status))
            .arg(QString::fromStdString(r.reference_capture_id.empty() ? r.reference_project_id : r.reference_capture_id))
            .arg(QString::fromStdString(r.moving_capture_id.empty() ? r.moving_project_id : r.moving_capture_id))
            .arg(r.overlap_ratio * 100.0, 0, 'f', 1)
            .arg(r.inlier_rmse_m * 1000.0, 0, 'f', 1);
        registrationList_.append(item);
    }
    emit registrationListChanged();
}

void QmlBridge::runRegistration(const QString& refProjectId, const QString& refCloudId,
                                const QString& movProjectId, const QString& movCloudId,
                                int scalePolicyMode) {
    if (!storage_) {
        emit errorOccurred("Registration Error", "Storage repository not initialized.");
        return;
    }

    QString refPid = refProjectId.trimmed();
    if (refPid.isEmpty()) refPid = projectId_.trimmed();

    QString movPid = movProjectId.trimmed();
    if (movPid.isEmpty()) movPid = refPid;

    QString refCid = refCloudId.trimmed();
    QString movCid = movCloudId.trimmed();

    if (refCid.isEmpty() || movCid.isEmpty()) {
        emit errorOccurred("Registration Error", "Please select both a Reference cloud (Source A) and a Moving cloud (Source B).");
        return;
    }

    // Resolve ref PCD path
    fs::path ref_pcd;
    if (refCid.endsWith(".pcd") && fs::exists(refCid.toStdString())) {
        ref_pcd = refCid.toStdString();
    } else {
        auto clouds = getProjectClouds(refPid);
        for (const auto& c : clouds) {
            auto m = c.toMap();
            if (m["cloud_id"].toString() == refCid || m["pcd_path"].toString() == refCid) {
                ref_pcd = m["pcd_path"].toString().toStdString();
                break;
            }
        }
    }
    if (ref_pcd.empty() && !refPid.isEmpty()) {
        fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / refPid.toStdString() / "sessions";
        if (fs::exists(sess_dir)) {
            for (const auto& se : fs::directory_iterator(sess_dir)) {
                if (!se.is_directory()) continue;
                fs::path cand = se.path() / "captures" / refCid.toStdString() / "map.pcd";
                if (fs::exists(cand)) { ref_pcd = cand; break; }
                if (refCid.startsWith("session_map")) {
                    fs::path sm = se.path() / "map.pcd";
                    if (fs::exists(sm)) { ref_pcd = sm; break; }
                }
            }
        }
    }

    // Resolve mov PCD path
    fs::path mov_pcd;
    if (movCid.endsWith(".pcd") && fs::exists(movCid.toStdString())) {
        mov_pcd = movCid.toStdString();
    } else {
        auto clouds = getProjectClouds(movPid);
        for (const auto& c : clouds) {
            auto m = c.toMap();
            if (m["cloud_id"].toString() == movCid || m["pcd_path"].toString() == movCid) {
                mov_pcd = m["pcd_path"].toString().toStdString();
                break;
            }
        }
    }
    if (mov_pcd.empty() && !movPid.isEmpty()) {
        fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / movPid.toStdString() / "sessions";
        if (fs::exists(sess_dir)) {
            for (const auto& se : fs::directory_iterator(sess_dir)) {
                if (!se.is_directory()) continue;
                fs::path cand = se.path() / "captures" / movCid.toStdString() / "map.pcd";
                if (fs::exists(cand)) { mov_pcd = cand; break; }
                if (movCid.startsWith("session_map")) {
                    fs::path sm = se.path() / "map.pcd";
                    if (fs::exists(sm)) { mov_pcd = sm; break; }
                }
            }
        }
    }

    if (!fs::exists(ref_pcd)) {
        emit errorOccurred("Registration Error", QString("Reference point cloud not found: %1").arg(refCid));
        return;
    }
    if (!fs::exists(mov_pcd)) {
        emit errorOccurred("Registration Error", QString("Moving point cloud not found: %1").arg(movCid));
        return;
    }
    if (ref_pcd == mov_pcd) {
        emit errorOccurred("Registration Error", "Reference and Moving clouds cannot be the exact same point cloud.");
        return;
    }

    isRegistering_ = true;
    registrationStatus_ = "Running Multi-Scale SAC-IA Coarse & GICP Fine Registration...";
    emit registrationStateChanged();

    core::registration::RegistrationParams params;
    params.scale_policy = (scalePolicyMode == 1) ? core::ScalePolicy::ESTIMATED_SIM3 : core::ScalePolicy::LOCKED_METRIC_SE3;
    params.enable_color_differentiation = true;

    auto res = registrationEngine_->registerClouds(ref_pcd.string(), mov_pcd.string(), params);

    uint64_t now_ms = QDateTime::currentMSecsSinceEpoch();
    res.registration_id = "reg_" + std::to_string(now_ms);
    res.reference_project_id = refPid.toStdString();
    res.reference_capture_id = refCid.toStdString();
    res.reference_pcd_path = ref_pcd.string();
    res.moving_project_id = movPid.toStdString();
    res.moving_capture_id = movCid.toStdString();
    res.moving_pcd_path = mov_pcd.string();

    // Determine target save session
    std::string save_pid = (!projectId_.isEmpty()) ? projectId_.toStdString() : refPid.toStdString();
    std::string save_sid = (!currentSessionId_.isEmpty()) ? currentSessionId_.toStdString() : "";
    if (save_sid.empty()) {
        auto sess_list = storage_->listSessions(save_pid);
        if (!sess_list.empty()) save_sid = sess_list.front();
        else save_sid = "session_" + std::to_string(now_ms);
    }
    res.project_id = save_pid;
    res.session_id = save_sid;

    Eigen::Matrix4f T_mat;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            T_mat(r, c) = static_cast<float>(res.transform_matrix[r][c]);
        }
    }

    fs::path reg_dir = fs::path(storage_->getRootDirectory()) / "projects" / save_pid / "sessions" / save_sid / "registrations";
    fs::create_directories(reg_dir);
    res.output_combined_pcd_path = (reg_dir / (res.registration_id + "_combined.pcd")).string();
    res.output_combined_diff_pcd_path = ""; // Only one merged PCD on disk; cyan/amber is strictly in-memory
    res.output_moving_aligned_pcd_path = (reg_dir / (res.registration_id + "_mov_aligned.pcd")).string();

    registrationEngine_->generateColoredMergedPCD(ref_pcd.string(), mov_pcd.string(), T_mat,
                                                  res.output_combined_pcd_path,
                                                  res.output_moving_aligned_pcd_path);

    storage_->saveRegistrationResult(res.project_id, res.session_id, res);

    activeRegistration_ = res;
    hasActiveRegistration_ = true;

    // Load point buffers for interactive GUI inspection
    loadMapPcd(ref_pcd.string(), regRefPoints_);
    loadMapPcd(res.output_moving_aligned_pcd_path, regMovPoints_);
    loadMapPcd(res.output_combined_pcd_path, regCombinedPointsOrig_);

    // Generate transient in-memory cyan/amber diagnostic overlay for visual inspection during merging
    // Cloud 1 (Reference Anchor): Cyan #00E5FF
    // Cloud 2 (Moving Aligned): Amber #FF9100
    // NOTE: This overlay exists ONLY in RAM for UI verification and is NEVER saved to disk.
    regCombinedPointsDiff_.clear();
    regCombinedPointsDiff_.reserve(regRefPoints_.size() + regMovPoints_.size());
    for (auto pt : regRefPoints_) {
        pt.r = 0; pt.g = 229; pt.b = 255; // Cyan #00E5FF
        regCombinedPointsDiff_.push_back(pt);
    }
    for (auto pt : regMovPoints_) {
        pt.r = 255; pt.g = 145; pt.b = 0; // Amber #FF9100
        regCombinedPointsDiff_.push_back(pt);
    }

    // Default to differentiated Cyan/Amber during the merging process so user knows which cloud is which
    registrationColorMode_ = 0;
    updateRegistrationDisplayPoints();

    isRegistering_ = false;
    registrationStatus_ = QString("Alignment %1: %2% overlap, %3 mm RMSE")
        .arg(QString::fromStdString(res.status))
        .arg(res.overlap_ratio * 100.0, 0, 'f', 1)
        .arg(res.inlier_rmse_m * 1000.0, 0, 'f', 1);

    emit registrationStateChanged();
    emit activeRegistrationChanged();
    refreshRegistrations();
    emit notification("Registration Complete", registrationStatus_);
}

void QmlBridge::runCaptureRegistration(const QString& refCaptureId, const QString& movCaptureId, int scalePolicyMode) {
    runRegistration(projectId_, refCaptureId, projectId_, movCaptureId, scalePolicyMode);
}

bool QmlBridge::saveRegistrationAsNewProject(const QString& newProjectName, const QString& newProjectDesc) {
    if (!storage_) {
        emit errorOccurred("Create Project Error", "Storage repository not initialized.");
        return false;
    }
    if (!hasActiveRegistration_ || activeRegistration_.output_combined_pcd_path.empty()) {
        emit errorOccurred("Create Project Error", "No active registered point cloud to create project from.");
        return false;
    }

    QString name = newProjectName.trimmed();
    if (name.isEmpty()) {
        name = QString("Merged Registration (%1 pts)").arg(pointsCaptured_);
    }

    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string pid = "proj_" + std::to_string(now_ms);
    std::string sess_id = "session_" + std::to_string(now_ms);

    core::schemas::Project proj(pid, name.toStdString(), "", newProjectDesc.toStdString());
    proj.set_created_at_ns(now_ms * 1000000ULL);
    proj.set_active_session_id(sess_id);
    proj.add_session(sess_id);

    nlohmann::json meta;
    meta["created_from_registration"] = true;
    meta["registration_id"] = activeRegistration_.registration_id;
    meta["reference_project_id"] = activeRegistration_.reference_project_id;
    meta["reference_capture_id"] = activeRegistration_.reference_capture_id;
    meta["moving_project_id"] = activeRegistration_.moving_project_id;
    meta["moving_capture_id"] = activeRegistration_.moving_capture_id;
    meta["overlap_ratio"] = activeRegistration_.overlap_ratio;
    meta["inlier_rmse_m"] = activeRegistration_.inlier_rmse_m;
    meta["fitness_score"] = activeRegistration_.fitness_score;
    proj.set_metadata(meta);

    if (!storage_->saveProject(proj)) {
        emit errorOccurred("Create Project Error", "Failed to save new project to storage repository.");
        return false;
    }

    core::schemas::Session sess(sess_id, pid, "Merged Cross-Registration Session");
    sess.set_start_time_ns(now_ms * 1000000ULL);
    storage_->saveSession(sess);

    // Copy combined registered PCD as the primary session map, project root map, and canonical capture
    fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / pid / "sessions" / sess_id;
    fs::create_directories(sess_dir);
    fs::path map_dest = sess_dir / "map.pcd";
    try {
        fs::copy_file(activeRegistration_.output_combined_pcd_path, map_dest, fs::copy_options::overwrite_existing);

        // Also copy to root project directory as map.pcd
        fs::path root_map_dest = fs::path(storage_->getRootDirectory()) / "projects" / pid / "map.pcd";
        fs::copy_file(activeRegistration_.output_combined_pcd_path, root_map_dest, fs::copy_options::overwrite_existing);

        // Create a canonical capture entry under sessions/<sess_id>/captures/cap_<sess_id>_merged/
        std::string cap_id = "cap_" + sess_id + "_merged";
        fs::path cap_dir = sess_dir / "captures" / cap_id;
        fs::create_directories(cap_dir);
        fs::copy_file(activeRegistration_.output_combined_pcd_path, cap_dir / "map.pcd", fs::copy_options::overwrite_existing);

        core::schemas::Capture cap;
        cap.set_capture_id(cap_id);
        cap.set_session_id(sess_id);
        cap.set_sensor_instance_id("merged_multisensor_01");
        cap.set_observation_count(regCombinedPointsOrig_.size() > 0 ? regCombinedPointsOrig_.size() : pointsCaptured_);
        cap.set_start_timestamp_ns(now_ms * 1000000ULL);
        cap.set_end_timestamp_ns(now_ms * 1000000ULL);
        cap.set_status("COMPLETED");
        storage_->saveCapture(cap);

        std::ofstream c_out(cap_dir / "capture.json");
        if (c_out.is_open()) {
            c_out << cap.to_json().dump(2);
        }
    } catch (const std::exception& e) {
        emit errorOccurred("Create Project Error", QString("Failed to copy combined PCD: %1").arg(e.what()));
        return false;
    }

    // Persist registration record into the new project as well
    activeRegistration_.project_id = pid;
    activeRegistration_.session_id = sess_id;
    storage_->saveRegistrationResult(pid, sess_id, activeRegistration_);

    // Immediately load and activate the new project in AV Scan
    openProject(QString::fromStdString(pid));
    refreshData();
    emit notification("Merged Project Created", QString("Project '%1' created with %2 merged points and is now active.").arg(name).arg(pointsCaptured_));
    return true;
}

void QmlBridge::loadRegistration(const QString& regId) {
    if (!storage_ || regId.isEmpty()) return;

    core::schemas::RegistrationResult res;
    bool loaded = false;
    if (!projectId_.isEmpty() && !currentSessionId_.isEmpty()) {
        loaded = storage_->loadRegistrationResult(projectId_.toStdString(), currentSessionId_.toStdString(), regId.toStdString(), res);
    }
    if (!loaded) {
        fs::path repo_root = storage_->getRootDirectory();
        fs::path pdir = repo_root / "projects";
        if (fs::exists(pdir)) {
            for (const auto& pe : fs::directory_iterator(pdir)) {
                if (!pe.is_directory()) continue;
                fs::path sdir = pe.path() / "sessions";
                if (!fs::exists(sdir)) continue;
                for (const auto& se : fs::directory_iterator(sdir)) {
                    if (!se.is_directory()) continue;
                    if (storage_->loadRegistrationResult(pe.path().filename().string(), se.path().filename().string(), regId.toStdString(), res)) {
                        loaded = true;
                        break;
                    }
                }
                if (loaded) break;
            }
        }
    }

    if (!loaded) {
        emit errorOccurred("Load Error", "Failed to locate registration record: " + regId);
        return;
    }

    activeRegistration_ = res;
    hasActiveRegistration_ = true;

    fs::path ref_pcd = res.reference_pcd_path;
    if (ref_pcd.empty() || !fs::exists(ref_pcd)) {
        fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / res.project_id / "sessions" / res.session_id;
        ref_pcd = sess_dir / "captures" / res.reference_capture_id / "map.pcd";
    }

    std::string rgb_combined = res.output_combined_pcd_path;

    loadMapPcd(ref_pcd.string(), regRefPoints_);
    loadMapPcd(res.output_moving_aligned_pcd_path, regMovPoints_);
    loadMapPcd(rgb_combined, regCombinedPointsOrig_);

    // Generate transient in-memory cyan/amber diagnostic overlay for visual inspection
    regCombinedPointsDiff_.clear();
    regCombinedPointsDiff_.reserve(regRefPoints_.size() + regMovPoints_.size());
    for (auto pt : regRefPoints_) {
        pt.r = 0; pt.g = 229; pt.b = 255; // Cyan #00E5FF
        regCombinedPointsDiff_.push_back(pt);
    }
    for (auto pt : regMovPoints_) {
        pt.r = 255; pt.g = 145; pt.b = 0; // Amber #FF9100
        regCombinedPointsDiff_.push_back(pt);
    }

    updateRegistrationDisplayPoints();

    registrationStatus_ = QString("Loaded %1: %2% overlap, %3 mm RMSE")
        .arg(regId)
        .arg(res.overlap_ratio * 100.0, 0, 'f', 1)
        .arg(res.inlier_rmse_m * 1000.0, 0, 'f', 1);

    emit registrationStateChanged();
    emit activeRegistrationChanged();
    emit notification("Registration Loaded", registrationStatus_);
}

void QmlBridge::updateRegistrationDisplayPoints() {
    if (!hasActiveRegistration_) return;

    if (registrationViewMode_ == 0) {
        // Combined
        if (registrationColorMode_ == 0) {
            // Differentiated (Cyan/Amber)
            mapPoints_ = regCombinedPointsDiff_;
        } else {
            // Original sensor colors
            mapPoints_ = regCombinedPointsOrig_;
        }
    } else if (registrationViewMode_ == 1) {
        // Reference Only
        mapPoints_ = regRefPoints_;
    } else if (registrationViewMode_ == 2) {
        // Moving Only
        mapPoints_ = regMovPoints_;
    }

    pointsCaptured_ = mapPoints_.size();
    hasMapData_ = (!mapPoints_.empty());
    emit pointsCapturedChanged();
    emit mapDataUpdated();
}

void QmlBridge::saveRegistrationAsSessionMap(const QString& regId) {
    (void)regId;
    if (!hasActiveRegistration_ || activeRegistration_.output_combined_pcd_path.empty()) {
        emit errorOccurred("Save Error", "No active registered point cloud to save.");
        return;
    }
    fs::path sess_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString() / "sessions" / currentSessionId_.toStdString();
    fs::path session_map = sess_dir / "map.pcd";
    try {
        fs::copy_file(activeRegistration_.output_combined_pcd_path, session_map, fs::copy_options::overwrite_existing);
        emit notification("Session Map Updated", "Combined registered cloud set as active session map.");
    } catch (const std::exception& e) {
        emit errorOccurred("Save Error", QString("Failed to save session map: %1").arg(e.what()));
    }
}

void QmlBridge::clearActiveRegistration() {
    hasActiveRegistration_ = false;
    for (const auto& s : activeSensors_) {
        if (s.id == activeDisplaySensor_.toStdString()) {
            mapPoints_ = s.map_points;
            trajectory_ = s.trajectory;
            pointsCaptured_ = mapPoints_.size();
            hasMapData_ = (!mapPoints_.empty());
            break;
        }
    }
    emit activeRegistrationChanged();
    emit pointsCapturedChanged();
    emit mapDataUpdated();
}

} // namespace av::gui
