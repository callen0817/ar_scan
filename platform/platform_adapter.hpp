#pragma once

#include <string>
#include <cstdint>

namespace av::platform {

struct HostSystemMetrics {
    std::string hostname;
    std::string os_name;
    std::string kernel_version;
    std::string architecture;
    uint64_t total_ram_bytes{0};
    uint64_t available_ram_bytes{0};
    uint64_t total_disk_bytes{0};
    uint64_t available_disk_bytes{0};
    int cpu_cores{0};
    double cpu_utilization_percent{0.0};
    bool is_jetson{false};
    std::string jetson_model;
};

class IPlatformAdapter {
public:
    virtual ~IPlatformAdapter() = default;

    virtual std::string getPlatformName() const = 0;
    virtual HostSystemMetrics getHostMetrics() = 0;
    virtual std::string getDeviceSerialNumber() = 0;
    virtual uint64_t getMonotonicTimeNs() = 0;
};

} // namespace av::platform
