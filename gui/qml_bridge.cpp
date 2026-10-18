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
            emit cslamStateChanged();
        });
        multiNodeOrchestrator_->setCslamUpdatedHandler([this]() {
            QMetaObject::invokeMethod(this, [this]() {
                if (selectedNodeViewIndex_ == -1) {
                    refreshCslamMergedCloud();
                } else {
                    emit cslamStateChanged();
                }
            }, Qt::QueuedConnection);
        });
    }

    connect(timer_, &QTimer::timeout, this, &QmlBridge::onTimerTick);
    connect(telemetryTimer_, &QTimer::timeout, this, &QmlBridge::onTelemetryTick);
    connect(this, &QmlBridge::mapDataUpdated, this, [this]() { invalidateDisplayPointsCache(); });
    connect(this, &QmlBridge::orientationDiagChanged, this, [this]() { invalidateDisplayPointsCache(); });
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
            if (cfg.extrinsic_status() == core::schemas::ExtrinsicStatus::VALIDATED ||
                cfg.extrinsic_status() == core::schemas::ExtrinsicStatus::ESTIMATED ||
                cfg.extrinsic_status() == core::schemas::ExtrinsicStatus::FACTORY_KNOWN) {
                QString extStat = QString::fromStdString(core::schemas::to_string(cfg.extrinsic_status()));
                item["validationNotice"] = QString("Modular Scanner Configuration — Extrinsics %1 (Rigid SE(3) Calibrated | Method: %2)")
                    .arg(extStat)
                    .arg(QString::fromStdString(cfg.method().empty() ? "TRAJECTORY_HAND_EYE_GICP_SE3" : cfg.method()));
            } else {
                item["validationNotice"] = "Modular Scanner Configuration — Extrinsics UNKNOWN (Independent SLAM Pipelines)";
            }
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
            } else if (prof.profile_id() == "scanar_01" || prof.device_family() == "HYBRID_LIDAR_STEREO_SCANNER") {
                item["validationNotice"] = "scanarMini scanAR: RoboSense Airy (Internal Airy 6-DoF IMU) + Orbbec Gemini 336L Stereo Camera (RGB Colorization & Short-Range Dense Stereo Cloud)";
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
        bool is_healthy = s.health_override_active ? s.health_override_healthy :
            (s.tracking_status != "LOST" && s.tracking_status != "OFFLINE" && s.device_status != "DISCONNECTED");
        bool is_degraded = s.health_override_active ? s.health_override_degraded :
            (s.tracking_status == "DEGRADED" || s.tracking_status == "RELOCALIZING");
        m["isHealthy"] = is_healthy;
        m["isDegraded"] = is_degraded;
        m["accuracyScore"] = s.accuracy_score;
        m["isAnchor"] = s.is_anchor || (activeAnchorSensor_ == QString::fromStdString(s.id));
        m["active"] = (activeDisplaySensor_ == "FUSED" || activeDisplaySensor_ == QString::fromStdString(s.id));
        list.append(m);
    }
    return list;
}

