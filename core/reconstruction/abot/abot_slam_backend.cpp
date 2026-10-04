#include "abot_slam_backend.hpp"
#include "core/logging/logger.hpp"
#include <cmath>

namespace av::core::reconstruction::abot {

ABotSlamBackend::ABotSlamBackend(std::shared_ptr<ABotIpcClient> ipc_client)
    : ipc_client_(std::move(ipc_client)) {
    if (!ipc_client_) {
        ipc_client_ = std::make_shared<ABotIpcClient>("127.0.0.1", 9103);
        own_client_ = true;
    }
}

ABotSlamBackend::~ABotSlamBackend() {
    stop();
}

bool ABotSlamBackend::initialize(const schemas::DeviceProfile& profile) {
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

    // STRICT DEVICE ISOLATION INVARIANT:
    // ABot must consume ONLY the ELP endpoint resolved from its Sensor Instance / Device Profile.
    // Refuse to fall back to /dev/video0 automatically or pick an arbitrary camera.
    if (device_path_.empty() || device_path_.rfind("/dev/video", 0) != 0) {
        AV_LOG_ERROR("ABotSlamBackend", "Init Failed", "Profile does not specify a valid V4L2 video device path (got: '" + device_path_ + "'). Refusing cross-device fallback.");
        return false;
    }

    if (!ipc_client_) {
        ipc_client_ = std::make_shared<ABotIpcClient>("127.0.0.1", 9103);
        own_client_ = true;
    }

    if (!ipc_client_->ensureDaemonRunning()) {
        AV_LOG_ERROR("ABotSlamBackend", "Init Failed", "Failed to start or connect to av_abot_daemon on port 9103");
        return false;
    }

    AV_LOG_INFO("ABotSlamBackend", "Initialized ABot-Recon with device path: " + device_path_);
    return true;
}

bool ABotSlamBackend::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return true;

    if (device_path_.empty() || device_path_.rfind("/dev/video", 0) != 0) {
        AV_LOG_ERROR("ABotSlamBackend", "Start Failed", "Cannot start acquisition: invalid video device path '" + device_path_ + "'");
        return false;
    }

    if (!ipc_client_ || !ipc_client_->startAcquisition(device_path_)) {
        AV_LOG_ERROR("ABotSlamBackend", "Start Failed", "Could not start ABot acquisition on " + device_path_);
        return false;
    }

    map_points_.clear();
    trajectory_.clear();
    point_count_ = 0;
    running_ = true;
    AV_LOG_INFO("ABotSlamBackend", "ABot-Recon SLAM acquisition running on " + device_path_);
    return true;
}

bool ABotSlamBackend::stop() {
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
    AV_LOG_INFO("ABotSlamBackend", "ABot-Recon SLAM acquisition stopped");
    return true;
}

bool ABotSlamBackend::reset() {
    stop();
    std::lock_guard<std::mutex> lock(mutex_);
    map_points_.clear();
    trajectory_.clear();
    point_count_ = 0;
    return true;
}

bool ABotSlamBackend::isRunning() const {
    return running_;
}

schemas::Pose3D ABotSlamBackend::getPose() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_pose_;
}

