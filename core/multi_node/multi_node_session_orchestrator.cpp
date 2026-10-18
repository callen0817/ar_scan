#include "multi_node_session_orchestrator.hpp"
#include "core/logging/logger.hpp"
#include <chrono>

namespace av::core::multi_node {

static uint64_t getNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

MultiNodeSessionOrchestrator::MultiNodeSessionOrchestrator()
    : discovery_service_(std::make_shared<DiscoveryService>()),
      coordination_service_(std::make_shared<CoordinationService>()) {}

MultiNodeSessionOrchestrator::~MultiNodeSessionOrchestrator() {
    shutdown();
}

bool MultiNodeSessionOrchestrator::initialize(std::shared_ptr<storage::StorageEngine> storage,
                                            const std::string& identity_file_path,
                                            int discovery_port,
                                            int coordination_port) {
    storage_ = storage;

    // 1. Initialize persistent node identity
    if (!NodeIdentityManager::instance().initialize(identity_file_path)) {
        AV_LOG_ERROR("MultiNodeOrchestrator", "Failed to initialize persistent node identity", "I/O or permission error");
        return false;
    }

    // 2. Start TCP Coordination Server
    if (!coordination_service_->start(coordination_port)) {
        AV_LOG_ERROR("MultiNodeOrchestrator", "Failed to start coordination service on port " + std::to_string(coordination_port), "Socket bind failed");
        return false;
    }

    // 3. Start UDP Discovery Service
    if (!discovery_service_->start(discovery_port)) {
        AV_LOG_WARN("MultiNodeOrchestrator", "UDP discovery service could not bind port " + std::to_string(discovery_port) + " (coordination remains functional)");
    }

    setupCallbacks();
    updateBeaconAvailability("AVAILABLE");

    AV_LOG_INFO("MultiNodeOrchestrator", "Multi-Node subsystem initialized successfully for node " + getLocalNodeUuid());
    return true;
}

void MultiNodeSessionOrchestrator::shutdown() {
    if (discovery_service_) discovery_service_->stop();
    if (coordination_service_) coordination_service_->stop();
}

void MultiNodeSessionOrchestrator::setupCallbacks() {
    coordination_service_->setInviteCallback([this](const std::string& sid, const std::string& coord_uuid, const std::string& pid) {
        AV_LOG_INFO("MultiNodeOrchestrator", "Session invitation received: " + sid + " from " + coord_uuid);
        if (on_invite_handler_) {
            on_invite_handler_(sid, coord_uuid, pid);
        }
    });

    coordination_service_->setMemberJoinedCallback([this](const std::string& node_uuid, const std::string& sid) {
        std::string display_name = node_uuid;
        auto members = coordination_service_->getSessionMembers();
        for (const auto& m : members) {
            if (m.node_uuid == node_uuid && !m.display_name.empty()) {
                display_name = m.display_name;
                break;
            }
        }
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            current_session_.add_scanner_node(node_uuid, "SESSION_PARTICIPANT");
        }
        cslam_engine_.registerScanner(node_uuid, display_name, false);

        // If we are Coordinator, push all existing scanner submaps and current alignment state
        // to the newly joined scanner so it immediately sees the live merged point cloud!
        if (coordination_service_->getSessionRole() == schemas::NodeSessionRole::SESSION_COORDINATOR) {
            auto states = cslam_engine_.getAllScannerStates();
            for (const auto& st : states) {
                if (!st.raw_points.empty() && st.node_uuid != node_uuid) {
                    nlohmann::json submap_msg = cslam_engine_.serializeSubmapMessage(st.node_uuid);
                    coordination_service_->sendDirectMessageToPeer(node_uuid, MessageType::CSLAM_SUBMAP_UPDATE, submap_msg);
                }
            }
            nlohmann::json align_msg = cslam_engine_.serializeAlignmentState();
            coordination_service_->sendDirectMessageToPeer(node_uuid, MessageType::CSLAM_ALIGNMENT_UPDATE, align_msg);
        }

        if (on_member_list_changed_) {
            on_member_list_changed_();
        }
        if (on_cslam_updated_) {
            on_cslam_updated_();
        }
    });

    coordination_service_->setMemberLeftCallback([this](const std::string& node_uuid, const std::string& sid) {
        if (on_member_list_changed_) {
            on_member_list_changed_();
        }
    });

