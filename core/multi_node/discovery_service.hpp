#pragma once

#include "multi_node_types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

namespace av::core::multi_node {

class DiscoveryService {
public:
    DiscoveryService();
    ~DiscoveryService();

    // Start UDP discovery beaconing and listener
    bool start(int port = DEFAULT_DISCOVERY_UDP_PORT);
    void stop();

    bool isRunning() const { return running_.load(); }

    // Update the local beacon broadcasted to peers
    void updateLocalBeacon(const DiscoveryBeacon& beacon);

    // Add manual / known-peer IP for routed, VPN, or Tailscale environments
    void addKnownPeer(const std::string& ip_address, int port = DEFAULT_DISCOVERY_UDP_PORT);
    void removeKnownPeer(const std::string& ip_address);
    std::vector<std::string> getKnownPeers();

    // Retrieve currently discovered peer nodes
    std::vector<DiscoveredNodeEntry> getDiscoveredNodes();

    // Find a specific discovered node by its persistent UUID
    bool getDiscoveredNode(const std::string& node_uuid, DiscoveredNodeEntry& out_entry);

private:
    std::atomic<bool> running_{false};
    int discovery_port_{DEFAULT_DISCOVERY_UDP_PORT};
    int listen_fd_{-1};
    int broadcast_fd_{-1};

    DiscoveryBeacon local_beacon_;
    std::mutex beacon_mutex_;

    std::unordered_map<std::string, DiscoveredNodeEntry> discovered_nodes_;
    std::mutex nodes_mutex_;

    std::vector<std::pair<std::string, int>> known_peers_;
    std::mutex peers_mutex_;

    std::thread broadcast_thread_;
    std::thread listen_thread_;

    void broadcastWorker();
    void listenWorker();
};

} // namespace av::core::multi_node