void QmlBridge::rebuildActiveSensorsForSelection() {
    activeSensors_.clear();

    std::vector<std::string> instance_ids;

    // Check if selected scanner is a ScannerConfig in storage or the composite scanAR profile
    core::schemas::ScannerConfig config;
    if (selectedScannerId_ == "scanar_01" || selectedScannerId_ == "scanar") {
        instance_ids = {"robosense_airy_01", "orbbec_gemini_336l_01"};
    } else if (storage_ && storage_->loadScannerConfig(selectedScannerId_.toStdString(), config)) {
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

        ctx.accuracy_score = core::fusion::MultiSensorFusionEngine::computeNominalAccuracyScore(
            ctx.id, ctx.scale_status == "METRIC_VERIFIED");
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
        ctx.accuracy_score = 100.0;
        ctx.is_anchor = true;
        ctx.map_points = airyMapPoints_;
        ctx.trajectory = airyTrajectory_;
        ctx.point_count = airyPointsCaptured_;
        activeSensors_.push_back(ctx);
    }

    isDualSensorMode_ = (activeSensors_.size() > 1);
    if (isDualSensorMode_) {
        activeDisplaySensor_ = "FUSED";
        updateLiveMultiSensorFusion(true);
    } else {
        activeDisplaySensor_ = QString::fromStdString(activeSensors_.front().id);
        activeAnchorSensor_ = activeDisplaySensor_;
        activeAnchorName_ = QString::fromStdString(activeSensors_.front().short_name);
        activeAnchorScore_ = activeSensors_.front().accuracy_score;
        isAnchorFailoverActive_ = false;
        activeAnchorRationale_ = "Primary single-sensor anchor (" + activeAnchorName_ + ").";
        activeSensors_.front().is_anchor = true;
    }

    if (activeSensors_.size() == 1 && !activeSensors_.front().is_capturable) {
        selectedScannerValidationNotice_ = "USB serial interfaces detected | Imaging stream not established | Mapping backend: None | Not currently available for spatial capture";
        selectedScannerStatus_ = "PROFILED";
        selectedScannerDetails_ = QString::fromStdString(activeSensors_.front().transport + " (Mapping Backend: None)");
        sensorStatus_ = QString::fromStdString(activeSensors_.front().name + " [PROFILED — Backend: None]");
    }

    emit activeSensorsChanged();
    emit isDualSensorModeChanged();
    emit activeDisplaySensorChanged();
    emit fusionStateChanged();
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
    hasCleanupPreview_ = false;
    preCleanupBackupPoints_.clear();

    if (sensorName == "FUSED" && !fusedMapPoints_.empty()) {
        mapPoints_ = fusedMapPoints_;
        trajectory_ = fusedTrajectory_;
        if (!trajectory_.empty()) {
            const auto& last = trajectory_.back();
            currentPoseX_ = last.position.x;
            currentPoseY_ = last.position.y;
            currentPoseZ_ = last.position.z;
        }
        hasMapData_ = true;
        emit activeDisplaySensorChanged();
        emit cleanupStateChanged();
        emit mapDataUpdated();
        emit trajectoryChanged();
        emit poseChanged();
        return;
    }

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
    emit cleanupStateChanged();
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
            if (lower == "scanar_01" || prof.device_family() == "HYBRID_LIDAR_STEREO_SCANNER") {
                item["sensorType"] = "Airy 3D LiDAR (Internal IMU) + Gemini 336L Stereo RGB-D";
                item["serialNumber"] = "SCANAR-MINI-001";
                item["icon"] = "🛰️";
                item["modality"] = "HYBRID_LIDAR_STEREO";
                item["tag"] = "scanAR Hybrid";
                item["tagColor"] = "#10b981";
            } else if (lower.find("airy") != std::string::npos || lower.find("robosense") != std::string::npos) {
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

        // Reset temporary registration inspection overlay state on opening a regular project
        hasActiveRegistration_ = false;
        hasCleanupPreview_ = false;
        preCleanupBackupPoints_.clear();
        isFusionActive_ = false;
        fusedMapPoints_.clear();
        fusedTrajectory_.clear();

        if (!activeSensors_.empty()) {
            for (auto& s : activeSensors_) {
                s.accuracy_score = core::fusion::MultiSensorFusionEngine::computeNominalAccuracyScore(
                    s.id, s.scale_status == "METRIC_VERIFIED");
            }
            isDualSensorMode_ = (activeSensors_.size() > 1);
            if (isDualSensorMode_) {
                activeDisplaySensor_ = "FUSED";
                updateLiveMultiSensorFusion(true);
            } else {
                activeDisplaySensor_ = QString::fromStdString(activeSensors_.front().id);
                mapPoints_ = activeSensors_.front().map_points;
                trajectory_ = activeSensors_.front().trajectory;
                pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
                hasMapData_ = (!mapPoints_.empty());
                activeAnchorSensor_ = activeDisplaySensor_;
                activeAnchorName_ = QString::fromStdString(activeSensors_.front().short_name);
                activeAnchorScore_ = activeSensors_.front().accuracy_score;
                activeSensors_.front().is_anchor = true;
                isAnchorFailoverActive_ = false;
                activeAnchorRationale_ = "Primary single-sensor anchor (" + activeAnchorName_ + ").";
            }

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

        emit activeRegistrationChanged();
        emit cleanupStateChanged();
        emit fusionStateChanged();

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
        s.raw_map_points.clear();
        s.trajectory.clear();
        s.point_count = 0;
    }
    liveCleanupTimeMs_ = 0.0;
    emit cleanupStateChanged();

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

    core::processing::CleanupParams liveParams;
    liveParams.enable_voxel_downsample = true;
    liveParams.voxel_leaf_size_m = 0.025f;
    liveParams.enable_ror = true;
    liveParams.ror_radius_m = 0.12;
    liveParams.ror_min_neighbors = 3;
    liveParams.enable_sor = true;
    liveParams.sor_mean_k = 12;
    liveParams.sor_std_mul = 2.0;
    liveParams.enable_ground_alignment = liveGroundLockEnabled_;
    liveParams.align_ground_to_z_zero = liveGroundLockEnabled_;
    liveParams.remove_ground_plane = false;

    uint64_t total_points = 0;
    for (auto& s : activeSensors_) {
        if (s.id == "AIRY") {
            if (airyBackend_) {
                airyBackend_->stop();
                s.raw_map_points = airyBackend_->getMapPoints();
                s.trajectory = airyBackend_->getTrajectory();
            }
            if (airyAdapter_) airyAdapter_->stop();
            if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
            } else {
                s.map_points = s.raw_map_points;
            }
            airyMapPoints_ = s.map_points;
            airyTrajectory_ = s.trajectory;
            airyPointsCaptured_ = s.map_points.size();
            airyDeviceStatus_ = "STANDBY";
            airyAcquisitionStatus_ = "STOPPED";
            airyTrackingStatus_ = "IDLE";
        } else if (s.id == "GEMINI") {
            if (geminiBackend_) {
                geminiBackend_->stop();
                s.raw_map_points = geminiBackend_->getMapPoints();
                s.trajectory = geminiBackend_->getTrajectory();
            }
            if (geminiAdapter_) geminiAdapter_->stop();
            if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
            } else {
                s.map_points = s.raw_map_points;
            }
            geminiMapPoints_ = s.map_points;
            geminiTrajectory_ = s.trajectory;
            geminiPointsCaptured_ = s.map_points.size();
            geminiDeviceStatus_ = "STANDBY";
            geminiAcquisitionStatus_ = "STOPPED";
            geminiTrackingStatus_ = "IDLE";
        } else if (s.id == "VITURE") {
            if (vitureBackend_) {
                vitureBackend_->stop();
                s.raw_map_points = vitureBackend_->getMapPoints();
                s.trajectory = vitureBackend_->getTrajectory();
            }
            if (vitureAdapter_) vitureAdapter_->stop();
            if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
            } else {
                s.map_points = s.raw_map_points;
            }
            vitureMapPoints_ = s.map_points;
            vitureTrajectory_ = s.trajectory;
            viturePointsCaptured_ = s.map_points.size();
            vitureDeviceStatus_ = "STANDBY";
            vitureAcquisitionStatus_ = "STOPPED";
            vitureTrackingStatus_ = "IDLE";
        } else if (s.id == "CAMERA") {
            if (cameraBackend_) {
                cameraBackend_->stop();
                s.raw_map_points = cameraBackend_->getMapPoints();
                s.trajectory = cameraBackend_->getTrajectory();
            }
            if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
            } else {
                s.map_points = s.raw_map_points;
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

    if (activeSensors_.size() > 1) {
        activeDisplaySensor_ = "FUSED";
        updateLiveMultiSensorFusion(true);
    } else {
        for (const auto& s : activeSensors_) {
            if (s.id == activeDisplaySensor_.toStdString()) {
                mapPoints_ = s.map_points;
                trajectory_ = s.trajectory;
                break;
            }
        }
    }

    if (multiNodeOrchestrator_ && !mapPoints_.empty() &&
        (multiNodeOrchestrator_->isSessionActive() || multiNodeOrchestrator_->cslam().scannerCount() > 0)) {
        core::schemas::Quaternion imu_q{0.0, 0.0, 0.0, 1.0};
        QString leadId = (activeSensors_.size() > 1) ? activeAnchorSensor_ : activeDisplaySensor_;
        if (airyBackend_ && leadId == "AIRY") {
            imu_q = airyBackend_->getPose().orientation;
        } else if (geminiBackend_ && leadId == "GEMINI") {
            imu_q = geminiBackend_->getPose().orientation;
        } else if (vitureBackend_ && leadId == "VITURE") {
            imu_q = vitureBackend_->getPose().orientation;
        } else if (cameraBackend_ && leadId == "CAMERA") {
            imu_q = cameraBackend_->getPose().orientation;
        }
        multiNodeOrchestrator_->publishLocalSubmap(mapPoints_, trajectory_, imu_q, true);
        if (selectedNodeViewIndex_ == -1) {
            mapPoints_ = multiNodeOrchestrator_->getMergedCslamPointCloud(cslamColorByScanner_);
            trajectory_ = multiNodeOrchestrator_->cslam().getMergedTrajectory();
        }
        emit cslamStateChanged();
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

            // Save map.pcd (and non-destructive raw backup if live cleanup was active)
            fs::path pcd_path = cap_dir / "map.pcd";
            saveMapPcd(pcd_path.string(), s.map_points);
            if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                saveMapPcd((cap_dir / "map_raw_backup.pcd").string(), s.raw_map_points);
            }

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

    core::processing::CleanupParams liveParams;
    liveParams.enable_voxel_downsample = true;
    liveParams.voxel_leaf_size_m = 0.025f;
    liveParams.enable_ror = true;
    liveParams.ror_radius_m = 0.12;
    liveParams.ror_min_neighbors = 3;
    liveParams.enable_sor = true;
    liveParams.sor_mean_k = 12;
    liveParams.sor_std_mul = 2.0;
    liveParams.enable_ground_alignment = liveGroundLockEnabled_;
    liveParams.align_ground_to_z_zero = liveGroundLockEnabled_;
    liveParams.remove_ground_plane = false;

    for (auto& s : activeSensors_) {
        if (s.id == "AIRY" && airyBackend_) {
            if (airyBackend_->updateTelemetry()) {
                any_updated = true;
                s.tracking_status = core::schemas::to_string(airyBackend_->getTrackingState());
                s.acquisition_rate = airyBackend_->getAcquisitionRate();
                s.slam_rate = airyBackend_->getSlamRate();
                if (airyAdapter_) {
                    s.device_status = core::schemas::to_string(airyAdapter_->getStatus());
                }
                airyBackend_->fetchNewPoints();
                s.raw_map_points = airyBackend_->getMapPoints();
                if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                    auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                    liveCleanupTimeMs_ = st.computation_time_ms;
                    if (s.map_points.empty()) s.map_points = s.raw_map_points;
                    emit cleanupStateChanged();
                } else {
                    s.map_points = s.raw_map_points;
                }
                s.point_count = s.map_points.size() > 0 ? s.map_points.size() : airyBackend_->getPointCount();
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
                s.tracking_status = core::schemas::to_string(geminiBackend_->getTrackingState());
                s.acquisition_rate = geminiBackend_->getAcquisitionRate();
                s.slam_rate = geminiBackend_->getSlamRate();
                if (geminiAdapter_) {
                    s.device_status = core::schemas::to_string(geminiAdapter_->getStatus());
                }
                geminiBackend_->fetchNewPoints();
                s.raw_map_points = geminiBackend_->getMapPoints();
                if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                    auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                    liveCleanupTimeMs_ = st.computation_time_ms;
                    if (s.map_points.empty()) s.map_points = s.raw_map_points;
                    emit cleanupStateChanged();
                } else {
                    s.map_points = s.raw_map_points;
                }
                s.point_count = s.map_points.size() > 0 ? s.map_points.size() : geminiBackend_->getPointCount();
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
                s.tracking_status = core::schemas::to_string(vitureBackend_->getTrackingState());
                s.acquisition_rate = vitureBackend_->getAcquisitionRate();
                s.slam_rate = vitureBackend_->getSlamRate();
                if (vitureAdapter_) {
                    s.device_status = core::schemas::to_string(vitureAdapter_->getStatus());
                }
                vitureBackend_->fetchNewPoints();
                s.raw_map_points = vitureBackend_->getMapPoints();
                if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                    auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                    liveCleanupTimeMs_ = st.computation_time_ms;
                    if (s.map_points.empty()) s.map_points = s.raw_map_points;
                    emit cleanupStateChanged();
                } else {
                    s.map_points = s.raw_map_points;
                }
                s.point_count = s.map_points.size() > 0 ? s.map_points.size() : vitureBackend_->getPointCount();
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
                s.tracking_status = core::schemas::to_string(cameraBackend_->getTrackingState());
                s.acquisition_rate = cameraBackend_->getAcquisitionRate();
                s.slam_rate = cameraBackend_->getSlamRate();
                s.device_status = "CONNECTED";
                cameraBackend_->fetchNewPoints();
                s.raw_map_points = cameraBackend_->getMapPoints();
                if (liveCleanupEnabled_ && !s.raw_map_points.empty()) {
                    auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(s.raw_map_points, s.map_points, liveParams);
                    liveCleanupTimeMs_ = st.computation_time_ms;
                    if (s.map_points.empty()) s.map_points = s.raw_map_points;
                    emit cleanupStateChanged();
                } else {
                    s.map_points = s.raw_map_points;
                }
                s.point_count = s.map_points.size() > 0 ? s.map_points.size() : cameraBackend_->getPointCount();
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

    emit hostMetricsChanged();
    if (!any_updated) {
        return;
    }

    pointsCaptured_ = total_points;
    core::schemas::Quaternion active_imu_q{0.0, 0.0, 0.0, 1.0};

    if (activeSensors_.size() > 1) {
        activeDisplaySensor_ = "FUSED";
        updateLiveMultiSensorFusion(true);
        sensorStatus_ = QString("SCANNER NODE: %1 (%2 Overlapped | Anchor: %3)")
            .arg(selectedScannerId_.isEmpty() ? "scanar-01" : selectedScannerId_)
            .arg(activeSensors_.size())
            .arg(activeAnchorName_);
        trackingStatus_ = trackSummaries.join(" | ");
    } else if (!activeSensors_.empty()) {
        sensorStatus_ = QString::fromStdString(activeSensors_.front().name);
        trackingStatus_ = QString::fromStdString(activeSensors_.front().tracking_status);
        mapPoints_ = activeSensors_.front().map_points;
        trajectory_ = activeSensors_.front().trajectory;
        activeAnchorSensor_ = QString::fromStdString(activeSensors_.front().id);
        activeAnchorName_ = QString::fromStdString(activeSensors_.front().short_name);
    }

    // Route live 6-DoF pose and IMU orientation from the active anchor sensor
    QString poseLeadId = (activeSensors_.size() > 1) ? activeAnchorSensor_ : activeDisplaySensor_;
    for (const auto& s : activeSensors_) {
        if (s.id == poseLeadId.toStdString()) {
            if (activeSensors_.size() == 1) {
                mapPoints_ = s.map_points;
                trajectory_ = s.trajectory;
            }
            if (s.id == "AIRY" && airyBackend_) {
                currentPoseX_ = airyBackend_->getPose().position.x;
                currentPoseY_ = airyBackend_->getPose().position.y;
                currentPoseZ_ = airyBackend_->getPose().position.z;
                currentYaw_ = airyBackend_->getCurrentYaw();
                active_imu_q = airyBackend_->getPose().orientation;
            } else if (s.id == "GEMINI" && geminiBackend_) {
                currentPoseX_ = geminiBackend_->getPose().position.x;
                currentPoseY_ = geminiBackend_->getPose().position.y;
                currentPoseZ_ = geminiBackend_->getPose().position.z;
                currentYaw_ = geminiBackend_->getCurrentYaw();
                active_imu_q = geminiBackend_->getPose().orientation;
            } else if (s.id == "VITURE" && vitureBackend_) {
                currentPoseX_ = vitureBackend_->getPose().position.x;
                currentPoseY_ = vitureBackend_->getPose().position.y;
                currentPoseZ_ = vitureBackend_->getPose().position.z;
                currentYaw_ = vitureBackend_->getCurrentYaw();
                active_imu_q = vitureBackend_->getPose().orientation;
            } else if (s.id == "CAMERA" && cameraBackend_) {
                currentPoseX_ = cameraBackend_->getPose().position.x;
                currentPoseY_ = cameraBackend_->getPose().position.y;
                currentPoseZ_ = cameraBackend_->getPose().position.z;
                currentYaw_ = cameraBackend_->getCurrentYaw();
                active_imu_q = cameraBackend_->getPose().orientation;
            }
            break;
        }
    }

    // Real-Time Collaborative SLAM (C-SLAM) submap streaming & merged viewport
    if (multiNodeOrchestrator_ && !mapPoints_.empty() &&
        (multiNodeOrchestrator_->isSessionActive() || multiNodeOrchestrator_->cslam().scannerCount() > 0)) {
        multiNodeOrchestrator_->publishLocalSubmap(mapPoints_, trajectory_, active_imu_q, false);
        if (selectedNodeViewIndex_ == -1) {
            mapPoints_ = multiNodeOrchestrator_->getMergedCslamPointCloud(cslamColorByScanner_);
            trajectory_ = multiNodeOrchestrator_->cslam().getMergedTrajectory();
            pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
        }
        emit cslamStateChanged();
    }

    hasMapData_ = (!mapPoints_.empty());
    invalidateDisplayPointsCache();

    emit poseChanged();
    emit trackingStatusChanged();
    emit sensorStatusChanged();
    emit pointsCapturedChanged();
    emit trajectoryChanged();
    emit mapDataUpdated();
}

