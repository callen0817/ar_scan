#include "lingbot_ipc_client.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

namespace av::core::reconstruction::lingbot {

LingBotIpcClient::LingBotIpcClient(const std::string& host, int port)
    : host_(host), port_(port) {}

LingBotIpcClient::~LingBotIpcClient() {
    disconnect();
}

bool LingBotIpcClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) return true;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

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
        tv.tv_usec = 500000;
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

    fcntl(fd, F_SETFL, flags);
    sock_fd_ = fd;
    return true;
}

void LingBotIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool LingBotIpcClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sock_fd_ >= 0;
}

bool LingBotIpcClient::ensureDaemonRunning() {
    if (connect() && ping()) {
        return true;
    }
    disconnect();

    AV_LOG_INFO("LingBotIpcClient", "Spawning isolated av_lingbot_daemon process...");
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int logfd = open("/tmp/av_lingbot_daemon.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        execlp("python3", "python3", "/home/scanar/av_scan/core/reconstruction/lingbot/av_lingbot_daemon.py", nullptr);
        _exit(1);
    } else if (pid > 0) {
        daemon_pid_ = pid;
        for (int i = 0; i < 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (connect() && ping()) {
                AV_LOG_INFO("LingBotIpcClient", "Successfully connected to av_lingbot_daemon (PID " + std::to_string(pid) + ")");
                return true;
            }
            disconnect();
        }
    }
    AV_LOG_ERROR("LingBotIpcClient", "Daemon Startup Failed", "Failed to start or connect to av_lingbot_daemon");
    return false;
}

bool LingBotIpcClient::sendCommand(const std::string& cmd) {
    if (sock_fd_ < 0) return false;
    std::string to_send = cmd + "\n";
    ssize_t sent = send(sock_fd_, to_send.c_str(), to_send.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(to_send.size());
}

bool LingBotIpcClient::readLine(std::string& out_line, int timeout_ms) {
    out_line.clear();
    if (sock_fd_ < 0) return false;

    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    char ch;
    while (true) {
        ssize_t r = recv(sock_fd_, &ch, 1, 0);
        if (r <= 0) return false;
        if (ch == '\n') break;
        out_line.push_back(ch);
    }
    return true;
}

bool LingBotIpcClient::readExact(uint8_t* buffer, size_t length, int timeout_ms) {
    if (sock_fd_ < 0) return false;
    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    size_t total = 0;
    while (total < length) {
        ssize_t r = recv(sock_fd_, buffer + total, length - total, 0);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

bool LingBotIpcClient::ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("PING")) return false;
    std::string resp;
    return readLine(resp, 1000) && resp == "PONG";
}

bool LingBotIpcClient::identify(nlohmann::json& out_info) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("IDENTIFY")) return false;
    std::string resp;
    if (!readLine(resp, 3000)) return false;
    try {
        out_info = nlohmann::json::parse(resp);
        return true;
    } catch (...) {
        return false;
    }
}

bool LingBotIpcClient::startAcquisition(const std::string& dev_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("START_ACQUISITION " + dev_path)) return false;
    std::string resp;
    return readLine(resp, 90000) && resp == "OK";
}

bool LingBotIpcClient::stopAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("STOP_ACQUISITION")) return false;
    std::string resp;
    return readLine(resp, 5000) && resp == "OK";
}

bool LingBotIpcClient::getTelemetry(nlohmann::json& out_telemetry) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TELEMETRY")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    try {
        out_telemetry = nlohmann::json::parse(resp);
        return true;
    } catch (...) {
        return false;
    }
}

bool LingBotIpcClient::getTrajectory(std::vector<schemas::Pose3D>& out_trajectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TRAJECTORY")) return false;
    std::string resp;
    if (!readLine(resp, 5000)) return false;
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

bool LingBotIpcClient::fetchNewPoints(std::vector<schemas::PointXYZRGB>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_NEW_POINTS")) return false;

    uint8_t header[8];
    if (!readExact(header, 8, 3000)) return false;

    bool is_valid = (std::memcmp(header, "CAPS", 4) == 0 || std::memcmp(header, "CNPT", 4) == 0);
    if (!is_valid) return false;

    uint32_t count = 0;
    std::memcpy(&count, header + 4, 4);

    out_points.clear();
    if (count == 0) return true;

    out_points.reserve(count);
    size_t payload_bytes = count * 20;
    std::vector<uint8_t> raw(payload_bytes);
    if (!readExact(raw.data(), payload_bytes, 10000)) return false;

    for (size_t i = 0; i < count; ++i) {
        const uint8_t* ptr = raw.data() + i * 20;
        schemas::PointXYZRGB pt;
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
    return true;
}

} // namespace av::core::reconstruction::lingbot
