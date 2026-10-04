#include "coordination_service.hpp"
#include "node_identity_manager.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <chrono>
#include <sstream>

namespace av::core::multi_node {

static uint64_t getNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

template<typename T>
static T getPayloadValue(const nlohmann::json& payload, const std::string& key, const T& default_val) {
    if (payload.is_object() && payload.contains(key) && !payload[key].is_null()) {
        try {
            return payload[key].get<T>();
        } catch (...) {
            return default_val;
        }
    }
    return default_val;
}

CoordinationService::CoordinationService() = default;

CoordinationService::~CoordinationService() {
    stop();
}

bool CoordinationService::start(int listen_port) {
    if (running_.load()) return true;

    listen_port_ = listen_port;

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        AV_LOG_ERROR("CoordinationService", "Failed to create TCP server socket", strerror(errno));
        return false;
    }

    int reuse = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(listen_port_);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        AV_LOG_ERROR("CoordinationService", "Failed to bind TCP server socket to port " + std::to_string(listen_port_), strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 16) < 0) {
        AV_LOG_ERROR("CoordinationService", "Failed to listen on TCP socket", strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_.store(true);
    server_thread_ = std::thread(&CoordinationService::serverAcceptWorker, this);
    heartbeat_thread_ = std::thread(&CoordinationService::heartbeatWorker, this);

    AV_LOG_INFO("CoordinationService", "Started TCP coordination server on port " + std::to_string(listen_port_));
    return true;
}

void CoordinationService::stop() {
    if (!running_.exchange(false)) return;

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }

    // Close all connected peer sockets to unblock client threads
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        for (auto& kv : peer_sockets_) {
            if (kv.second >= 0) {
                close(kv.second);
                kv.second = -1;
            }
        }
    }

    if (server_thread_.joinable()) server_thread_.join();
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();

    for (auto& t : client_threads_) {
        if (t.joinable()) t.join();
    }
    client_threads_.clear();

    AV_LOG_INFO("CoordinationService", "Stopped TCP coordination service");
}

int CoordinationService::connectSocket(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

bool CoordinationService::sendMessage(int socket_fd, const CoordinationMessage& msg) {
    if (socket_fd < 0) return false;
    std::string serialized = msg.to_json().dump() + "\n";
    ssize_t sent = send(socket_fd, serialized.data(), serialized.size(), MSG_NOSIGNAL);
    return (sent == static_cast<ssize_t>(serialized.size()));
}

bool CoordinationService::broadcastToMembers(const CoordinationMessage& msg) {
    std::lock_guard<std::mutex> lock(members_mutex_);
    bool all_ok = true;
    for (const auto& kv : peer_sockets_) {
        if (kv.second >= 0) {
            if (!sendMessage(kv.second, msg)) {
                all_ok = false;
            }
        }
    }
    return all_ok;
}

bool CoordinationService::createSession(const std::string& session_id, const std::string& project_id) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    active_session_id_ = session_id;
    active_project_id_ = project_id;
    coordinator_uuid_ = NodeIdentityManager::instance().getNodeUuid();
    session_role_ = schemas::NodeSessionRole::SESSION_COORDINATOR;
    is_capturing_.store(false);

    // Add self to members
    std::lock_guard<std::mutex> mem_lock(members_mutex_);
    session_members_.clear();
    auto local_node = NodeIdentityManager::instance().getNode();
    SessionMemberInfo self_info;
    self_info.node_uuid = local_node.node_uuid();
    self_info.display_name = local_node.display_name();
    self_info.platform = local_node.platform();
    self_info.ip_address = local_node.ip_address();
    self_info.port = listen_port_;
    self_info.role = schemas::NodeSessionRole::SESSION_COORDINATOR;
    self_info.is_connected = true;
    self_info.is_ready = true;
    self_info.capture_state = "IDLE";
    session_members_[self_info.node_uuid] = self_info;

    AV_LOG_INFO("CoordinationService", "Created session " + session_id + " as COORDINATOR");
    return true;
}