    coordination_service_->setConnectionCallback([this](const std::string& node_uuid, bool connected) {
        if (on_member_list_changed_) {
            on_member_list_changed_();
        }
    });

    coordination_service_->setPrepareCaptureCallback([this](const std::string& sid, uint64_t req_start) -> bool {
        capture_requested_start_ns_ = req_start;
        AV_LOG_INFO("MultiNodeOrchestrator", "Participant prepared for capture in session " + sid);
        return true;
    });

    coordination_service_->setStartCaptureCallback([this](const std::string& sid, const std::string& cap_id, uint64_t req_start) {
        capture_requested_start_ns_ = req_start;
        capture_actual_start_ns_ = getNowNs();
        updateBeaconAvailability("CAPTURING");
        if (on_capture_state_changed_) {
            on_capture_state_changed_(true, capture_actual_start_ns_);
        }
    });

    coordination_service_->setStopCaptureCallback([this](const std::string& sid, uint64_t req_stop) {
        capture_requested_stop_ns_ = req_stop;
        capture_actual_stop_ns_ = getNowNs();
        updateBeaconAvailability("IN_SESSION");
        if (on_capture_state_changed_) {
            on_capture_state_changed_(false, capture_actual_stop_ns_);
        }
    });

    coordination_service_->setCslamSubmapCallback([this](const std::string& source_node_uuid, const nlohmann::json& payload) {
        (void)source_node_uuid;
        if (cslam_engine_.ingestSerializedSubmapMessage(payload)) {
            if (coordination_service_->getSessionRole() == schemas::NodeSessionRole::SESSION_COORDINATOR) {
                cslam_engine_.evaluateAndSnapOverlaps();
                nlohmann::json align_payload = cslam_engine_.serializeAlignmentState();
                coordination_service_->broadcastAlignmentUpdate(align_payload);
            }
            if (on_cslam_updated_) {
                on_cslam_updated_();
            }
        }
    });

    coordination_service_->setCslamAlignmentCallback([this](const std::string& source_node_uuid, const nlohmann::json& payload) {
        (void)source_node_uuid;
        if (cslam_engine_.applySerializedAlignmentState(payload)) {
            if (on_cslam_updated_) {
                on_cslam_updated_();
            }
        }
    });
}

void MultiNodeSessionOrchestrator::setLocalIdentityOverride(const std::string& node_uuid, const std::string& display_name) {
    override_node_uuid_ = node_uuid;
    override_display_name_ = display_name;
    if (coordination_service_) {
        coordination_service_->setLocalIdentityOverride(node_uuid, display_name);
    }
}

void MultiNodeSessionOrchestrator::updateBeaconAvailability(const std::string& state) {
    auto node = getLocalNode();
    DiscoveryBeacon b;
    b.node_uuid = node.node_uuid();
    b.display_name = node.display_name();
    b.hostname = node.hostname();
    b.platform = node.platform();
    b.av_scan_version = node.av_scan_version();
    b.coordination_endpoint = node.ip_address() + ":" + std::to_string(coordination_service_->getListenPort());
    b.capabilities = node.capabilities();
    b.session_state = state;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        b.current_session_id = active_session_id_;
    }
    discovery_service_->updateLocalBeacon(b);
}

schemas::ScannerNode MultiNodeSessionOrchestrator::getLocalNode() {
    auto node = NodeIdentityManager::instance().getNode();
    if (!override_node_uuid_.empty()) {
        node.set_node_uuid(override_node_uuid_);
    }
    if (!override_display_name_.empty()) {
        node.set_display_name(override_display_name_);
    }
    return node;
}

std::string MultiNodeSessionOrchestrator::getLocalNodeUuid() {
    if (!override_node_uuid_.empty()) return override_node_uuid_;
    return NodeIdentityManager::instance().getNodeUuid();
}

bool MultiNodeSessionOrchestrator::setDisplayName(const std::string& name) {
    if (!override_node_uuid_.empty()) {
        override_display_name_ = name;
        if (coordination_service_) {
            coordination_service_->setLocalIdentityOverride(override_node_uuid_, override_display_name_);
        }
        return true;
    }
    bool ok = NodeIdentityManager::instance().setDisplayName(name);
    if (ok) updateBeaconAvailability(coordination_service_->isCapturing() ? "CAPTURING" : "AVAILABLE");
    return ok;
}

