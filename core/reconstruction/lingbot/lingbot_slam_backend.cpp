#include "lingbot_slam_backend.hpp"
#include "core/logging/logger.hpp"

namespace av::core::reconstruction::lingbot {

LingBotSlamBackend::LingBotSlamBackend(std::shared_ptr<LingBotIpcClient> ipc_client)
    : ipc_client_(std::move(ipc_client)) {
    if (!ipc_client_) {
        ipc_client_ = std::make_shared<LingBotIpcClient>("127.0.0.1", 9102);
        own_client_ = true;
    }
}

LingBotSlamBackend::~LingBotSlamBackend() {
    stop();
}

bool LingBotSlamBackend::initialize(const schemas::DeviceProfile& profile) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_profile_ = profile;

    // Extract device path if specified in intrinsics or transport
    if (profile.intrinsics().contains("device_path")) {
        device_path_ = profile.intrinsics()["device_path"].get<std::string>();
    } else if (profile.transport().rfind("/dev/video", 0) == 0) {
        device_path_ = profile.transport();
    } else {
        device_path_ = "";
    }

    if (device_path_.empty() || device_path_.rfind("/dev/video", 0) != 0) {
        AV_LOG_ERROR("LingBotSlamBackend", "Init Failed", "Profile does not specify a valid V4L2 video device path (got: '" + device_path_ + "')");
        return false;
    }

    if (!ipc_client_) {
        ipc_client_ = std::make_shared<LingBotIpcClient>("127.0.0.1", 9102);
        own_client_ = true;
    }

    if (!ipc_client_->ensureDaemonRunning()) {
        AV_LOG_ERROR("LingBotSlamBackend", "Init Failed", "Failed to start or connect to av_lingbot_daemon");
        return false;
    }

    AV_LOG_INFO("LingBotSlamBackend", "Initialized with device path: " + device_path_);
    return true;
}

bool LingBotSlamBackend::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;

    if (device_path_.empty() || device_path_.rfind("/dev/video", 0) != 0) {
        AV_LOG_ERROR("LingBotSlamBackend", "Start Failed", "Cannot start acquisition: invalid video device path '" + device_path_ + "'");
        return false;
    }

    if (!ipc_client_ || !ipc_client_->startAcquisition(device_path_)) {
        AV_LOG_ERROR("LingBotSlamBackend", "Start Failed", "Could not start LingBot acquisition on " + device_path_);
        return false;
    }

    map_points_.clear();
    trajectory_.clear();
    point_count_ = 0;
    running_ = true;
    AV_LOG_INFO("LingBotSlamBackend", "LingBot SLAM acquisition running on " + device_path_);
    return true;
}

bool LingBotSlamBackend::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return true;

    if (ipc_client_) {
        std::vector<schemas::PointXYZRGB> remaining;
        if (ipc_client_->fetchNewPoints(remaining) && !remaining.empty()) {
            map_points_.insert(map_points_.end(), remaining.begin(), remaining.end());
            point_count_ = map_points_.size();
        }
        std::vector<schemas::Pose3D> final_traj;
        if (ipc_client_->getTrajectory(final_traj)) {
            trajectory_ = std::move(final_traj);
        }
        ipc_client_->stopAcquisition();
    }
    running_ = false;
    AV_LOG_INFO("LingBotSlamBackend", "LingBot SLAM acquisition stopped");
    return true;
}

bool LingBotSlamBackend::reset() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    map_points_.clear();
    trajectory_.clear();
    point_count_ = 0;
    return true;
}

bool LingBotSlamBackend::isRunning() const {
    return running_;
}

schemas::Pose3D LingBotSlamBackend::getPose() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_pose_;
}

std::vector<schemas::Pose3D> LingBotSlamBackend::getTrajectory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!trajectory_.empty()) return trajectory_;
    std::vector<schemas::Pose3D> traj;
    if (ipc_client_) {
        ipc_client_->getTrajectory(traj);
        if (!traj.empty()) {
            current_pose_ = traj.back();
        }
    }
    return traj;
}

schemas::MapMetadata LingBotSlamBackend::getMapMetadata() const {
    std::lock_guard<std::mutex> lock(mutex_);
    schemas::MapMetadata meta(
        "map_lingbot_" + std::to_string(current_pose_.timestamp_ns),
        "session_active",
        current_profile_.profile_id().empty() ? "generic_camera" : current_profile_.profile_id(),
        "camera_node"
    );
    meta.set_timestamp_ns(current_pose_.timestamp_ns);
    meta.set_current_pose(current_pose_);
    meta.set_map_geometry_format("PCL_PCD");
    meta.set_tracking_state(running_ ? schemas::TrackingState::TRACKING_OK : schemas::TrackingState::NO_TRACKING);

    schemas::CalibrationProvenance cal;
    cal.set_source(schemas::CalibrationSource::INFERRED_FROM_GEOMETRY);
    cal.set_value("MONOCULAR_NEURAL_RECONSTRUCTION");
    cal.set_confidence(0.95);
    cal.set_firmware("LingBot-Map-v1.0.0");
    cal.set_evidence("Geometry Source: MONOCULAR_NEURAL_RECONSTRUCTION | Metric Constraint: NONE | Scale Status: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED");
    cal.set_validated(false);
    meta.set_provenance(cal);

    return meta;
}