std::vector<schemas::Pose3D> ABotSlamBackend::getTrajectory() const {
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

schemas::MapMetadata ABotSlamBackend::getMapMetadata() const {
    std::lock_guard<std::mutex> lock(mutex_);
    schemas::MapMetadata meta(
        "map_abot_" + std::to_string(current_pose_.timestamp_ns),
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
    cal.set_firmware("ABot-Recon-v1.0.0");
    cal.set_evidence("Geometry Source: MONOCULAR_NEURAL_RECONSTRUCTION | Metric Constraint: NONE | Scale Status: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED | Qualification: RESEARCH_EXPERIMENTAL");
    cal.set_validated(false);
    meta.set_provenance(cal);

    return meta;
}

schemas::TrackingState ABotSlamBackend::getTrackingState() const {
    return running_ ? schemas::TrackingState::TRACKING_OK : schemas::TrackingState::NO_TRACKING;
}

void ABotSlamBackend::feedPointCloud(const schemas::PointCloudFrame& /*cloud*/) {
    // ABot processes camera RGB directly from video device node
}

void ABotSlamBackend::feedImu(const schemas::ImuSample& /*imu*/) {
    // Monocular visual odometry
}

void ABotSlamBackend::feedImage(const schemas::ImageFrame& /*image*/) {
    // Direct capture in daemon process
}

bool ABotSlamBackend::getNewMapPoints(std::vector<schemas::PointXYZRGB>& points) {
    if (!ipc_client_) return false;
    return ipc_client_->fetchNewPoints(points);
}

bool ABotSlamBackend::getTelemetry(nlohmann::json& telemetry) {
    if (!ipc_client_) return false;
    return ipc_client_->getTelemetry(telemetry);
}

bool ABotSlamBackend::updateTelemetry() {
    if (!ipc_client_) return false;
    nlohmann::json tel;
    if (!ipc_client_->getTelemetry(tel)) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    acquisition_rate_ = tel.value("camera_fps", 0.0);
    slam_rate_ = tel.value("inference_fps", 0.0);
    std::string state_str = tel.value("status", "IDLE");

    if (state_str == "STREAMING") {
        tracking_state_ = schemas::TrackingState::TRACKING_OK;
    } else {
        tracking_state_ = schemas::TrackingState::NO_TRACKING;
    }

    std::vector<schemas::Pose3D> traj;
    if (ipc_client_->getTrajectory(traj) && !traj.empty()) {
        trajectory_ = std::move(traj);
        current_pose_ = trajectory_.back();

        // Calculate yaw in AV frame: yaw = atan2(2*(w*z + x*y), 1 - 2*(y^2 + z^2))
        double qw = current_pose_.orientation.w;
        double qx = current_pose_.orientation.x;
        double qy = current_pose_.orientation.y;
        double qz = current_pose_.orientation.z;
        double siny_cosp = 2.0 * (qw * qz + qx * qy);
        double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
        current_yaw_ = std::atan2(siny_cosp, cosy_cosp) * (180.0 / M_PI);
    }

    return true;
}

bool ABotSlamBackend::fetchNewPoints() {
    if (!ipc_client_) return false;
    std::vector<schemas::PointXYZRGB> new_pts;
    if (ipc_client_->fetchNewPoints(new_pts) && !new_pts.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_points_.insert(map_points_.end(), new_pts.begin(), new_pts.end());
        point_count_ = map_points_.size();
        return true;
    }
    return false;
}

std::vector<schemas::PointXYZRGB> ABotSlamBackend::getMapPoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return map_points_;
}

double ABotSlamBackend::getCurrentYaw() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_yaw_;
}

uint64_t ABotSlamBackend::getPointCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return point_count_;
}

double ABotSlamBackend::getAcquisitionRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return acquisition_rate_;
}

double ABotSlamBackend::getSlamRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return slam_rate_;
}

std::vector<schemas::CalibrationProvenance> ABotSlamBackend::getCalibration() const {
    std::vector<schemas::CalibrationProvenance> list;
    schemas::CalibrationProvenance cal;
    cal.set_source(schemas::CalibrationSource::INFERRED_FROM_GEOMETRY);
    cal.set_value("MONOCULAR_NEURAL_RECONSTRUCTION");
    cal.set_confidence(0.95);
    cal.set_firmware("ABot-Recon-v1.0.0");
    cal.set_evidence("Geometry Source: MONOCULAR_NEURAL_RECONSTRUCTION | Metric Constraint: NONE | Scale Status: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED | Qualification: RESEARCH_EXPERIMENTAL (Non-Commercial)");
    cal.set_validated(false);
    list.push_back(cal);
    return list;
}

} // namespace av::core::reconstruction::abot