void MultiNodeSessionOrchestrator::registerLocalSensor(const schemas::SensorInstance& sensor) {
    (void)sensor;
}

void MultiNodeSessionOrchestrator::clearLocalSensors() {}

std::vector<DiscoveredNodeEntry> MultiNodeSessionOrchestrator::getDiscoveredNodes() {
    return discovery_service_->getDiscoveredNodes();
}

void MultiNodeSessionOrchestrator::addKnownPeer(const std::string& ip, int port) {
    discovery_service_->addKnownPeer(ip, port);
}

bool MultiNodeSessionOrchestrator::connectDirectPeer(const std::string& ip, int port) {
    return coordination_service_->connectToPeer(ip, port);
}

bool MultiNodeSessionOrchestrator::createSession(const std::string& project_id, const std::string& session_name) {
    uint64_t now_ms = getNowNs() / 1000000ULL;
    std::string session_id = "session_" + std::to_string(now_ms);

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        active_project_id_ = project_id;
        active_session_id_ = session_id;
        current_session_ = schemas::Session(session_id, project_id, session_name, getNowNs());
        current_session_.set_coordinator_node_uuid(getLocalNodeUuid());
        current_session_.add_scanner_node(getLocalNodeUuid(), "SESSION_COORDINATOR");
    }

    cslam_engine_.clear();
    cslam_engine_.registerScanner(getLocalNodeUuid(), getLocalNode().display_name(), true);

    bool ok = coordination_service_->createSession(session_id, project_id);
    if (ok) {
        updateBeaconAvailability("IN_SESSION");
        if (storage_) {
            storage_->saveSession(current_session_);
            storage_->saveSessionScannerNode(project_id, session_id, getLocalNode());
        }
    }
    return ok;
}

bool MultiNodeSessionOrchestrator::inviteNode(const std::string& node_uuid) {
    DiscoveredNodeEntry entry;
    if (discovery_service_->getDiscoveredNode(node_uuid, entry)) {
        return coordination_service_->invitePeer(entry.ip_address, entry.coordination_port, node_uuid);
    }
    AV_LOG_WARN("MultiNodeOrchestrator", "Cannot invite node " + node_uuid + ": not in discovery cache");
    return false;
}

bool MultiNodeSessionOrchestrator::prepareCapture() {
    capture_requested_start_ns_ = getNowNs() + 200000000ULL; // 200ms lead time
    return coordination_service_->prepareCapture(capture_requested_start_ns_);
}

bool MultiNodeSessionOrchestrator::startCapture() {
    bool ok = coordination_service_->startCapture(capture_requested_start_ns_);
    if (ok) {
        capture_actual_start_ns_ = getNowNs();
        updateBeaconAvailability("CAPTURING");
        if (on_capture_state_changed_) {
            on_capture_state_changed_(true, capture_actual_start_ns_);
        }
    }
    return ok;
}

bool MultiNodeSessionOrchestrator::stopCapture() {
    capture_requested_stop_ns_ = getNowNs();
    bool ok = coordination_service_->stopCapture(capture_requested_stop_ns_);
    if (ok) {
        capture_actual_stop_ns_ = getNowNs();
        updateBeaconAvailability("IN_SESSION");
        if (on_capture_state_changed_) {
            on_capture_state_changed_(false, capture_actual_stop_ns_);
        }
    }
    return ok;
}

bool MultiNodeSessionOrchestrator::saveSession() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    if (active_session_id_.empty() || active_project_id_.empty()) return false;

    current_session_.set_end_time_ns(getNowNs());
    current_session_.set_status(schemas::SessionStatus::SAVED);

    if (storage_) {
        // 1. Save all participating nodes
        auto members = coordination_service_->getSessionMembers();
        for (const auto& m : members) {
            schemas::ScannerNode node(m.node_uuid, m.display_name, m.platform, m.ip_address, to_string(m.role));
            node.set_node_uuid(m.node_uuid);
            node.set_display_name(m.display_name);
            node.set_platform(m.platform);
            node.set_session_role(m.role);
            node.set_estimated_clock_offset_ns(m.clock_offset_ns);
            storage_->saveSessionScannerNode(active_project_id_, active_session_id_, node);
            current_session_.add_scanner_node(m.node_uuid, to_string(m.role));
        }

        // 2. Save session manifest
        return storage_->saveSession(current_session_);
    }
    return false;
}