bool CoordinationService::invitePeer(const std::string& peer_ip, int peer_port, const std::string& peer_node_uuid) {
    std::string sess_id;
    std::string proj_id;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (active_session_id_.empty() || session_role_ != schemas::NodeSessionRole::SESSION_COORDINATOR) {
            AV_LOG_WARN("CoordinationService", "Cannot invite peer: not an active coordinator");
            return false;
        }
        sess_id = active_session_id_;
        proj_id = active_project_id_;
    }

    int sock = -1;
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = peer_sockets_.find(peer_node_uuid);
        if (it != peer_sockets_.end() && it->second >= 0) {
            sock = it->second;
        }
    }

    if (sock < 0) {
        sock = connectSocket(peer_ip, peer_port);
        if (sock < 0) {
            AV_LOG_ERROR("CoordinationService", "Failed to connect to peer " + peer_ip + ":" + std::to_string(peer_port), strerror(errno));
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(members_mutex_);
            peer_sockets_[peer_node_uuid] = sock;
        }
        client_threads_.emplace_back(&CoordinationService::clientConnectionWorker, this, sock, peer_ip);
    }

    // Send HELLO first
    CoordinationMessage hello;
    hello.message_id = NodeIdentityManager::generateUuidV4();
    hello.session_id = sess_id;
    hello.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    hello.destination_node_uuid = peer_node_uuid;
    hello.timestamp_ns = getNowNs();
    hello.message_type = MessageType::HELLO;
    hello.payload = {
        {"display_name", NodeIdentityManager::instance().getNode().display_name()},
        {"platform", NodeIdentityManager::instance().detectPlatform()},
        {"coordination_port", listen_port_}
    };
    sendMessage(sock, hello);

    // Send INVITE_SESSION
    CoordinationMessage invite;
    invite.message_id = NodeIdentityManager::generateUuidV4();
    invite.session_id = sess_id;
    invite.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    invite.destination_node_uuid = peer_node_uuid;
    invite.timestamp_ns = getNowNs();
    invite.message_type = MessageType::INVITE_SESSION;
    invite.payload = {
        {"session_id", sess_id},
        {"project_id", proj_id},
        {"coordinator_node_uuid", NodeIdentityManager::instance().getNodeUuid()}
    };
    return sendMessage(sock, invite);
}

bool CoordinationService::connectToCoordinator(const std::string& coord_ip, int coord_port) {
    int sock = connectSocket(coord_ip, coord_port);
    if (sock < 0) {
        AV_LOG_ERROR("CoordinationService", "Failed to connect to coordinator at " + coord_ip + ":" + std::to_string(coord_port), strerror(errno));
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        coordinator_ip_ = coord_ip;
        coordinator_port_ = coord_port;
        session_role_ = schemas::NodeSessionRole::SESSION_PARTICIPANT;
    }

    client_threads_.emplace_back(&CoordinationService::clientConnectionWorker, this, sock, coord_ip);

    // Send HELLO
    CoordinationMessage hello;
    hello.message_id = NodeIdentityManager::generateUuidV4();
    hello.session_id = active_session_id_;
    hello.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    hello.timestamp_ns = getNowNs();
    hello.message_type = MessageType::HELLO;
    hello.payload = {
        {"display_name", NodeIdentityManager::instance().getNode().display_name()},
        {"platform", NodeIdentityManager::instance().detectPlatform()},
        {"coordination_port", listen_port_}
    };
    return sendMessage(sock, hello);
}