schemas::TrackingState LingBotSlamBackend::getTrackingState() const {
    return running_ ? schemas::TrackingState::TRACKING_OK : schemas::TrackingState::NO_TRACKING;
}

void LingBotSlamBackend::feedPointCloud(const schemas::PointCloudFrame& /*cloud*/) {
    // Monocular backend does not ingest point clouds
}

void LingBotSlamBackend::feedImu(const schemas::ImuSample& /*imu*/) {
    // Monocular neural backend uses visual pose estimation
}

void LingBotSlamBackend::feedImage(const schemas::ImageFrame& /*image*/) {
    // In direct camera mode, av_lingbot_daemon reads frames at line rate
}

bool LingBotSlamBackend::getNewMapPoints(std::vector<schemas::PointXYZRGB>& points) {
    if (!ipc_client_) return false;
    return ipc_client_->fetchNewPoints(points);
}

bool LingBotSlamBackend::getTelemetry(nlohmann::json& telemetry) {
    if (!ipc_client_) return false;
    return ipc_client_->getTelemetry(telemetry);
}

bool LingBotSlamBackend::updateTelemetry() {
    if (!ipc_client_) return false;
    nlohmann::json telem;
    if (ipc_client_->getTelemetry(telem)) {
        std::lock_guard<std::mutex> lock(mutex_);
        acquisition_rate_ = telem.value("acquisition_rate_hz", 0.0);
        slam_rate_ = telem.value("slam_rate_hz", 0.0);
        point_count_ = telem.value("total_points", 0ULL);
        std::string state_str = telem.value("tracking_state", "NO_TRACKING");
        if (state_str == "TRACKING_OK") {
            tracking_state_ = schemas::TrackingState::TRACKING_OK;
        } else if (state_str == "INITIALIZING") {
            tracking_state_ = schemas::TrackingState::INITIALIZING;
        } else {
            tracking_state_ = schemas::TrackingState::NO_TRACKING;
        }

        std::vector<schemas::Pose3D> new_traj;
        if (ipc_client_->getTrajectory(new_traj)) {
            trajectory_ = std::move(new_traj);
            if (!trajectory_.empty()) {
                current_pose_ = trajectory_.back();
                double qx = current_pose_.orientation.x;
                double qy = current_pose_.orientation.y;
                double qz = current_pose_.orientation.z;
                double qw = current_pose_.orientation.w;
                current_yaw_ = std::atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz));
            }
        }
        return true;
    }
    return false;
}

bool LingBotSlamBackend::fetchNewPoints() {
    if (!ipc_client_) return false;
    std::vector<schemas::PointXYZRGB> new_pts;
    if (ipc_client_->fetchNewPoints(new_pts)) {
        if (!new_pts.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            map_points_.insert(map_points_.end(), new_pts.begin(), new_pts.end());
            point_count_ = map_points_.size();
        }
        return true;
    }
    return false;
}

std::vector<schemas::PointXYZRGB> LingBotSlamBackend::getMapPoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_points_;
}

double LingBotSlamBackend::getCurrentYaw() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_yaw_;
}

uint64_t LingBotSlamBackend::getPointCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return point_count_;
}

double LingBotSlamBackend::getAcquisitionRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return acquisition_rate_;
}

double LingBotSlamBackend::getSlamRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slam_rate_;
}

std::vector<schemas::CalibrationProvenance> LingBotSlamBackend::getCalibration() const {
    std::vector<schemas::CalibrationProvenance> list;
    schemas::CalibrationProvenance cal;
    cal.set_source(schemas::CalibrationSource::INFERRED_FROM_GEOMETRY);
    cal.set_value("MONOCULAR_NEURAL_RECONSTRUCTION");
    cal.set_confidence(0.95);
    cal.set_firmware("LingBot-Map-v1.0.0");
    cal.set_evidence("Geometry Source: MONOCULAR_NEURAL_RECONSTRUCTION | Metric Constraint: NONE | Scale Status: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED");
    cal.set_validated(false);
    list.push_back(cal);
    return list;
}

} // namespace av::core::reconstruction::lingbot
