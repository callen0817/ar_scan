#include "generic_uvc_sensor_adapter.hpp"
#include "core/logging/logger.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace av::core::drivers::uvc {

GenericUvcSensorAdapter::GenericUvcSensorAdapter(const std::string& device_path)
    : device_path_(device_path) {}

GenericUvcSensorAdapter::~GenericUvcSensorAdapter() {
    disconnect();
}

std::string GenericUvcSensorAdapter::identify() const {
    return model_name_ + " (" + device_path_ + ")";
}

bool GenericUvcSensorAdapter::connect(const schemas::SensorInstance& instance, const schemas::DeviceProfile& profile) {
    current_instance_ = instance;
    current_profile_ = profile;

    if (!profile.model().empty()) {
        model_name_ = profile.manufacturer() + " " + profile.model();
    }

    if (profile.intrinsics().contains("device_path")) {
        device_path_ = profile.intrinsics()["device_path"].get<std::string>();
    } else if (profile.transport().rfind("/dev/video", 0) == 0) {
        device_path_ = profile.transport();
    }

    if (device_path_.empty() || device_path_.rfind("/dev/video", 0) != 0) {
        AV_LOG_ERROR("GenericUvcSensorAdapter", "Connect Failed", "Invalid or non-video device path '" + device_path_ + "'");
        return false;
    }

    int fd = ::open(device_path_.c_str(), O_RDWR);
    if (fd < 0) {
        AV_LOG_ERROR("GenericUvcSensorAdapter", "Connect Failed", "Unable to open device node " + device_path_);
        return false;
    }

    struct v4l2_capability cap{};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        AV_LOG_ERROR("GenericUvcSensorAdapter", "Connect Failed", "Failed VIDIOC_QUERYCAP on " + device_path_);
        ::close(fd);
        return false;
    }
    ::close(fd);

    connected_ = true;
    AV_LOG_INFO("GenericUvcSensorAdapter", "Connected to " + model_name_ + " on " + device_path_);
    return true;
}

bool GenericUvcSensorAdapter::disconnect() {
    if (streaming_) {
        stop();
    }
    connected_ = false;
    AV_LOG_INFO("GenericUvcSensorAdapter", "Disconnected from " + device_path_);
    return true;
}

bool GenericUvcSensorAdapter::isConnected() const {
    return connected_;
}

std::vector<std::string> GenericUvcSensorAdapter::getCapabilities() const {
    return {"RGB_CAMERA", "MONOCULAR_OPTICS", "UVC_STREAMING"};
}

std::vector<schemas::StreamInfo> GenericUvcSensorAdapter::getStreams() const {
    schemas::StreamInfo info{"rgb_image", "RGB", "RGB8_1080P", fps_, true};
    return {info};
}

std::vector<schemas::CalibrationProvenance> GenericUvcSensorAdapter::getCalibration() const {
    schemas::CalibrationProvenance calib;
    calib.set_source(schemas::CalibrationSource::DEVICE_FACTORY);
    calib.set_value("NOMINAL_PINHOLE_INTRINSICS");
    calib.set_confidence(0.85);
    calib.set_evidence("V4L2 format enumeration on " + device_path_);
    calib.set_validated(false);
    return {calib};
}

std::string GenericUvcSensorAdapter::getTiming() const {
    return "HOST_MONOTONIC";
}

schemas::SensorStatus GenericUvcSensorAdapter::getStatus() const {
    if (streaming_) return schemas::SensorStatus::STREAMING;
    if (connected_) return schemas::SensorStatus::CONNECTED;
    return schemas::SensorStatus::DISCONNECTED;
}

bool GenericUvcSensorAdapter::start() {
    if (streaming_) return true;
    streaming_ = true;
    capture_thread_ = std::thread(&GenericUvcSensorAdapter::captureLoop, this);
    AV_LOG_INFO("GenericUvcSensorAdapter", "Started capture thread on " + device_path_);
    return true;
}

bool GenericUvcSensorAdapter::stop() {
    if (!streaming_) return true;
    streaming_ = false;
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }
    AV_LOG_INFO("GenericUvcSensorAdapter", "Stopped capture thread on " + device_path_);
    return true;
}

bool GenericUvcSensorAdapter::isStreaming() const {
    return streaming_;
}

void GenericUvcSensorAdapter::registerPointCloudCallback(interfaces::PointCloudCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    point_cloud_callback_ = std::move(cb);
}

void GenericUvcSensorAdapter::registerImuCallback(interfaces::ImuCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    imu_callback_ = std::move(cb);
}

void GenericUvcSensorAdapter::registerImageCallback(interfaces::ImageCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    image_callback_ = std::move(cb);
}

void GenericUvcSensorAdapter::setDevicePath(const std::string& path) {
    device_path_ = path;
}

std::string GenericUvcSensorAdapter::devicePath() const {
    return device_path_;
}

void GenericUvcSensorAdapter::captureLoop() {
    int fd = ::open(device_path_.c_str(), O_RDWR);
    if (fd < 0) {
        AV_LOG_ERROR("GenericUvcSensorAdapter", "Capture Error", "Failed to open " + device_path_);
        streaming_ = false;
        return;
    }

    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width_;
    fmt.fmt.pix.height = height_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (::ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        // Fallback to YUYV if MJPEG not supported
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        ::ioctl(fd, VIDIOC_S_FMT, &fmt);
    }

    struct v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (::ioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count == 0) {
        ::close(fd);
        streaming_ = false;
        return;
    }

    struct BufferInfo {
        void* start{nullptr};
        size_t length{0};
    };
    std::vector<BufferInfo> buffers(req.count);

    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (::ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) continue;
        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
        ::ioctl(fd, VIDIOC_QBUF, &buf);
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (::ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        for (auto& b : buffers) {
            if (b.start && b.start != MAP_FAILED) munmap(b.start, b.length);
        }
        ::close(fd);
        streaming_ = false;
        return;
    }

    uint64_t seq = 0;
    while (streaming_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        int r = ::select(fd + 1, &fds, NULL, NULL, &tv);
        if (r <= 0) continue;

        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (::ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;

        if (buf.index < buffers.size() && buffers[buf.index].start) {
            schemas::ImageFrame frame;
            frame.width = width_;
            frame.height = height_;
            frame.channels = 3;
            frame.encoding = (fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) ? "mjpeg" : "yuyv";
            frame.sequence_id = seq++;
            frame.frame_id = "camera_optical_frame";
            frame.sensor_instance_id = current_instance_.instance_id();
            frame.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            const uint8_t* ptr = static_cast<const uint8_t*>(buffers[buf.index].start);
            frame.data.assign(ptr, ptr + buf.bytesused);

            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (image_callback_) {
                image_callback_(frame);
            }
        }

        ::ioctl(fd, VIDIOC_QBUF, &buf);
    }

    ::ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (auto& b : buffers) {
        if (b.start && b.start != MAP_FAILED) {
            munmap(b.start, b.length);
        }
    }
    ::close(fd);
}

} // namespace av::core::drivers::uvc