QVariantList QmlBridge::getDisplayPoints(int maxPoints) const {
    if (mapPoints_.empty()) {
        return QVariantList();
    }
    if (displayPointsCacheValid_ &&
        cachedMaxPoints_ == maxPoints &&
        cachedMapPointsSize_ == mapPoints_.size()) {
        return cachedDisplayPoints_;
    }

    QVariantList list;
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

    cachedDisplayPoints_ = list;
    cachedMaxPoints_ = maxPoints;
    cachedMapPointsSize_ = total;
    displayPointsCacheValid_ = true;
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
                invalidateDisplayPointsCache();
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

    std::string chunk;
    chunk.reserve(65536);
    char line_buf[128];

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
            int n = std::snprintf(line_buf, sizeof(line_buf), "%.5g %.5g %.5g %u %.4g\n",
                                  pt.x, pt.y, pt.z, rgb_packed, pt.intensity);
            if (n > 0) {
                chunk.append(line_buf, static_cast<size_t>(n));
                if (chunk.size() >= 60000) {
                    ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                    chunk.clear();
                }
            }
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
            int n = std::snprintf(line_buf, sizeof(line_buf), "%.5g %.5g %.5g %.4g\n",
                                  pt.x, pt.y, pt.z, pt.intensity);
            if (n > 0) {
                chunk.append(line_buf, static_cast<size_t>(n));
                if (chunk.size() >= 60000) {
                    ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
                    chunk.clear();
                }
            }
        }
    }
    if (!chunk.empty()) {
        ofs.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
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
    size_t declared_points = 0;

    while (std::getline(ifs, line)) {
        if (!in_data) {
            if (line.rfind("FIELDS", 0) == 0) {
                if (line.find("rgb") != std::string::npos || line.find("RGB") != std::string::npos) {
                    has_rgb_field = true;
                }
            } else if (line.rfind("POINTS ", 0) == 0) {
                declared_points = static_cast<size_t>(std::strtoull(line.c_str() + 7, nullptr, 10));
            } else if (line.rfind("DATA", 0) == 0) {
                in_data = true;
                if (declared_points > 0 && declared_points < 20000000) {
                    out_points.reserve(declared_points);
                }
            }
            continue;
        }

        const char* p = line.c_str();
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '\0' || *p == '\r' || *p == '\n' || *p == '#') continue;

        char* end = nullptr;
        core::schemas::PointXYZI pt;
        pt.x = std::strtof(p, &end);
        if (end == p) continue;
        p = end;
        pt.y = std::strtof(p, &end);
        if (end == p) continue;
        p = end;
        pt.z = std::strtof(p, &end);
        if (end == p) continue;
        p = end;

        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;

        if (has_rgb_field) {
            while (*p == ' ' || *p == '\t') ++p;
            if (*p == '\0' || *p == '\r' || *p == '\n') continue;

            const char* tok_start = p;
            bool is_float_rgb = false;
            while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
                if (*p == '.' || *p == 'e' || *p == 'E') is_float_rgb = true;
                ++p;
            }
            uint32_t rgb_packed = 0;
            if (is_float_rgb) {
                float rgb_f = std::strtof(tok_start, nullptr);
                std::memcpy(&rgb_packed, &rgb_f, sizeof(uint32_t));
            } else {
                rgb_packed = static_cast<uint32_t>(std::strtoul(tok_start, nullptr, 10));
            }
            pt.r = static_cast<uint8_t>((rgb_packed >> 16) & 0xFF);
            pt.g = static_cast<uint8_t>((rgb_packed >> 8) & 0xFF);
            pt.b = static_cast<uint8_t>(rgb_packed & 0xFF);
            pt.has_color = true;

            float inten = std::strtof(p, &end);
            pt.intensity = (end != p && std::isfinite(inten)) ? inten : 1.0f;
            out_points.push_back(pt);
        } else {
            float inten = std::strtof(p, &end);
            pt.intensity = (end != p && std::isfinite(inten)) ? inten : 1.0f;
            pt.r = 255;
            pt.g = 255;
            pt.b = 255;
            pt.has_color = false;
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

void QmlBridge::autoFitCameraToMapBounds() {
    if (mapPoints_.empty()) {
        recenter3D();
        recenter2D();
        return;
    }

    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    size_t valid_count = 0;
    const size_t n = mapPoints_.size();
    const size_t step = (n > 8192) ? (n / 4096) : 1;
    for (size_t i = 0; i < n; i += step) {
        const auto& pt = mapPoints_[i];
        if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
        if (pt.x < min_x) min_x = pt.x;
        if (pt.x > max_x) max_x = pt.x;
        if (pt.y < min_y) min_y = pt.y;
        if (pt.y > max_y) max_y = pt.y;
        if (pt.z < min_z) min_z = pt.z;
        if (pt.z > max_z) max_z = pt.z;
        ++valid_count;
    }

    if (valid_count == 0) {
        recenter3D();
        recenter2D();
        return;
    }

    const double cx = 0.5 * (static_cast<double>(min_x) + static_cast<double>(max_x));
    const double cy = 0.5 * (static_cast<double>(min_y) + static_cast<double>(max_y));
    const double cz = 0.5 * (static_cast<double>(min_z) + static_cast<double>(max_z));
    const double dx = static_cast<double>(max_x - min_x);
    const double dy = static_cast<double>(max_y - min_y);
    const double dz = static_cast<double>(max_z - min_z);
    const double span = std::max({dx, dy, dz, 1.0});

    camTargetX3D_ = cx;
    camTargetY3D_ = cy;
    camTargetZ3D_ = cz;
    camDistance3D_ = std::clamp(span * 1.15, 2.5, 120.0);
    camYaw3D_ = 45.0;
    camPitch3D_ = 30.0;
    emit cam3DChanged();

    pan2DX_ = cx;
    pan2DY_ = cy;
    const double xy_span = std::max({dx, dy, 1.0});
    zoom2D_ = std::clamp(500.0 / (xy_span * 1.25), 4.0, 200.0);
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
    bool updated_from_active_sensor = false;
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
                updated_from_active_sensor = true;
            }
            break;
        }
    }

    // Also update global mapPoints_ if loaded statically (e.g. diagnostic PCD without active sensor)
    if (!updated_from_active_sensor && activeDisplaySensor_.toStdString() == s_id) {
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

    // 2. Save candidate transform to profiles/validated/<sensor>_profile.json (or experimental for CAMERA)
    fs::path prof_path;
    if (s_id == "GEMINI") {
        prof_path = fs::path("/home/scanar/av_scan/profiles/validated/gemini_336l_profile.json");
    } else if (s_id == "VITURE") {
        prof_path = fs::path("/home/scanar/av_scan/profiles/validated/viture_ultra_profile.json");
    } else if (s_id == "CAMERA") {
        prof_path = fs::path("/home/scanar/av_scan/profiles/experimental/camera_32e4_4802_2023081002.json");
    }

    if (!prof_path.empty() && fs::exists(prof_path)) {
        try {
            std::ifstream pf(prof_path);
            nlohmann::json pj = nlohmann::json::parse(pf);
            pf.close();

            pj["canonical_av_transform"] = {
                {"target_convention", "CANONICAL_AV (+X Fwd, +Y Left, +Z Up, Ground XY)"},
                {"source_frame", s_id == "GEMINI" ? "camera_link" : (s_id == "VITURE" ? "carina_gl_origin" : "optical_camera_frame")},
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
    } else if (s_id == "CAMERA") {
        daemon_tf_path = "/home/scanar/av_scan/core/drivers/uvc/canonical_transform.json";
    }
    if (!daemon_tf_path.empty()) {
        try {
            fs::create_directories(daemon_tf_path.parent_path());
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

bool QmlBridge::autoLevelOrientationDiag() {
    if (orientationDiagIsFrozen()) {
        orientationDiagStatusMsg_ = "Airy is FROZEN Ground Truth reference; auto-level locked.";
        emit orientationDiagChanged();
        return false;
    }
    if (mapPoints_.size() < 50) {
        orientationDiagStatusMsg_ = "Insufficient point cloud data (< 50 pts) for RANSAC floor auto-level.";
        emit orientationDiagChanged();
        return false;
    }

    // Apply current orientationDiagR_ to a subsampled working copy of mapPoints_
    std::vector<core::schemas::PointXYZI> working_pts;
    const size_t step = (mapPoints_.size() > 25000) ? (mapPoints_.size() / 25000) : 1;
    working_pts.reserve(mapPoints_.size() / step + 1);
    for (size_t i = 0; i < mapPoints_.size(); i += step) {
        core::schemas::PointXYZI pt = mapPoints_[i];
        double rx = orientationDiagR_[0][0] * pt.x + orientationDiagR_[0][1] * pt.y + orientationDiagR_[0][2] * pt.z;
        double ry = orientationDiagR_[1][0] * pt.x + orientationDiagR_[1][1] * pt.y + orientationDiagR_[1][2] * pt.z;
        double rz = orientationDiagR_[2][0] * pt.x + orientationDiagR_[2][1] * pt.y + orientationDiagR_[2][2] * pt.z;
        pt.x = static_cast<float>(rx);
        pt.y = static_cast<float>(ry);
        pt.z = static_cast<float>(rz);
        working_pts.push_back(pt);
    }

    auto gp = core::processing::PointCloudProcessor::detectAndAlignGroundPlane(
        working_pts, 0.04, 150, 45.0, false, false);

    if (!gp.found) {
        orientationDiagStatusMsg_ = "RANSAC could not find a dominant floor plane within 45 deg of +Z.";
        emit orientationDiagChanged();
        return false;
    }

    // Compose R_total = R_align * R_current
    Eigen::Matrix3d R_current;
    Eigen::Matrix3d R_align;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            R_current(r, c) = orientationDiagR_[r][c];
            R_align(r, c) = gp.alignment_transform[r][c];
        }
    }
    Eigen::Matrix3d R_total = R_align * R_current;

    // Decompose R_total = Rz(yaw) * Ry(pitch) * Rx(roll)
    double sin_pitch = std::clamp(-R_total(2, 0), -1.0, 1.0);
    double pitch_rad = std::asin(sin_pitch);
    double roll_rad = 0.0;
    double yaw_rad = 0.0;
    if (std::abs(std::cos(pitch_rad)) > 1e-6) {
        roll_rad = std::atan2(R_total(2, 1), R_total(2, 2));
        yaw_rad = std::atan2(R_total(1, 0), R_total(0, 0));
    } else {
        roll_rad = std::atan2(-R_total(1, 2), R_total(1, 1));
        yaw_rad = 0.0;
    }

    orientationDiagRoll_ = roll_rad * 180.0 / M_PI;
    orientationDiagPitch_ = pitch_rad * 180.0 / M_PI;
    orientationDiagYaw_ = yaw_rad * 180.0 / M_PI;
    orientationDiagStatusMsg_ = QString("Auto-Leveled Floor via RANSAC (%1 inliers, corrected %2° tilt).")
                                    .arg(gp.inlier_count)
                                    .arg(gp.tilt_deg, 0, 'f', 2);
    updateOrientationDiagMatrix();
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
        if (selectedNodeViewIndex_ == -1) {
            refreshCslamMergedCloud();
        } else if (selectedNodeViewIndex_ == 0) {
            for (const auto& s : activeSensors_) {
                if (s.id == activeDisplaySensor_.toStdString()) {
                    mapPoints_ = s.map_points;
                    trajectory_ = s.trajectory;
                    pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
                    hasMapData_ = !mapPoints_.empty();
                    invalidateDisplayPointsCache();
                    emit pointsCapturedChanged();
                    emit trajectoryChanged();
                    emit mapDataUpdated();
                    break;
                }
            }
        } else if (multiNodeOrchestrator_ && selectedNodeViewIndex_ > 0) {
            auto nodes = multiNodeOrchestrator_->getCslamScannerNodes();
            int idx = 1;
            for (const auto& n : nodes) {
                if (n.node_uuid != multiNodeOrchestrator_->getLocalNodeUuid()) {
                    if (idx == selectedNodeViewIndex_) {
                        mapPoints_ = multiNodeOrchestrator_->cslam().getScannerPointCloud(n.node_uuid, cslamColorByScanner_);
                        trajectory_ = n.trajectory;
                        pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
                        hasMapData_ = !mapPoints_.empty();
                        invalidateDisplayPointsCache();
                        emit pointsCapturedChanged();
                        emit trajectoryChanged();
                        emit mapDataUpdated();
                        break;
                    }
                    idx++;
                }
            }
        }
    }
}

void QmlBridge::selectNodeView(int index) {
    setSelectedNodeViewIndex(index);
}

QString QmlBridge::selectedNodeViewName() const {
    if (selectedNodeViewIndex_ == -1) {
        return QString("Real-Time Merged C-SLAM (%1 Scanners)").arg(cslamScannerCount());
    }
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
    auto cslamNodes = multiNodeOrchestrator_->getCslamScannerNodes();
    idx = 1;
    for (const auto& n : cslamNodes) {
        if (n.node_uuid != multiNodeOrchestrator_->getLocalNodeUuid()) {
            if (idx == selectedNodeViewIndex_) {
                return QString("Scanner: %1 [%2]").arg(QString::fromStdString(n.display_name)).arg(QString::fromStdString(core::multi_node::to_string(n.alignment_state)));
            }
            idx++;
        }
    }
    return QString("Local Device (%1)").arg(nodeDisplayName());
}

void QmlBridge::setCslamColorByScanner(bool enabled) {
    if (cslamColorByScanner_ != enabled) {
        cslamColorByScanner_ = enabled;
        if (selectedNodeViewIndex_ == -1) {
            refreshCslamMergedCloud();
        } else {
            emit cslamStateChanged();
        }
    }
}

int QmlBridge::cslamScannerCount() const {
    if (!multiNodeOrchestrator_) return 0;
    return static_cast<int>(multiNodeOrchestrator_->cslam().scannerCount());
}

int QmlBridge::cslamSnappedCount() const {
    if (!multiNodeOrchestrator_) return 0;
    return static_cast<int>(multiNodeOrchestrator_->cslam().snappedScannerCount());
}

qlonglong QmlBridge::cslamMergedPointCount() const {
    if (!multiNodeOrchestrator_) return 0;
    auto nodes = multiNodeOrchestrator_->getCslamScannerNodes();
    qlonglong total = 0;
    for (const auto& n : nodes) {
        total += static_cast<qlonglong>(n.raw_points.size());
    }
    return total;
}

QVariantList QmlBridge::cslamNodesList() const {
    QVariantList list;
    if (!multiNodeOrchestrator_) return list;
    auto nodes = multiNodeOrchestrator_->getCslamScannerNodes();
    for (const auto& n : nodes) {
        QVariantMap m;
        m["uuid"] = QString::fromStdString(n.node_uuid);
        m["shortUuid"] = QString::fromStdString(n.node_uuid.substr(0, 8));
        m["displayName"] = QString::fromStdString(n.display_name);
        m["scannerIndex"] = static_cast<int>(n.scanner_index);
        m["colorHex"] = QString("#%1%2%3")
            .arg(static_cast<int>(n.color_r), 2, 16, QChar('0'))
            .arg(static_cast<int>(n.color_g), 2, 16, QChar('0'))
            .arg(static_cast<int>(n.color_b), 2, 16, QChar('0'));
        m["alignmentState"] = QString::fromStdString(core::multi_node::to_string(n.alignment_state));
        m["isSnapped"] = (n.alignment_state == core::multi_node::CSlamAlignmentState::ANCHOR ||
                          n.alignment_state == core::multi_node::CSlamAlignmentState::OVERLAP_LOCKED);
        m["isAnchor"] = (n.alignment_state == core::multi_node::CSlamAlignmentState::ANCHOR);
        m["overlapPercent"] = QString::number(n.overlap_ratio * 100.0, 'f', 1) + "%";
        m["rmseMm"] = QString::number(n.inlier_rmse_m * 1000.0, 'f', 1) + " mm";
        m["pointCount"] = static_cast<qlonglong>(n.raw_points.size());
        m["offsetMeters"] = QString("(%1, %2, %3) m")
            .arg(n.T_world_from_scanner.matrix[0][3], 0, 'f', 1)
            .arg(n.T_world_from_scanner.matrix[1][3], 0, 'f', 1)
            .arg(n.T_world_from_scanner.matrix[2][3], 0, 'f', 1);
        list.append(m);
    }
    return list;
}

int QmlBridge::triggerCslamSnap() {
    if (!multiNodeOrchestrator_) return 0;
    int snapped = multiNodeOrchestrator_->triggerCslamOverlapEvaluation();
    refreshCslamMergedCloud();
    cslamStatusSummary_ = QString("Evaluated overlap across %1 scanners: %2 snapped into unified frame (%3 newly locked).")
        .arg(cslamScannerCount())
        .arg(cslamSnappedCount())
        .arg(snapped);
    emit cslamStateChanged();
    return snapped;
}

void QmlBridge::addSimulatedCslamScanner(const QString& scannerName, bool overlapping) {
    if (!multiNodeOrchestrator_) return;

    // Ensure local scanner has its submap registered as Anchor first
    if (multiNodeOrchestrator_->cslam().scannerCount() == 0) {
        std::vector<core::schemas::PointXYZI> anchor_pts = mapPoints_;
        if (anchor_pts.empty()) {
            // Generate synthetic anchor room geometry (floor + 2 walls + table)
            for (int ix = -25; ix <= 25; ++ix) {
                for (int iy = -25; iy <= 25; ++iy) {
                    core::schemas::PointXYZI p{};
                    p.x = ix * 0.10f;
                    p.y = iy * 0.10f;
                    p.z = 0.0f;
                    p.intensity = 120.0f;
                    p.r = 180; p.g = 190; p.b = 200;
                    anchor_pts.push_back(p);
                }
            }
            for (int ix = -25; ix <= 25; ++ix) {
                for (int iz = 0; iz <= 20; ++iz) {
                    core::schemas::PointXYZI w1{};
                    w1.x = ix * 0.10f; w1.y = 2.5f; w1.z = iz * 0.10f;
                    w1.intensity = 160.0f; w1.r = 200; w1.g = 210; w1.b = 220;
                    anchor_pts.push_back(w1);

                    core::schemas::PointXYZI w2{};
                    w2.x = 2.5f; w2.y = ix * 0.10f; w2.z = iz * 0.10f;
                    w2.intensity = 150.0f; w2.r = 190; w2.g = 200; w2.b = 210;
                    anchor_pts.push_back(w2);
                }
            }
        }
        core::schemas::Quaternion q_id{0.0, 0.0, 0.0, 1.0};
        multiNodeOrchestrator_->publishLocalSubmap(anchor_pts, trajectory_, q_id, false);
    }

    simulatedCslamCounter_++;
    std::string sim_uuid = "sim-scanar-" + std::to_string(simulatedCslamCounter_) + "-node";
    std::string sim_name = scannerName.isEmpty()
        ? ("scanAR-" + std::to_string(simulatedCslamCounter_ + 1))
        : scannerName.toStdString();

    // Build point cloud from anchor reference
    auto nodes = multiNodeOrchestrator_->getCslamScannerNodes();
    std::vector<core::schemas::PointXYZI> ref_pts;
    if (!nodes.empty()) {
        ref_pts = nodes.front().raw_points;
    }

    std::vector<core::schemas::PointXYZI> sim_pts;
    core::schemas::Quaternion imu_q{0.0, 0.0, 0.0, 1.0};

    if (overlapping && !ref_pts.empty()) {
        // Overlapping scan rotated by 15 degrees yaw + small translation (0.35m, -0.25m, 0.0f)
        double yaw = 15.0 * M_PI / 180.0;
        double cy = std::cos(yaw), sy = std::sin(yaw);
        imu_q.x = 0.0;
        imu_q.y = 0.0;
        imu_q.z = std::sin(yaw * 0.5);
        imu_q.w = std::cos(yaw * 0.5);

        sim_pts.reserve(ref_pts.size());
        for (size_t i = 0; i < ref_pts.size(); ++i) {
            const auto& p = ref_pts[i];
            // Keep 75% overlapping region + add new extension points
            core::schemas::PointXYZI sp = p;
            if (i % 5 == 0) {
                sp.x += 1.2f; // Extended scanned area
            }
            // Express in scanner's local coordinates (inverse of yaw + shift)
            double dx = sp.x - 0.35;
            double dy = sp.y + 0.25;
            sp.x = static_cast<float>(cy * dx + sy * dy);
            sp.y = static_cast<float>(-sy * dx + cy * dy);
            sim_pts.push_back(sp);
        }
    } else {
        // Non-overlapping distinct structure (e.g. a cylindrical pillar / separate room dome)
        // Oriented by a 35-degree IMU heading so IMU orientation is visible while separated
        double yaw = (35.0 * simulatedCslamCounter_) * M_PI / 180.0;
        imu_q.x = 0.0;
        imu_q.y = 0.0;
        imu_q.z = std::sin(yaw * 0.5);
        imu_q.w = std::cos(yaw * 0.5);

        for (int a = 0; a < 72; ++a) {
            double theta = a * (2.0 * M_PI / 72.0);
            for (int h = 0; h < 25; ++h) {
                core::schemas::PointXYZI sp{};
                double r = 1.4 + 0.15 * std::sin(h * 0.3);
                sp.x = static_cast<float>(r * std::cos(theta));
                sp.y = static_cast<float>(r * std::sin(theta));
                sp.z = static_cast<float>(h * 0.10);
                sp.intensity = 180.0f;
                sp.r = 200; sp.g = 200; sp.b = 200;
                sim_pts.push_back(sp);
            }
        }
    }

    std::vector<core::schemas::Pose3D> sim_traj;
    for (int t = 0; t < 12; ++t) {
        core::schemas::Pose3D pose{};
        pose.position.x = t * 0.15;
        pose.position.y = 0.1 * std::sin(t * 0.5);
        pose.position.z = 0.0;
        pose.orientation = imu_q;
        sim_traj.push_back(pose);
    }

    multiNodeOrchestrator_->cslam().updateScannerSubmap(
        sim_uuid, sim_name, sim_pts, sim_traj, imu_q, overlapping);

    selectedNodeViewIndex_ = -1; // Automatically switch to Merged C-SLAM view
    emit selectedNodeViewChanged();
    refreshCslamMergedCloud();

    cslamStatusSummary_ = QString("Added %1 (%2) — %3 total scanners (%4 snapped, %5 IMU-separated).")
        .arg(QString::fromStdString(sim_name))
        .arg(overlapping ? "Overlapping -> Snapped" : "Non-Overlapping -> Separated IMU Offset")
        .arg(cslamScannerCount())
        .arg(cslamSnappedCount())
        .arg(cslamScannerCount() - cslamSnappedCount());
    emit cslamStateChanged();
}

void QmlBridge::clearCslamScanners() {
    if (!multiNodeOrchestrator_) return;
    multiNodeOrchestrator_->cslam().clear();
    simulatedCslamCounter_ = 0;
    selectedNodeViewIndex_ = 0;
    cslamStatusSummary_ = "Cleared C-SLAM collaborative submaps.";
    emit selectedNodeViewChanged();
    emit cslamStateChanged();
}

void QmlBridge::refreshCslamMergedCloud() {
    if (!multiNodeOrchestrator_) return;
    if (selectedNodeViewIndex_ == -1 && multiNodeOrchestrator_->cslam().scannerCount() > 0) {
        mapPoints_ = multiNodeOrchestrator_->getMergedCslamPointCloud(cslamColorByScanner_);
        trajectory_ = multiNodeOrchestrator_->cslam().getMergedTrajectory();
        pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
        hasMapData_ = !mapPoints_.empty();
        invalidateDisplayPointsCache();
        emit pointsCapturedChanged();
        emit trajectoryChanged();
        emit mapDataUpdated();
    }
    emit cslamStateChanged();
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
        selectedNodeViewIndex_ = -1;
        multiNodeNotice_ = QString("Session %1 active as Coordinator (Real-Time C-SLAM Merged View).").arg(currentSessionId_);
        emit multiNodeSessionChanged();
        emit sessionMembersChanged();
        emit selectedNodeViewChanged();
        emit multiNodeNoticeChanged();
        emit cslamStateChanged();
        emit notification("Multi-Node Session", "Created multi-node C-SLAM session as Coordinator.");
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
        selectedNodeViewIndex_ = -1;
        multiNodeNotice_ = QString("Connected to session %1 as Participant (Real-Time C-SLAM Merged View).").arg(sessionId);
        emit multiNodeSessionChanged();
        emit sessionMembersChanged();
        emit selectedNodeViewChanged();
        emit multiNodeNoticeChanged();
        emit cslamStateChanged();
        emit notification("Joined Session", QString("Connected to session %1 at %2:%3").arg(sessionId).arg(coordIp).arg(coordPort));
    } else {
        emit errorOccurred("Join Failed", QString("Failed to join session %1 at %2:%3").arg(sessionId).arg(coordIp).arg(coordPort));
    }
    return ok;
}

bool QmlBridge::leaveMultiNodeSession() {
    if (!multiNodeOrchestrator_) return false;
    bool ok = multiNodeOrchestrator_->leaveSession();
    selectedNodeViewIndex_ = 0;
    multiNodeNotice_ = "Single-node standalone mode.";
    emit multiNodeSessionChanged();
    emit sessionMembersChanged();
    emit selectedNodeViewChanged();
    emit multiNodeNoticeChanged();
    emit cslamStateChanged();
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

// =============================================================================
// Milestone 12: Point Cloud Cleanup, Post-Processing & Multi-Format Export
// =============================================================================

void QmlBridge::setLiveCleanupEnabled(bool enabled) {
    if (liveCleanupEnabled_ == enabled) return;
    liveCleanupEnabled_ = enabled;

    core::processing::CleanupParams liveParams;
    liveParams.enable_voxel_downsample = true;
    liveParams.voxel_leaf_size_m = 0.025f;
    liveParams.enable_ror = true;
    liveParams.ror_radius_m = 0.08;
    liveParams.ror_min_neighbors = 3;
    liveParams.enable_sor = true;
    liveParams.sor_std_mul = 2.0;
    liveParams.enable_ground_alignment = liveGroundLockEnabled_;
    liveParams.align_ground_to_z_zero = true;

    // Immediately re-evaluate active display sensor buffer if we have raw_map_points
    for (auto& s : activeSensors_) {
        if (!s.raw_map_points.empty()) {
            if (liveCleanupEnabled_) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(
                    s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
            } else {
                s.map_points = s.raw_map_points;
                liveCleanupTimeMs_ = 0.0;
            }
            s.point_count = s.map_points.size();
            if (s.id == activeDisplaySensor_.toStdString()) {
                mapPoints_ = s.map_points;
                pointsCaptured_ = mapPoints_.size();
                emit pointsCapturedChanged();
                emit mapDataUpdated();
            }
        }
    }
    emit cleanupStateChanged();
}

void QmlBridge::setLiveGroundLockEnabled(bool enabled) {
    if (liveGroundLockEnabled_ == enabled) return;
    liveGroundLockEnabled_ = enabled;

    if (liveCleanupEnabled_) {
        core::processing::CleanupParams liveParams;
        liveParams.enable_voxel_downsample = true;
        liveParams.voxel_leaf_size_m = 0.025f;
        liveParams.enable_ror = true;
        liveParams.ror_radius_m = 0.08;
        liveParams.ror_min_neighbors = 3;
        liveParams.enable_sor = true;
        liveParams.sor_std_mul = 2.0;
        liveParams.enable_ground_alignment = liveGroundLockEnabled_;
        liveParams.align_ground_to_z_zero = true;

        for (auto& s : activeSensors_) {
            if (!s.raw_map_points.empty()) {
                auto st = core::processing::PointCloudProcessor::cleanRealtimeFast(
                    s.raw_map_points, s.map_points, liveParams);
                liveCleanupTimeMs_ = st.computation_time_ms;
                if (s.map_points.empty()) s.map_points = s.raw_map_points;
                s.point_count = s.map_points.size();
                if (s.id == activeDisplaySensor_.toStdString()) {
                    mapPoints_ = s.map_points;
                    pointsCaptured_ = mapPoints_.size();
                    emit pointsCapturedChanged();
                    emit mapDataUpdated();
                }
            }
        }
    }
    emit cleanupStateChanged();
}

bool QmlBridge::runPointCloudCleanup(bool enableVoxel, double voxelLeafM,
                                     bool enableSor, int sorMeanK, double sorStdMul,
                                     bool enableRor, double rorRadiusM, int rorMinNeighbors,
                                     bool enableGroundAlign, bool removeGroundPlane) {
    if (mapPoints_.empty() && preCleanupBackupPoints_.empty()) {
        emit errorOccurred("Cleanup Error", "No active point cloud loaded in viewport to clean.");
        return false;
    }

    if (!hasCleanupPreview_ || preCleanupBackupPoints_.empty()) {
        preCleanupBackupPoints_ = mapPoints_;
    }

    core::processing::CleanupParams params;
    params.enable_voxel_downsample = enableVoxel;
    params.voxel_leaf_size_m = static_cast<float>(std::max(0.002, voxelLeafM));
    params.enable_sor = enableSor;
    params.sor_mean_k = std::max(3, sorMeanK);
    params.sor_std_mul = std::max(0.1, sorStdMul);
    params.enable_ror = enableRor;
    params.ror_radius_m = std::max(0.005, rorRadiusM);
    params.ror_min_neighbors = std::max(1, rorMinNeighbors);
    params.enable_ground_alignment = enableGroundAlign;
    params.align_ground_to_z_zero = enableGroundAlign;
    params.remove_ground_plane = removeGroundPlane;

    std::vector<core::schemas::PointXYZI> cleaned;
    auto stats = core::processing::PointCloudProcessor::runCleanupPipeline(
        preCleanupBackupPoints_, cleaned, params);

    if (cleaned.empty()) {
        emit errorOccurred("Cleanup Warning", "Filter parameters removed all points. Restoring original cloud.");
        return false;
    }

    mapPoints_ = std::move(cleaned);
    hasCleanupPreview_ = true;
    cleanupInputPoints_ = static_cast<qlonglong>(stats.input_point_count);
    cleanupOutputPoints_ = static_cast<qlonglong>(stats.output_point_count);
    cleanupRemovedPoints_ = cleanupInputPoints_ - cleanupOutputPoints_;
    cleanupGroundTiltDeg_ = stats.ground_plane.found ? stats.ground_plane.tilt_deg : 0.0;
    pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
    hasMapData_ = true;

    QString groundSummary = stats.ground_plane.found
        ? QString(" | Ground leveled (%1° tilt)").arg(stats.ground_plane.tilt_deg, 0, 'f', 1)
        : QString("");

    cleanupStatus_ = QString("Cleaned: %1 -> %2 pts (-%3 outliers)%4 in %5 ms")
        .arg(cleanupInputPoints_)
        .arg(cleanupOutputPoints_)
        .arg(cleanupRemovedPoints_)
        .arg(groundSummary)
        .arg(static_cast<int>(std::round(stats.computation_time_ms)));

    emit cleanupStateChanged();
    emit pointsCapturedChanged();
    emit mapDataUpdated();
    emit notification("Point Cloud Cleaned", cleanupStatus_);
    return true;
}

void QmlBridge::resetPointCloudCleanup() {
    if (!hasCleanupPreview_ || preCleanupBackupPoints_.empty()) {
        return;
    }
    mapPoints_ = preCleanupBackupPoints_;
    preCleanupBackupPoints_.clear();
    hasCleanupPreview_ = false;
    cleanupRemovedPoints_ = 0;
    cleanupOutputPoints_ = static_cast<qlonglong>(mapPoints_.size());
    pointsCaptured_ = static_cast<qlonglong>(mapPoints_.size());
    cleanupStatus_ = "Restored original uncleaned point cloud.";

    emit cleanupStateChanged();
    emit pointsCapturedChanged();
    emit mapDataUpdated();
    emit notification("Cleanup Reset", cleanupStatus_);
}

bool QmlBridge::commitCleanedPointCloud() {
    if (mapPoints_.empty()) {
        emit errorOccurred("Commit Error", "No active point cloud to commit.");
        return false;
    }

    // Update active sensor runtime buffer
    std::string activeId = activeDisplaySensor_.toStdString();
    for (auto& s : activeSensors_) {
        if (s.id == activeId) {
            s.map_points = mapPoints_;
            s.point_count = mapPoints_.size();
            break;
        }
    }
    if (activeId == "AIRY") {
        airyMapPoints_ = mapPoints_;
        airyPointsCaptured_ = mapPoints_.size();
    } else if (activeId == "GEMINI") {
        geminiMapPoints_ = mapPoints_;
        geminiPointsCaptured_ = mapPoints_.size();
    } else if (activeId == "VITURE") {
        vitureMapPoints_ = mapPoints_;
        viturePointsCaptured_ = mapPoints_.size();
    } else if (activeId == "CAMERA") {
        cameraMapPoints_ = mapPoints_;
        cameraPointsCaptured_ = mapPoints_.size();
    } else if (activeId == "FUSED") {
        fusedMapPoints_ = mapPoints_;
    }

    // Persist to disk if a project is open
    if (storage_ && !projectId_.isEmpty()) {
        fs::path proj_dir = fs::path(storage_->getRootDirectory()) / "projects" / projectId_.toStdString();
        if (!currentSessionId_.isEmpty()) {
            fs::path sess_dir = proj_dir / "sessions" / currentSessionId_.toStdString();
            fs::create_directories(sess_dir);
            if (!preCleanupBackupPoints_.empty() && !fs::exists(sess_dir / "map_raw_backup.pcd")) {
                saveMapPcd((sess_dir / "map_raw_backup.pcd").string(), preCleanupBackupPoints_);
            }
            saveMapPcd((sess_dir / "map.pcd").string(), mapPoints_);
        }
        saveMapPcd((proj_dir / "map.pcd").string(), mapPoints_);
    }

    preCleanupBackupPoints_.clear();
    hasCleanupPreview_ = false;
    cleanupStatus_ = QString("Committed cleaned map (%1 points) to project.").arg(mapPoints_.size());

    emit cleanupStateChanged();
    emit activeSensorsChanged();
    emit notification("Cleaned Cloud Saved", cleanupStatus_);
    return true;
}

QString QmlBridge::exportActivePointCloud(const QString& formatStr,
                                          const QString& customOutputPath,
                                          double meshGridResM) {
    if (mapPoints_.empty()) {
        emit errorOccurred("Export Error", "No active point cloud available to export.");
        return "";
    }

    auto fmt = core::processing::PointCloudProcessor::parseExportFormat(formatStr.toStdString());
    std::string ext = core::processing::PointCloudProcessor::exportFormatExtension(fmt);

    fs::path out_path;
    if (!customOutputPath.trimmed().isEmpty()) {
        out_path = customOutputPath.trimmed().toStdString();
    } else {
        fs::path base_dir = storage_ ? fs::path(storage_->getRootDirectory()) : fs::current_path();
        std::string pid = projectId_.isEmpty() ? "workspace" : projectId_.toStdString();
        fs::path export_dir = base_dir / "projects" / pid / "exports";
        fs::create_directories(export_dir);

        auto now = std::chrono::system_clock::now().time_since_epoch();
        uint64_t ts_sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
        std::string sensor_tag = activeDisplaySensor_.isEmpty() ? "map" : activeDisplaySensor_.toLower().toStdString();
        out_path = export_dir / (pid + "_" + sensor_tag + "_" + std::to_string(ts_sec) + ext);
    }

    auto res = core::processing::PointCloudProcessor::exportPointCloud(
        mapPoints_, out_path.string(), fmt, static_cast<float>(std::max(0.01, meshGridResM)));

    if (!res.success) {
        exportStatus_ = "Export failed: " + QString::fromStdString(res.error_message);
        emit exportStateChanged();
        emit errorOccurred("Export Failed", exportStatus_);
        return "";
    }

    lastExportPath_ = QString::fromStdString(res.file_path);
    if (fmt == core::processing::ExportFormat::OBJ_MESH) {
        exportStatus_ = QString("Exported %1 (%2 vertices, %3 triangles) -> %4")
            .arg(QString::fromStdString(res.format_name))
            .arg(res.exported_points)
            .arg(res.exported_triangles)
            .arg(lastExportPath_);
    } else {
        exportStatus_ = QString("Exported %1 (%2 points, %3 KB) -> %4")
            .arg(QString::fromStdString(res.format_name))
            .arg(res.exported_points)
            .arg(res.file_size_bytes / 1024)
            .arg(lastExportPath_);
    }

    emit exportStateChanged();
    emit notification("Export Complete", exportStatus_);
    return lastExportPath_;
}

// =============================================================================
// Milestones 7 & 8: Rigid Extrinsic Calibration & Multi-Sensor Spatial Fusion
// =============================================================================

bool QmlBridge::calibrateActiveScannerExtrinsics() {
    // Collect all active sensors that have captured or loaded point clouds
    struct SensorCloudEntry {
        std::string role;
        std::string instance_id;
        const std::vector<core::schemas::PointXYZI>* cloud = nullptr;
        const std::vector<core::schemas::Pose3D>* traj = nullptr;
    };
    std::vector<SensorCloudEntry> entries;

    for (const auto& s : activeSensors_) {
        if (!s.map_points.empty() && s.id != "MERGED" && s.id != "FUSED") {
            entries.push_back({s.id, s.instance_id, &s.map_points, &s.trajectory});
        }
    }

    if (entries.size() < 2) {
        emit errorOccurred("Extrinsic Calibration Error",
                           "At least 2 sensors with spatial point clouds are required in the current project/session to calibrate rigid extrinsics.");
        return false;
    }

    // Load or initialize the active ScannerConfig
    core::schemas::ScannerConfig cfg;
    bool loaded_cfg = false;
    if (storage_ && !selectedScannerId_.isEmpty()) {
        loaded_cfg = storage_->loadScannerConfig(selectedScannerId_.toStdString(), cfg);
    }
    if (!loaded_cfg && storage_) {
        for (const auto& cid : storage_->listScannerConfigs()) {
            core::schemas::ScannerConfig candidate;
            if (storage_->loadScannerConfig(cid, candidate) && candidate.sensor_instances().size() >= entries.size()) {
                cfg = candidate;
                loaded_cfg = true;
                break;
            }
        }
    }
    if (!loaded_cfg) {
        std::vector<std::string> insts;
        for (const auto& e : entries) insts.push_back(e.instance_id);
        cfg = core::schemas::ScannerConfig("cfg_calibrated_multisensor", "Calibrated Multi-Sensor Rig", "local_node");
        cfg.set_sensor_instances(insts);
    }

    const auto& ref = entries[0];
    QStringList pairSummaries;
    double best_rmse_mm = 0.0;
    double best_overlap_pct = 0.0;
    double best_conf = 0.0;
    bool any_converged = false;

    for (size_t i = 1; i < entries.size(); ++i) {
        const auto& sec = entries[i];
        auto cal_res = core::fusion::ExtrinsicCalibrator::calibrateSensorPair(
            ref.instance_id,
            sec.instance_id,
            *ref.cloud,
            *sec.cloud,
            *ref.traj,
            *sec.traj);

        if (cal_res.converged) {
            any_converged = true;
            core::fusion::ExtrinsicCalibrator::persistCalibrationToConfig(
                cfg, cal_res, currentSessionId_.toStdString(), storage_.get());
            // Also store under role names ("AIRY<-GEMINI", "GEMINI") for role-based lookup
            cfg.set_relative_transform(ref.role + "<-" + sec.role, cal_res.transform);
            cfg.set_relative_transform(sec.role, cal_res.transform);
            if (storage_) {
                storage_->saveScannerConfig(cfg);
            }

            best_rmse_mm = cal_res.inlier_rmse_m * 1000.0;
            best_overlap_pct = cal_res.overlap_ratio * 100.0;
            best_conf = cal_res.confidence;

            pairSummaries.append(QString("%1 <- %2: dT=[%3, %4, %5]m, RPY=[%6°, %7°, %8°], RMSE=%9mm, Overlap=%10%")
                .arg(QString::fromStdString(ref.role))
                .arg(QString::fromStdString(sec.role))
                .arg(cal_res.transform.translation.x, 0, 'f', 3)
                .arg(cal_res.transform.translation.y, 0, 'f', 3)
                .arg(cal_res.transform.translation.z, 0, 'f', 3)
                .arg(cal_res.roll_deg, 0, 'f', 1)
                .arg(cal_res.pitch_deg, 0, 'f', 1)
                .arg(cal_res.yaw_deg, 0, 'f', 1)
                .arg(best_rmse_mm, 0, 'f', 1)
                .arg(best_overlap_pct, 0, 'f', 1));
        }
    }

    if (!any_converged) {
        extrinsicCalibrationStatus_ = "FAILED";
        emit fusionStateChanged();
        emit errorOccurred("Extrinsic Calibration Failed", "Could not converge on a rigid SE(3) transform between active sensor clouds.");
        return false;
    }

    extrinsicCalibrationStatus_ = QString::fromStdString(core::schemas::to_string(cfg.extrinsic_status()));
    extrinsicRmseMm_ = best_rmse_mm;
    extrinsicOverlapPercent_ = best_overlap_pct;
    extrinsicConfidence_ = best_conf;
    extrinsicTransformSummary_ = pairSummaries.join("\n");

    refreshData();
    emit fusionStateChanged();
    emit notification("Extrinsics Calibrated",
                      QString("Persisted %1 extrinsic calibration to '%2':\n%3")
                          .arg(extrinsicCalibrationStatus_)
                          .arg(QString::fromStdString(cfg.name()))
                          .arg(extrinsicTransformSummary_));
    return true;
}

bool QmlBridge::fuseActiveMultiSensorMaps(double voxelLeafM,
                                          bool transferOpticalRgb,
                                          bool diagnosticRoleColors) {
    std::vector<core::fusion::SensorFusionInput> inputs;
    for (const auto& s : activeSensors_) {
        if (s.id == "MERGED" || s.id == "FUSED") continue;
        core::fusion::SensorFusionInput in;
        in.sensor_id = s.instance_id.empty() ? s.id : s.instance_id;
        in.sensor_role = s.id;
        in.cloud = s.map_points;
        in.trajectory = s.trajectory;
        in.is_optical_rgb_source = (s.id == "GEMINI" || s.id == "CAMERA" || s.id == "VITURE");
        in.geometry_weight = (s.id == "AIRY") ? 2.0f : 1.0f;
        in.is_metric_scale_verified = (s.scale_status == "METRIC_VERIFIED");
        in.slam_rate_hz = s.slam_rate;
        if (s.health_override_active) {
            in.is_tracking_healthy = s.health_override_healthy;
            in.is_degraded = s.health_override_degraded;
        } else {
            in.is_tracking_healthy = (s.tracking_status != "LOST" && s.tracking_status != "OFFLINE" && s.device_status != "DISCONNECTED");
            in.is_degraded = (s.tracking_status == "DEGRADED" || s.tracking_status == "RELOCALIZING");
        }
        inputs.push_back(std::move(in));
    }

    bool any_non_empty = false;
    for (const auto& in : inputs) {
        if (!in.cloud.empty()) {
            any_non_empty = true;
            break;
        }
    }
    if (inputs.empty() || !any_non_empty) {
        emit errorOccurred("Multi-Sensor Fusion Error", "No sensor point clouds available to fuse.");
        return false;
    }

    core::schemas::ScannerConfig cfg;
    bool loaded_cfg = false;
    if (storage_ && !selectedScannerId_.isEmpty()) {
        loaded_cfg = storage_->loadScannerConfig(selectedScannerId_.toStdString(), cfg);
    }
    if (!loaded_cfg && storage_) {
        for (const auto& cid : storage_->listScannerConfigs()) {
            core::schemas::ScannerConfig candidate;
            if (storage_->loadScannerConfig(cid, candidate) && candidate.sensor_instances().size() >= inputs.size()) {
                cfg = candidate;
                loaded_cfg = true;
                break;
            }
        }
    }

    core::fusion::MultiSensorFusionParams params;
    params.voxel_leaf_size_m = static_cast<float>(voxelLeafM);
    params.transfer_optical_rgb_to_lidar = transferOpticalRgb;
    params.color_by_sensor_role = diagnosticRoleColors;
    params.auto_calibrate_missing = true;
    params.real_time_mode = false;

    core::fusion::MultiSensorFusionEngine engine;
    auto res = engine.fuseSensors(inputs, cfg, params);
    if (!res.success || res.fused_cloud.empty()) {
        fusionStatus_ = "Fusion failed: " + QString::fromStdString(res.status_message);
        emit fusionStateChanged();
        emit errorOccurred("Fusion Error", fusionStatus_);
        return false;
    }

    if (storage_ && !cfg.config_id().empty() && !res.calibration_results.empty()) {
        storage_->saveScannerConfig(cfg);
        extrinsicCalibrationStatus_ = QString::fromStdString(core::schemas::to_string(cfg.extrinsic_status()));
        for (const auto& kv : res.calibration_results) {
            extrinsicRmseMm_ = kv.second.inlier_rmse_m * 1000.0;
            extrinsicOverlapPercent_ = kv.second.overlap_ratio * 100.0;
            extrinsicConfidence_ = kv.second.confidence;
        }
    }

    fusedMapPoints_ = std::move(res.fused_cloud);
    fusedTrajectory_ = std::move(res.fused_trajectory);
    fusedColorizedPoints_ = static_cast<qlonglong>(res.colorized_lidar_points);
    isFusionActive_ = true;
    fusionStatus_ = QString::fromStdString(res.status_message);

    activeAnchorSensor_ = QString::fromStdString(res.anchor_sensor_role);
    activeAnchorScore_ = res.anchor_accuracy_score;
    isAnchorFailoverActive_ = res.anchor_failover_active;
    activeAnchorRationale_ = QString::fromStdString(res.anchor_rationale);

    for (auto& s : activeSensors_) {
        std::string sid = s.instance_id.empty() ? s.id : s.instance_id;
        auto it = res.sensor_accuracy_scores.find(sid);
        if (it != res.sensor_accuracy_scores.end()) {
            s.accuracy_score = it->second;
        }
        s.is_anchor = (s.id == res.anchor_sensor_role || sid == res.anchor_sensor_id);
        if (s.is_anchor) {
            activeAnchorName_ = QString::fromStdString(s.short_name);
        }
    }

    activeDisplaySensor_ = "FUSED";
    mapPoints_ = fusedMapPoints_;
    trajectory_ = fusedTrajectory_;
    pointsCaptured_ = static_cast<qlonglong>(fusedMapPoints_.size());
    hasMapData_ = true;

    invalidateDisplayPointsCache();
    emit activeSensorsChanged();
    emit activeDisplaySensorChanged();
    emit fusionStateChanged();
    emit pointsCapturedChanged();
    emit mapDataUpdated();
    emit trajectoryChanged();
    emit notification("Multi-Sensor Fusion Complete", fusionStatus_);
    return true;
}

void QmlBridge::updateLiveMultiSensorFusion(bool realTimeMode) {
    if (activeSensors_.size() <= 1) {
        if (!activeSensors_.empty()) {
            auto& s = activeSensors_.front();
            s.is_anchor = true;
            s.accuracy_score = core::fusion::MultiSensorFusionEngine::computeNominalAccuracyScore(
                s.id, s.scale_status == "METRIC_VERIFIED");
            activeAnchorSensor_ = QString::fromStdString(s.id);
            activeAnchorName_ = QString::fromStdString(s.short_name);
            activeAnchorScore_ = s.accuracy_score;
            isAnchorFailoverActive_ = false;
            activeAnchorRationale_ = "Primary single-sensor anchor (" + activeAnchorName_ + ").";
        }
        return;
    }

    std::vector<core::fusion::SensorFusionInput> inputs;
    inputs.reserve(activeSensors_.size());
    for (const auto& s : activeSensors_) {
        if (s.id == "MERGED" || s.id == "FUSED") continue;
        core::fusion::SensorFusionInput in;
        in.sensor_id = s.instance_id.empty() ? s.id : s.instance_id;
        in.sensor_role = s.id;
        in.cloud = s.map_points;
        in.trajectory = s.trajectory;
        in.is_optical_rgb_source = (s.id == "GEMINI" || s.id == "CAMERA" || s.id == "VITURE");
        in.geometry_weight = (s.id == "AIRY") ? 2.0f : 1.0f;
        in.is_metric_scale_verified = (s.scale_status == "METRIC_VERIFIED");
        in.slam_rate_hz = s.slam_rate;
        if (s.health_override_active) {
            in.is_tracking_healthy = s.health_override_healthy;
            in.is_degraded = s.health_override_degraded;
        } else {
            in.is_tracking_healthy = (s.tracking_status != "LOST" && s.tracking_status != "OFFLINE" && s.device_status != "DISCONNECTED");
            in.is_degraded = (s.tracking_status == "DEGRADED" || s.tracking_status == "RELOCALIZING");
        }
        inputs.push_back(std::move(in));
    }

    if (inputs.empty()) return;

    core::schemas::ScannerConfig cfg;
    bool loaded_cfg = false;
    if (storage_ && !selectedScannerId_.isEmpty()) {
        loaded_cfg = storage_->loadScannerConfig(selectedScannerId_.toStdString(), cfg);
    }
    if (!loaded_cfg && storage_) {
        for (const auto& cid : storage_->listScannerConfigs()) {
            core::schemas::ScannerConfig candidate;
            if (storage_->loadScannerConfig(cid, candidate) && candidate.sensor_instances().size() >= inputs.size()) {
                cfg = candidate;
                loaded_cfg = true;
                break;
            }
        }
    }

    core::fusion::MultiSensorFusionParams params;
    params.voxel_leaf_size_m = 0.025f;
    params.transfer_optical_rgb_to_lidar = true;
    params.color_by_sensor_role = false;
    params.auto_calibrate_missing = !realTimeMode;
    params.real_time_mode = realTimeMode;

    core::fusion::MultiSensorFusionEngine engine;
    auto res = engine.fuseSensors(inputs, cfg, params);

    activeAnchorSensor_ = QString::fromStdString(res.anchor_sensor_role);
    activeAnchorScore_ = res.anchor_accuracy_score;
    isAnchorFailoverActive_ = res.anchor_failover_active;
    activeAnchorRationale_ = QString::fromStdString(res.anchor_rationale);

    for (auto& s : activeSensors_) {
        std::string sid = s.instance_id.empty() ? s.id : s.instance_id;
        auto it = res.sensor_accuracy_scores.find(sid);
        if (it != res.sensor_accuracy_scores.end()) {
            s.accuracy_score = it->second;
        }
        s.is_anchor = (s.id == res.anchor_sensor_role || sid == res.anchor_sensor_id);
        if (s.is_anchor) {
            activeAnchorName_ = QString::fromStdString(s.short_name);
        }
    }

    if (res.success && !res.fused_cloud.empty()) {
        fusedMapPoints_ = std::move(res.fused_cloud);
        fusedTrajectory_ = std::move(res.fused_trajectory);
        fusedColorizedPoints_ = static_cast<qlonglong>(res.colorized_lidar_points);
        isFusionActive_ = true;
        fusionStatus_ = QString::fromStdString(res.status_message);

        if (activeDisplaySensor_ == "FUSED") {
            mapPoints_ = fusedMapPoints_;
            trajectory_ = fusedTrajectory_;
            qlonglong totalSensorPts = 0;
            for (const auto& s : activeSensors_) {
                totalSensorPts += s.point_count;
            }
            pointsCaptured_ = (totalSensorPts > 0) ? totalSensorPts : static_cast<qlonglong>(fusedMapPoints_.size());
            hasMapData_ = true;
            invalidateDisplayPointsCache();
        }
    } else {
        isFusionActive_ = true;
        fusionStatus_ = activeAnchorRationale_;
    }

    emit activeSensorsChanged();
    emit fusionStateChanged();
}

void QmlBridge::setSensorHealthOverride(const QString& sensorId, bool healthy, bool degraded) {
    QString target = sensorId.trimmed().toUpper();
    bool matched = false;
    for (auto& s : activeSensors_) {
        if (QString::fromStdString(s.id).toUpper() == target ||
            QString::fromStdString(s.instance_id).toUpper() == target) {
            s.health_override_active = true;
            s.health_override_healthy = healthy;
            s.health_override_degraded = degraded;
            if (!healthy) {
                s.tracking_status = "LOST";
            } else if (degraded) {
                s.tracking_status = "DEGRADED";
            } else {
                s.tracking_status = isCapturing_ ? "TRACKING" : "IDLE";
            }
            matched = true;
        }
    }
    if (matched) {
        updateLiveMultiSensorFusion(true);
        emit activeSensorsChanged();
        emit fusionStateChanged();
        emit mapDataUpdated();
        emit trajectoryChanged();
    }
}

void QmlBridge::clearSensorHealthOverrides() {
    for (auto& s : activeSensors_) {
        s.health_override_active = false;
        s.health_override_healthy = true;
        s.health_override_degraded = false;
        if (s.tracking_status == "LOST" || s.tracking_status == "DEGRADED") {
            s.tracking_status = isCapturing_ ? "TRACKING" : "IDLE";
        }
    }
    updateLiveMultiSensorFusion(true);
    emit activeSensorsChanged();
    emit fusionStateChanged();
    emit mapDataUpdated();
    emit trajectoryChanged();
}

bool QmlBridge::saveFusedMapToProject() {
    if (fusedMapPoints_.empty()) {
        emit errorOccurred("Save Fused Map Error", "No fused multi-sensor map available to save.");
        return false;
    }
    if (!storage_ || projectId_.isEmpty()) {
        emit errorOccurred("Save Fused Map Error", "No active project open.");
        return false;
    }

    auto now = std::chrono::system_clock::now().time_since_epoch();
    uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::string pid = projectId_.toStdString();
    std::string sess_id = currentSessionId_.isEmpty() ? ("session_" + std::to_string(now_ms)) : currentSessionId_.toStdString();

    fs::path proj_dir = fs::path(storage_->getRootDirectory()) / "projects" / pid;
    fs::path sess_dir = proj_dir / "sessions" / sess_id;
    fs::create_directories(sess_dir);

    saveMapPcd((sess_dir / "map.pcd").string(), fusedMapPoints_);
    saveMapPcd((proj_dir / "map.pcd").string(), fusedMapPoints_);

    std::string cap_id = "cap_" + sess_id + "_merged";
    fs::path cap_dir = sess_dir / "captures" / cap_id;
    fs::create_directories(cap_dir);
    saveMapPcd((cap_dir / "map.pcd").string(), fusedMapPoints_);

    core::schemas::Capture cap;
    cap.set_capture_id(cap_id);
    cap.set_session_id(sess_id);
    cap.set_sensor_instance_id("merged_multisensor_01");
    cap.set_observation_count(fusedMapPoints_.size());
    cap.set_start_timestamp_ns(now_ms * 1000000ULL);
    cap.set_end_timestamp_ns(now_ms * 1000000ULL);
    cap.set_map_path((cap_dir / "map.pcd").string());
    cap.set_status("COMPLETED");
    storage_->saveCapture(cap);

    std::ofstream c_out(cap_dir / "capture.json");
    if (c_out.is_open()) {
        c_out << cap.to_json().dump(2);
    }

    fusionStatus_ = QString("Saved fused map (%1 points) to project '%2'.").arg(fusedMapPoints_.size()).arg(projectName_);
    emit fusionStateChanged();
    emit notification("Fused Map Saved", fusionStatus_);
    return true;
}

} // namespace av::gui
