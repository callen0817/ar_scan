#include "discovery_service.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <chrono>

namespace av::core::multi_node {

static uint64_t getNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

DiscoveryService::DiscoveryService() = default;

DiscoveryService::~DiscoveryService() {
    stop();
}

bool DiscoveryService::start(int port) {
    if (running_.load()) return true;

    discovery_port_ = port;

    // 1. Create Broadcast Socket
    broadcast_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_fd_ < 0) {
        AV_LOG_ERROR("DiscoveryService", "Failed to create broadcast UDP socket", strerror(errno));
        return false;
    }
    int broadcast_enable = 1;
    if (setsockopt(broadcast_fd_, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        AV_LOG_WARN("DiscoveryService", std::string("setsockopt SO_BROADCAST failed: ") + strerror(errno));
    }

    // 2. Create Listen Socket
    listen_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_fd_ < 0) {
        AV_LOG_ERROR("DiscoveryService", "Failed to create listener UDP socket", strerror(errno));
        close(broadcast_fd_);
        broadcast_fd_ = -1;
        return false;
    }

    int reuse = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    struct sockaddr_in recv_addr{};
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_port = htons(discovery_port_);
    recv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&recv_addr), sizeof(recv_addr)) < 0) {
        AV_LOG_ERROR("DiscoveryService", "Failed to bind UDP listen socket to port " + std::to_string(discovery_port_), strerror(errno));
        close(listen_fd_);
        close(broadcast_fd_);
        listen_fd_ = -1;
        broadcast_fd_ = -1;
        return false;
    }

    running_.store(true);
    listen_thread_ = std::thread(&DiscoveryService::listenWorker, this);
    broadcast_thread_ = std::thread(&DiscoveryService::broadcastWorker, this);

    AV_LOG_INFO("DiscoveryService", "Started UDP discovery service on port " + std::to_string(discovery_port_));
    return true;
}

void DiscoveryService::stop() {
    if (!running_.exchange(false)) return;

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
    if (broadcast_fd_ >= 0) {
        close(broadcast_fd_);
        broadcast_fd_ = -1;
    }

    if (broadcast_thread_.joinable()) broadcast_thread_.join();
    if (listen_thread_.joinable()) listen_thread_.join();

    AV_LOG_INFO("DiscoveryService", "Stopped UDP discovery service");
}

void DiscoveryService::updateLocalBeacon(const DiscoveryBeacon& beacon) {
    std::lock_guard<std::mutex> lock(beacon_mutex_);
    local_beacon_ = beacon;
}

void DiscoveryService::addKnownPeer(const std::string& ip_address, int port) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (const auto& p : known_peers_) {
        if (p.first == ip_address && p.second == port) return;
    }
    known_peers_.emplace_back(ip_address, port);
    AV_LOG_INFO("DiscoveryService", "Added routed/VPN known peer: " + ip_address + ":" + std::to_string(port));
}

