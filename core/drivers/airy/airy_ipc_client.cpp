#include "airy_ipc_client.hpp"
#include "core/logging/logger.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <chrono>
#include <thread>

namespace av::core::drivers::airy {

AiryIpcClient::AiryIpcClient(const std::string& host, int port)
    : host_(host), port_(port) {}

AiryIpcClient::~AiryIpcClient() {
    disconnect();
}

bool AiryIpcClient::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sock_fd_ >= 0;
}

bool AiryIpcClient::connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) return true;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &serv_addr.sin_addr) <= 0) {
        close(fd);
        return false;
    }

    struct timeval tv{};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (::connect(fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(fd);
        return false;
    }

    sock_fd_ = fd;
    return true;
}

void AiryIpcClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool AiryIpcClient::ensureDaemonRunning() {
    if (connect() && ping()) {
        return true;
    }
    disconnect();

    AV_LOG_INFO("AiryIpcClient", "Spawning isolated av_airy_daemon process...");
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
        execlp("python3", "python3", "/home/scanar/av_scan/core/drivers/airy/av_airy_daemon.py", nullptr);
        _exit(1);
    } else if (pid > 0) {
        daemon_pid_ = pid;
        // Wait up to 3 seconds for daemon to bind and become ready
        for (int i = 0; i < 15; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (connect() && ping()) {
                AV_LOG_INFO("AiryIpcClient", "Successfully connected to av_airy_daemon (PID " + std::to_string(pid) + ")");
                return true;
            }
            disconnect();
        }
    }
    AV_LOG_ERROR("AiryIpcClient", "Daemon Startup Failed", "Failed to start or connect to av_airy_daemon");
    return false;
}

bool AiryIpcClient::sendCommand(const std::string& cmd) {
    if (sock_fd_ < 0) return false;
    std::string to_send = cmd + "\n";
    ssize_t sent = send(sock_fd_, to_send.c_str(), to_send.size(), MSG_NOSIGNAL);
    return sent == static_cast<ssize_t>(to_send.size());
}

bool AiryIpcClient::readLine(std::string& out_line, int timeout_ms) {
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

bool AiryIpcClient::readExact(uint8_t* buffer, size_t size, int timeout_ms) {
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

bool AiryIpcClient::ping() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("PING")) return false;
    std::string resp;
    if (!readLine(resp, 1000)) return false;
    return resp.find("PONG") != std::string::npos;
}

bool AiryIpcClient::identify(nlohmann::json& out_info) {
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

bool AiryIpcClient::startAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("START")) return false;
    std::string resp;
    if (!readLine(resp, 5000)) return false;
    return resp.find("START_OK") != std::string::npos;
}

bool AiryIpcClient::stopAcquisition() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("STOP")) return false;
    std::string resp;
    if (!readLine(resp, 4000)) return false;
    return resp.find("STOP_OK") != std::string::npos;
}

bool AiryIpcClient::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("RESET")) return false;
    std::string resp;
    if (!readLine(resp, 2000)) return false;
    return resp.find("RESET_OK") != std::string::npos;
}

bool AiryIpcClient::getTelemetry(AiryTelemetry& out_telem) {
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

bool AiryIpcClient::getNewPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_NEW_POINTS")) return false;

    uint8_t hdr[8];
    if (!readExact(hdr, 8, 3000)) return false;

    if (std::memcmp(hdr, "NPTS", 4) != 0) return false;
    uint32_t count = 0;
    std::memcpy(&count, hdr + 4, 4);

    if (count == 0) return true;

    size_t payload_bytes = count * 4 * sizeof(float);
    std::vector<float> raw(count * 4);
    if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), payload_bytes, 5000)) return false;

    out_points.reserve(out_points.size() + count);
    for (uint32_t i = 0; i < count; ++i) {
        schemas::PointXYZI pt;
        pt.x = raw[i * 4 + 0];
        pt.y = raw[i * 4 + 1];
        pt.z = raw[i * 4 + 2];
        pt.intensity = raw[i * 4 + 3];
        out_points.push_back(pt);
    }
    return true;
}

bool AiryIpcClient::getAllPoints(std::vector<schemas::PointXYZI>& out_points) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_ALL_POINTS")) return false;

    uint8_t hdr[8];
    if (!readExact(hdr, 8, 3000)) return false;

    if (std::memcmp(hdr, "APTS", 4) != 0) return false;
    uint32_t count = 0;
    std::memcpy(&count, hdr + 4, 4);

    out_points.clear();
    if (count == 0) return true;

    size_t payload_bytes = count * 4 * sizeof(float);
    std::vector<float> raw(count * 4);
    if (!readExact(reinterpret_cast<uint8_t*>(raw.data()), payload_bytes, 8000)) return false;

    out_points.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        out_points[i].x = raw[i * 4 + 0];
        out_points[i].y = raw[i * 4 + 1];
        out_points[i].z = raw[i * 4 + 2];
        out_points[i].intensity = raw[i * 4 + 3];
    }
    return true;
}

bool AiryIpcClient::getTrajectory(std::vector<schemas::Pose3D>& out_trajectory) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("GET_TRAJECTORY")) return false;
    std::string resp;
    if (!readLine(resp, 4000)) return false;
    try {
        auto arr = nlohmann::json::parse(resp);
        out_trajectory.clear();
        for (const auto& item : arr) {
            schemas::Pose3D pose;
            pose.timestamp_ns = item[0].get<uint64_t>();
            pose.position.x = item[1].get<double>();
            pose.position.y = item[2].get<double>();
            pose.position.z = item[3].get<double>();
            pose.orientation.x = item[4].get<double>();
            pose.orientation.y = item[5].get<double>();
            pose.orientation.z = item[6].get<double>();
            pose.orientation.w = item[7].get<double>();
            out_trajectory.push_back(pose);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool AiryIpcClient::shutdownDaemon() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sendCommand("SHUTDOWN")) return false;
    std::string resp;
    readLine(resp, 2000);
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
    return true;
}

} // namespace av::core::drivers::airy
