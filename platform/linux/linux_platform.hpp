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
};

} // namespace av::platform::linux_os
