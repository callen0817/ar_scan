#include "gemini_ipc_client.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <iostream>

namespace av::core::drivers::gemini {

GeminiIpcClient::GeminiIpcClient(const std::string& host, int port)
    : host_(host), port_(port) {}

GeminiIpcClient::~GeminiIpcClient() {
    disconnect();
}

bool GeminiIpcClient::connect() {
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

void GeminiIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool GeminiIpcClient::ensureDaemonRunning() {
    if (connect() && ping()) {
        return true;
    }
    disconnect();

    AV_LOG_INFO("GeminiIpcClient", "Spawning isolated av_gemini_daemon process...");
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
        execlp("python3", "python3", "/home/scanar/av_scan/core/drivers/gemini/av_gemini_daemon.py", nullptr);
        _exit(1);
    } else if (pid > 0) {
        daemon_pid_ = pid;
        // Wait up to 3 seconds for daemon to bind and become ready
        for (int i = 0; i < 15; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (connect() && ping()) {
                AV_LOG_INFO("GeminiIpcClient", "Successfully connected to av_gemini_daemon (PID " + std::to_string(pid) + ")");
                return true;
            }
            disconnect();
        }
    }
    AV_LOG_ERROR("GeminiIpcClient", "Daemon Startup Failed", "Failed to start or connect to av_gemini_daemon");
    return false;
}

bool GeminiIpcClient::sendCommand(const std::string& cmd) {
    if (sock_fd_ < 0) return false;
    std::string to_send = cmd + "\n";
    ssize_t sent = send(sock_fd_, to_send.c_str(), to_send.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(to_send.size());
}

bool GeminiIpcClient::readLine(std::string& out_line, int timeout_ms) {
    out_line.clear();
    if (sock_fd_ < 0) return false;

    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    char ch = 0;
    while (true) {
        ssize_t n = recv(sock_fd_, &ch, 1, 0);
        if (n <= 0) return false;
        if (ch == '\n') break;
        out_line.push_back(ch);
        if (out_line.size() > 65536) return false; // Safety limit
    }
    return true;
}

bool GeminiIpcClient::readExact(uint8_t* buffer, size_t size, int timeout_ms) {
    if (sock_fd_ < 0) return false;
    struct timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    size_t total = 0;
    while (total < size) {
        ssize_t n = recv(sock_fd_, buffer + total, size - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

bool GeminiIpcClient::ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("PING")) return false;
    std::string resp;
    if (!readLine(resp, 1000)) return false;
    return resp.find("PONG") != std::string::npos;
}

bool GeminiIpcClient::identify(nlohmann::json& out_info) {
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

bool GeminiIpcClient::startAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("START")) return false;
    std::string resp;
    if (!readLine(resp, 5000)) return false;
    return resp.find("START_OK") != std::string::npos;
}

bool GeminiIpcClient::stopAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("STOP")) return false;
    std::string resp;
    if (!readLine(resp, 4000)) return false;
    return resp.find("STOP_OK") != std::string::npos;
}

bool GeminiIpcClient::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("RESET")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    return resp.find("RESET_OK") != std::string::npos;
}

bool GeminiIpcClient::getTelemetry(GeminiTelemetry& out_telem) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TELEMETRY")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    try {
        auto j = nlohmann::json::parse(resp);
        out_telem.status = j.value("status", "UNKNOWN");
        out_telem.tracking_state = j.value("tracking_state", "NO_TRACKING");
        out_telem.is_capturing = j.value("is_capturing", false);
        out_telem.point_count = j.value("point_count", 0ULL);
        out_telem.trajectory_count = j.value("trajectory_count", 0ULL);
        out_telem.feature_count = j.value("feature_count", 0);
        out_telem.feature_quality = j.value("feature_quality", 0.0);

        if (j.contains("current_pose")) {
            auto cp = j["current_pose"];
            out_telem.current_pose.timestamp_ns = cp.value("timestamp_ns", 0ULL);
            out_telem.current_pose.position.x = cp.value("x", 0.0);
            out_telem.current_pose.position.y = cp.value("y", 0.0);
            out_telem.current_pose.position.z = cp.value("z", 0.0);
            out_telem.current_pose.orientation.x = cp.value("qx", 0.0);
            out_telem.current_pose.orientation.y = cp.value("qy", 0.0);
            out_telem.current_pose.orientation.z = cp.value("qz", 0.0);
            out_telem.current_pose.orientation.w = cp.value("qw", 1.0);
            out_telem.current_yaw = cp.value("yaw", 0.0);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool GeminiIpcClient::getNewPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_NEW_POINTS")) return false;

    uint8_t hdr[8];
    if (!readExact(hdr, 8, 3000)) return false;

    bool is_colored = (std::memcmp(hdr, "CNPT", 4) == 0);
    if (!is_colored && std::memcmp(hdr, "NPTS", 4) != 0) return false;
    uint32_t count = 0;
    std::memcpy(&count, hdr + 4, 4);

    if (count == 0) return true;

    out_points.reserve(out_points.size() + count);
    if (is_colored) {
        size_t payload_bytes = count * 20;
        std::vector<uint8_t> raw(payload_bytes);
        if (!readExact(raw.data(), payload_bytes, 5000)) return false;

        for (uint32_t i = 0; i < count; ++i) {
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
        size_t payload_bytes = count * 4 * sizeof(float);
        std::vector<float> raw(count * 4);
        if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), payload_bytes, 5000)) return false;

        for (uint32_t i = 0; i < count; ++i) {
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

bool GeminiIpcClient::getAllPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_ALL_POINTS")) return false;

    uint8_t hdr[8];
    if (!readExact(hdr, 8, 3000)) return false;

    bool is_colored = (std::memcmp(hdr, "CAPS", 4) == 0);
    if (!is_colored && std::memcmp(hdr, "APTS", 4) != 0) return false;
    uint32_t count = 0;
    std::memcpy(&count, hdr + 4, 4);

    if (count == 0) return true;

    out_points.clear();
    out_points.reserve(count);
    if (is_colored) {
        size_t payload_bytes = count * 20;
        std::vector<uint8_t> raw(payload_bytes);
        if (!readExact(raw.data(), payload_bytes, 8000)) return false;

        for (uint32_t i = 0; i < count; ++i) {
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
        size_t payload_bytes = count * 4 * sizeof(float);
        std::vector<float> raw(count * 4);
        if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), payload_bytes, 8000)) return false;

        for (uint32_t i = 0; i < count; ++i) {
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

bool GeminiIpcClient::getTrajectory(std::vector<schemas::Pose3D>& out_trajectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TRAJECTORY")) return false;
    std::string resp;
    if (!readLine(resp, 3000)) return false;
    try {
        auto j = nlohmann::json::parse(resp);
        out_trajectory.clear();
        for (const auto& item : j) {
            schemas::Pose3D p;
            p.timestamp_ns = item.value("timestamp_ns", 0ULL);
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

bool GeminiIpcClient::getGridMap(nlohmann::json& out_grid) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_GRID_MAP")) return false;
    std::string resp;
    if (!readLine(resp, 3000)) return false;
    try {
        out_grid = nlohmann::json::parse(resp);
        return true;
    } catch (...) {
        return false;
    }
}

bool GeminiIpcClient::shutdownDaemon() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ < 0) return true;
    sendCommand("SHUTDOWN");
    std::string resp;
    readLine(resp, 1000);
    close(sock_fd_);
    sock_fd_ = -1;
    return true;
}

bool GeminiIpcClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sock_fd_ >= 0;
}

} // namespace av::core::drivers::gemini
