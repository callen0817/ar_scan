#pragma once

#include "types.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

enum class SensorStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    STREAMING,
    ERROR
};

inline std::string to_string(SensorStatus s) {
    switch (s) {
        case SensorStatus::DISCONNECTED: return "DISCONNECTED";
        case SensorStatus::CONNECTING: return "CONNECTING";
        case SensorStatus::CONNECTED: return "CONNECTED";
        case SensorStatus::STREAMING: return "STREAMING";
        case SensorStatus::ERROR: return "ERROR";
        default: return "DISCONNECTED";
    }
}

inline SensorStatus sensor_status_from_string(const std::string& str) {
    if (str == "CONNECTING") return SensorStatus::CONNECTING;
    if (str == "CONNECTED") return SensorStatus::CONNECTED;
    if (str == "STREAMING") return SensorStatus::STREAMING;
    if (str == "ERROR") return SensorStatus::ERROR;
    return SensorStatus::DISCONNECTED;
}

class SensorInstance {
public:
    SensorInstance() = default;

    SensorInstance(const std::string& instance_id,
                   const std::string& node_id,
                   const std::string& profile_id,
                   const std::string& serial = "",
                   const std::string& transport_endpoint = "",
                   const std::string& mount_label = "DEFAULT")
        : schema_version_(CURRENT_SCHEMA_VERSION),
          instance_id_(instance_id),
          scanner_node_id_(node_id),
          device_profile_id_(profile_id),
          serial_number_(serial),
          transport_endpoint_(transport_endpoint),
          mount_label_(mount_label),
          status_(SensorStatus::DISCONNECTED) {}

    int schema_version() const noexcept { return schema_version_; }
    const std::string& instance_id() const noexcept { return instance_id_; }
    const std::string& scanner_node_id() const noexcept { return scanner_node_id_; }
    const std::string& device_profile_id() const noexcept { return device_profile_id_; }
    const std::string& serial_number() const noexcept { return serial_number_; }
    const std::string& transport_endpoint() const noexcept { return transport_endpoint_; }
    const std::string& mount_label() const noexcept { return mount_label_; }
    SensorStatus status() const noexcept { return status_; }

    void set_instance_id(const std::string& id) { instance_id_ = id; }
    void set_scanner_node_id(const std::string& nid) { scanner_node_id_ = nid; }
    void set_device_profile_id(const std::string& pid) { device_profile_id_ = pid; }
    void set_serial_number(const std::string& sn) { serial_number_ = sn; }
    void set_transport_endpoint(const std::string& ep) { transport_endpoint_ = ep; }
    void set_mount_label(const std::string& ml) { mount_label_ = ml; }
    void set_status(SensorStatus st) { status_ = st; }

    nlohmann::json to_json() const;
    static SensorInstance from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string instance_id_;
    std::string scanner_node_id_;
    std::string device_profile_id_;
    std::string serial_number_;
    std::string transport_endpoint_;
    std::string mount_label_{"DEFAULT"};
    SensorStatus status_{SensorStatus::DISCONNECTED};
};

} // namespace av::core::schemas