bool CoordinationService::acceptSessionInvite(const std::string& session_id) {
    std::string coord_uuid;
    int coord_sock = -1;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        active_session_id_ = session_id;
        session_role_ = schemas::NodeSessionRole::SESSION_PARTICIPANT;
        coord_uuid = coordinator_uuid_;
    }
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = peer_sockets_.find(coord_uuid);
        if (it != peer_sockets_.end()) coord_sock = it->second;
    }

    if (coord_sock >= 0) {
        CoordinationMessage join;
        join.message_id = NodeIdentityManager::generateUuidV4();
        join.session_id = session_id;
        join.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
        join.destination_node_uuid = coord_uuid;
        join.timestamp_ns = getNowNs();
        join.message_type = MessageType::JOIN_SESSION;
        join.payload = {
            {"display_name", NodeIdentityManager::instance().getNode().display_name()},
            {"platform", NodeIdentityManager::instance().detectPlatform()}
        };
        return sendMessage(coord_sock, join);
    }
    return false;
}

bool CoordinationService::reportReadyForCapture(const std::string& session_id) {
    std::string coord_uuid;
    int coord_sock = -1;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        coord_uuid = coordinator_uuid_;
    }
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = peer_sockets_.find(coord_uuid);
        if (it != peer_sockets_.end()) coord_sock = it->second;
    }

    if (coord_sock >= 0) {
        CoordinationMessage msg;
        msg.message_id = NodeIdentityManager::generateUuidV4();
        msg.session_id = session_id;
        msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
        msg.destination_node_uuid = coord_uuid;
        msg.timestamp_ns = getNowNs();
        msg.message_type = MessageType::READY_FOR_CAPTURE;
        msg.payload = {
            {"is_ready", true},
            {"local_timestamp_ns", getNowNs()}
        };
        return sendMessage(coord_sock, msg);
    }
    return false;
}

bool CoordinationService::reportCaptureStatus(const std::string& session_id, const std::string& capture_id, const std::string& status, uint64_t obs_count) {
    std::string coord_uuid;
    int coord_sock = -1;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        coord_uuid = coordinator_uuid_;
    }
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = peer_sockets_.find(coord_uuid);
        if (it != peer_sockets_.end()) coord_sock = it->second;
    }

    if (coord_sock >= 0) {
        CoordinationMessage msg;
        msg.message_id = NodeIdentityManager::generateUuidV4();
        msg.session_id = session_id;
        msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
        msg.destination_node_uuid = coord_uuid;
        msg.timestamp_ns = getNowNs();
        msg.message_type = MessageType::CAPTURE_STATUS;
        msg.payload = {
            {"capture_id", capture_id},
            {"status", status},
            {"observation_count", obs_count}
        };
        return sendMessage(coord_sock, msg);
    }
    return false;
}

bool CoordinationService::leaveSession() {
    std::string sess_id;
    std::string coord_uuid;
    int coord_sock = -1;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        sess_id = active_session_id_;
        coord_uuid = coordinator_uuid_;
        active_session_id_.clear();
        session_role_ = schemas::NodeSessionRole::UNASSIGNED;
        is_capturing_.store(false);
    }
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = peer_sockets_.find(coord_uuid);
        if (it != peer_sockets_.end()) coord_sock = it->second;
        session_members_.clear();
    }

    if (coord_sock >= 0 && !sess_id.empty()) {
        CoordinationMessage msg;
        msg.message_id = NodeIdentityManager::generateUuidV4();
        msg.session_id = sess_id;
        msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
        msg.destination_node_uuid = coord_uuid;
        msg.timestamp_ns = getNowNs();
        msg.message_type = MessageType::LEAVE_SESSION;
        sendMessage(coord_sock, msg);
    }
    return true;
}

bool CoordinationService::connectToPeer(const std::string& peer_ip, int peer_port) {
    int sock = connectSocket(peer_ip, peer_port);
    if (sock < 0) return false;

    client_threads_.emplace_back(&CoordinationService::clientConnectionWorker, this, sock, peer_ip);

    CoordinationMessage hello;
    hello.message_id = NodeIdentityManager::generateUuidV4();
    hello.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    hello.timestamp_ns = getNowNs();
    hello.message_type = MessageType::HELLO;
    hello.payload = {
        {"display_name", NodeIdentityManager::instance().getNode().display_name()},
        {"platform", NodeIdentityManager::instance().detectPlatform()},
        {"coordination_port", listen_port_}
    };
    return sendMessage(sock, hello);
}