void DiscoveryService::removeKnownPeer(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    for (auto it = known_peers_.begin(); it != known_peers_.end(); ) {
        if (it->first == ip_address) {
            it = known_peers_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> DiscoveryService::getKnownPeers() {
    std::lock_guard<std::mutex> lock(peers_mutex_);
    std::vector<std::string> result;
    for (const auto& p : known_peers_) {
        result.push_back(p.first + ":" + std::to_string(p.second));
    }
    return result;
}

std::vector<DiscoveredNodeEntry> DiscoveryService::getDiscoveredNodes() {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    uint64_t now_ns = getNowNs();
    std::vector<DiscoveredNodeEntry> nodes;

    for (auto it = discovered_nodes_.begin(); it != discovered_nodes_.end(); ) {
        uint64_t elapsed_ns = now_ns - it->second.last_seen_ns;
        if (elapsed_ns > 20ULL * 1000000000ULL) {
            // Prune node unseen for > 20 seconds
            it = discovered_nodes_.erase(it);
        } else {
            if (elapsed_ns > 6ULL * 1000000000ULL) {
                it->second.is_online = false;
            }
            nodes.push_back(it->second);
            ++it;
        }
    }
    return nodes;
}

bool DiscoveryService::getDiscoveredNode(const std::string& node_uuid, DiscoveredNodeEntry& out_entry) {
    std::lock_guard<std::mutex> lock(nodes_mutex_);
    auto it = discovered_nodes_.find(node_uuid);
    if (it != discovered_nodes_.end()) {
        out_entry = it->second;
        return true;
    }
    return false;
}

void DiscoveryService::broadcastWorker() {
    while (running_.load()) {
        DiscoveryBeacon beacon_copy;
        {
            std::lock_guard<std::mutex> lock(beacon_mutex_);
            beacon_copy = local_beacon_;
        }

        if (!beacon_copy.node_uuid.empty() && broadcast_fd_ >= 0) {
            beacon_copy.timestamp_ns = getNowNs();
            std::string payload = beacon_copy.to_json().dump();

            // 1. Send to local broadcast
            struct sockaddr_in bcast_addr{};
            bcast_addr.sin_family = AF_INET;
            bcast_addr.sin_port = htons(discovery_port_);
            bcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");

            sendto(broadcast_fd_, payload.data(), payload.size(), 0,
                   reinterpret_cast<struct sockaddr*>(&bcast_addr), sizeof(bcast_addr));

            // Also broadcast on localhost for single-machine tests
            struct sockaddr_in loopback_addr{};
            loopback_addr.sin_family = AF_INET;
            loopback_addr.sin_port = htons(discovery_port_);
            loopback_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            sendto(broadcast_fd_, payload.data(), payload.size(), 0,
                   reinterpret_cast<struct sockaddr*>(&loopback_addr), sizeof(loopback_addr));

            // 2. Send unicast to known routed/VPN/Tailscale peers
            std::vector<std::pair<std::string, int>> peers_copy;
            {
                std::lock_guard<std::mutex> lock(peers_mutex_);
                peers_copy = known_peers_;
            }
            for (const auto& peer : peers_copy) {
                struct sockaddr_in peer_addr{};
                peer_addr.sin_family = AF_INET;
                peer_addr.sin_port = htons(peer.second);
                peer_addr.sin_addr.s_addr = inet_addr(peer.first.c_str());
                sendto(broadcast_fd_, payload.data(), payload.size(), 0,
                       reinterpret_cast<struct sockaddr*>(&peer_addr), sizeof(peer_addr));
            }
        }

        // Sleep 1 second in 50ms increments for fast responsiveness to stop
        for (int i = 0; i < 20 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void DiscoveryService::listenWorker() {
    char buffer[4096];

    while (running_.load()) {
        struct pollfd pfd{};
        pfd.fd = listen_fd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 250); // 250ms poll timeout
        if (ret <= 0 || !(pfd.revents & POLLIN)) {
            continue;
        }

        struct sockaddr_in sender_addr{};
        socklen_t addr_len = sizeof(sender_addr);
        ssize_t bytes_read = recvfrom(listen_fd_, buffer, sizeof(buffer) - 1, 0,
                                      reinterpret_cast<struct sockaddr*>(&sender_addr), &addr_len);
        if (bytes_read <= 0) continue;

        buffer[bytes_read] = '\0';

        try {
            nlohmann::json j = nlohmann::json::parse(buffer);
            DiscoveryBeacon beacon = DiscoveryBeacon::from_json(j);

            if (beacon.protocol_identifier != PROTOCOL_IDENTIFIER) {
                continue; // Not an AV Scan beacon
            }

            std::string my_uuid;
            {
                std::lock_guard<std::mutex> lock(beacon_mutex_);
                my_uuid = local_beacon_.node_uuid;
            }

            // Ignore self-beacons
            if (beacon.node_uuid == my_uuid) {
                continue;
            }

            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);

            int coord_port = DEFAULT_COORDINATION_TCP_PORT;
            if (!beacon.coordination_endpoint.empty()) {
                size_t colon = beacon.coordination_endpoint.find(':');
                if (colon != std::string::npos) {
                    try {
                        coord_port = std::stoi(beacon.coordination_endpoint.substr(colon + 1));
                    } catch (...) {}
                }
            }

            {
                std::lock_guard<std::mutex> lock(nodes_mutex_);
                auto& entry = discovered_nodes_[beacon.node_uuid];
                entry.beacon = beacon;
                entry.ip_address = sender_ip;
                entry.coordination_port = coord_port;
                entry.last_seen_ns = getNowNs();
                entry.is_online = true;
                entry.in_active_session = (beacon.session_state == "IN_SESSION" || beacon.session_state == "CAPTURING");
            }

        } catch (const std::exception& e) {
            // Ignore malformed packets from foreign network traffic
        }
    }
}

} // namespace av::core::multi_node
