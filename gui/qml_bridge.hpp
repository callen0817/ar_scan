#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <memory>
#include <vector>
#include "core/storage/storage_engine.hpp"
#include "platform/platform_adapter.hpp"
#include "core/drivers/airy/airy_sensor_adapter.hpp"
#include "core/slam/airy/airy_lio_backend.hpp"
#include "core/drivers/gemini/gemini_sensor_adapter.hpp"
#include "core/slam/gemini/gemini_rgbd_backend.hpp"
#include "core/drivers/viture/viture_sensor_adapter.hpp"
#include "core/slam/viture/viture_vio_backend.hpp"
#include "core/drivers/uvc/generic_uvc_sensor_adapter.hpp"
#include "core/interfaces/ineural_recon_backend.hpp"
#include "core/reconstruction/lingbot/lingbot_slam_backend.hpp"
#include "core/reconstruction/abot/abot_slam_backend.hpp"
#include "core/profiling/device_profiler.hpp"
#include "core/profiling/capability_matcher.hpp"
#include "core/ecosystem/vendor_ecosystem.hpp"
#include "core/multi_node/multi_node_session_orchestrator.hpp"
#include "core/schemas/registration_result.hpp"
#include "core/registration/registration_engine.hpp"
#include "core/processing/point_cloud_processor.hpp"
#include "core/fusion/extrinsic_calibrator.hpp"
#include "core/fusion/multi_sensor_fusion_engine.hpp"

namespace av::gui {

struct SensorRuntimeContext {
    std::string id;              // "AIRY", "GEMINI", "VITURE", "CAMERA"
    std::string instance_id;     // "robosense_airy_01", "orbbec_gemini_336l_01", "viture_luma_ultra_01", "camera_32e4_4802_2023081002"
    std::string name;            // "RoboSense RS-Airy", "Orbbec Gemini 336L", "VITURE Luma Ultra", "48MP USB Camera"
    std::string short_name;      // "Airy LiDAR", "Gemini 336L", "Luma Ultra", "USB Camera"
    std::string icon;            // "📡", "📷", "🕶️", "📷"
    std::string profile_id;      // "robosense_airy_01", "gemini_336l_validated", "viture_luma_ultra_01", "camera_32e4_4802_2023081002"
    std::string serial_number;   // "AIRY-2024-99812", "CPC6463000XW", "USB-35CA-1104", "2023081002"
    std::string transport;       // "192.168.1.200:6699", "/dev/video0", "USB_3_UVC_HID", "/dev/video2"
    std::string mount_label;     // "TOP_MOUNT", "FRONT_FACING", "HEAD_MOUNT", "HANDHELD"
    std::string scale_status{"METRIC_VERIFIED"};

    bool is_capturable{false};
    std::string adapter_name{"NONE"};
    std::string backend_name{"NONE"};
    std::string reconstruction_family{"UNKNOWN"};

    std::string device_status{"STANDBY"};
    std::string acquisition_status{"STOPPED"};
    std::string tracking_status{"IDLE"};
    double acquisition_rate{0.0};
    double slam_rate{0.0};
    uint64_t point_count{0};

    bool health_override_active{false};
    bool health_override_healthy{true};
    bool health_override_degraded{false};
    double accuracy_score{100.0};
    bool is_anchor{false};

    std::vector<core::schemas::PointXYZI> map_points;
    std::vector<core::schemas::PointXYZI> raw_map_points;
    std::vector<core::schemas::Pose3D> trajectory;
};

class QmlBridge : public QObject {
    Q_OBJECT

    // Project & Workspace Properties
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString projectId READ projectId NOTIFY projectIdChanged)
    Q_PROPERTY(QString projectDescription READ projectDescription NOTIFY projectDescriptionChanged)
    Q_PROPERTY(QString projectCreatedAt READ projectCreatedAt NOTIFY projectCreatedAtChanged)
    Q_PROPERTY(QStringList availableProjects READ availableProjects NOTIFY availableProjectsChanged)
    Q_PROPERTY(QVariantList projectList READ projectList NOTIFY projectListChanged)
    Q_PROPERTY(int projectSortMode READ projectSortMode WRITE setProjectSortMode NOTIFY projectSortModeChanged)

    // Capture & State Machine Properties
    Q_PROPERTY(QString captureState READ captureState NOTIFY captureStateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY canStartChanged)
    Q_PROPERTY(bool canStop READ canStop NOTIFY captureStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY captureStateChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY isCapturingChanged)
    Q_PROPERTY(QString elapsedTimeString READ elapsedTimeString NOTIFY elapsedTimeStringChanged)
    Q_PROPERTY(QString sensorStatus READ sensorStatus NOTIFY sensorStatusChanged)
    Q_PROPERTY(QString trackingStatus READ trackingStatus NOTIFY trackingStatusChanged)
    Q_PROPERTY(qlonglong pointsCaptured READ pointsCaptured NOTIFY pointsCapturedChanged)

    // Spatial Telemetry Properties
    Q_PROPERTY(qreal currentPoseX READ currentPoseX NOTIFY poseChanged)
    Q_PROPERTY(qreal currentPoseY READ currentPoseY NOTIFY poseChanged)
    Q_PROPERTY(qreal currentPoseZ READ currentPoseZ NOTIFY poseChanged)
    Q_PROPERTY(qreal currentYaw READ currentYaw NOTIFY poseChanged)
    Q_PROPERTY(int trajectoryPointCount READ trajectoryPointCount NOTIFY trajectoryChanged)
    Q_PROPERTY(bool hasMapData READ hasMapData NOTIFY mapDataUpdated)

    // Scanner / Profile Properties
    Q_PROPERTY(QString selectedScannerId READ selectedScannerId NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerName READ selectedScannerName NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerStatus READ selectedScannerStatus NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerDetails READ selectedScannerDetails NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerValidationNotice READ selectedScannerValidationNotice NOTIFY selectedScannerChanged)
    Q_PROPERTY(int selectedScannerIndex READ selectedScannerIndex NOTIFY selectedScannerChanged)
    Q_PROPERTY(QStringList availableScanners READ availableScanners NOTIFY availableScannersChanged)
    Q_PROPERTY(QVariantList scannerList READ scannerList NOTIFY scannerListChanged)

    // Platform & Host Metrics
    Q_PROPERTY(QString hostPlatform READ hostPlatform CONSTANT)
    Q_PROPERTY(QString hostHardware READ hostHardware CONSTANT)
    Q_PROPERTY(QString hostMemory READ hostMemory NOTIFY hostMetricsChanged)
    Q_PROPERTY(QString hostDisk READ hostDisk NOTIFY hostMetricsChanged)
    Q_PROPERTY(double hostCpuUtil READ hostCpuUtil NOTIFY hostMetricsChanged)
    Q_PROPERTY(double hostGpuUtil READ hostGpuUtil NOTIFY hostMetricsChanged)

