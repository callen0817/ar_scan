#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>
#include "core/storage/storage_engine.hpp"
#include "platform/platform_adapter.hpp"

namespace av::gui {

class QmlBridge : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString projectName READ projectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString projectId READ projectId NOTIFY projectIdChanged)
    Q_PROPERTY(QString captureState READ captureState NOTIFY captureStateChanged)
    Q_PROPERTY(QString selectedScannerConfig READ selectedScannerConfig WRITE setSelectedScannerConfig NOTIFY selectedScannerConfigChanged)
    Q_PROPERTY(QString elapsedTimeString READ elapsedTimeString NOTIFY elapsedTimeStringChanged)
    Q_PROPERTY(QString sensorStatus READ sensorStatus NOTIFY sensorStatusChanged)
    Q_PROPERTY(QString trackingStatus READ trackingStatus NOTIFY trackingStatusChanged)
    Q_PROPERTY(qlonglong pointsCaptured READ pointsCaptured NOTIFY pointsCapturedChanged)
    Q_PROPERTY(QStringList availableScanners READ availableScanners NOTIFY availableScannersChanged)
    Q_PROPERTY(QStringList availableProjects READ availableProjects NOTIFY availableProjectsChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY isCapturingChanged)
    Q_PROPERTY(bool canSave READ canSave NOTIFY canSaveChanged)
    Q_PROPERTY(QString hostPlatform READ hostPlatform CONSTANT)

public:
    explicit QmlBridge(std::shared_ptr<core::storage::StorageEngine> storage,
                       std::shared_ptr<platform::IPlatformAdapter> platform,
                       QObject *parent = nullptr);
    ~QmlBridge() override = default;

    QString projectName() const { return projectName_; }
    QString projectId() const { return projectId_; }
    QString captureState() const { return captureState_; }
    QString selectedScannerConfig() const { return selectedScannerConfig_; }
    QString elapsedTimeString() const { return elapsedTimeString_; }
    QString sensorStatus() const { return sensorStatus_; }
    QString trackingStatus() const { return trackingStatus_; }
    qlonglong pointsCaptured() const { return pointsCaptured_; }
    QStringList availableScanners() const { return availableScanners_; }
    QStringList availableProjects() const { return availableProjects_; }
    bool isCapturing() const { return isCapturing_; }
    bool canSave() const { return canSave_; }
    QString hostPlatform() const;

    void setSelectedScannerConfig(const QString& cfg);

    Q_INVOKABLE bool createProject(const QString& name, const QString& description);
    Q_INVOKABLE bool openProject(const QString& projectId);
    Q_INVOKABLE void selectScannerConfig(const QString& configId);
    Q_INVOKABLE bool startCapture();
    Q_INVOKABLE bool stopCapture();
    Q_INVOKABLE bool saveCapture();
    Q_INVOKABLE void refreshData();

signals:
    void projectNameChanged();
    void projectIdChanged();
    void captureStateChanged();
    void selectedScannerConfigChanged();
    void elapsedTimeStringChanged();
    void sensorStatusChanged();
    void trackingStatusChanged();
    void pointsCapturedChanged();
    void availableScannersChanged();
    void availableProjectsChanged();
    void isCapturingChanged();
    void canSaveChanged();
    void errorOccurred(const QString& errorTitle, const QString& errorMessage);
    void notification(const QString& title, const QString& message);

private slots:
    void onTimerTick();

private:
    std::shared_ptr<core::storage::StorageEngine> storage_;
    std::shared_ptr<platform::IPlatformAdapter> platform_;

    QString projectName_{"No Project Loaded"};
    QString projectId_{""};
    QString currentSessionId_{""};
    QString captureState_{"IDLE"};
    QString selectedScannerConfig_{""};
    QString elapsedTimeString_{"00:00:00"};
    QString sensorStatus_{"READY"};
    QString trackingStatus_{"NO_TRACKING"};
    qlonglong pointsCaptured_{0};
    QStringList availableScanners_;
    QStringList availableProjects_;
    bool isCapturing_{false};
    bool canSave_{false};

    QTimer* timer_{nullptr};
    int elapsed_seconds_{0};
};

} // namespace av::gui
