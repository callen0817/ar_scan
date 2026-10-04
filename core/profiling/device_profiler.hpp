#pragma once

#include "core/schemas/device_profile.hpp"
#include "core/interfaces/isensor_adapter.hpp"
#include "capability_matcher.hpp"
#include <string>
#include <memory>
#include <filesystem>

namespace av::core::profiling {

enum class FieldProvenance {
    DEVICE_INTERROGATED,
    OFFICIAL_SDK,
    OFFICIAL_DOCUMENTATION,
    OFFICIAL_DRIVER,
    AV_INFERRED,
    UNKNOWN
};

inline std::string to_string(FieldProvenance prov) {
    switch (prov) {
        case FieldProvenance::DEVICE_INTERROGATED: return "DEVICE_INTERROGATED";
        case FieldProvenance::OFFICIAL_SDK: return "OFFICIAL_SDK";
        case FieldProvenance::OFFICIAL_DOCUMENTATION: return "OFFICIAL_DOCUMENTATION";
        case FieldProvenance::OFFICIAL_DRIVER: return "OFFICIAL_DRIVER";
        case FieldProvenance::AV_INFERRED: return "AV_INFERRED";
        default: return "UNKNOWN";
    }
}

class DeviceProfiler {
public:
    DeviceProfiler() = default;

    /**
     * Interrogates an active physical sensor adapter and associated hardware endpoints
     * to autonomously construct a schema-compliant DeviceProfile.
     * 
     * Strictly avoids inspecting any reference profiles in excluded_reference/.
     */
    schemas::DeviceProfile generateProfileFromSensor(
        std::shared_ptr<interfaces::ISensorAdapter> sensor,
        const std::string& profile_id = ""
    );

    /**
     * Structure representing a physical V4L2 / UVC camera discovered on the host system.
     */
    /**
     * Structure representing an individual hardware interface / stream node of a physical device.
     */
    struct DiscoveredInterface {
        std::string node_path;       // e.g. "/dev/video2"
        std::string interface_type;  // e.g. "VIDEO_CAPTURE", "METADATA_CAPTURE", "USB_ENDPOINT"
        std::string format_summary;  // e.g. "MJPG 1920x1080@30fps, YUYV 1920x1080@10fps"
        bool is_primary{false};
    };

    /**
     * Structure representing a physical hardware sensor device attached to the host.
     * Groups multiple endpoints/interfaces belonging to the same physical hardware.
     */
    struct DiscoveredPhysicalDevice {
        std::string device_id;        // stable unique ID (e.g. "camera_32e4_4802_2023081002")
        std::string display_name;     // real hardware card/product name (e.g. "48MP USB Camera")
        std::string manufacturer;     // from USB sysfs (e.g. "Fic")
        std::string product_name;     // from USB sysfs
        std::string vendor_id;        // e.g. "32e4"
        std::string product_id;       // e.g. "4802"
        std::string serial_number;    // e.g. "2023081002"
        std::string bus_type;         // "USB", "ETHERNET", "PLATFORM"
        std::string bus_info;         // e.g. "usb-3610000.usb-2.3" or "1-2.3"
        std::string sysfs_path;       // e.g. "/sys/devices/platform/bus@0/3610000.usb/usb1/1-2/1-2.3"
        std::string primary_endpoint; // primary video capture node, e.g. "/dev/video2"
        std::string driver_name;      // e.g. "uvcvideo"
        
        std::vector<DiscoveredInterface> interfaces;
        
        struct FormatInfo {
            std::string pixel_format;
            int width{0};
            int height{0};
            double max_fps{0.0};
        };
        std::vector<FormatInfo> formats;
        
        std::vector<std::string> native_capabilities;
        std::vector<schemas::StreamInfo> supported_streams;
        
        // Status: PROFILED, UNPROFILED, UNSUPPORTED, DISCONNECTED
        std::string profile_status{"UNPROFILED"};
        std::string existing_profile_id;
        std::string compatible_reconstruction{"Monocular Neural Reconstruction"};
        std::string recommended_backend{"LingBot-Map"};
        std::string status_rationale;
    };

    /**
     * Structure representing a physical V4L2 / UVC camera discovered on the host system.
     */
    struct DiscoveredV4L2Device {
        std::string device_path;         // e.g. "/dev/video2"
        std::string vendor_id;           // from USB sysfs idVendor (e.g. "32e4")
        std::string product_id;          // from USB sysfs idProduct (e.g. "4802")
        std::string manufacturer;        // from USB sysfs manufacturer (e.g. "Fic")
        std::string product_name;        // from USB sysfs product (e.g. "48MP USB Camera")
        std::string serial_number;       // from USB sysfs serial (e.g. "2023081002")
        std::string bus_info;            // from VIDIOC_QUERYCAP
        std::string driver_name;         // from VIDIOC_QUERYCAP (e.g. "uvcvideo")
        uint32_t card_capabilities{0};
        
        struct FormatInfo {
            std::string pixel_format;
            int width{0};
            int height{0};
            double max_fps{0.0};
        };
        std::vector<FormatInfo> formats;
        std::vector<std::string> native_capabilities;
        std::vector<schemas::StreamInfo> supported_streams;
    };

    /**
     * Autonomously discovers and groups all physical devices across Linux V4L2, USB, and network.
     */
    std::vector<DiscoveredPhysicalDevice> discoverPhysicalDevices(const std::filesystem::path& repo_root);

    /**
     * Autonomously discovers all active V4L2 video capture devices attached to the host.
     */
    std::vector<DiscoveredV4L2Device> discoverV4L2Devices();

    /**
     * Generates an EXPERIMENTAL DeviceProfile for a discovered physical device.
     */
    schemas::DeviceProfile generateProfileFromPhysicalDevice(
        const DiscoveredPhysicalDevice& dev,
        const std::string& profile_id = ""
    );

    /**
     * Generates an EXPERIMENTAL DeviceProfile for an unknown physical V4L2 device.
     */
    schemas::DeviceProfile generateProfileFromV4L2(
        const DiscoveredV4L2Device& dev,
        const std::string& profile_id = ""
    );

    /**
     * Checks if a profile for the specified physical hardware identity already exists.
     * Enables automatic profile reuse on reconnect.
     */
    bool findExistingProfile(
        const DiscoveredV4L2Device& dev,
        const std::filesystem::path& repo_root,
        std::string& out_profile_id
    );

    /**
     * Specialized interrogation for RoboSense Ethernet LiDAR devices.
     */
    schemas::DeviceProfile interrogateRoboSenseLidar(
        const std::string& sensor_ip = "192.168.1.200",
        int msop_port = 6699,
        int difop_port = 7788,
        int imu_port = 6688
    );

    /**
     * Persists generated profile to profiles/experimental/<profile_id>.json.
     */
    bool saveExperimentalProfile(
        const schemas::DeviceProfile& profile,
        const std::filesystem::path& repository_root,
        const std::string& filename = ""
    );
};

} // namespace av::core::profiling

