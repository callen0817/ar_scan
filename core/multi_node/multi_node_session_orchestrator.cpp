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
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            current_session_.add_scanner_node(node_uuid, "SESSION_PARTICIPANT");
        }
        if (on_member_list_changed_) {
            on_member_list_changed_();
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
}

void MultiNodeSessionOrchestrator::updateBeaconAvailability(const std::string& state) {
    auto node = NodeIdentityManager::instance().getNode();
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
    return NodeIdentityManager::instance().getNode();
}

std::string MultiNodeSessionOrchestrator::getLocalNodeUuid() {
    return NodeIdentityManager::instance().getNodeUuid();
}

bool MultiNodeSessionOrchestrator::setDisplayName(const std::string& name) {
    bool ok = NodeIdentityManager::instance().setDisplayName(name);
    if (ok) updateBeaconAvailability(coordination_service_->isCapturing() ? "CAPTURING" : "AVAILABLE");
    return ok;
}

void MultiNodeSessionOrchestrator::registerLocalSensor(const schemas::SensorInstance& sensor) {
    // Keep local node schemas updated
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
    updateBeaconAvailability("AVAILABLE");
    return ok;
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