    // Multi-Sensor & M6 Modular Scanner Architecture
    Q_PROPERTY(QVariantList activeSensors READ activeSensors NOTIFY activeSensorsChanged)
    Q_PROPERTY(int activeSensorCount READ activeSensorCount NOTIFY activeSensorsChanged)
    Q_PROPERTY(bool isMultiSensorMode READ isMultiSensorMode NOTIFY activeSensorsChanged)
    Q_PROPERTY(bool isDualSensorMode READ isDualSensorMode NOTIFY isDualSensorModeChanged)
    Q_PROPERTY(QString activeDisplaySensor READ activeDisplaySensor WRITE setActiveDisplaySensor NOTIFY activeDisplaySensorChanged)

    Q_PROPERTY(QString airyDeviceStatus READ airyDeviceStatus NOTIFY airyTelemetryChanged)
    Q_PROPERTY(QString airyAcquisitionStatus READ airyAcquisitionStatus NOTIFY airyTelemetryChanged)
    Q_PROPERTY(QString airyTrackingStatus READ airyTrackingStatus NOTIFY airyTelemetryChanged)
    Q_PROPERTY(qlonglong airyPointsCaptured READ airyPointsCaptured NOTIFY airyTelemetryChanged)
    Q_PROPERTY(qreal airyAcquisitionRate READ airyAcquisitionRate NOTIFY airyTelemetryChanged)
    Q_PROPERTY(qreal airySlamRate READ airySlamRate NOTIFY airyTelemetryChanged)

    Q_PROPERTY(QString geminiDeviceStatus READ geminiDeviceStatus NOTIFY geminiTelemetryChanged)
    Q_PROPERTY(QString geminiAcquisitionStatus READ geminiAcquisitionStatus NOTIFY geminiTelemetryChanged)
    Q_PROPERTY(QString geminiTrackingStatus READ geminiTrackingStatus NOTIFY geminiTelemetryChanged)
    Q_PROPERTY(qlonglong geminiPointsCaptured READ geminiPointsCaptured NOTIFY geminiTelemetryChanged)
    Q_PROPERTY(qreal geminiAcquisitionRate READ geminiAcquisitionRate NOTIFY geminiTelemetryChanged)
    Q_PROPERTY(qreal geminiSlamRate READ geminiSlamRate NOTIFY geminiTelemetryChanged)

    Q_PROPERTY(QString vitureDeviceStatus READ vitureDeviceStatus NOTIFY vitureTelemetryChanged)
    Q_PROPERTY(QString vitureAcquisitionStatus READ vitureAcquisitionStatus NOTIFY vitureTelemetryChanged)
    Q_PROPERTY(QString vitureTrackingStatus READ vitureTrackingStatus NOTIFY vitureTelemetryChanged)
    Q_PROPERTY(qlonglong viturePointsCaptured READ viturePointsCaptured NOTIFY vitureTelemetryChanged)
    Q_PROPERTY(qreal vitureAcquisitionRate READ vitureAcquisitionRate NOTIFY vitureTelemetryChanged)
    Q_PROPERTY(qreal vitureSlamRate READ vitureSlamRate NOTIFY vitureTelemetryChanged)

