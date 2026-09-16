#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <memory>
#include "core/storage/storage_engine.hpp"
#include "platform/platform_adapter.hpp"

namespace av::gui {

class QmlBridge : public QObject {
    Q_OBJECT

    // Project & Workspace Properties
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString projectId READ projectId NOTIFY projectIdChanged)
    Q_PROPERTY(QString projectDescription READ projectDescription NOTIFY projectDescriptionChanged)
    Q_PROPERTY(QString projectCreatedAt READ projectCreatedAt NOTIFY projectCreatedAtChanged)
    Q_PROPERTY(QStringList availableProjects READ availableProjects NOTIFY availableProjectsChanged)
    Q_PROPERTY(QVariantList projectList READ projectList NOTIFY projectListChanged)

    // Capture & State Machine Properties
    Q_PROPERTY(QString captureState READ captureState NOTIFY captureStateChanged)
    Q_PROPERTY(bool canStart READ canStart NOTIFY captureStateChanged)
    Q_PROPERTY(bool canStop READ canStop NOTIFY captureStateChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY captureStateChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY isCapturingChanged)
    Q_PROPERTY(QString elapsedTimeString READ elapsedTimeString NOTIFY elapsedTimeStringChanged)
    Q_PROPERTY(QString sensorStatus READ sensorStatus NOTIFY sensorStatusChanged)
    Q_PROPERTY(QString trackingStatus READ trackingStatus NOTIFY trackingStatusChanged)
    Q_PROPERTY(qlonglong pointsCaptured READ pointsCaptured NOTIFY pointsCapturedChanged)

    // Scanner / Profile Properties
    Q_PROPERTY(QString selectedScannerId READ selectedScannerId NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerName READ selectedScannerName NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerStatus READ selectedScannerStatus NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerDetails READ selectedScannerDetails NOTIFY selectedScannerChanged)
    Q_PROPERTY(QString selectedScannerValidationNotice READ selectedScannerValidationNotice NOTIFY selectedScannerChanged)
    Q_PROPERTY(QStringList availableScanners READ availableScanners NOTIFY availableScannersChanged)
    Q_PROPERTY(QVariantList scannerList READ scannerList NOTIFY scannerListChanged)

    // Platform & Host Metrics
    Q_PROPERTY(QString hostPlatform READ hostPlatform CONSTANT)
    Q_PROPERTY(QString hostHardware READ hostHardware CONSTANT)
    Q_PROPERTY(QString hostMemory READ hostMemory NOTIFY hostMetricsChanged)
    Q_PROPERTY(QString hostDisk READ hostDisk NOTIFY hostMetricsChanged)

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

public:
    explicit QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                       std::shared_ptr<platform::IPlatformAdapter> platform,
                       QObject *parent = nullptr);
    ~QmlBridge() override = default;

    // Getters: Projects
    QString projectName() const { return projectName_; }
    QString projectId() const { return projectId_; }
    QString projectDescription() const { return projectDescription_; }
    QString projectCreatedAt() const { return projectCreatedAt_; }
    QStringList availableProjects() const { return availableProjects_; }
    QVariantList projectList() const { return projectList_; }

    // Getters: State & Capture
    QString captureState() const { return captureState_; }
    bool canStart() const { return (captureState_ == "READY" || captureState_ == "SAVED") && !projectId_.isEmpty(); }
    bool canStop() const { return isCapturing_; }
    bool canSave() const { return canSave_; }
    bool isCapturing() const { return isCapturing_; }
    QString elapsedTimeString() const { return elapsedTimeString_; }
    QString sensorStatus() const { return sensorStatus_; }
    QString trackingStatus() const { return trackingStatus_; }
    qlonglong pointsCaptured() const { return pointsCaptured_; }

    // Getters: Scanners
    QString selectedScannerId() const { return selectedScannerId_; }
    QString selectedScannerName() const { return selectedScannerName_; }
    QString selectedScannerStatus() const { return selectedScannerStatus_; }
    QString selectedScannerDetails() const { return selectedScannerDetails_; }
    QString selectedScannerValidationNotice() const { return selectedScannerValidationNotice_; }
    QStringList availableScanners() const { return availableScanners_; }
    QVariantList scannerList() const { return scannerList_; }

    // Getters: Host
    QString hostPlatform() const;
    QString hostHardware() const;
    QString hostMemory() const;
    QString hostDisk() const;

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

    // Q_INVOKABLE: Project Operations
    Q_INVOKABLE bool createProject(const QString& name, const QString& description);
    Q_INVOKABLE bool openProject(const QString& projectId);
    Q_INVOKABLE void refreshData();

    // Q_INVOKABLE: Scanner Selection
    Q_INVOKABLE void selectScannerByIndex(int index);
    Q_INVOKABLE void selectScannerById(const QString& scannerId);

    // Q_INVOKABLE: Capture Control State Machine
    Q_INVOKABLE bool startCapture();
    Q_INVOKABLE bool stopCapture();
    Q_INVOKABLE bool saveCapture();

    // Q_INVOKABLE: 3D Camera Interaction
    Q_INVOKABLE void orbit3D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void pan3D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void zoom3D(qreal deltaFactor);
    Q_INVOKABLE void recenter3D();
    Q_INVOKABLE void setPreset3D(const QString& preset);

    // Q_INVOKABLE: 2D Camera Interaction
    Q_INVOKABLE void pan2D(qreal deltaX, qreal deltaY);
    Q_INVOKABLE void zoom2DByFactor(qreal factor);
    Q_INVOKABLE void recenter2D();

signals:
    void projectNameChanged();
    void projectIdChanged();
    void projectDescriptionChanged();
    void projectCreatedAtChanged();
    void availableProjectsChanged();
    void projectListChanged();
    void captureStateChanged();
    void isCapturingChanged();
    void canSaveChanged();
    void elapsedTimeStringChanged();
    void sensorStatusChanged();
    void trackingStatusChanged();
    void pointsCapturedChanged();
    void selectedScannerChanged();
    void availableScannersChanged();
    void scannerListChanged();
    void hostMetricsChanged();
    void cam3DChanged();
    void cam2DChanged();
    void errorOccurred(const QString& errorTitle, const QString& errorMessage);
    void notification(const QString& title, const QString& message);

private slots:
    void onTimerTick();

private:
    std::shared_ptr<core::storage::StorageEngine> storage_;
    std::shared_ptr<platform::IPlatformAdapter> platform_;

    // Project state
    QString projectName_{"No Project Loaded"};
    QString projectId_{""};
    QString projectDescription_{""};
    QString projectCreatedAt_{""};
    QString currentSessionId_{""};
    QStringList availableProjects_;
    QVariantList projectList_;

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
    int elapsed_seconds_{0};
};

} // namespace av::gui