// 2-Phase Coordinated Capture Initiation
bool CoordinationService::prepareCapture(uint64_t requested_start_ns) {
    std::string sess_id;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (session_role_ != schemas::NodeSessionRole::SESSION_COORDINATOR || active_session_id_.empty()) {
            return false;
        }
        sess_id = active_session_id_;
    }

    // Reset member ready states (except self)
    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        for (auto& kv : session_members_) {
            if (kv.first != NodeIdentityManager::instance().getNodeUuid()) {
                kv.second.is_ready = false;
                kv.second.capture_state = "PREPARING";
            }
        }
    }

    CoordinationMessage msg;
    msg.message_id = NodeIdentityManager::generateUuidV4();
    msg.session_id = sess_id;
    msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    msg.destination_node_uuid = "*";
    msg.timestamp_ns = getNowNs();
    msg.message_type = MessageType::PREPARE_CAPTURE;
    msg.payload = {
        {"requested_start_time_ns", requested_start_ns}
    };
    return broadcastToMembers(msg);
}

bool CoordinationService::startCapture(uint64_t requested_start_ns) {
    std::string sess_id;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (session_role_ != schemas::NodeSessionRole::SESSION_COORDINATOR || active_session_id_.empty()) {
            return false;
        }
        sess_id = active_session_id_;
    }

    // Idempotency: if already capturing, do not re-issue
    if (is_capturing_.exchange(true)) {
        AV_LOG_WARN("CoordinationService", "startCapture called while already capturing (idempotent ignore)");
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        for (auto& kv : session_members_) {
            kv.second.capture_state = "CAPTURING";
        }
    }

    CoordinationMessage msg;
    msg.message_id = NodeIdentityManager::generateUuidV4();
    msg.session_id = sess_id;
    msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    msg.destination_node_uuid = "*";
    msg.timestamp_ns = getNowNs();
    msg.message_type = MessageType::START_CAPTURE;
    msg.payload = {
        {"requested_start_time_ns", requested_start_ns}
    };
    return broadcastToMembers(msg);
}

bool CoordinationService::stopCapture(uint64_t requested_stop_ns) {
    std::string sess_id;
    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (session_role_ != schemas::NodeSessionRole::SESSION_COORDINATOR || active_session_id_.empty()) {
            return false;
        }
        sess_id = active_session_id_;
    }

    // Idempotency: if already stopped, ignore
    if (!is_capturing_.exchange(false)) {
        AV_LOG_WARN("CoordinationService", "stopCapture called while not capturing (idempotent ignore)");
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(members_mutex_);
        for (auto& kv : session_members_) {
            kv.second.capture_state = "STOPPED";
        }
    }

    CoordinationMessage msg;
    msg.message_id = NodeIdentityManager::generateUuidV4();
    msg.session_id = sess_id;
    msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
    msg.destination_node_uuid = "*";
    msg.timestamp_ns = getNowNs();
    msg.message_type = MessageType::STOP_CAPTURE;
    msg.payload = {
        {"requested_stop_time_ns", requested_stop_ns}
    };
    return broadcastToMembers(msg);
}

bool CoordinationService::closeSession() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    active_session_id_.clear();
    active_project_id_.clear();
    session_role_ = schemas::NodeSessionRole::UNASSIGNED;
    is_capturing_.store(false);

    std::lock_guard<std::mutex> mem_lock(members_mutex_);
    session_members_.clear();
    return true;
}

std::string CoordinationService::getActiveSessionId() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return active_session_id_;
}

schemas::NodeSessionRole CoordinationService::getSessionRole() {
    std::lock_guard<std::mutex> lock(session_mutex_);
    return session_role_;
}

