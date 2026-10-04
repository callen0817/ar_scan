#pragma once

#include "multi_node_types.hpp"
#include "node_identity_manager.hpp"
#include "discovery_service.hpp"
#include "coordination_service.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/schemas/session.hpp"
#include "core/schemas/capture.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>

namespace av::core::multi_node {

class MultiNodeSessionOrchestrator {
public:
    MultiNodeSessionOrchestrator();
    ~MultiNodeSessionOrchestrator();

    // Initialize multi-node subsystem with storage engine and optional identity file
    bool initialize(std::shared_ptr<storage::StorageEngine> storage,
                    const std::string& identity_file_path = "",
                    int discovery_port = DEFAULT_DISCOVERY_UDP_PORT,
                    int coordination_port = DEFAULT_COORDINATION_TCP_PORT);

    void shutdown();

    // Local Node Identity
    schemas::ScannerNode getLocalNode();
    std::string getLocalNodeUuid();
    bool setDisplayName(const std::string& name);
    void registerLocalSensor(const schemas::SensorInstance& sensor);
    void clearLocalSensors();

    // Discovery & Peer Connections
    std::vector<DiscoveredNodeEntry> getDiscoveredNodes();
    void addKnownPeer(const std::string& ip, int port = DEFAULT_DISCOVERY_UDP_PORT);
    bool connectDirectPeer(const std::string& ip, int port = DEFAULT_COORDINATION_TCP_PORT);

    // Session Orchestration (Coordinator)
    bool createSession(const std::string& project_id, const std::string& session_name);
    bool inviteNode(const std::string& node_uuid);
    bool prepareCapture();
    bool startCapture();
    bool stopCapture();
    bool saveSession();

    // Session Orchestration (Participant)
    bool joinSession(const std::string& session_id, const std::string& coord_ip, int coord_port);
    bool leaveSession();

    // Callbacks for GUI / App Bridge
    using InviteReceivedHandler = std::function<void(const std::string& session_id, const std::string& coordinator_uuid, const std::string& project_id)>;
    using CaptureStateChangedHandler = std::function<void(bool capturing, uint64_t timestamp_ns)>;
    using MemberListChangedHandler = std::function<void()>;

    void setInviteHandler(InviteReceivedHandler h) { on_invite_handler_ = h; }
    void setCaptureStateChangedHandler(CaptureStateChangedHandler h) { on_capture_state_changed_ = h; }
    void setMemberListChangedHandler(MemberListChangedHandler h) { on_member_list_changed_ = h; }

    // Session State
    std::string getActiveSessionId();
    std::string getActiveProjectId();
    schemas::NodeSessionRole getSessionRole();
    std::vector<SessionMemberInfo> getSessionMembers();
    bool isCapturing() const;

    // Component accessors for testing
    std::shared_ptr<DiscoveryService> discovery() { return discovery_service_; }
    std::shared_ptr<CoordinationService> coordination() { return coordination_service_; }

private:
    std::shared_ptr<storage::StorageEngine> storage_;
    std::shared_ptr<DiscoveryService> discovery_service_;
    std::shared_ptr<CoordinationService> coordination_service_;

    std::string active_project_id_;
    std::string active_session_id_;
    schemas::Session current_session_;
    std::mutex session_mutex_;

    uint64_t capture_requested_start_ns_{0};
    uint64_t capture_actual_start_ns_{0};
    uint64_t capture_requested_stop_ns_{0};
    uint64_t capture_actual_stop_ns_{0};

    InviteReceivedHandler on_invite_handler_;
    CaptureStateChangedHandler on_capture_state_changed_;
    MemberListChangedHandler on_member_list_changed_;

    void setupCallbacks();
    void updateBeaconAvailability(const std::string& state);
};

} // namespace av::core::multi_node
