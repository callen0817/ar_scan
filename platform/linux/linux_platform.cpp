#include "linux_platform.hpp"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <chrono>

namespace av::platform::linux_os {

LinuxPlatformAdapter::LinuxPlatformAdapter() {
    detectJetsonHardware();
}

void LinuxPlatformAdapter::detectJetsonHardware() {
    std::ifstream dt_model("/proc/device-tree/model");
    if (dt_model.is_open()) {
        std::string line;
        std::getline(dt_model, line);
        // Strip trailing null characters if present in device-tree string
        while (!line.empty() && (line.back() == '\0' || line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.find("Jetson") != std::string::npos || line.find("NVIDIA") != std::string::npos) {
            is_jetson_ = true;
            jetson_model_ = line;
            return;
        }
    }

    // Secondary check via /etc/nv_tegra_release
    std::ifstream tegra_rel("/etc/nv_tegra_release");
    if (tegra_rel.is_open()) {
        is_jetson_ = true;
        jetson_model_ = "NVIDIA Jetson (Tegra L4T)";
    }
}

std::string LinuxPlatformAdapter::getPlatformName() const {
    if (is_jetson_) {
        return "JETSON_LINUX";
    }
    return "X86_LINUX";
}

HostSystemMetrics LinuxPlatformAdapter::getHostMetrics() {
    HostSystemMetrics m;
    m.is_jetson = is_jetson_;
    m.jetson_model = jetson_model_;

    char hostname_buf[256] = {0};
    if (gethostname(hostname_buf, sizeof(hostname_buf)) == 0) {
        m.hostname = hostname_buf;
    }

    struct utsname uts;
    if (uname(&uts) == 0) {
        m.os_name = uts.sysname;
        m.kernel_version = uts.release;
        m.architecture = uts.machine;
    }

    m.cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);

    // Read Memory metrics from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string key;
        uint64_t val = 0;
        std::string unit;
        while (meminfo >> key >> val >> unit) {
            if (key == "MemTotal:") {
                m.total_ram_bytes = val * 1024ULL;
            } else if (key == "MemAvailable:") {
                m.available_ram_bytes = val * 1024ULL;
            }
        }
    }

    // Read Disk metrics for root filesystem
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        m.total_disk_bytes = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
        m.available_disk_bytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
    }

    return m;
}

std::string LinuxPlatformAdapter::getDeviceSerialNumber() {
    // Check Tegra serial from /proc/device-tree/serial-number
    std::ifstream dt_serial("/proc/device-tree/serial-number");
    if (dt_serial.is_open()) {
        std::string serial;
        std::getline(dt_serial, serial);
        while (!serial.empty() && (serial.back() == '\0' || serial.back() == '\n' || serial.back() == '\r')) {
            serial.pop_back();
        }
        if (!serial.empty()) {
            return serial;
        }
    }

    // Fallback: machine-id
    std::ifstream mid("/etc/machine-id");
    if (mid.is_open()) {
        std::string id;
        std::getline(mid, id);
        return id;
    }

    return "UNKNOWN_HOST_SERIAL";
}

uint64_t LinuxPlatformAdapter::getMonotonicTimeNs() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

} // namespace av::platform::linux_os