std::vector<SessionMemberInfo> CoordinationService::getSessionMembers() {
    std::lock_guard<std::mutex> lock(members_mutex_);
    std::vector<SessionMemberInfo> result;
    for (const auto& kv : session_members_) {
        result.push_back(kv.second);
    }
    return result;
}

bool CoordinationService::isAllMembersReady() {
    std::lock_guard<std::mutex> lock(members_mutex_);
    if (session_members_.empty()) return false;
    for (const auto& kv : session_members_) {
        if (!kv.second.is_ready) return false;
    }
    return true;
}

bool CoordinationService::isCapturing() {
    return is_capturing_.load();
}

void CoordinationService::serverAcceptWorker() {
    while (running_.load()) {
        struct pollfd pfd{};
        pfd.fd = server_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 250);
        if (ret <= 0 || !(pfd.revents & POLLIN)) continue;

        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_sock < 0) continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        client_threads_.emplace_back(&CoordinationService::clientConnectionWorker, this, client_sock, std::string(ip_str));
    }
}

void CoordinationService::clientConnectionWorker(int socket_fd, std::string peer_ip) {
    std::string line_buffer;
    char buffer[4096];
    std::string associated_node_uuid;

    while (running_.load()) {
        struct pollfd pfd{};
        pfd.fd = socket_fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 250);
        if (ret < 0) break;
        if (ret == 0) continue; // poll timeout

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }

        if (pfd.revents & POLLIN) {
            ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0);
            if (bytes_read <= 0) {
                // Connection closed or error
                break;
            }

            line_buffer.append(buffer, bytes_read);
            size_t pos = 0;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);

                if (!line.empty()) {
                    try {
                        nlohmann::json j = nlohmann::json::parse(line);
                        CoordinationMessage msg = CoordinationMessage::from_json(j);
                        if (!msg.source_node_uuid.empty()) {
                            associated_node_uuid = msg.source_node_uuid;
                            std::lock_guard<std::mutex> lock(members_mutex_);
                            peer_sockets_[associated_node_uuid] = socket_fd;
                        }
                        handleIncomingMessage(socket_fd, msg, peer_ip);
                    } catch (const std::exception& e) {
                        AV_LOG_WARN("CoordinationService", "Malformed coordination packet: " + std::string(e.what()));
                    }
                }
            }
        }
    }

    close(socket_fd);

    // Requirement 9: Network failure is NOT capture failure!
    // Local capture continues independently. Notify UI and state machine.
    if (!associated_node_uuid.empty()) {
        AV_LOG_WARN("CoordinationService", "Peer disconnected: " + associated_node_uuid + " (Local capture preserved)");
        {
            std::lock_guard<std::mutex> lock(members_mutex_);
            auto it = session_members_.find(associated_node_uuid);
            if (it != session_members_.end()) {
                it->second.is_connected = false;
            }
            peer_sockets_.erase(associated_node_uuid);
        }
        if (on_connection_changed_) {
            on_connection_changed_(associated_node_uuid, false);
        }
    }
}

