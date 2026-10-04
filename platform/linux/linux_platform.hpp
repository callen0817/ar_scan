#pragma once

#include "platform/platform_adapter.hpp"

namespace av::platform::linux_os {

class LinuxPlatformAdapter : public IPlatformAdapter {
public:
    LinuxPlatformAdapter();
    ~LinuxPlatformAdapter() override = default;

    std::string getPlatformName() const override;
    HostSystemMetrics getHostMetrics() override;
    std::string getDeviceSerialNumber() override;
    uint64_t getMonotonicTimeNs() override;

private:
    void detectJetsonHardware();

    bool is_jetson_{false};
    std::string jetson_model_;
    uint64_t prev_cpu_total_{0};
    uint64_t prev_cpu_idle_{0};
};

} // namespace av::platform::linux_os
