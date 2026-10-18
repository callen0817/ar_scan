#pragma once

#include "core/schemas/types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::multi_node {

constexpr int DEFAULT_DISCOVERY_UDP_PORT = 9110;
constexpr int DEFAULT_COORDINATION_TCP_PORT = 9111;
constexpr const char* PROTOCOL_IDENTIFIER = "AV_SCAN_DISCOVERY";
constexpr const char* PROTOCOL_VERSION = "1.0";

// Lightweight LAN Discovery Beacon (UDP :9110)
struct DiscoveryBeacon {
    std::string protocol_identifier{PROTOCOL_IDENTIFIER};
    std::string protocol_version{PROTOCOL_VERSION};
    std::string node_uuid;
    std::string display_name;
    std::string hostname;
    std::string av_scan_version{"1.0.0"};
    std::string platform{"LINUX_ARM64"};
    std::string coordination_endpoint; // e.g. "10.0.0.80:9111"
    std::vector<std::string> capabilities;
    std::string session_state{"AVAILABLE"}; // "AVAILABLE", "IN_SESSION", "CAPTURING"
    std::string current_session_id;
    uint64_t timestamp_ns{0};

    nlohmann::json to_json() const {
        return {
            {"protocol_identifier", protocol_identifier},
            {"protocol_version", protocol_version},
            {"node_uuid", node_uuid},
            {"display_name", display_name},
            {"hostname", hostname},
            {"av_scan_version", av_scan_version},
            {"platform", platform},
            {"coordination_endpoint", coordination_endpoint},
            {"capabilities", capabilities},
            {"session_state", session_state},
            {"current_session_id", current_session_id},
            {"timestamp_ns", timestamp_ns}
        };
    }

    static DiscoveryBeacon from_json(const nlohmann::json& j) {
        DiscoveryBeacon b;
        b.protocol_identifier = j.value("protocol_identifier", "");
        b.protocol_version = j.value("protocol_version", "");
        b.node_uuid = j.value("node_uuid", "");
        b.display_name = j.value("display_name", "");
        b.hostname = j.value("hostname", "");
        b.av_scan_version = j.value("av_scan_version", "1.0.0");
        b.platform = j.value("platform", "UNKNOWN");
        b.coordination_endpoint = j.value("coordination_endpoint", "");
        if (j.contains("capabilities") && j["capabilities"].is_array()) {
            b.capabilities = j["capabilities"].get<std::vector<std::string>>();
        }
        b.session_state = j.value("session_state", "AVAILABLE");
        b.current_session_id = j.value("current_session_id", "");
        b.timestamp_ns = j.value("timestamp_ns", static_cast<uint64_t>(0));
        return b;
    }
};

// Typed TCP Coordination Message (TCP :9111)
struct CoordinationMessage {
    std::string protocol_version{PROTOCOL_VERSION};
    std::string message_id;
    std::string session_id;
    std::string source_node_uuid;
    std::string destination_node_uuid; // Empty or "*" for broadcast
    uint64_t timestamp_ns{0};
    std::string message_type; // HELLO, INVITE_SESSION, JOIN_SESSION, PREPARE_CAPTURE, etc.
    nlohmann::json payload = nlohmann::json::object();

    nlohmann::json to_json() const {
        return {
            {"protocol_version", protocol_version},
            {"message_id", message_id},
            {"session_id", session_id},
            {"source_node_uuid", source_node_uuid},
            {"destination_node_uuid", destination_node_uuid},
            {"timestamp_ns", timestamp_ns},
            {"message_type", message_type},
            {"payload", payload.is_object() ? payload : nlohmann::json::object()}
        };
    }

    static CoordinationMessage from_json(const nlohmann::json& j) {
        CoordinationMessage m;
        m.protocol_version = j.value("protocol_version", PROTOCOL_VERSION);
        m.message_id = j.value("message_id", "");
        m.session_id = j.value("session_id", "");
        m.source_node_uuid = j.value("source_node_uuid", "");
        m.destination_node_uuid = j.value("destination_node_uuid", "");
        m.timestamp_ns = j.value("timestamp_ns", static_cast<uint64_t>(0));
        m.message_type = j.value("message_type", "");
        if (j.contains("payload") && j["payload"].is_object()) {
            m.payload = j["payload"];
        } else {
            m.payload = nlohmann::json::object();
        }
        return m;
    }
};

// Message Type Enumeration Strings
namespace MessageType {
    constexpr const char* HELLO = "HELLO";
    constexpr const char* ANNOUNCE_CAPABILITIES = "ANNOUNCE_CAPABILITIES";
    constexpr const char* INVITE_SESSION = "INVITE_SESSION";
    constexpr const char* JOIN_SESSION = "JOIN_SESSION";
    constexpr const char* LEAVE_SESSION = "LEAVE_SESSION";
    constexpr const char* HEARTBEAT = "HEARTBEAT";
    constexpr const char* PREPARE_CAPTURE = "PREPARE_CAPTURE";
    constexpr const char* READY_FOR_CAPTURE = "READY_FOR_CAPTURE";
    constexpr const char* START_CAPTURE = "START_CAPTURE";
    constexpr const char* STOP_CAPTURE = "STOP_CAPTURE";
    constexpr const char* CAPTURE_STATUS = "CAPTURE_STATUS";
    constexpr const char* SESSION_STATUS = "SESSION_STATUS";
    constexpr const char* SYNC_STATUS = "SYNC_STATUS";
    constexpr const char* CSLAM_SUBMAP_UPDATE = "CSLAM_SUBMAP_UPDATE";
    constexpr const char* CSLAM_ALIGNMENT_UPDATE = "CSLAM_ALIGNMENT_UPDATE";
}

// Discovered Node Status in Local Registry
struct DiscoveredNodeEntry {
    DiscoveryBeacon beacon;
    std::string ip_address;
    int coordination_port{DEFAULT_COORDINATION_TCP_PORT};
    uint64_t last_seen_ns{0};
    bool is_online{true};
    bool in_active_session{false};
};

} // namespace av::core::multi_node
