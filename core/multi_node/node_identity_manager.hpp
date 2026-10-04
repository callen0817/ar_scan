#pragma once

#include "core/schemas/scanner_node.hpp"
#include <string>
#include <mutex>
#include <filesystem>

namespace av::core::multi_node {

class NodeIdentityManager {
public:
    static NodeIdentityManager& instance();

    // Initialize identity with optional explicit storage path (defaults to ~/.av_scan/node_identity.json)
    bool initialize(const std::string& identity_file_path = "");

    // Retrieve the persistent ScannerNode representing this local installation
    schemas::ScannerNode getNode();

    // Node UUID (immutable across restarts and network changes)
    std::string getNodeUuid();

    // Update and persist a customized display name for this node
    bool setDisplayName(const std::string& display_name);

    // Refresh dynamic network endpoints (active IP addresses)
    void updateNetworkEndpoints(const std::vector<std::string>& endpoints);

    // Platform detection string
    static std::string detectPlatform();

    // Hardware metadata detection string (e.g. Jetson Orin model)
    static std::string detectHardwareMetadata();

    // RFC 4122 compliant UUIDv4 generator
    static std::string generateUuidV4();

private:
    NodeIdentityManager() = default;
    ~NodeIdentityManager() = default;

    std::mutex mutex_;
    std::filesystem::path identity_path_;
    schemas::ScannerNode local_node_;
    bool initialized_{false};

    bool loadFromFileLocked();
    bool saveToFileLocked();
};

} // namespace av::core::multi_node
