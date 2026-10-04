#include "node_identity_manager.hpp"
#include "core/logging/logger.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <unistd.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace av::core::multi_node {

NodeIdentityManager& NodeIdentityManager::instance() {
    static NodeIdentityManager inst;
    return inst;
}

std::string NodeIdentityManager::generateUuidV4() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    // RFC 4122 compliance: set version to 4 and variant to 10xx
    part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (part1 >> 32);
    ss << "-";
    ss << std::setw(4) << ((part1 >> 16) & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part1 & 0xFFFF);
    ss << "-";
    ss << std::setw(4) << (part2 >> 48);
    ss << "-";
    ss << std::setw(12) << (part2 & 0xFFFFFFFFFFFFULL);

    return ss.str();
}

std::string NodeIdentityManager::detectPlatform() {
#if defined(_WIN32)
    return "WINDOWS_X64";
#elif defined(__APPLE__)
    #if defined(__aarch64__)
        return "MACOS_ARM64";
    #else
        return "MACOS_X64";
    #endif
#elif defined(__ANDROID__)
    return "ANDROID";
#elif defined(__linux__)
    #if defined(__aarch64__)
        return "LINUX_ARM64";
    #elif defined(__x86_64__)
        return "LINUX_X64";
    #else
        return "LINUX_UNKNOWN";
    #endif
#else
    return "UNKNOWN";
#endif
}

std::string NodeIdentityManager::detectHardwareMetadata() {
    std::string meta;
    // Check Jetson device-tree model
    std::ifstream dt_model("/proc/device-tree/model");
    if (dt_model.is_open()) {
        std::getline(dt_model, meta);
        // Remove trailing null bytes
        while (!meta.empty() && (meta.back() == '\0' || meta.back() == '\n' || meta.back() == '\r')) {
            meta.pop_back();
        }
    }

    if (meta.empty()) {
        std::ifstream tegra_rel("/etc/nv_tegra_release");
        if (tegra_rel.is_open()) {
            std::string line;
            std::getline(tegra_rel, line);
            meta = "NVIDIA Tegra (" + line.substr(0, 30) + ")";
        }
    }

    if (meta.empty()) {
        meta = "Generic " + detectPlatform() + " System";
    }

    return meta;
}

bool NodeIdentityManager::initialize(const std::string& identity_file_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (identity_file_path.empty()) {
        const char* home = std::getenv("HOME");
        std::filesystem::path home_path = home ? home : "/tmp";
        identity_path_ = home_path / ".av_scan" / "node_identity.json";
    } else {
        identity_path_ = identity_file_path;
    }

    if (std::filesystem::exists(identity_path_)) {
        if (loadFromFileLocked()) {
            initialized_ = true;
            AV_LOG_INFO("NodeIdentityManager", "Loaded persistent node identity: " + local_node_.node_uuid() +
                        " (" + local_node_.display_name() + ")");
            return true;
        }
    }

    // First run: generate persistent identity
    std::string uuid = generateUuidV4();
    char hostname_buf[256] = {0};
    gethostname(hostname_buf, sizeof(hostname_buf) - 1);
    std::string hostname(hostname_buf);
    if (hostname.empty()) hostname = "av-scanner-node";

    local_node_.set_node_uuid(uuid);
    local_node_.set_node_id(uuid);
    local_node_.set_display_name(hostname);
    local_node_.set_hostname(hostname);
    local_node_.set_platform(detectPlatform());
    local_node_.set_hardware_metadata(detectHardwareMetadata());
    local_node_.set_av_scan_version("1.0.0");
    local_node_.set_protocol_version("1.0");
    local_node_.set_online_state(schemas::NodeOnlineState::ONLINE);
    local_node_.set_session_role(schemas::NodeSessionRole::SESSION_COORDINATOR);

    // Discover active IP addresses
    std::vector<std::string> endpoints;
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* pAddr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                char ip_buf[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(pAddr->sin_addr), ip_buf, INET_ADDRSTRLEN);
                std::string ip(ip_buf);
                if (ip != "127.0.0.1") {
                    endpoints.push_back(ip + ":9111");
                }
            }
        }
        freeifaddrs(ifaddr);
    }
    if (endpoints.empty()) {
        endpoints.push_back("127.0.0.1:9111");
    }
    local_node_.set_network_endpoints(endpoints);
    local_node_.set_ip_address(endpoints.front().substr(0, endpoints.front().find(':')));

    bool saved = saveToFileLocked();
    initialized_ = true;
    AV_LOG_INFO("NodeIdentityManager", "Generated and persisted new node identity: " + uuid +
                " (" + local_node_.display_name() + ")");
    return saved;
}

schemas::ScannerNode NodeIdentityManager::getNode() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        // Fallback default init if not called explicitly
        const char* home = std::getenv("HOME");
        std::filesystem::path home_path = home ? home : "/tmp";
        identity_path_ = home_path / ".av_scan" / "node_identity.json";
        if (std::filesystem::exists(identity_path_)) {
            loadFromFileLocked();
        } else {
            saveToFileLocked();
        }
        initialized_ = true;
    }
    return local_node_;
}

std::string NodeIdentityManager::getNodeUuid() {
    std::lock_guard<std::mutex> lock(mutex_);
    return local_node_.node_uuid();
}

bool NodeIdentityManager::setDisplayName(const std::string& display_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    local_node_.set_display_name(display_name);
    return saveToFileLocked();
}

void NodeIdentityManager::updateNetworkEndpoints(const std::vector<std::string>& endpoints) {
    std::lock_guard<std::mutex> lock(mutex_);
    local_node_.set_network_endpoints(endpoints);
    if (!endpoints.empty()) {
        std::string ip = endpoints.front();
        size_t colon = ip.find(':');
        if (colon != std::string::npos) ip = ip.substr(0, colon);
        local_node_.set_ip_address(ip);
    }
}

bool NodeIdentityManager::loadFromFileLocked() {
    try {
        std::ifstream file(identity_path_);
        if (!file.is_open()) return false;
        nlohmann::json j;
        file >> j;
        local_node_ = schemas::ScannerNode::from_json(j);
        
        // Refresh dynamic runtime metadata while preserving UUID and custom name
        char hostname_buf[256] = {0};
        gethostname(hostname_buf, sizeof(hostname_buf) - 1);
        std::string current_hostname(hostname_buf);
        if (!current_hostname.empty()) {
            local_node_.set_hostname(current_hostname);
        }
        local_node_.set_platform(detectPlatform());
        local_node_.set_hardware_metadata(detectHardwareMetadata());
        return true;
    } catch (const std::exception& e) {
        AV_LOG_ERROR("NodeIdentityManager", "Failed to load identity file", e.what());
        return false;
    }
}

bool NodeIdentityManager::saveToFileLocked() {
    try {
        std::filesystem::create_directories(identity_path_.parent_path());
        std::string temp_file = identity_path_.string() + ".tmp";
        {
            std::ofstream file(temp_file, std::ios::trunc);
            if (!file.is_open()) return false;
            file << local_node_.to_json().dump(4);
        }
        std::filesystem::rename(temp_file, identity_path_);
        return true;
    } catch (const std::exception& e) {
        AV_LOG_ERROR("NodeIdentityManager", "Failed to write identity file", e.what());
        return false;
    }
}

} // namespace av::core::multi_node
