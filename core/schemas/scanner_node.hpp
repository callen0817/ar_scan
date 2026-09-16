#pragma once

#include "types.hpp"
#include "sensor_instance.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class ScannerNode {
public:
    ScannerNode() = default;

    ScannerNode(const std::string& node_id,
                const std::string& hostname,
                const std::string& platform = "JETSON_LINUX",
                const std::string& ip_address = "127.0.0.1",
                const std::string& role = "PRIMARY_ORCHESTRATOR")
        : schema_version_(CURRENT_SCHEMA_VERSION),
          node_id_(node_id),
          hostname_(hostname),
          platform_(platform),
          ip_address_(ip_address),
          role_(role),
          status_("ONLINE") {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& node_id() const noexcept { return node_id_; }
    const std::string& hostname() const noexcept { return hostname_; }
    const std::string& platform() const noexcept { return platform_; }
    const std::string& ip_address() const noexcept { return ip_address_; }
    const std::string& role() const noexcept { return role_; }
    const std::string& status() const noexcept { return status_; }
    const std::vector<SensorInstance>& attached_sensors() const noexcept { return attached_sensors_; }

    void set_node_id(const std::string& id) { node_id_ = id; }
    void set_hostname(const std::string& hn) { hostname_ = hn; }
    void set_platform(const std::string& p) { platform_ = p; }
    void set_ip_address(const std::string& ip) { ip_address_ = ip; }
    void set_role(const std::string& r) { role_ = r; }
    void set_status(const std::string& st) { status_ = st; }

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
    std::string hostname_;
    std::string platform_{"JETSON_LINUX"};
    std::string ip_address_{"127.0.0.1"};
    std::string role_{"PRIMARY_ORCHESTRATOR"};
    std::string status_{"ONLINE"};
    std::vector<SensorInstance> attached_sensors_;
};

} // namespace av::core::schemas
