#pragma once

#include "multi_node_types.hpp"
#include "core/schemas/scanner_node.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

namespace av::core::multi_node {

struct SessionMemberInfo {
    std::string node_uuid;
    std::string display_name;
    std::string platform;
    std::string ip_address;
    int port{DEFAULT_COORDINATION_TCP_PORT};
    schemas::NodeSessionRole role{schemas::NodeSessionRole::SESSION_PARTICIPANT};
    bool is_connected{false};
    bool is_ready{false};
    std::string capture_state{"IDLE"}; // IDLE, READY, CAPTURING, STOPPED
    uint64_t last_heartbeat_ns{0};
    uint64_t observation_count{0};
    int64_t clock_offset_ns{0};
};

class CoordinationService {
public:
    using InviteCallback = std::function<void(const std::string& session_id, const std::string& coordinator_uuid, const std::string& project_id)>;
    using MemberCallback = std::function<void(const std::string& node_uuid, const std::string& session_id)>;
    using PrepareCaptureCallback = std::function<bool(const std::string& session_id, uint64_t requested_start_ns)>;
    using StartCaptureCallback = std::function<void(const std::string& session_id, const std::string& capture_id, uint64_t requested_start_ns)>;
    using StopCaptureCallback = std::function<void(const std::string& session_id, uint64_t requested_stop_ns)>;
    using StatusCallback = std::function<void(const std::string& node_uuid, const std::string& capture_id, const std::string& status, uint64_t obs_count)>;
    using ConnectionCallback = std::function<void(const std::string& node_uuid, bool connected)>;

    CoordinationService();
    ~CoordinationService();

    // Start TCP coordination server on specified port
    bool start(int listen_port = DEFAULT_COORDINATION_TCP_PORT);
    void stop();
    bool isRunning() const { return running_.load(); }
    int getListenPort() const { return listen_port_; }

    // Register Handlers
    void setInviteCallback(InviteCallback cb) { on_invite_ = cb; }
    void setMemberJoinedCallback(MemberCallback cb) { on_member_joined_ = cb; }
    void setMemberLeftCallback(MemberCallback cb) { on_member_left_ = cb; }
    void setPrepareCaptureCallback(PrepareCaptureCallback cb) { on_prepare_capture_ = cb; }
    void setStartCaptureCallback(StartCaptureCallback cb) { on_start_capture_ = cb; }
    void setStopCaptureCallback(StopCaptureCallback cb) { on_stop_capture_ = cb; }
    void setCaptureStatusCallback(StatusCallback cb) { on_capture_status_ = cb; }
    void setConnectionCallback(ConnectionCallback cb) { on_connection_changed_ = cb; }

    // Session Management (Coordinator)
    bool createSession(const std::string& session_id, const std::string& project_id);
    bool invitePeer(const std::string& peer_ip, int peer_port, const std::string& peer_node_uuid);
    bool prepareCapture(uint64_t requested_start_ns);
    bool startCapture(uint64_t requested_start_ns);
    bool stopCapture(uint64_t requested_stop_ns);
    bool closeSession();

    // Session Management (Participant)
    bool connectToCoordinator(const std::string& coord_ip, int coord_port);
    bool acceptSessionInvite(const std::string& session_id);
    bool reportReadyForCapture(const std::string& session_id);
    bool reportCaptureStatus(const std::string& session_id, const std::string& capture_id, const std::string& status, uint64_t obs_count);
    bool leaveSession();

    // Manual peer connection (for routed/Tailscale non-broadcast environments)
    bool connectToPeer(const std::string& peer_ip, int peer_port);

    // Current State
    std::string getActiveSessionId();
    schemas::NodeSessionRole getSessionRole();
    std::vector<SessionMemberInfo> getSessionMembers();
    bool isAllMembersReady();
    bool isCapturing();

private:
    std::atomic<bool> running_{false};
    int listen_port_{DEFAULT_COORDINATION_TCP_PORT};
    int server_fd_{-1};

    std::string active_session_id_;
    std::string active_project_id_;
    std::string coordinator_uuid_;
    std::string coordinator_ip_;
    int coordinator_port_{DEFAULT_COORDINATION_TCP_PORT};
    schemas::NodeSessionRole session_role_{schemas::NodeSessionRole::UNASSIGNED};
    std::atomic<bool> is_capturing_{false};
    std::mutex session_mutex_;

    std::unordered_map<std::string, SessionMemberInfo> session_members_;
    std::unordered_map<std::string, int> peer_sockets_; // node_uuid -> socket fd
    std::mutex members_mutex_;

    // Sockets to threads mapping
    std::vector<std::thread> client_threads_;
    std::thread server_thread_;
    std::thread heartbeat_thread_;

    // Callbacks
    InviteCallback on_invite_;
    MemberCallback on_member_joined_;
    MemberCallback on_member_left_;
    PrepareCaptureCallback on_prepare_capture_;
    StartCaptureCallback on_start_capture_;
    StopCaptureCallback on_stop_capture_;
    StatusCallback on_capture_status_;
    ConnectionCallback on_connection_changed_;

    void serverAcceptWorker();
    void clientConnectionWorker(int socket_fd, std::string peer_ip);
    void heartbeatWorker();

    bool sendMessage(int socket_fd, const CoordinationMessage& msg);
    bool broadcastToMembers(const CoordinationMessage& msg);
    void handleIncomingMessage(int socket_fd, const CoordinationMessage& msg, const std::string& peer_ip);

    int connectSocket(const std::string& ip, int port);
};

} // namespace av::core::multi_node
