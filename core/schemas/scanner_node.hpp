#pragma once

#include "types.hpp"
#include "sensor_instance.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class ScannerNode {
public:
    ScannerNode() = default;

    ScannerNode(const std::string& node_id,
                const std::string& hostname,
                const std::string& platform = "LINUX_ARM64",
                const std::string& ip_address = "127.0.0.1",
                const std::string& role = "SESSION_COORDINATOR")
        : schema_version_(CURRENT_SCHEMA_VERSION),
          node_id_(node_id),
          node_uuid_(node_id),
          display_name_(hostname),
          hostname_(hostname),
          platform_(platform),
          ip_address_(ip_address),
          role_(role),
          status_("ONLINE"),
          session_role_(node_session_role_from_string(role)),
          online_state_(NodeOnlineState::ONLINE) {}

    int schema_version() const noexcept { return schema_version_; }

    // Primary Identity (UUIDv4)
    const std::string& node_uuid() const noexcept {
        return node_uuid_.empty() ? node_id_ : node_uuid_;
    }
    void set_node_uuid(const std::string& uuid) {
        node_uuid_ = uuid;
        if (node_id_.empty()) node_id_ = uuid;
    }

    // Backward-compatible node_id
    const std::string& node_id() const noexcept {
        return node_id_.empty() ? node_uuid_ : node_id_;
    }
    void set_node_id(const std::string& id) {
        node_id_ = id;
        if (node_uuid_.empty()) node_uuid_ = id;
    }

    const std::string& display_name() const noexcept {
        return display_name_.empty() ? hostname_ : display_name_;
    }
    void set_display_name(const std::string& name) { display_name_ = name; }

    const std::string& hostname() const noexcept { return hostname_; }
    void set_hostname(const std::string& hn) { hostname_ = hn; }

    const std::string& platform() const noexcept { return platform_; }
    void set_platform(const std::string& p) { platform_ = p; }

    const std::string& hardware_metadata() const noexcept { return hardware_metadata_; }
    void set_hardware_metadata(const std::string& meta) { hardware_metadata_ = meta; }

    const std::string& av_scan_version() const noexcept { return av_scan_version_; }
    void set_av_scan_version(const std::string& ver) { av_scan_version_ = ver; }

    const std::string& protocol_version() const noexcept { return protocol_version_; }
    void set_protocol_version(const std::string& pver) { protocol_version_ = pver; }

    const std::string& ip_address() const noexcept { return ip_address_; }
    void set_ip_address(const std::string& ip) { ip_address_ = ip; }

    const std::vector<std::string>& network_endpoints() const noexcept { return network_endpoints_; }
    void set_network_endpoints(const std::vector<std::string>& ep) { network_endpoints_ = ep; }
    void add_network_endpoint(const std::string& ep) { network_endpoints_.push_back(ep); }

    const std::vector<std::string>& capabilities() const noexcept { return capabilities_; }
    void set_capabilities(const std::vector<std::string>& caps) { capabilities_ = caps; }
    void add_capability(const std::string& cap) { capabilities_.push_back(cap); }

    // Session-specific role
    NodeSessionRole session_role() const noexcept { return session_role_; }
    void set_session_role(NodeSessionRole r) {
        session_role_ = r;
        role_ = to_string(r);
    }

    const std::string& role() const noexcept { return role_; }
    void set_role(const std::string& r) {
        role_ = r;
        session_role_ = node_session_role_from_string(r);
    }

    // Online state
    NodeOnlineState online_state() const noexcept { return online_state_; }
    void set_online_state(NodeOnlineState s) {
        online_state_ = s;
        status_ = to_string(s);
    }

    const std::string& status() const noexcept { return status_; }
    void set_status(const std::string& st) {
        status_ = st;
        online_state_ = node_online_state_from_string(st);
    }

    // Clock synchronization metadata
    ClockSyncSource clock_sync_source() const noexcept { return clock_sync_source_; }
    void set_clock_sync_source(ClockSyncSource src) { clock_sync_source_ = src; }

    int64_t estimated_clock_offset_ns() const noexcept { return estimated_clock_offset_ns_; }
    void set_estimated_clock_offset_ns(int64_t offset) { estimated_clock_offset_ns_ = offset; }

    uint64_t clock_uncertainty_ns() const noexcept { return clock_uncertainty_ns_; }
    void set_clock_uncertainty_ns(uint64_t unc) { clock_uncertainty_ns_ = unc; }

    uint64_t last_seen_timestamp_ns() const noexcept { return last_seen_timestamp_ns_; }
    void set_last_seen_timestamp_ns(uint64_t ts) { last_seen_timestamp_ns_ = ts; }

    // Attached sensors
    const std::vector<SensorInstance>& attached_sensors() const noexcept { return attached_sensors_; }
    void add_sensor(const SensorInstance& sensor) {
        attached_sensors_.push_back(sensor);
    }
    void clear_sensors() {
        attached_sensors_.clear();
    }

    nlohmann::json to_json() const;
    static ScannerNode from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string node_id_;
    std::string node_uuid_;
    std::string display_name_;
    std::string hostname_;
    std::string platform_{"LINUX_ARM64"};
    std::string hardware_metadata_;
    std::string av_scan_version_{"1.0.0"};
    std::string protocol_version_{"1.0"};
    std::string ip_address_{"127.0.0.1"};
    std::vector<std::string> network_endpoints_;
    std::vector<std::string> capabilities_;
    std::string role_{"SESSION_COORDINATOR"};
    std::string status_{"ONLINE"};
    NodeSessionRole session_role_{NodeSessionRole::SESSION_COORDINATOR};
    NodeOnlineState online_state_{NodeOnlineState::ONLINE};
    ClockSyncSource clock_sync_source_{ClockSyncSource::SYSTEM_UNSYNCED};
    int64_t estimated_clock_offset_ns_{0};
    uint64_t clock_uncertainty_ns_{0};
    uint64_t last_seen_timestamp_ns_{0};
    std::vector<SensorInstance> attached_sensors_;
};

} // namespace av::core::schemas