    Q_PROPERTY(QString cameraDeviceStatus READ cameraDeviceStatus NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(QString cameraAcquisitionStatus READ cameraAcquisitionStatus NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(QString cameraTrackingStatus READ cameraTrackingStatus NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(qlonglong cameraPointsCaptured READ cameraPointsCaptured NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(qreal cameraAcquisitionRate READ cameraAcquisitionRate NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(qreal cameraSlamRate READ cameraSlamRate NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(QString cameraScaleStatus READ cameraScaleStatus NOTIFY cameraTelemetryChanged)
    Q_PROPERTY(QString selectedCameraBackend READ selectedCameraBackend WRITE setSelectedCameraBackend NOTIFY selectedCameraBackendChanged)
    Q_PROPERTY(QString cameraBackendStatus READ cameraBackendStatus NOTIFY cameraBackendStatusChanged)
    Q_PROPERTY(QString cameraBackendNotice READ cameraBackendNotice NOTIFY cameraBackendNoticeChanged)

    // Device Discovery and Profiling Properties (Milestone 14)
    Q_PROPERTY(QVariantList discoveredPhysicalDevices READ discoveredPhysicalDevices NOTIFY discoveredDevicesChanged)
    Q_PROPERTY(bool isDiscovering READ isDiscovering NOTIFY isDiscoveringChanged)
    Q_PROPERTY(QString discoveryStatusText READ discoveryStatusText NOTIFY discoveryStatusTextChanged)
    Q_PROPERTY(int discoveredDeviceCount READ discoveredDeviceCount NOTIFY discoveredDevicesChanged)
    Q_PROPERTY(int unprofiledDeviceCount READ unprofiledDeviceCount NOTIFY discoveredDevicesChanged)

    // 3D Viewport Camera Properties
    Q_PROPERTY(qreal camYaw3D READ camYaw3D NOTIFY cam3DChanged)
    Q_PROPERTY(qreal camPitch3D READ camPitch3D NOTIFY cam3DChanged)
    Q_PROPERTY(qreal camDistance3D READ camDistance3D NOTIFY cam3DChanged)
    Q_PROPERTY(qreal camTargetX3D READ camTargetX3D NOTIFY cam3DChanged)
    Q_PROPERTY(qreal camTargetY3D READ camTargetY3D NOTIFY cam3DChanged)
    Q_PROPERTY(qreal camTargetZ3D READ camTargetZ3D NOTIFY cam3DChanged)

    // 2D Viewport Camera Properties
    Q_PROPERTY(qreal pan2DX READ pan2DX NOTIFY cam2DChanged)
    Q_PROPERTY(qreal pan2DY READ pan2DY NOTIFY cam2DChanged)
    Q_PROPERTY(qreal zoom2D READ zoom2D NOTIFY cam2DChanged)

    // Milestone 5.5: Orientation Diagnostic Tool
    Q_PROPERTY(bool orientationDiagOpen READ orientationDiagOpen NOTIFY orientationDiagChanged)
    Q_PROPERTY(QString orientationDiagSensor READ orientationDiagSensor NOTIFY orientationDiagChanged)
    Q_PROPERTY(qreal orientationDiagRoll READ orientationDiagRoll NOTIFY orientationDiagChanged)
    Q_PROPERTY(qreal orientationDiagPitch READ orientationDiagPitch NOTIFY orientationDiagChanged)
    Q_PROPERTY(qreal orientationDiagYaw READ orientationDiagYaw NOTIFY orientationDiagChanged)
    Q_PROPERTY(QString orientationDiagMatrixStr READ orientationDiagMatrixStr NOTIFY orientationDiagChanged)
    Q_PROPERTY(QString orientationDiagTransformStr READ orientationDiagTransformStr NOTIFY orientationDiagChanged)
    Q_PROPERTY(bool orientationDiagIsFrozen READ orientationDiagIsFrozen NOTIFY orientationDiagChanged)
    Q_PROPERTY(QString orientationDiagStatusMsg READ orientationDiagStatusMsg NOTIFY orientationDiagChanged)

    // Milestone 14.5: Sensor Ecosystem & Catalog Properties
    Q_PROPERTY(int ecosystemModelCount READ ecosystemModelCount NOTIFY ecosystemChanged)
    Q_PROPERTY(int ecosystemVendorCount READ ecosystemVendorCount NOTIFY ecosystemChanged)

    // Milestone 10: Multi-Node Session Infrastructure
    Q_PROPERTY(QString nodeUuid READ nodeUuid NOTIFY nodeIdentityChanged)
    Q_PROPERTY(QString nodeDisplayName READ nodeDisplayName NOTIFY nodeIdentityChanged)
    Q_PROPERTY(QString nodePlatform READ nodePlatform NOTIFY nodeIdentityChanged)
    Q_PROPERTY(QString nodeHardware READ nodeHardware NOTIFY nodeIdentityChanged)
    Q_PROPERTY(QString nodeRole READ nodeRole NOTIFY multiNodeSessionChanged)
    Q_PROPERTY(QString nodeStatus READ nodeStatus NOTIFY multiNodeSessionChanged)
    Q_PROPERTY(QString activeMultiNodeSessionId READ activeMultiNodeSessionId NOTIFY multiNodeSessionChanged)
    Q_PROPERTY(bool isMultiNodeSessionActive READ isMultiNodeSessionActive NOTIFY multiNodeSessionChanged)
    Q_PROPERTY(bool isCoordinator READ isCoordinator NOTIFY multiNodeSessionChanged)
    Q_PROPERTY(QVariantList discoveredNodes READ discoveredNodes NOTIFY discoveredNodesChanged)
    Q_PROPERTY(QVariantList sessionMembers READ sessionMembers NOTIFY sessionMembersChanged)
    Q_PROPERTY(int selectedNodeViewIndex READ selectedNodeViewIndex WRITE setSelectedNodeViewIndex NOTIFY selectedNodeViewChanged)
    Q_PROPERTY(QString selectedNodeViewName READ selectedNodeViewName NOTIFY selectedNodeViewChanged)
    Q_PROPERTY(QString multiNodeNotice READ multiNodeNotice NOTIFY multiNodeNoticeChanged)
    Q_PROPERTY(bool cslamColorByScanner READ cslamColorByScanner WRITE setCslamColorByScanner NOTIFY cslamStateChanged)
    Q_PROPERTY(int cslamScannerCount READ cslamScannerCount NOTIFY cslamStateChanged)
    Q_PROPERTY(int cslamSnappedCount READ cslamSnappedCount NOTIFY cslamStateChanged)
    Q_PROPERTY(qlonglong cslamMergedPointCount READ cslamMergedPointCount NOTIFY cslamStateChanged)
    Q_PROPERTY(QVariantList cslamNodesList READ cslamNodesList NOTIFY cslamStateChanged)
    Q_PROPERTY(QString cslamStatusSummary READ cslamStatusSummary NOTIFY cslamStateChanged)

    // Milestone 9: Cross-Sensor Offline Registration
    Q_PROPERTY(bool isRegistering READ isRegistering NOTIFY registrationStateChanged)
    Q_PROPERTY(QString registrationStatus READ registrationStatus NOTIFY registrationStateChanged)
    Q_PROPERTY(QVariantList registrationList READ registrationList NOTIFY registrationListChanged)
    Q_PROPERTY(QString activeRegistrationId READ activeRegistrationId NOTIFY activeRegistrationChanged)
    Q_PROPERTY(bool isRegistrationActive READ isRegistrationActive NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qreal registrationOverlapPercent READ registrationOverlapPercent NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qlonglong registrationInlierCount READ registrationInlierCount NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qlonglong registrationTotalEvaluated READ registrationTotalEvaluated NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qreal registrationRmseMm READ registrationRmseMm NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qreal registrationFitness READ registrationFitness NOTIFY activeRegistrationChanged)
    Q_PROPERTY(qreal registrationScale READ registrationScale NOTIFY activeRegistrationChanged)
    Q_PROPERTY(QString registrationScalePolicy READ registrationScalePolicy NOTIFY activeRegistrationChanged)
    Q_PROPERTY(QString registrationTransformStr READ registrationTransformStr NOTIFY activeRegistrationChanged)
    Q_PROPERTY(int registrationViewMode READ registrationViewMode WRITE setRegistrationViewMode NOTIFY registrationViewModeChanged)
    Q_PROPERTY(int registrationColorMode READ registrationColorMode WRITE setRegistrationColorMode NOTIFY registrationColorModeChanged)
    Q_PROPERTY(QString registrationRefCaptureId READ registrationRefCaptureId NOTIFY activeRegistrationChanged)
    Q_PROPERTY(QString registrationMovCaptureId READ registrationMovCaptureId NOTIFY activeRegistrationChanged)
    Q_PROPERTY(QVariantList availableSessionCaptures READ availableSessionCaptures NOTIFY availableSessionCapturesChanged)

    // Milestone 12: Point Cloud Cleanup, Post-Processing & Multi-Format Export
    Q_PROPERTY(QString cleanupStatus READ cleanupStatus NOTIFY cleanupStateChanged)
    Q_PROPERTY(bool hasCleanupPreview READ hasCleanupPreview NOTIFY cleanupStateChanged)
    Q_PROPERTY(qlonglong cleanupInputPoints READ cleanupInputPoints NOTIFY cleanupStateChanged)
    Q_PROPERTY(qlonglong cleanupOutputPoints READ cleanupOutputPoints NOTIFY cleanupStateChanged)
    Q_PROPERTY(qlonglong cleanupRemovedPoints READ cleanupRemovedPoints NOTIFY cleanupStateChanged)
    Q_PROPERTY(qreal cleanupGroundTiltDeg READ cleanupGroundTiltDeg NOTIFY cleanupStateChanged)
    Q_PROPERTY(bool liveCleanupEnabled READ liveCleanupEnabled WRITE setLiveCleanupEnabled NOTIFY cleanupStateChanged)
    Q_PROPERTY(bool liveGroundLockEnabled READ liveGroundLockEnabled WRITE setLiveGroundLockEnabled NOTIFY cleanupStateChanged)
    Q_PROPERTY(qreal liveCleanupTimeMs READ liveCleanupTimeMs NOTIFY cleanupStateChanged)
    Q_PROPERTY(QString lastExportPath READ lastExportPath NOTIFY exportStateChanged)
    Q_PROPERTY(QString exportStatus READ exportStatus NOTIFY exportStateChanged)

    // Milestones 7 & 8: Rigid Extrinsic Calibration & Multi-Sensor Spatial Fusion
    Q_PROPERTY(QString extrinsicCalibrationStatus READ extrinsicCalibrationStatus NOTIFY fusionStateChanged)
    Q_PROPERTY(QString fusionStatus READ fusionStatus NOTIFY fusionStateChanged)
    Q_PROPERTY(bool isFusionActive READ isFusionActive NOTIFY fusionStateChanged)
    Q_PROPERTY(qlonglong fusedPointCount READ fusedPointCount NOTIFY fusionStateChanged)
    Q_PROPERTY(qlonglong fusedColorizedPoints READ fusedColorizedPoints NOTIFY fusionStateChanged)
    Q_PROPERTY(qreal extrinsicRmseMm READ extrinsicRmseMm NOTIFY fusionStateChanged)
    Q_PROPERTY(qreal extrinsicOverlapPercent READ extrinsicOverlapPercent NOTIFY fusionStateChanged)
    Q_PROPERTY(qreal extrinsicConfidence READ extrinsicConfidence NOTIFY fusionStateChanged)
    Q_PROPERTY(QString extrinsicTransformSummary READ extrinsicTransformSummary NOTIFY fusionStateChanged)
    Q_PROPERTY(QString activeAnchorSensor READ activeAnchorSensor NOTIFY fusionStateChanged)
    Q_PROPERTY(QString activeAnchorName READ activeAnchorName NOTIFY fusionStateChanged)
    Q_PROPERTY(QString activeAnchorRationale READ activeAnchorRationale NOTIFY fusionStateChanged)
    Q_PROPERTY(bool isAnchorFailoverActive READ isAnchorFailoverActive NOTIFY fusionStateChanged)
    Q_PROPERTY(qreal activeAnchorScore READ activeAnchorScore NOTIFY fusionStateChanged)

public:
    explicit QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                       std::shared_ptr<platform::IPlatformAdapter> platform,
                       std::shared_ptr<core::drivers::airy::AirySensorAdapter> airyAdapter = nullptr,
                       std::shared_ptr<core::slam::airy::AiryLioBackend> airyBackend = nullptr,
                       std::shared_ptr<core::drivers::gemini::GeminiSensorAdapter> geminiAdapter = nullptr,
                       std::shared_ptr<core::slam::gemini::GeminiRgbdBackend> geminiBackend = nullptr,
                       std::shared_ptr<core::drivers::viture::VitureSensorAdapter> vitureAdapter = nullptr,
                       std::shared_ptr<core::slam::viture::VitureVioBackend> vitureBackend = nullptr,
                       QObject *parent = nullptr);
    ~QmlBridge() override;

    // Getters: Projects
    QString projectName() const { return projectName_; }
    QString projectId() const { return projectId_; }
    QString projectDescription() const { return projectDescription_; }
    QString projectCreatedAt() const { return projectCreatedAt_; }
    QStringList availableProjects() const { return availableProjects_; }
    QVariantList projectList() const { return projectList_; }
    int projectSortMode() const { return projectSortMode_; }

    // Getters: State & Capture
    QString captureState() const { return captureState_; }
    bool canStart() const {
        if ((captureState_ != "READY" && captureState_ != "SAVED") || projectId_.isEmpty()) return false;
        if (activeSensors_.empty()) return false;
        for (const auto& s : activeSensors_) {
            if (!s.is_capturable) return false;
        }
        return true;
    }
    bool canStop() const { return isCapturing_; }
    bool canSave() const { return canSave_; }
    bool isCapturing() const { return isCapturing_; }
    QString elapsedTimeString() const { return elapsedTimeString_; }
    QString sensorStatus() const { return sensorStatus_; }
    QString trackingStatus() const { return trackingStatus_; }
    qlonglong pointsCaptured() const { return pointsCaptured_; }

    // Getters: Spatial Telemetry
    qreal currentPoseX() const { return currentPoseX_; }
    qreal currentPoseY() const { return currentPoseY_; }
    qreal currentPoseZ() const { return currentPoseZ_; }
    qreal currentYaw() const { return currentYaw_; }
    int trajectoryPointCount() const { return static_cast<int>(trajectory_.size()); }
    bool hasMapData() const { return hasMapData_ || pointsCaptured_ > 0; }

    // Getters: Scanners
    QString selectedScannerId() const { return selectedScannerId_; }
    QString selectedScannerName() const { return selectedScannerName_; }
    QString selectedScannerStatus() const { return selectedScannerStatus_; }
    QString selectedScannerDetails() const { return selectedScannerDetails_; }
    QString selectedScannerValidationNotice() const { return selectedScannerValidationNotice_; }
    int selectedScannerIndex() const;
    QStringList availableScanners() const { return availableScanners_; }
    QVariantList scannerList() const { return scannerList_; }

    // Getters: Host
    QString hostPlatform() const;
    QString hostHardware() const;
    QString hostMemory() const;
    QString hostDisk() const;
    double hostCpuUtil() const;
    double hostGpuUtil() const;

    // Getters: Multi-Sensor & Modular Platform
    QVariantList activeSensors() const;
    int activeSensorCount() const { return static_cast<int>(activeSensors_.size()); }
    bool isMultiSensorMode() const { return activeSensors_.size() > 1; }
    bool isDualSensorMode() const { return activeSensors_.size() > 1; }
    QString activeDisplaySensor() const { return activeDisplaySensor_; }

    QString airyDeviceStatus() const { return airyDeviceStatus_; }
    QString airyAcquisitionStatus() const { return airyAcquisitionStatus_; }
    QString airyTrackingStatus() const { return airyTrackingStatus_; }
    qlonglong airyPointsCaptured() const { return airyPointsCaptured_; }
    qreal airyAcquisitionRate() const { return airyAcquisitionRate_; }
    qreal airySlamRate() const { return airySlamRate_; }

    QString geminiDeviceStatus() const { return geminiDeviceStatus_; }
    QString geminiAcquisitionStatus() const { return geminiAcquisitionStatus_; }
    QString geminiTrackingStatus() const { return geminiTrackingStatus_; }
    qlonglong geminiPointsCaptured() const { return geminiPointsCaptured_; }
    qreal geminiAcquisitionRate() const { return geminiAcquisitionRate_; }
    qreal geminiSlamRate() const { return geminiSlamRate_; }

    QString vitureDeviceStatus() const { return vitureDeviceStatus_; }
    QString vitureAcquisitionStatus() const { return vitureAcquisitionStatus_; }
    QString vitureTrackingStatus() const { return vitureTrackingStatus_; }
    qlonglong viturePointsCaptured() const { return viturePointsCaptured_; }
    qreal vitureAcquisitionRate() const { return vitureAcquisitionRate_; }
    qreal vitureSlamRate() const { return vitureSlamRate_; }

    QString cameraDeviceStatus() const { return cameraDeviceStatus_; }
    QString cameraAcquisitionStatus() const { return cameraAcquisitionStatus_; }
    QString cameraTrackingStatus() const { return cameraTrackingStatus_; }
    qlonglong cameraPointsCaptured() const { return cameraPointsCaptured_; }
    qreal cameraAcquisitionRate() const { return cameraAcquisitionRate_; }
    qreal cameraSlamRate() const { return cameraSlamRate_; }
    QString cameraScaleStatus() const { return cameraScaleStatus_; }
    QString selectedCameraBackend() const { return selectedCameraBackend_; }
    void setSelectedCameraBackend(const QString& name);
    QString cameraBackendStatus() const;
    QString cameraBackendNotice() const;
    Q_INVOKABLE void selectCameraBackend(const QString& backendName);

    // Getters: Device Discovery (Milestone 14)
    QVariantList discoveredPhysicalDevices() const { return discoveredPhysicalDevices_; }
    bool isDiscovering() const { return isDiscovering_; }
    QString discoveryStatusText() const { return discoveryStatusText_; }
    int discoveredDeviceCount() const { return discoveredDeviceCount_; }
    int unprofiledDeviceCount() const { return unprofiledDeviceCount_; }

    // Getters: Universal Sensor Ecosystem & Catalog (Milestone 14.5)
    int ecosystemModelCount() const;
    int ecosystemVendorCount() const;

    // Getters: 3D Camera
    qreal camYaw3D() const { return camYaw3D_; }
    qreal camPitch3D() const { return camPitch3D_; }
    qreal camDistance3D() const { return camDistance3D_; }
    qreal camTargetX3D() const { return camTargetX3D_; }
    qreal camTargetY3D() const { return camTargetY3D_; }
    qreal camTargetZ3D() const { return camTargetZ3D_; }

    // Getters: 2D Camera
    qreal pan2DX() const { return pan2DX_; }
    qreal pan2DY() const { return pan2DY_; }
    qreal zoom2D() const { return zoom2D_; }

    // Getters: Milestone 5.5 Orientation Diagnostic
    bool orientationDiagOpen() const { return orientationDiagOpen_; }
    QString orientationDiagSensor() const { return orientationDiagSensor_; }
    qreal orientationDiagRoll() const { return orientationDiagRoll_; }
    qreal orientationDiagPitch() const { return orientationDiagPitch_; }
    qreal orientationDiagYaw() const { return orientationDiagYaw_; }
    QString orientationDiagMatrixStr() const { return orientationDiagMatrixStr_; }
    QString orientationDiagTransformStr() const { return orientationDiagTransformStr_; }
    bool orientationDiagIsFrozen() const { return orientationDiagSensor_ == "AIRY"; }
    QString orientationDiagStatusMsg() const { return orientationDiagStatusMsg_; }

    // Getters: Milestone 10 Multi-Node Session Infrastructure
    QString nodeUuid() const;
    QString nodeDisplayName() const;
    QString nodePlatform() const;
    QString nodeHardware() const;
    QString nodeRole() const;
    QString nodeStatus() const;
    QString activeMultiNodeSessionId() const;
    bool isMultiNodeSessionActive() const;
    bool isCoordinator() const;
    QVariantList discoveredNodes() const;
    QVariantList sessionMembers() const;
    int selectedNodeViewIndex() const { return selectedNodeViewIndex_; }
    void setSelectedNodeViewIndex(int index);
    QString selectedNodeViewName() const;
    QString multiNodeNotice() const { return multiNodeNotice_; }
    bool cslamColorByScanner() const { return cslamColorByScanner_; }
    void setCslamColorByScanner(bool enabled);
    int cslamScannerCount() const;
    int cslamSnappedCount() const;
    qlonglong cslamMergedPointCount() const;
    QVariantList cslamNodesList() const;
    QString cslamStatusSummary() const { return cslamStatusSummary_; }

    // Getters: Milestone 9 Cross-Sensor Offline Registration
    bool isRegistering() const { return isRegistering_; }
    QString registrationStatus() const { return registrationStatus_; }
    QVariantList registrationList() const { return registrationList_; }
    QString activeRegistrationId() const { return QString::fromStdString(activeRegistration_.registration_id); }
    bool isRegistrationActive() const { return hasActiveRegistration_; }
    qreal registrationOverlapPercent() const { return activeRegistration_.overlap_ratio * 100.0; }
    qlonglong registrationInlierCount() const { return activeRegistration_.inlier_count; }
    qlonglong registrationTotalEvaluated() const { return activeRegistration_.total_points_evaluated; }
    qreal registrationRmseMm() const { return activeRegistration_.inlier_rmse_m * 1000.0; }
    qreal registrationFitness() const { return activeRegistration_.fitness_score; }
    qreal registrationScale() const { return activeRegistration_.scale_factor; }
    QString registrationScalePolicy() const { return QString::fromStdString(core::schemas::scalePolicyToString(activeRegistration_.scale_policy)); }
    QString registrationTransformStr() const;
    int registrationViewMode() const { return registrationViewMode_; }
    void setRegistrationViewMode(int mode);
    int registrationColorMode() const { return registrationColorMode_; }
    void setRegistrationColorMode(int mode);
    QString registrationRefCaptureId() const { return QString::fromStdString(activeRegistration_.reference_capture_id); }
    QString registrationMovCaptureId() const { return QString::fromStdString(activeRegistration_.moving_capture_id); }
    QVariantList availableSessionCaptures() const { return availableSessionCaptures_; }

    // Getters: Milestone 12 Cleanup & Export
    QString cleanupStatus() const { return cleanupStatus_; }
    bool hasCleanupPreview() const { return hasCleanupPreview_; }
    qlonglong cleanupInputPoints() const { return cleanupInputPoints_; }
    qlonglong cleanupOutputPoints() const { return cleanupOutputPoints_; }
    qlonglong cleanupRemovedPoints() const { return cleanupRemovedPoints_; }
    qreal cleanupGroundTiltDeg() const { return cleanupGroundTiltDeg_; }
    bool liveCleanupEnabled() const { return liveCleanupEnabled_; }
    void setLiveCleanupEnabled(bool enabled);
    bool liveGroundLockEnabled() const { return liveGroundLockEnabled_; }
    void setLiveGroundLockEnabled(bool enabled);
    qreal liveCleanupTimeMs() const { return liveCleanupTimeMs_; }
    QString lastExportPath() const { return lastExportPath_; }
    QString exportStatus() const { return exportStatus_; }

    // Getters: Milestones 7 & 8 Extrinsic Calibration & Multi-Sensor Fusion
    QString extrinsicCalibrationStatus() const { return extrinsicCalibrationStatus_; }
    QString fusionStatus() const { return fusionStatus_; }
    bool isFusionActive() const { return isFusionActive_; }
    qlonglong fusedPointCount() const { return static_cast<qlonglong>(fusedMapPoints_.size()); }
    qlonglong fusedColorizedPoints() const { return fusedColorizedPoints_; }
    qreal extrinsicRmseMm() const { return extrinsicRmseMm_; }
    qreal extrinsicOverlapPercent() const { return extrinsicOverlapPercent_; }
    qreal extrinsicConfidence() const { return extrinsicConfidence_; }
    QString extrinsicTransformSummary() const { return extrinsicTransformSummary_; }
    QString activeAnchorSensor() const { return activeAnchorSensor_; }
    QString activeAnchorName() const { return activeAnchorName_; }
    QString activeAnchorRationale() const { return activeAnchorRationale_; }
    bool isAnchorFailoverActive() const { return isAnchorFailoverActive_; }
    qreal activeAnchorScore() const { return activeAnchorScore_; }

    // Q_INVOKABLE: Project Operations
    Q_INVOKABLE bool createProject(const QString& name, const QString& description);
    Q_INVOKABLE bool openProject(const QString& projectId);
    Q_INVOKABLE void refreshData();
    Q_INVOKABLE void setProjectSortMode(int mode);

    // Q_INVOKABLE: Scanner Selection & Modular Composition
    Q_INVOKABLE void selectScannerByIndex(int index);
    Q_INVOKABLE void selectScannerById(const QString& scannerId);
    Q_INVOKABLE void setActiveDisplaySensor(const QString& sensorName);
    Q_INVOKABLE QVariantList getAvailableDeviceProfiles() const;
    Q_INVOKABLE bool saveCustomScannerConfig(const QString& name, const QStringList& profileIds);

    // Q_INVOKABLE: Device Discovery and Onboarding (Milestone 14)
    Q_INVOKABLE QVariantList discoverHardwareDevices();
    Q_INVOKABLE void refreshDiscovery();
    Q_INVOKABLE QVariantMap createProfileForDevice(const QString& deviceId);
    Q_INVOKABLE void selectProfileForCapture(const QString& profileId);

    // Q_INVOKABLE: Universal Sensor Ecosystem & Catalog (Milestone 14.5)
    Q_INVOKABLE QStringList getEcosystemManufacturers() const;
    Q_INVOKABLE QStringList getSensorTypesForManufacturer(const QString& vendor = "") const;
    Q_INVOKABLE QVariantList getEcosystemModels(const QString& vendor = "", const QString& sensorType = "", const QString& searchQuery = "") const;
    Q_INVOKABLE QVariantMap getModelDetails(const QString& modelId) const;
    Q_INVOKABLE QVariantMap createTemplateProfileFromModel(const QString& modelId);

    // Q_INVOKABLE: Capture Control State Machine
    Q_INVOKABLE bool startCapture();
    Q_INVOKABLE bool stopCapture();
    Q_INVOKABLE bool saveCapture();

    // Q_INVOKABLE: 3D and 2D Geometry Access
    Q_INVOKABLE QVariantList getDisplayPoints(int maxPoints = 8000) const;
    Q_INVOKABLE QVariantList getTrajectoryPoints() const;
    Q_INVOKABLE bool loadSavedCapture(const QString& captureId);

    // Q_INVOKABLE: 3D Camera Interaction
    Q_INVOKABLE void orbit3D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void pan3D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void zoom3D(qreal deltaFactor);
    Q_INVOKABLE void recenter3D();
    Q_INVOKABLE void autoFitCameraToMapBounds();
    Q_INVOKABLE void setPreset3D(const QString& preset);

    // Q_INVOKABLE: 2D Camera Interaction
    Q_INVOKABLE void pan2D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void zoom2DByFactor(qreal factor);
    Q_INVOKABLE void recenter2D();

    // Q_INVOKABLE: Milestone 5.5 Orientation Diagnostic
    Q_INVOKABLE void openOrientationDiagnostic(const QString& sensorId = "");
    Q_INVOKABLE void closeOrientationDiagnostic();
    Q_INVOKABLE void setOrientationDiagSensor(const QString& sensorId);
    Q_INVOKABLE void setOrientationDiagRotation(qreal rollDeg, qreal pitchDeg, qreal yawDeg);
    Q_INVOKABLE void stepOrientationDiagRotation(const QString& axis, qreal deltaDeg);
    Q_INVOKABLE void resetOrientationDiagRotation();
    Q_INVOKABLE bool autoLevelOrientationDiag();
    Q_INVOKABLE bool acceptOrientationDiagRotation();

    // Q_INVOKABLE: Milestone 10 Multi-Node Session Operations
    Q_INVOKABLE void setNodeDisplayName(const QString& name);
    Q_INVOKABLE void refreshMultiNodeDiscovery();
    Q_INVOKABLE void addManualPeer(const QString& ip, int port = 9110);
    Q_INVOKABLE bool createMultiNodeSession(const QString& sessionName);
    Q_INVOKABLE bool inviteNodeToSession(const QString& nodeUuid);
    Q_INVOKABLE bool joinMultiNodeSession(const QString& sessionId, const QString& coordIp, int coordPort = 9111);
    Q_INVOKABLE bool leaveMultiNodeSession();
    Q_INVOKABLE bool startCoordinatedCapture();
    Q_INVOKABLE bool stopCoordinatedCapture();
    Q_INVOKABLE void selectNodeView(int index);
    Q_INVOKABLE int triggerCslamSnap();
    Q_INVOKABLE void addSimulatedCslamScanner(const QString& scannerName, bool overlapping);
    Q_INVOKABLE void clearCslamScanners();
    Q_INVOKABLE void refreshCslamMergedCloud();

    // Q_INVOKABLE: Milestone 9 Offline Registration Operations (Intra-Project & Cross-Project)
    Q_INVOKABLE QVariantList getProjectClouds(const QString& projectId);
    Q_INVOKABLE void runRegistration(const QString& refProjectId, const QString& refCloudId,
                                    const QString& movProjectId, const QString& movCloudId,
                                    int scalePolicyMode = 0);
    Q_INVOKABLE void runCaptureRegistration(const QString& refCaptureId, const QString& movCaptureId, int scalePolicyMode = 0);
    Q_INVOKABLE bool saveRegistrationAsNewProject(const QString& newProjectName, const QString& newProjectDesc = "");
    Q_INVOKABLE void loadRegistration(const QString& regId);
    Q_INVOKABLE void refreshRegistrations();
    Q_INVOKABLE void refreshSessionCaptures();
    Q_INVOKABLE void saveRegistrationAsSessionMap(const QString& regId);
    Q_INVOKABLE void clearActiveRegistration();

    // Q_INVOKABLE: Milestone 12 Point Cloud Cleanup, Post-Processing & Export
    Q_INVOKABLE bool runPointCloudCleanup(bool enableVoxel, double voxelLeafM,
                                          bool enableSor, int sorMeanK, double sorStdMul,
                                          bool enableRor, double rorRadiusM, int rorMinNeighbors,
                                          bool enableGroundAlign, bool removeGroundPlane = false);
    Q_INVOKABLE void resetPointCloudCleanup();
    Q_INVOKABLE bool commitCleanedPointCloud();
    Q_INVOKABLE QString exportActivePointCloud(const QString& formatStr,
                                               const QString& customOutputPath = "",
                                               double meshGridResM = 0.04);

    // Q_INVOKABLE: Milestones 7 & 8 Extrinsic Calibration & Multi-Sensor Spatial Fusion
    Q_INVOKABLE bool calibrateActiveScannerExtrinsics();
    Q_INVOKABLE bool fuseActiveMultiSensorMaps(double voxelLeafM = 0.015,
                                               bool transferOpticalRgb = true,
                                               bool diagnosticRoleColors = false);
    Q_INVOKABLE bool saveFusedMapToProject();
    Q_INVOKABLE void setSensorHealthOverride(const QString& sensorId, bool healthy, bool degraded = false);
    Q_INVOKABLE void clearSensorHealthOverrides();

    // Direct PCD serialization & deserialization helpers
    void saveMapPcd(const std::string& filepath, const std::vector<core::schemas::PointXYZI>& points) const;
    bool loadMapPcd(const std::string& filepath, std::vector<core::schemas::PointXYZI>& out_points);

signals:
    void cleanupStateChanged();
    void exportStateChanged();
    void fusionStateChanged();
    void registrationStateChanged();
    void registrationListChanged();
    void activeRegistrationChanged();
    void registrationViewModeChanged();
    void registrationColorModeChanged();
    void availableSessionCapturesChanged();

    void projectNameChanged();
    void projectIdChanged();
    void projectDescriptionChanged();
    void projectCreatedAtChanged();
    void availableProjectsChanged();
    void projectListChanged();
    void projectSortModeChanged();
    void captureStateChanged();
    void canStartChanged();
    void isCapturingChanged();
    void canSaveChanged();
    void elapsedTimeStringChanged();
    void sensorStatusChanged();
    void trackingStatusChanged();
    void pointsCapturedChanged();
    void poseChanged();
    void trajectoryChanged();
    void mapDataUpdated();
    void selectedScannerChanged();
    void availableScannersChanged();
    void scannerListChanged();
    void hostMetricsChanged();
    void cam3DChanged();
    void cam2DChanged();
    void errorOccurred(const QString& errorTitle, const QString& errorMessage);
    void notification(const QString& title, const QString& message);
    void isDualSensorModeChanged();
    void activeSensorsChanged();
    void activeDisplaySensorChanged();
    void airyTelemetryChanged();
    void geminiTelemetryChanged();
    void vitureTelemetryChanged();
    void cameraTelemetryChanged();
    void selectedCameraBackendChanged();
    void cameraBackendStatusChanged();
    void cameraBackendNoticeChanged();
    void orientationDiagChanged();
    void discoveredDevicesChanged();
    void isDiscoveringChanged();
    void discoveryStatusTextChanged();
    void ecosystemChanged();
    void nodeIdentityChanged();
    void multiNodeSessionChanged();
    void discoveredNodesChanged();
    void sessionMembersChanged();
    void selectedNodeViewChanged();
    void multiNodeNoticeChanged();
    void cslamStateChanged();
    void sessionInviteReceived(const QString& sessionId, const QString& coordinatorUuid, const QString& projectId);

private slots:
    void onTimerTick();
    void onTelemetryTick();

private:
    void rebuildActiveSensorsForSelection();
    void updateLiveMultiSensorFusion(bool realTimeMode = true);
    void invalidateDisplayPointsCache() const { displayPointsCacheValid_ = false; }

    std::shared_ptr<core::storage::StorageEngine> storage_;
    std::shared_ptr<platform::IPlatformAdapter> platform_;
    std::shared_ptr<core::drivers::airy::AirySensorAdapter> airyAdapter_;
    std::shared_ptr<core::slam::airy::AiryLioBackend> airyBackend_;
    std::shared_ptr<core::drivers::gemini::GeminiSensorAdapter> geminiAdapter_;
    std::shared_ptr<core::slam::gemini::GeminiRgbdBackend> geminiBackend_;
    std::shared_ptr<core::drivers::viture::VitureSensorAdapter> vitureAdapter_;
    std::shared_ptr<core::slam::viture::VitureVioBackend> vitureBackend_;
    std::shared_ptr<core::drivers::uvc::GenericUvcSensorAdapter> cameraAdapter_;
    std::shared_ptr<core::interfaces::INeuralReconstructionBackend> cameraBackend_;

    // Project state
    QString projectName_{"No Project Loaded"};
    QString projectId_{""};
    QString projectDescription_{""};
    QString projectCreatedAt_{""};
    QString currentSessionId_{""};
    QStringList availableProjects_;
    QVariantList projectList_;
    int projectSortMode_{0}; // 0: Date Descending (Newest First, default), 1: Date Ascending, 2: Name A-Z, 3: Name Z-A
    void sortProjectList();

    // Scanner state
    QString selectedScannerId_{""};
    QString selectedScannerName_{""};
    QString selectedScannerStatus_{""};
    QString selectedScannerDetails_{""};
    QString selectedScannerValidationNotice_{""};
    QStringList availableScanners_;
    QVariantList scannerList_;

    // Capture state
    QString captureState_{"NO_PROJECT"};
    bool isCapturing_{false};
    bool canSave_{false};
    QString elapsedTimeString_{"00:00:00"};
    QString sensorStatus_{"STANDBY"};
    QString trackingStatus_{"IDLE"};
    qlonglong pointsCaptured_{0};

    // Live Spatial Data & Geometry
    qreal currentPoseX_{0.0};
    qreal currentPoseY_{0.0};
    qreal currentPoseZ_{0.0};
    qreal currentYaw_{0.0};
    bool hasMapData_{false};
    std::vector<core::schemas::PointXYZI> mapPoints_;
    std::vector<core::schemas::Pose3D> trajectory_;
    mutable QVariantList cachedDisplayPoints_;
    mutable bool displayPointsCacheValid_{false};
    mutable int cachedMaxPoints_{0};
    mutable size_t cachedMapPointsSize_{0};

    // M6 Modular Multi-Sensor Platform Buffers & States
    std::vector<SensorRuntimeContext> activeSensors_;
    bool isDualSensorMode_{false};
    QString activeDisplaySensor_{"AIRY"};

    QString airyDeviceStatus_{"STANDBY"};
    QString airyAcquisitionStatus_{"STOPPED"};
    QString airyTrackingStatus_{"IDLE"};
    qlonglong airyPointsCaptured_{0};
    qreal airyAcquisitionRate_{0.0};
    qreal airySlamRate_{0.0};
    std::vector<core::schemas::PointXYZI> airyMapPoints_;
    std::vector<core::schemas::Pose3D> airyTrajectory_;

    QString geminiDeviceStatus_{"STANDBY"};
    QString geminiAcquisitionStatus_{"STOPPED"};
    QString geminiTrackingStatus_{"IDLE"};
    qlonglong geminiPointsCaptured_{0};
    qreal geminiAcquisitionRate_{0.0};
    qreal geminiSlamRate_{0.0};
    std::vector<core::schemas::PointXYZI> geminiMapPoints_;
    std::vector<core::schemas::Pose3D> geminiTrajectory_;

    QString vitureDeviceStatus_{"STANDBY"};
    QString vitureAcquisitionStatus_{"STOPPED"};
    QString vitureTrackingStatus_{"IDLE"};
    qlonglong viturePointsCaptured_{0};
    qreal vitureAcquisitionRate_{0.0};
    qreal vitureSlamRate_{0.0};
    std::vector<core::schemas::PointXYZI> vitureMapPoints_;
    std::vector<core::schemas::Pose3D> vitureTrajectory_;

    QString cameraDeviceStatus_{"STANDBY"};
    QString cameraAcquisitionStatus_{"STOPPED"};
    QString cameraTrackingStatus_{"IDLE"};
    qlonglong cameraPointsCaptured_{0};
    qreal cameraAcquisitionRate_{0.0};
    qreal cameraSlamRate_{0.0};
    QString cameraScaleStatus_{"MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED"};
    std::vector<core::schemas::PointXYZI> cameraMapPoints_;
    std::vector<core::schemas::Pose3D> cameraTrajectory_;

    // 3D Camera state
    qreal camYaw3D_{45.0};
    qreal camPitch3D_{30.0};
    qreal camDistance3D_{8.0};
    qreal camTargetX3D_{0.0};
    qreal camTargetY3D_{0.0};
    qreal camTargetZ3D_{0.0};

    // 2D Camera state
    qreal pan2DX_{0.0};
    qreal pan2DY_{0.0};
    qreal zoom2D_{40.0};

    QTimer* timer_{nullptr};
    QTimer* telemetryTimer_{nullptr};
    int elapsed_seconds_{0};

    // Milestone 5.5 Orientation Diagnostic state
    bool orientationDiagOpen_{false};
    QString orientationDiagSensor_{"GEMINI"};
    qreal orientationDiagRoll_{0.0};
    qreal orientationDiagPitch_{0.0};
    qreal orientationDiagYaw_{0.0};
    double orientationDiagR_[3][3]{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
    QString orientationDiagMatrixStr_{"[ +1.000, +0.000, +0.000 ]\n[ +0.000, +1.000, +0.000 ]\n[ +0.000, +0.000, +1.000 ]"};
    QString orientationDiagTransformStr_{"X_av = (+1.000)*X\nY_av = (+1.000)*Y\nZ_av = (+1.000)*Z"};
    QString orientationDiagStatusMsg_{"Adjust Roll/Pitch/Yaw until floor is horizontal and +Z is UP."};
    void updateOrientationDiagMatrix();

    // Hardware Discovery State (Milestone 14)
    QVariantList discoveredPhysicalDevices_;
    bool isDiscovering_{false};
    QString discoveryStatusText_{"Ready to scan interfaces."};
    int discoveredDeviceCount_{0};
    int unprofiledDeviceCount_{0};

    // Camera Neural Reconstruction Backend State (Milestone 14.6)
    QString selectedCameraBackend_{"LingBotStreamingBackend"};

    // Milestone 10: Multi-Node Session Infrastructure & Real-Time C-SLAM State
    std::shared_ptr<core::multi_node::MultiNodeSessionOrchestrator> multiNodeOrchestrator_;
    int selectedNodeViewIndex_{0}; // 0: Local, -1: Real-Time Merged C-SLAM, >0: Specific Remote Node
    QString multiNodeNotice_{"Single-node standalone mode."};
    bool cslamColorByScanner_{true};
    int simulatedCslamCounter_{0};
    QString cslamStatusSummary_{"Ready for multi-scanner collaborative mapping."};

    // Milestone 9: Cross-Sensor Offline Registration State
    std::unique_ptr<core::registration::RegistrationEngine> registrationEngine_;
    bool isRegistering_{false};
    QString registrationStatus_{"Ready"};
    QVariantList registrationList_;
    QVariantList availableSessionCaptures_;
    core::schemas::RegistrationResult activeRegistration_;
    bool hasActiveRegistration_{false};
    int registrationViewMode_{0}; // 0: Combined, 1: Reference Only, 2: Moving Only
    int registrationColorMode_{1}; // 1: True Merged RGB, 0: Differentiated (Cyan/Amber)
    std::vector<core::schemas::PointXYZI> regRefPoints_;
    std::vector<core::schemas::PointXYZI> regMovPoints_;
    std::vector<core::schemas::PointXYZI> regCombinedPointsDiff_;
    std::vector<core::schemas::PointXYZI> regCombinedPointsOrig_;
    void updateRegistrationDisplayPoints();

    // Milestone 12: Point Cloud Cleanup, Post-Processing & Export State
    QString cleanupStatus_{"Ready for post-processing."};
    bool hasCleanupPreview_{false};
    qlonglong cleanupInputPoints_{0};
    qlonglong cleanupOutputPoints_{0};
    qlonglong cleanupRemovedPoints_{0};
    qreal cleanupGroundTiltDeg_{0.0};
    bool liveCleanupEnabled_{true};
    bool liveGroundLockEnabled_{false};
    qreal liveCleanupTimeMs_{0.0};
    std::vector<core::schemas::PointXYZI> preCleanupBackupPoints_;
    QString lastExportPath_{""};
    QString exportStatus_{"Ready to export (.ply, .pcd, .xyz, .obj)."};

    // Milestones 7 & 8: Rigid Extrinsic Calibration & Multi-Sensor Spatial Fusion State
    QString extrinsicCalibrationStatus_{"UNKNOWN"};
    QString fusionStatus_{"Ready for multi-sensor spatial fusion."};
    bool isFusionActive_{false};
    qlonglong fusedColorizedPoints_{0};
    qreal extrinsicRmseMm_{0.0};
    qreal extrinsicOverlapPercent_{0.0};
    qreal extrinsicConfidence_{0.0};
    QString extrinsicTransformSummary_{"No calibrated extrinsic transform loaded."};
    std::vector<core::schemas::PointXYZI> fusedMapPoints_;
    std::vector<core::schemas::Pose3D> fusedTrajectory_;
    QString activeAnchorSensor_{"AIRY"};
    QString activeAnchorName_{"Airy LiDAR"};
    QString activeAnchorRationale_{"Highest nominal metric accuracy (LiDAR + IMU)."};
    bool isAnchorFailoverActive_{false};
    qreal activeAnchorScore_{100.0};
};

} // namespace av::gui