bool MultiNodeSessionOrchestrator::joinSession(const std::string& session_id, const std::string& coord_ip, int coord_port) {
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        active_session_id_ = session_id;
    }
    cslam_engine_.clear();
    if (!coordination_service_->connectToCoordinator(coord_ip, coord_port)) {
        return false;
    }
    return coordination_service_->acceptSessionInvite(session_id);
}

bool MultiNodeSessionOrchestrator::leaveSession() {
    bool ok = coordination_service_->leaveSession();
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        active_session_id_.clear();
        active_project_id_.clear();
    }
    cslam_engine_.clear();
    updateBeaconAvailability("AVAILABLE");
    return ok;
}

bool MultiNodeSessionOrchestrator::publishLocalSubmap(const std::vector<schemas::PointXYZI>& points,
                                                      const schemas::Quaternion& imu_orientation,
                                                      const schemas::Pose3D& local_pose) {
    return publishLocalSubmap(points, local_pose.timestamp_ns > 0 ? std::vector<schemas::Pose3D>{local_pose} : std::vector<schemas::Pose3D>{}, imu_orientation, true);
}

bool MultiNodeSessionOrchestrator::publishLocalSubmap(const std::vector<schemas::PointXYZI>& points,
                                                      const std::vector<schemas::Pose3D>& trajectory,
                                                      const schemas::Quaternion& imu_orientation,
                                                      bool force_snap) {
    std::string local_uuid = getLocalNodeUuid();
    std::string display_name = getLocalNode().display_name();

    cslam_engine_.updateScannerSubmap(local_uuid, display_name, points, trajectory, imu_orientation, force_snap);

    nlohmann::json payload = cslam_engine_.serializeSubmapMessage(local_uuid);
    bool net_sent = coordination_service_->sendSubmapUpdate(payload);

    if (force_snap &&
        (coordination_service_->getSessionRole() == schemas::NodeSessionRole::SESSION_COORDINATOR ||
         coordination_service_->getActiveSessionId().empty())) {
        cslam_engine_.evaluateAndSnapOverlaps();
        if (!coordination_service_->getActiveSessionId().empty()) {
            nlohmann::json align_payload = cslam_engine_.serializeAlignmentState();
            coordination_service_->broadcastAlignmentUpdate(align_payload);
        }
    }

    if (on_cslam_updated_) {
        on_cslam_updated_();
    }
    return net_sent;
}

size_t MultiNodeSessionOrchestrator::triggerCslamOverlapEvaluation() {
    size_t newly_snapped = cslam_engine_.evaluateAndSnapOverlaps();
    if (!coordination_service_->getActiveSessionId().empty()) {
        nlohmann::json align_payload = cslam_engine_.serializeAlignmentState();
        coordination_service_->broadcastAlignmentUpdate(align_payload);
    }
    if (on_cslam_updated_) {
        on_cslam_updated_();
    }
    return newly_snapped;
}

std::vector<schemas::PointXYZI> MultiNodeSessionOrchestrator::getMergedCslamPointCloud(bool differentiate_scanner_colors) const {
    return cslam_engine_.getMergedPointCloud(differentiate_scanner_colors);
}

std::vector<ScannerSubmapState> MultiNodeSessionOrchestrator::getCslamScannerNodes() const {
    return cslam_engine_.getAllScannerStates();
}

std::string MultiNodeSessionOrchestrator::getActiveSessionId() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return active_session_id_;
}

std::string MultiNodeSessionOrchestrator::getActiveProjectId() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return active_project_id_;
}

schemas::NodeSessionRole MultiNodeSessionOrchestrator::getSessionRole() {
    return coordination_service_->getSessionRole();
}

std::vector<SessionMemberInfo> MultiNodeSessionOrchestrator::getSessionMembers() {
    return coordination_service_->getSessionMembers();
}

bool MultiNodeSessionOrchestrator::isCapturing() const {
    return coordination_service_->isCapturing();
}

} // namespace av::core::multi_node
