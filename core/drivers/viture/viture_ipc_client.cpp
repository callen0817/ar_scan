#include "viture_ipc_client.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

namespace av::core::drivers::viture {

VitureIpcClient::VitureIpcClient(const std::string& host, int port)
    : host_(host), port_(port) {}

VitureIpcClient::~VitureIpcClient() {
    disconnect();
}

bool VitureIpcClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) return true;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    // Set non-blocking for connect with timeout
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &serv_addr.sin_addr) <= 0) {
        close(fd);
        return false;
    }

    int res = ::connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (res < 0 && errno != EINPROGRESS) {
        close(fd);
        return false;
    }

    if (res < 0) {
        fd_set wait_set;
        FD_ZERO(&wait_set);
        FD_SET(fd, &wait_set);
        struct timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 500000; // 1.5s timeout
        int sel = select(fd + 1, nullptr, &wait_set, nullptr, &tv);
        if (sel <= 0) {
            close(fd);
            return false;
        }
        int so_error = 0;
        socklen_t len = sizeof(so_error);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            close(fd);
            return false;
        }
    }

    // Set back to blocking
    fcntl(fd, F_SETFL, flags);

    // Default socket timeouts
    struct timeval tv{};
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    sock_fd_ = fd;
    return true;
}

void VitureIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool VitureIpcClient::ensureDaemonRunning() {
    if (connect() && ping()) {
        return true;
    }
    disconnect();

    AV_LOG_INFO("VitureIpcClient", "Spawning isolated av_viture_daemon process...");
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("python3", "python3", "/home/scanar/av_scan/core/drivers/viture/av_viture_daemon.py", nullptr);
        _exit(1);
    } else if (pid > 0) {
        daemon_pid_ = pid;
        // Wait up to 3 seconds for daemon to bind and become ready
        for (int i = 0; i < 15; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (connect() && ping()) {
                AV_LOG_INFO("VitureIpcClient", "Successfully connected to av_viture_daemon (PID " + std::to_string(pid) + ")");
                return true;
            }
            disconnect();
        }
    }
    AV_LOG_ERROR("VitureIpcClient", "Daemon Startup Failed", "Failed to start or connect to av_viture_daemon");
    return false;
}

bool VitureIpcClient::sendCommand(const std::string& cmd) {
    if (sock_fd_ < 0) return false;
    std::string to_send = cmd + "\n";
    ssize_t sent = send(sock_fd_, to_send.c_str(), to_send.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(to_send.size());
}

bool VitureIpcClient::readLine(std::string& out_line, int timeout_ms) {
    out_line.clear();
    if (sock_fd_ < 0) return false;

    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    char ch;
    while (true) {
        ssize_t n = recv(sock_fd_, &ch, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (ch == '\n') {
            break;
        }
        out_line.push_back(ch);
    }
    return true;
}

bool VitureIpcClient::readExact(uint8_t* buffer, size_t size, int timeout_ms) {
    if (sock_fd_ < 0) return false;

    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    size_t total_read = 0;
    while (total_read < size) {
        ssize_t n = recv(sock_fd_, buffer + total_read, size - total_read, 0);
        if (n <= 0) {
            return false;
        }
        total_read += static_cast<size_t>(n);
    }
    return true;
}

bool VitureIpcClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sock_fd_ >= 0;
}

bool VitureIpcClient::ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("PING")) return false;
    std::string resp;
    if (!readLine(resp, 1000)) return false;
    return resp == "PONG";
}

bool VitureIpcClient::identify(nlohmann::json& out_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("IDENTIFY")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    try {
        out_info = nlohmann::json::parse(resp);
        return true;
    } catch (...) {
        return false;
    }
}

bool VitureIpcClient::startAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("START")) return false;
    std::string resp;
    if (!readLine(resp, 5000)) return false;
    return resp.find("START_OK") != std::string::npos;
}

bool VitureIpcClient::stopAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("STOP")) return false;
    std::string resp;
    if (!readLine(resp, 3000)) return false;
    return resp.find("STOP_OK") != std::string::npos;
}

bool VitureIpcClient::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("RESET")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    return resp.find("RESET_OK") != std::string::npos;
}