void CoordinationService::handleIncomingMessage(int socket_fd, const CoordinationMessage& msg, const std::string& peer_ip) {
    // Protocol version validation
    if (msg.protocol_version != PROTOCOL_VERSION) {
        AV_LOG_WARN("CoordinationService", "Rejected message with mismatched protocol version: " + msg.protocol_version);
        return;
    }

    if (msg.message_type == MessageType::HELLO) {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto& member = session_members_[msg.source_node_uuid];
        member.node_uuid = msg.source_node_uuid;
        member.ip_address = peer_ip;
        member.display_name = getPayloadValue<std::string>(msg.payload, "display_name", msg.source_node_uuid);
        member.platform = getPayloadValue<std::string>(msg.payload, "platform", "UNKNOWN");
        member.port = getPayloadValue<int>(msg.payload, "coordination_port", DEFAULT_COORDINATION_TCP_PORT);
        member.is_connected = true;
        member.last_heartbeat_ns = getNowNs();

        if (on_connection_changed_) {
            on_connection_changed_(msg.source_node_uuid, true);
        }

    } else if (msg.message_type == MessageType::INVITE_SESSION) {
        std::string sid = getPayloadValue<std::string>(msg.payload, "session_id", "");
        std::string pid = getPayloadValue<std::string>(msg.payload, "project_id", "");
        std::string coord_uuid = getPayloadValue<std::string>(msg.payload, "coordinator_node_uuid", msg.source_node_uuid);

        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            coordinator_uuid_ = coord_uuid;
        }

        AV_LOG_INFO("CoordinationService", "Received SESSION_INVITE for session " + sid + " from " + coord_uuid);
        if (on_invite_) {
            on_invite_(sid, coord_uuid, pid);
        }

    } else if (msg.message_type == MessageType::JOIN_SESSION) {
        // Participant joining our coordinated session
        std::string sid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = active_session_id_;
        }

        if (!sid.empty() && msg.session_id == sid) {
            {
                std::lock_guard<std::mutex> lock(members_mutex_);
                auto& member = session_members_[msg.source_node_uuid];
                member.node_uuid = msg.source_node_uuid;
                member.display_name = getPayloadValue<std::string>(msg.payload, "display_name", msg.source_node_uuid);
                member.platform = getPayloadValue<std::string>(msg.payload, "platform", "UNKNOWN");
                member.ip_address = peer_ip;
                member.role = schemas::NodeSessionRole::SESSION_PARTICIPANT;
                member.is_connected = true;
                member.is_ready = true;
                member.capture_state = is_capturing_.load() ? "CAPTURING" : "IDLE";
                member.last_heartbeat_ns = getNowNs();
            }

            AV_LOG_INFO("CoordinationService", "Node " + msg.source_node_uuid + " successfully JOINED session " + sid);
            if (on_member_joined_) {
                on_member_joined_(msg.source_node_uuid, sid);
            }

            // Acknowledge session status
            CoordinationMessage status_msg;
            status_msg.message_id = NodeIdentityManager::generateUuidV4();
            status_msg.session_id = sid;
            status_msg.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
            status_msg.destination_node_uuid = msg.source_node_uuid;
            status_msg.timestamp_ns = getNowNs();
            status_msg.message_type = MessageType::SESSION_STATUS;
            status_msg.payload = {
                {"is_capturing", is_capturing_.load()},
                {"coordinator_node_uuid", NodeIdentityManager::instance().getNodeUuid()}
            };
            sendMessage(socket_fd, status_msg);
        }

    } else if (msg.message_type == MessageType::LEAVE_SESSION) {
        {
            std::lock_guard<std::mutex> lock(members_mutex_);
            session_members_.erase(msg.source_node_uuid);
        }
        AV_LOG_INFO("CoordinationService", "Node " + msg.source_node_uuid + " LEFT session " + msg.session_id);
        if (on_member_left_) {
            on_member_left_(msg.source_node_uuid, msg.session_id);
        }

    } else if (msg.message_type == MessageType::PREPARE_CAPTURE) {
        // Validate session membership
        std::string sid;
        std::string coord_uuid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = active_session_id_;
            coord_uuid = coordinator_uuid_;
        }

        if (!sid.empty() && msg.session_id == sid && msg.source_node_uuid == coord_uuid) {
            uint64_t req_start = getPayloadValue<uint64_t>(msg.payload, "requested_start_time_ns", 0ULL);
            bool ready = true;
            if (on_prepare_capture_) {
                ready = on_prepare_capture_(sid, req_start);
            }
            if (ready) {
                reportReadyForCapture(sid);
            }
        }

    } else if (msg.message_type == MessageType::READY_FOR_CAPTURE) {
        std::lock_guard<std::mutex> lock(members_mutex_);
        auto it = session_members_.find(msg.source_node_uuid);
        if (it != session_members_.end()) {
            it->second.is_ready = true;
            it->second.capture_state = "READY";
            AV_LOG_INFO("CoordinationService", "Node " + msg.source_node_uuid + " is READY for capture");
        }

    } else if (msg.message_type == MessageType::START_CAPTURE) {
        // Validate session membership
        std::string sid;
        std::string coord_uuid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = active_session_id_;
            coord_uuid = coordinator_uuid_;
        }

        if (!sid.empty() && msg.session_id == sid && msg.source_node_uuid == coord_uuid) {
            // Idempotency check: don't start again if already capturing
            if (!is_capturing_.exchange(true)) {
                uint64_t req_start = getPayloadValue<uint64_t>(msg.payload, "requested_start_time_ns", 0ULL);
                std::string cap_id = getPayloadValue<std::string>(msg.payload, "capture_id", "");
                AV_LOG_INFO("CoordinationService", "Coordinated START_CAPTURE command executed for session " + sid);
                if (on_start_capture_) {
                    on_start_capture_(sid, cap_id, req_start);
                }
            }
        }

    } else if (msg.message_type == MessageType::STOP_CAPTURE) {
        // Validate session membership
        std::string sid;
        std::string coord_uuid;
        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            sid = active_session_id_;
            coord_uuid = coordinator_uuid_;
        }

        if (!sid.empty() && msg.session_id == sid && msg.source_node_uuid == coord_uuid) {
            // Idempotency check: don't stop again if already stopped
            if (is_capturing_.exchange(false)) {
                uint64_t req_stop = getPayloadValue<uint64_t>(msg.payload, "requested_stop_time_ns", 0ULL);
                AV_LOG_INFO("CoordinationService", "Coordinated STOP_CAPTURE command executed for session " + sid);
                if (on_stop_capture_) {
                    on_stop_capture_(sid, req_stop);
                }
            }
        }

    } else if (msg.message_type == MessageType::CAPTURE_STATUS) {
        std::string cap_id = getPayloadValue<std::string>(msg.payload, "capture_id", "");
        std::string st = getPayloadValue<std::string>(msg.payload, "status", "ACTIVE");
        uint64_t obs = getPayloadValue<uint64_t>(msg.payload, "observation_count", 0ULL);

        {
            std::lock_guard<std::mutex> lock(members_mutex_);
            auto it = session_members_.find(msg.source_node_uuid);
            if (it != session_members_.end()) {
                it->second.observation_count = obs;
                it->second.capture_state = st;
            }
        }
        if (on_capture_status_) {
            on_capture_status_(msg.source_node_uuid, cap_id, st, obs);
        }

    } else if (msg.message_type == MessageType::HEARTBEAT) {
        uint64_t now_ns = getNowNs();
        uint64_t sender_ts = msg.timestamp_ns;
        int64_t clock_offset = (sender_ts > 0) ? static_cast<int64_t>(sender_ts) - static_cast<int64_t>(now_ns) : 0;

        {
            std::lock_guard<std::mutex> lock(members_mutex_);
            auto it = session_members_.find(msg.source_node_uuid);
            if (it != session_members_.end()) {
                it->second.last_heartbeat_ns = now_ns;
                it->second.clock_offset_ns = clock_offset;
                it->second.is_connected = true;
            }
        }
    }
}

void CoordinationService::heartbeatWorker() {
    while (running_.load()) {
        CoordinationMessage hb;
        hb.message_id = NodeIdentityManager::generateUuidV4();
        hb.timestamp_ns = getNowNs();
        hb.source_node_uuid = NodeIdentityManager::instance().getNodeUuid();
        hb.message_type = MessageType::HEARTBEAT;

        {
            std::lock_guard<std::mutex> lock(session_mutex_);
            hb.session_id = active_session_id_;
        }

        broadcastToMembers(hb);

        // Sleep 2 seconds in 50ms increments
        for (int i = 0; i < 40 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

} // namespace av::core::multi_node
