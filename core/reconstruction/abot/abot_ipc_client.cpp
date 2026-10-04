#include "abot_ipc_client.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

namespace av::core::reconstruction::abot {

ABotIpcClient::ABotIpcClient(const std::string& host, int port)
    : host_(host), port_(port) {}

ABotIpcClient::~ABotIpcClient() {
    disconnect();
}

bool ABotIpcClient::connect() {
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
        tv.tv_sec = 2;
        tv.tv_usec = 0;
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

void ABotIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool ABotIpcClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sock_fd_ >= 0;
}

bool ABotIpcClient::ensureDaemonRunning() {
    if (connect() && ping()) {
        return true;
    }
    disconnect();

    AV_LOG_INFO("ABotIpcClient", "Spawning isolated av_abot_daemon process...");
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        int logfd = open("/tmp/av_abot_daemon.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (logfd >= 0) {
            dup2(logfd, STDOUT_FILENO);
            dup2(logfd, STDERR_FILENO);
            close(logfd);
        }
        execlp("python3", "python3", "/home/scanar/av_scan/core/reconstruction/abot/av_abot_daemon.py", nullptr);
        _exit(127);
    }
    daemon_pid_ = pid;

    // Retry connection up to 30s as model weights load
    for (int i = 0; i < 30; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        if (connect() && ping()) {
            AV_LOG_INFO("ABotIpcClient", "Successfully connected to av_abot_daemon");
            return true;
        }
        disconnect();
    }

    AV_LOG_ERROR("ABotIpcClient", "Daemon Error", "Timed out waiting for av_abot_daemon to start on port " + std::to_string(port_));
    return false;
}

bool ABotIpcClient::ping() {
    if (!sendCommand("PING\n")) return false;
    std::string line;
    if (!readLine(line, 1000)) return false;
    return line == "PONG";
}

bool ABotIpcClient::identify(nlohmann::json& out_info) {
    if (!sendCommand("IDENTIFY\n")) return false;
    std::string line;
    if (!readLine(line, 2000)) return false;
    try {
        out_info = nlohmann::json::parse(line);
        return true;
    } catch (...) {
        return false;
    }
}

bool ABotIpcClient::startAcquisition(const std::string& dev_path) {
    std::string cmd = "START_ACQUISITION " + dev_path + "\n";
    if (!sendCommand(cmd)) return false;
    std::string line;
    if (!readLine(line, 30000)) return false; // Model preload / init can take up to 30s
    return line == "OK";
}

bool ABotIpcClient::stopAcquisition() {
    if (!sendCommand("STOP_ACQUISITION\n")) return false;
    std::string line;
    if (!readLine(line, 5000)) return false;
    return line == "OK";
}

bool ABotIpcClient::getTelemetry(nlohmann::json& out_telemetry) {
    if (!sendCommand("GET_TELEMETRY\n")) return false;
    std::string line;
    if (!readLine(line, 1000)) return false;
    try {
        out_telemetry = nlohmann::json::parse(line);
        return true;
    } catch (...) {
        return false;
    }
}

bool ABotIpcClient::getTrajectory(std::vector<schemas::Pose3D>& out_trajectory) {
    if (!sendCommand("GET_TRAJECTORY\n")) return false;
    std::string line;
    if (!readLine(line, 2000)) return false;
    try {
        auto j = nlohmann::json::parse(line);
        out_trajectory.clear();
        for (const auto& item : j) {
            schemas::Pose3D pose;
            pose.timestamp_ns = item.value("timestamp_ns", uint64_t(0));
            pose.position.x = item.value("x", item.contains("position") ? item["position"].value("x", 0.0) : 0.0);
            pose.position.y = item.value("y", item.contains("position") ? item["position"].value("y", 0.0) : 0.0);
            pose.position.z = item.value("z", item.contains("position") ? item["position"].value("z", 0.0) : 0.0);
            pose.orientation.w = item.value("qw", item.contains("orientation") ? item["orientation"].value("w", 1.0) : 1.0);
            pose.orientation.x = item.value("qx", item.contains("orientation") ? item["orientation"].value("x", 0.0) : 0.0);
            pose.orientation.y = item.value("qy", item.contains("orientation") ? item["orientation"].value("y", 0.0) : 0.0);
            pose.orientation.z = item.value("qz", item.contains("orientation") ? item["orientation"].value("z", 0.0) : 0.0);
            out_trajectory.push_back(pose);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ABotIpcClient::fetchNewPoints(std::vector<schemas::PointXYZRGB>& out_points) {
    if (!sendCommand("GET_NEW_POINTS\n")) return false;

    // Read 8 byte header: "CNPT" (4 bytes) + count (uint32_t)
    uint8_t header[8];
    if (!readExact(header, 8, 2000)) return false;

    if (std::memcmp(header, "CNPT", 4) != 0) {
        return false;
    }

    uint32_t count = 0;
    std::memcpy(&count, header + 4, 4);

    if (count == 0) {
        out_points.clear();
        return true;
    }

    // Each point: float32 x, y, z, intensity (4f = 16B), uint8 r, g, b, flag (4B) = 20B
    size_t body_len = static_cast<size_t>(count) * 20;
    std::vector<uint8_t> body(body_len);
    if (!readExact(body.data(), body_len, 3000)) return false;

    out_points.clear();
    out_points.reserve(count);

    const uint8_t* ptr = body.data();
    for (uint32_t i = 0; i < count; ++i) {
        float x, y, z, intensity;
        std::memcpy(&x, ptr, 4);
        std::memcpy(&y, ptr + 4, 4);
        std::memcpy(&z, ptr + 8, 4);
        std::memcpy(&intensity, ptr + 12, 4);
        uint8_t r = ptr[16];
        uint8_t g = ptr[17];
        uint8_t b = ptr[18];
        uint8_t flag = ptr[19];
        ptr += 20;

        schemas::PointXYZRGB pt;
        pt.x = x;
        pt.y = y;
        pt.z = z;
        pt.intensity = intensity;
        pt.r = r;
        pt.g = g;
        pt.b = b;
        pt.has_color = (flag != 0);
        out_points.push_back(pt);
    }

    return true;
}

bool ABotIpcClient::sendCommand(const std::string& cmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ < 0) {
        if (!connect()) return false;
    }
    ssize_t sent = ::send(sock_fd_, cmd.c_str(), cmd.size(), 0);
    return sent == static_cast<ssize_t>(cmd.size());
}

bool ABotIpcClient::readLine(std::string& out_line, int timeout_ms) {
    out_line.clear();
    auto start = std::chrono::steady_clock::now();
    while (true) {
        char ch;
        int res = ::recv(sock_fd_, &ch, 1, 0);
        if (res > 0) {
            if (ch == '\n') break;
            out_line += ch;
        } else if (res == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                auto now = std::chrono::steady_clock::now();
                int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (elapsed >= timeout_ms) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else {
                return false;
            }
        }
    }
    return true;
}

bool ABotIpcClient::readExact(uint8_t* buffer, size_t length, int timeout_ms) {
    size_t total_read = 0;
    auto start = std::chrono::steady_clock::now();
    while (total_read < length) {
        ssize_t res = ::recv(sock_fd_, buffer + total_read, length - total_read, 0);
        if (res > 0) {
            total_read += res;
        } else if (res == 0) {
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                auto now = std::chrono::steady_clock::now();
                int elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
                if (elapsed >= timeout_ms) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            } else {
                return false;
            }
        }
    }
    return true;
}

} // namespace av::core::reconstruction::abot