bool VitureIpcClient::getTelemetry(VitureTelemetry& out_telem) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TELEMETRY")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    try {
        auto j = nlohmann::json::parse(resp);
        out_telem.status = j.value("status", "UNKNOWN");
        out_telem.tracking_state = j.value("tracking_state", "NO_TRACKING");
        out_telem.is_capturing = j.value("is_capturing", false);
        out_telem.point_count = j.value("point_count", uint64_t(0));
        out_telem.trajectory_count = j.value("trajectory_count", uint64_t(0));
        out_telem.current_yaw = j.value("current_yaw", 0.0);
        out_telem.feature_count = j.value("feature_count", 0);
        out_telem.feature_quality = j.value("feature_quality", 0.0);

        if (j.contains("current_pose")) {
            auto& p = j["current_pose"];
            out_telem.current_pose.position.x = p.value("x", 0.0);
            out_telem.current_pose.position.y = p.value("y", 0.0);
            out_telem.current_pose.position.z = p.value("z", 0.0);
            out_telem.current_pose.orientation.x = p.value("qx", 0.0);
            out_telem.current_pose.orientation.y = p.value("qy", 0.0);
            out_telem.current_pose.orientation.z = p.value("qz", 0.0);
            out_telem.current_pose.orientation.w = p.value("qw", 1.0);
            out_telem.current_pose.timestamp_ns = p.value("timestamp_ns", uint64_t(0));
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool VitureIpcClient::getNewPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_NEW_POINTS")) return false;

    // Read 8-byte header: 4 bytes tag + 4 bytes uint32 count
    uint8_t header[8];
    if (!readExact(header, 8, 2000)) return false;

    bool is_colored = (std::memcmp(header, "CNPT", 4) == 0);
    if (!is_colored && std::memcmp(header, "NPTS", 4) != 0) {
        return false;
    }

    uint32_t count = 0;
    std::memcpy(&count, header + 4, 4);

    out_points.clear();
    if (count == 0) return true;

    out_points.reserve(count);
    if (is_colored) {
        size_t payload_bytes = count * 20;
        std::vector<uint8_t> raw(payload_bytes);
        if (!readExact(raw.data(), payload_bytes, 5000)) return false;

        for (size_t i = 0; i < count; ++i) {
            const uint8_t* ptr = raw.data() + i * 20;
            schemas::PointXYZI pt;
            std::memcpy(&pt.x, ptr + 0, 4);
            std::memcpy(&pt.y, ptr + 4, 4);
            std::memcpy(&pt.z, ptr + 8, 4);
            std::memcpy(&pt.intensity, ptr + 12, 4);
            pt.r = ptr[16];
            pt.g = ptr[17];
            pt.b = ptr[18];
            pt.has_color = (ptr[19] != 0);
            out_points.push_back(pt);
        }
    } else {
        std::vector<float> raw(count * 4);
        if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), count * 16, 5000)) {
            return false;
        }

        for (size_t i = 0; i < count; ++i) {
            schemas::PointXYZI pt;
            pt.x = raw[i * 4 + 0];
            pt.y = raw[i * 4 + 1];
            pt.z = raw[i * 4 + 2];
            pt.intensity = raw[i * 4 + 3];
            pt.r = 255; pt.g = 255; pt.b = 255;
            pt.has_color = false;
            out_points.push_back(pt);
        }
    }
    return true;
}

bool VitureIpcClient::getAllPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_ALL_POINTS")) return false;

    // Read 8-byte header: 4 bytes tag + 4 bytes uint32 count
    uint8_t header[8];
    if (!readExact(header, 8, 2000)) return false;

    bool is_colored = (std::memcmp(header, "CAPS", 4) == 0);
    if (!is_colored && std::memcmp(header, "APTS", 4) != 0) {
        return false;
    }

    uint32_t count = 0;
    std::memcpy(&count, header + 4, 4);

    out_points.clear();
    if (count == 0) return true;

    out_points.reserve(count);
    if (is_colored) {
        size_t payload_bytes = count * 20;
        std::vector<uint8_t> raw(payload_bytes);
        if (!readExact(raw.data(), payload_bytes, 10000)) return false;

        for (size_t i = 0; i < count; ++i) {
            const uint8_t* ptr = raw.data() + i * 20;
            schemas::PointXYZI pt;
            std::memcpy(&pt.x, ptr + 0, 4);
            std::memcpy(&pt.y, ptr + 4, 4);
            std::memcpy(&pt.z, ptr + 8, 4);
            std::memcpy(&pt.intensity, ptr + 12, 4);
            pt.r = ptr[16];
            pt.g = ptr[17];
            pt.b = ptr[18];
            pt.has_color = (ptr[19] != 0);
            out_points.push_back(pt);
        }
    } else {
        std::vector<float> raw(count * 4);
        if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), count * 16, 10000)) {
            return false;
        }

        for (size_t i = 0; i < count; ++i) {
            schemas::PointXYZI pt;
            pt.x = raw[i * 4 + 0];
            pt.y = raw[i * 4 + 1];
            pt.z = raw[i * 4 + 2];
            pt.intensity = raw[i * 4 + 3];
            pt.r = 255; pt.g = 255; pt.b = 255;
            pt.has_color = false;
            out_points.push_back(pt);
        }
    }
    return true;
}

bool VitureIpcClient::getTrajectory(std::vector<schemas::Pose3D>& out_trajectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TRAJECTORY")) return false;
    std::string resp;
    if (!readLine(resp, 3000)) return false;
    try {
        auto j = nlohmann::json::parse(resp);
        out_trajectory.clear();
        for (const auto& item : j) {
            schemas::Pose3D p;
            p.timestamp_ns = item.value("timestamp_ns", uint64_t(0));
            p.position.x = item.value("x", 0.0);
            p.position.y = item.value("y", 0.0);
            p.position.z = item.value("z", 0.0);
            p.orientation.x = item.value("qx", 0.0);
            p.orientation.y = item.value("qy", 0.0);
            p.orientation.z = item.value("qz", 0.0);
            p.orientation.w = item.value("qw", 1.0);
            out_trajectory.push_back(p);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool VitureIpcClient::getGridMap(nlohmann::json& out_grid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_GRID_MAP")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    try {
        out_grid = nlohmann::json::parse(resp);
        return true;
    } catch (...) {
        return false;
    }
}

bool VitureIpcClient::shutdownDaemon() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ < 0) return true;
    sendCommand("SHUTDOWN");
    std::string resp;
    readLine(resp, 1000);
    close(sock_fd_);
    sock_fd_ = -1;
    return true;
}

} // namespace av::core::drivers::viture
