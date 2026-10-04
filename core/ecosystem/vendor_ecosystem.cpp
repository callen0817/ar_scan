#include "vendor_ecosystem.hpp"
#include <algorithm>
#include <iostream>
#include <set>

namespace av::core::ecosystem {

VendorEcosystemRegistry& VendorEcosystemRegistry::instance() {
    static VendorEcosystemRegistry registry;
    return registry;
}

VendorEcosystemRegistry::VendorEcosystemRegistry() {
    populateAuthoritativeEcosystem();
}

void VendorEcosystemRegistry::registerModel(const schemas::DeviceModelDefinition& def) {
    models_.push_back(def);
    id_index_[def.model_id()] = models_.size() - 1;
}

const schemas::DeviceModelDefinition* VendorEcosystemRegistry::findModelById(const std::string& model_id) const {
    auto it = id_index_.find(model_id);
    if (it != id_index_.end() && it->second < models_.size()) {
        return &models_[it->second];
    }
    return nullptr;
}

const schemas::DeviceModelDefinition* VendorEcosystemRegistry::findModelByHardware(
    const std::string& vendor_id,
    const std::string& product_id,
    const std::string& vendor_name,
    const std::string& product_name,
    const std::string& transport_address
) const {
    if (!vendor_id.empty() && !product_id.empty()) {
        for (const auto& m : models_) {
            const auto& vids = m.vendor_ids();
            const auto& pids = m.product_ids();
            bool vid_match = std::find(vids.begin(), vids.end(), vendor_id) != vids.end();
            bool pid_match = std::find(pids.begin(), pids.end(), product_id) != pids.end();
            if (vid_match && pid_match) {
                return &m;
            }
        }
    }

    if (!transport_address.empty()) {
        for (const auto& m : models_) {
            if (!m.default_ip().empty() && transport_address.find(m.default_ip()) != std::string::npos) {
                return &m;
            }
        }
    }

    std::string v_lower = vendor_name;
    std::string p_lower = product_name;
    std::transform(v_lower.begin(), v_lower.end(), v_lower.begin(), ::tolower);
    std::transform(p_lower.begin(), p_lower.end(), p_lower.begin(), ::tolower);

    if (!p_lower.empty()) {
        for (const auto& m : models_) {
            std::string m_model = m.model();
            std::transform(m_model.begin(), m_model.end(), m_model.begin(), ::tolower);
            if (p_lower.find(m_model) != std::string::npos || m_model.find(p_lower) != std::string::npos) {
                return &m;
            }
        }
    }

    return nullptr;
}

std::vector<schemas::DeviceModelDefinition> VendorEcosystemRegistry::getModelsByVendor(const std::string& vendor) const {
    std::vector<schemas::DeviceModelDefinition> res;
    if (vendor.empty()) return res;
    std::string v_target = vendor;
    std::transform(v_target.begin(), v_target.end(), v_target.begin(), ::tolower);
    for (const auto& m : models_) {
        std::string mv = m.vendor();
        std::transform(mv.begin(), mv.end(), mv.begin(), ::tolower);
        if (mv.find(v_target) != std::string::npos) {
            res.push_back(m);
        }
    }
    return res;
}

std::vector<schemas::DeviceModelDefinition> VendorEcosystemRegistry::getModelsBySensorType(const std::string& sensor_type) const {
    std::vector<schemas::DeviceModelDefinition> res;
    if (sensor_type.empty()) return res;
    std::string st_target = sensor_type;
    std::transform(st_target.begin(), st_target.end(), st_target.begin(), ::tolower);
    for (const auto& m : models_) {
        std::string mst = m.sensor_type();
        std::transform(mst.begin(), mst.end(), mst.begin(), ::tolower);
        if (mst.find(st_target) != std::string::npos) {
            res.push_back(m);
        }
    }
    return res;
}

std::vector<schemas::DeviceModelDefinition> VendorEcosystemRegistry::getModelsByStatus(schemas::ProductionStatus status) const {
    std::vector<schemas::DeviceModelDefinition> res;
    for (const auto& m : models_) {
        if (m.production_status() == status) {
            res.push_back(m);
        }
    }
    return res;
}

std::vector<std::string> VendorEcosystemRegistry::getManufacturers() const {
    std::vector<std::string> res;
    std::set<std::string> seen;
    for (const auto& m : models_) {
        if (m.vendor() == "Generic") continue; // Exclude Generic from manufacturer list
        if (seen.insert(m.vendor()).second) {
            res.push_back(m.vendor());
        }
    }
    return res;
}

std::vector<std::string> VendorEcosystemRegistry::getSensorTypes() const {
    std::vector<std::string> res;
    std::set<std::string> seen;
    for (const auto& m : models_) {
        if (seen.insert(m.sensor_type()).second) {
            res.push_back(m.sensor_type());
        }
    }
    return res;
}

std::vector<std::string> VendorEcosystemRegistry::getSensorTypesForManufacturer(const std::string& vendor) const {
    std::vector<std::string> res;
    std::set<std::string> seen;
    std::string v_target = vendor;
    std::transform(v_target.begin(), v_target.end(), v_target.begin(), ::tolower);
    for (const auto& m : models_) {
        std::string mv = m.vendor();
        std::transform(mv.begin(), mv.end(), mv.begin(), ::tolower);
        if (v_target.empty() || mv == v_target || mv.find(v_target) != std::string::npos) {
            if (seen.insert(m.sensor_type()).second) {
                res.push_back(m.sensor_type());
            }
        }
    }
    return res;
}

std::vector<schemas::DeviceModelDefinition> VendorEcosystemRegistry::getModels(const std::string& vendor, const std::string& sensor_type) const {
    std::vector<schemas::DeviceModelDefinition> res;
    std::string v_target = vendor;
    std::transform(v_target.begin(), v_target.end(), v_target.begin(), ::tolower);
    std::string st_target = sensor_type;
    std::transform(st_target.begin(), st_target.end(), st_target.begin(), ::tolower);

    for (const auto& m : models_) {
        std::string mv = m.vendor();
        std::transform(mv.begin(), mv.end(), mv.begin(), ::tolower);
        bool match_vendor = v_target.empty() || (mv.find(v_target) != std::string::npos);

        std::string mst = m.sensor_type();
        std::transform(mst.begin(), mst.end(), mst.begin(), ::tolower);
        bool match_type = st_target.empty() || (mst.find(st_target) != std::string::npos);

        if (match_vendor && match_type) {
            res.push_back(m);
        }
    }
    return res;
}

std::vector<schemas::DeviceModelDefinition> VendorEcosystemRegistry::searchModels(const std::string& query) const {
    std::vector<schemas::DeviceModelDefinition> res;
    if (query.empty()) return models_;
    std::string q = query;
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    for (const auto& m : models_) {
        std::string haystack = m.model_id() + " " + m.vendor() + " " + m.family() + " " + m.model() + " " + 
                               m.sensor_type() + " " + m.hardware_revision();
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
        if (haystack.find(q) != std::string::npos) {
            res.push_back(m);
        }
    }
    return res;
}

nlohmann::json VendorEcosystemRegistry::generateCapabilityMatrixJson() const {
    nlohmann::json root = nlohmann::json::object();
    root["schema_version"] = "1.0.0";
    root["title"] = "AV Scan — Universal Sensor Ecosystem & Capability Matrix";
    root["total_models"] = models_.size();

    nlohmann::json vendors_json = nlohmann::json::object();
    for (const auto& m : models_) {
        std::string v = m.vendor();
        if (!vendors_json.contains(v)) {
            vendors_json[v] = nlohmann::json::object();
            vendors_json[v]["vendor"] = v;
            vendors_json[v]["models"] = nlohmann::json::array();
        }
        vendors_json[v]["models"].push_back(m.to_json());
    }

    root["ecosystem"] = vendors_json;
    return root;
}

void VendorEcosystemRegistry::populateAuthoritativeEcosystem() {
    {
        schemas::DeviceModelDefinition m("hesai_ot128", "Hesai", "OT-Series", "OT128 360° Omnidirectional", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/ot128/", "Hesai OT128 360 Degree LiDAR Manual v1.0", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar128", "Hesai", "Pandar", "Pandar128 High-Density Mechanical", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandar128/", "Hesai Pandar128 High-Density LiDAR Manual v1.4", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_mid_100_legacy", "Livox", "Mid", "Livox Mid-100 Triple Cluster", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("BROADCAST_SEARCH");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({65000, 65001});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.3", "Livox-SDK", "livox_ros_driver", "MIT", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.2.x"}, "1.2.3", "1.2.3", "livox_eth_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxLegacyAdapter", "FAST-LIO2", "LIO", "1.2.3", "Livox-SDK", "1.7.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/mid-100", "Livox Mid-100 User Manual v1.7", "Livox-SDK v1.2.3", "Livox Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK v1.2.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_cx", "LSLiDAR", "CX-Series", "LSLiDAR CX Automotive Grade", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x"}, "2.2.0", "1.5.0", "lslidar_cx_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/cx/", "LSLiDAR CX Automotive LiDAR Datasheet v1.0", "lslidar_driver 2.2.0", "LSLiDAR CX Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("seyond_robin_e", "Seyond", "Robin", "Robin E High-Sensitivity Solid-State", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.205");
        m.set_default_ports({8010, 8011});
        schemas::VendorSdkInfo sdk{"Seyond", "inno-lidar-sdk", "2.6.0", "inno_lidar_driver", "inno_ros", "Apache-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.6.x"}, "2.6.0", "2.0.0", "inno_robin_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "INNO_FLASH_CALIB";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SeyondAdapter", "FAST-LIO2", "LIO", "2.6.0", "inno_lidar_driver", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.seyond.com/products/robin", "Seyond Robin E Solid-State LiDAR Manual v2.6", "inno-lidar-sdk 2.6.0", "Seyond Robin Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "inno-lidar-sdk 2.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_astra_mini_legacy", "Orbbec", "Astra", "Astra Mini Embedded 3D", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0404"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OpenNI2", "2.3.0", "OpenNI2-Astra", "astra_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.2.0", {"2.3.x"}, "2.3.0", "2.3.0", "openni2_proto", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "OPENNI_CALIB";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OrbbecAstraLegacyAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.3.0", "OpenNI2-Astra", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/structured-light-camera/astra-mini/", "Orbbec Astra Mini OpenNI2 Manual v2.3", "OpenNI2 2.3.0", "OpenNI2 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OpenNI2 2.3.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_sr300_legacy", "Intel", "RealSense", "RealSense SR300 Short-Range Coded Light", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0aa5"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.50.0", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.26.1", {"2.50.x"}, "2.50.0", "2.50.0", "realsense_sr300_proto", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseLegacyAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.50.0", "librealsense2", "3.26.1", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com", "Intel RealSense SR300 Short-Range Camera Datasheet", "librealsense2 2.50.0", "SR300 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.50.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("luxonis_oak_d_w", "Luxonis", "OAK", "OAK-D W 120° Wide FOV Stereo", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("DEPTHAI_BOOT_PROBE");
        m.set_vendor_ids({"0x03e7"});
        m.set_product_ids({"0x2485"});
        schemas::VendorSdkInfo sdk{"Luxonis", "DepthAI", "2.28.0", "depthai-core", "depthai-ros 2.9", "MIT", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.28.x"}, "2.28.0", "2.15.0", "depthai_xlink_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "EEPROM_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"DepthAIAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.28.0", "depthai-core", "2.28.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://docs.luxonis.com/hardware/products/OAK-D-W/", "Luxonis OAK-D W Wide-Angle Product Manual v2.28", "depthai-core 2.28.0", "DepthAI XLink Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "depthai-core 2.28.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("generic_v4l2_ir", "Generic", "UVC", "Generic V4L2 Infrared Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Other");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("V4L2");
        schemas::VendorSdkInfo sdk{"Standard", "V4L2", "1.0.0", "uvcvideo", "v4l2_camera", "GPL-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.0.0", {"1.0.0"}, "1.0.0", "1.0.0", "v4l2_ioctl", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"ir_intensity"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GenericUvcSensorAdapter", "LingBotStreamingBackend", "UNKNOWN", "1.0.0", "uvcvideo", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html", "Linux V4L2 IR Capture Specification", "V4L2 1.0.0", "v4l2_ioctl", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "V4L2 1.0.0", true, true, true, false, false, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("generic_rtsp_camera", "Generic", "IP", "Generic RTSP / ONVIF IP Stream", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB Camera");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("ONVIF_PROBE");
        m.set_default_ip("192.168.1.50");
        m.set_default_ports({554, 80});
        schemas::VendorSdkInfo sdk{"Standard", "RTSP", "1.0.0", "ffmpeg", "image_transport", "LGPL-2.1", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.0.0", {"1.0.0"}, "1.0.0", "1.0.0", "rtsp_h264_rtp", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb"};
        feat.scan_rates_hz = {25.0, 30.0};
        feat.factory_calibration_api = "";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GenericUvcSensorAdapter", "LingBotStreamingBackend", "MONOCULAR_NEURAL_RECONSTRUCTION", "1.0.0", "ffmpeg", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.ietf.org/rfc/rfc2326.txt", "RFC 2326 Real Time Streaming Protocol (RTSP)", "RTSP 1.0", "RTSP/RTP/RTCP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "RTSP 1.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("generic_imu_serial", "Generic", "IMU", "Generic 9-DOF Serial IMU", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("IMU");
        m.set_transport_type("USB_SERIAL");
        m.set_discovery_mechanism("SERIAL_PROBE");
        schemas::VendorSdkInfo sdk{"Standard", "SerialIMU", "1.0.0", "serial", "imu_tools", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.0.0", {"1.0.0"}, "1.0.0", "1.0.0", "nmea_imu_stream", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {};
        feat.point_cloud_fields = {"accel", "gyro", "mag"};
        feat.scan_rates_hz = {100.0, 200.0};
        feat.factory_calibration_api = "";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GenericImuAdapter", "VitureVioBackend", "VIO", "1.0.0", "serial", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.kernel.org/doc/html/latest/driver-api/serial/index.html", "Linux Serial Driver Architecture & IMU Streaming", "Serial 1.0", "ASCII/Binary Serial Stream", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Serial 1.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("viture_luma_ultra", "VITURE", "Luma", "VITURE Luma Ultra AR Glasses", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Multi-Sensor");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x35ca"});
        m.set_product_ids({"0x1011"});
        schemas::VendorSdkInfo sdk{"VITURE", "VitureSDK", "1.3.0", "viture_driver", "viture_ros", "Proprietary", {"Linux", "Android", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.3.x"}, "1.3.0", "1.0.0", "viture_hid_uvc", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"rgb", "imu_orientation"};
        feat.scan_rates_hz = {60.0, 120.0};
        feat.factory_calibration_api = "VITURE_OPTICAL_CALIB";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"VitureGlassesAdapter", "VitureVioBackend", "VIO", "1.3.0", "viture_driver", "1.1.0", schemas::ValidationTier::AV_VALIDATED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::AV_VALIDATED);
        schemas::ProvenanceRecord prov{"https://www.viture.com/product/luma-ultra", "VITURE Luma Ultra Spatial AR Glasses Manual v1.3", "VitureSDK 1.3.0", "VITURE HID/UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "VitureSDK 1.3.0", true, true, true, true, true, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os1_rev8_rgb", "Ouster", "OS1", "OS1-128 RGB Flagship", "Rev8", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL", "NEAR_IR"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "rgb", "reflectivity", "ambient"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://data.ouster.io/downloads/software-user-manual/software-user-manual-v4.0.0.pdf", "Ouster Sensor User Manual Rev 4.0.0 (Rev8 Native Color)", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v4", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os1_max", "Ouster", "OS1", "OS1 Max Extended Range 200m+", "Rev8", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "rgb", "reflectivity"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/os1-lidar-sensor", "Ouster OS1 Max Extended Range Manual v4.0", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v4", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os1_rev7", "Ouster", "OS1", "OS1-64/128 Standard", "Rev7", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.x.x", {"0.16.x", "0.15.x"}, "0.16.2", "0.10.x", "ouster_tcp_v3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "reflectivity", "ambient"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "3.0.1", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://data.ouster.io/downloads/software-user-manual/software-user-manual-v3.0.1.pdf", "Ouster Sensor User Manual Rev 3.0.1 (Rev7 Dual Return)", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v3", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os1_gen1_legacy", "Ouster", "OS1", "OS1-16/64 Classic Gen1", "RevC/D/5/6", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("HTTP_REST_PROBE");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.10.0", "ouster_legacy", "ouster-ros 0.10", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.5.x", {"0.10.x", "0.9.x"}, "0.10.0", "0.10.0", "ouster_legacy_v2", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "noise"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "HTTP_GET_METADATA";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterLegacyAdapter", "FAST-LIO2", "LIO", "0.10.0", "ouster_legacy", "2.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://data.ouster.io/downloads/software-user-manual/software-user-manual-v2.2.0.pdf", "Ouster Sensor User Manual FW 2.2.0 Legacy Protocol", "ouster-sdk 0.10.0", "Ouster Legacy HTTP/UDP Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.10.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os0_rev8_rgb", "Ouster", "OS0", "OS0-128 RGB Ultra-Wide", "Rev8", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL", "NEAR_IR"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "rgb", "reflectivity"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/os0-lidar-sensor", "Ouster OS0 Rev8 90° FOV Ultra-Wide Manual", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v4", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os0_rev7", "Ouster", "OS0", "OS0-64/128 Ultra-Wide", "Rev7", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "reflectivity"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "3.0.1", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/os0-lidar-sensor", "Ouster OS0 Rev7 Manual v3.0", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v3", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os0_gen1_legacy", "Ouster", "OS0", "OS0-32/64 Classic Gen1", "RevC/D", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("HTTP_REST_PROBE");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.10.0", "ouster_legacy", "ouster-ros 0.10", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.5.x", {"0.10.x"}, "0.10.0", "0.10.0", "ouster_legacy_v2", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "HTTP_GET_METADATA";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterLegacyAdapter", "FAST-LIO2", "LIO", "0.10.0", "ouster_legacy", "2.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://data.ouster.io/downloads/software-user-manual/software-user-manual-v2.2.0.pdf", "Ouster OS0 Gen1 Manual FW 2.2", "ouster-sdk 0.10.0", "Ouster Legacy Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.10.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os2_rev8_rgb", "Ouster", "OS2", "OS2-128 RGB Long-Range 240m", "Rev8", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "rgb"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/os2-lidar-sensor", "Ouster OS2 Rev8 240m Long-Range Manual", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v4", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_os2_rev7", "Ouster", "OS2", "OS2-64/128 Long-Range", "Rev7", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "reflectivity"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "3.0.1", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/os2-lidar-sensor", "Ouster OS2 Rev7 Long-Range Manual v3.0", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v3", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_osdome_rev8_rgb", "Ouster", "OSDome", "OSDome-128 RGB Hemispherical", "Rev8", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range", "rgb"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/osdome-lidar-sensor", "Ouster OSDome 180° Hemispherical User Manual", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v4", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("ouster_osdome_rev7", "Ouster", "OSDome", "OSDome-64/128 Hemispherical", "Rev7", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.100");
        m.set_default_ports({7502, 7503});
        schemas::VendorSdkInfo sdk{"Ouster", "OS", "0.16.2", "ouster_sensor", "ouster-ros 0.16", "BSD-3-Clause", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.x.x", {"0.16.x"}, "0.16.2", "0.10.x", "ouster_tcp_v3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"SINGLE", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "range"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "FLASH_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OusterAdapter", "FAST-LIO2", "LIO", "0.16.2", "ouster_sensor", "3.0.1", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://ouster.com/products/hardware/osdome-lidar-sensor", "Ouster OSDome Rev7 User Manual v3.0", "ouster-sdk 0.16.2", "Ouster TCP/UDP Protocol v3", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ouster-sdk 0.16.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_airy", "RoboSense", "Airy", "RS-Airy Solid-State Blind-Spot", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_AIRY", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"AirySensorAdapter", "AiryLioBackend", "FAST-LIO2", "1.5.8", "rs_driver", "1.2.0", schemas::ValidationTier::AV_VALIDATED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::AV_VALIDATED);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-Airy", "RoboSense RS-Airy Blind-Spot LiDAR User Manual v1.2", "rs_driver 1.5.8", "RS-LiDAR MSOP/DIFOP Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", true, true, true, true, true, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_airy_lite", "RoboSense", "Airy", "RS-Airy Lite Compact", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_AIRY_LITE", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"AirySensorAdapter", "AiryLioBackend", "FAST-LIO2", "1.5.8", "rs_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-Airy-Lite", "RoboSense RS-Airy Lite Technical Spec v1.0", "rs_driver 1.5.8", "RS-LiDAR MSOP/DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_m1", "RoboSense", "M-Series", "RS-LiDAR-M1 Automotive Solid-State", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "3.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_M1", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "2.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-M1", "RoboSense RS-LiDAR-M1 User Guide v2.1", "rs_driver 1.5.8", "RS-LiDAR M1 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_m2", "RoboSense", "M-Series", "RS-LiDAR-M2 High-Resolution MEMS", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_M2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-M2", "RoboSense RS-LiDAR-M2 Specification v1.0", "rs_driver 1.5.8", "RS-LiDAR M2 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_m3", "RoboSense", "M-Series", "RS-LiDAR-M3 Ultra-Long-Range 300m", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_M3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-M3", "RoboSense RS-LiDAR-M3 300m Long-Range Datasheet", "rs_driver 1.5.8", "RS-LiDAR M3 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_e1", "RoboSense", "E-Series", "RS-LiDAR-E1 Flash Solid-State", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_E1", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-E1", "RoboSense RS-LiDAR-E1 Flash LiDAR Spec v1.0", "rs_driver 1.5.8", "RS-LiDAR E1 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_helios", "RoboSense", "Helios", "RS-Helios 16/32-Beam Mechanical", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_HELIOS", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-Helios", "RoboSense RS-Helios User Manual v1.1", "rs_driver 1.5.8", "RS-LiDAR MSOP/DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_ruby", "RoboSense", "Ruby", "RS-Ruby 128-Beam Mechanical", "RevA", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_RUBY", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.3.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-Ruby", "RoboSense RS-Ruby 128-Beam Manual v1.3", "rs_driver 1.5.8", "RS-LiDAR MSOP/DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_bpearl", "RoboSense", "Bpearl", "RS-Bpearl 100°x90° Hemispherical", "RevA", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_BPEARL", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.0.2", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-Bpearl", "RoboSense RS-Bpearl Super Wide FOV Manual v1.0", "rs_driver 1.5.8", "RS-LiDAR MSOP/DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_rs16_legacy", "RoboSense", "RS-Puck", "RS-LiDAR-16 Classic Puck", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.5.x", "1.3.x"}, "1.5.8", "1.3.0", "RS_DIFOP_16", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-16", "RoboSense RS-LiDAR-16 User Guide v4.0", "rs_driver 1.5.8", "RS-LiDAR 16 DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("robosense_rs32_legacy", "RoboSense", "RS-Puck", "RS-LiDAR-32 Classic Mechanical", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("MSOP_DIFOP");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({6699, 7788});
        schemas::VendorSdkInfo sdk{"RoboSense", "RS-LiDAR", "1.5.8", "rs_driver", "rslidar_sdk 1.5.8", "BSD-3-Clause", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.5.x"}, "1.5.8", "1.3.0", "RS_DIFOP_32", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_PACKET_DECODE";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RoboSenseAdapter", "FAST-LIO2", "LIO", "1.5.8", "rs_driver", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.robosense.ai/en/rslidar/RS-LiDAR-32", "RoboSense RS-LiDAR-32 User Guide v3.0", "rs_driver 1.5.8", "RS-LiDAR 32 DIFOP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "rs_driver 1.5.8", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_at128", "Hesai", "AT-Series", "AT128 Automotive Long-Range", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/at128/", "Hesai AT128 Automotive LiDAR User Manual v1.4", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_et25", "Hesai", "ET-Series", "ET25 In-Cabin Ultra-Thin Solid-State", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/et25/", "Hesai ET25 Cabin LiDAR User Guide v1.0", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_ft120", "Hesai", "FT-Series", "FT120 Pure Solid-State Blind Spot", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/ft120/", "Hesai FT120 Solid-State Blind Spot LiDAR Manual v1.0", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar_xt32", "Hesai", "Pandar", "PandarXT-32 High Precision", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandarxt-32/", "Hesai PandarXT-32 Robotics LiDAR User Manual v1.2", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar_xt16", "Hesai", "Pandar", "PandarXT-16 Robotics", "RevB", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandarxt-16/", "Hesai PandarXT-16 Robotics LiDAR Manual v1.1", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar_qt128", "Hesai", "Pandar", "PandarQT128 Short-Range Blind Spot", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "2.0.0", "HesaiLidar_SDK", "hesai_ros 2.0", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "hesai_ptc_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiAdapter", "FAST-LIO2", "LIO", "2.0.0", "HesaiLidar_SDK", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandarqt128/", "Hesai PandarQT128 Blind Spot LiDAR Manual v1.0", "HesaiLidar_SDK_2.0", "Hesai PTC/PDC Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "HesaiLidar_SDK_2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar40p_legacy", "Hesai", "Pandar", "Pandar40P Mechanical Classic", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "1.0.0", "PandarGeneralSDK", "pandar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.0.x"}, "1.0.0", "1.0.0", "hesai_ptc_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiLegacyAdapter", "FAST-LIO2", "LIO", "1.0.0", "PandarGeneralSDK", "1.3.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandar40p/", "Hesai Pandar40P Mechanical LiDAR User Manual v1.3", "PandarGeneralSDK 1.0.0", "Hesai PTC/PDC v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "PandarGeneralSDK 1.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("hesai_pandar64_legacy", "Hesai", "Pandar", "Pandar64 Classic Mechanical", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.201");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"Hesai", "HesaiLidar", "1.0.0", "PandarGeneralSDK", "pandar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.0.x"}, "1.0.0", "1.0.0", "hesai_ptc_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "PTC_CORRECTION_FILE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"HesaiLegacyAdapter", "FAST-LIO2", "LIO", "1.0.0", "PandarGeneralSDK", "1.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.hesaitech.com/product/pandar64/", "Hesai Pandar64 Mechanical LiDAR User Manual v1.2", "PandarGeneralSDK 1.0.0", "Hesai PTC/PDC v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "PandarGeneralSDK 1.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_mid_360", "Livox", "Mid", "Livox Mid-360 Omnidirectional", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({56100, 56101});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.5", "Livox-SDK2", "livox_ros_driver2", "MIT", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.2.x"}, "1.2.5", "1.2.3", "livox_eth_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "tag", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxAdapter", "FAST-LIO2", "LIO", "1.2.5", "Livox-SDK2", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/mid-360", "Livox Mid-360 User Manual v1.0 (Built-in IMU)", "Livox-SDK2 v1.2.5", "Livox Communication Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK2 v1.2.5", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_mid_360l", "Livox", "Mid", "Livox Mid-360L Low-Profile Omnidirectional", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({56100, 56101});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.5", "Livox-SDK2 (Target / Pending Qualification)", "livox_ros_driver2 (Evaluation/Test Driver)", "MIT", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.0.x"}, "Livox-SDK2", "1.2.5", "livox_eth_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "tag", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxAdapter", "FAST-LIO2", "LIO", "1.2.5", "Livox-SDK2", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/mid-360l", "Livox Mid-360L Hardware Reference & Datasheet v1.0", "Livox-SDK2 (Pending Driver Verification)", "Livox Communication Protocol v2 (Evaluation)", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK2 (Target / Pending Qualification)", false, false, false, false, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_hap", "Livox", "HAP", "Livox HAP Automotive Long-Range", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({56100, 56101});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.5", "Livox-SDK2", "livox_ros_driver2", "MIT", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.2.x"}, "1.2.5", "1.2.3", "livox_eth_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxAdapter", "FAST-LIO2", "LIO", "1.2.5", "Livox-SDK2", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/hap", "Livox HAP Automotive LiDAR User Manual v1.1", "Livox-SDK2 v1.2.5", "Livox Communication Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK2 v1.2.5", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_mid_70", "Livox", "Mid", "Livox Mid-70 Circular FOV", "RevA", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("BROADCAST_SEARCH");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({65000, 65001});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.3", "Livox-SDK", "livox_ros_driver", "MIT", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.2.x"}, "1.2.3", "1.2.3", "livox_eth_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxLegacyAdapter", "FAST-LIO2", "LIO", "1.2.3", "Livox-SDK", "1.5.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/mid-70", "Livox Mid-70 User Manual v1.5", "Livox-SDK v1.2.3", "Livox Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK v1.2.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_horizon", "Livox", "Horizon", "Livox Horizon 81.7° Horizontal", "RevA", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("BROADCAST_SEARCH");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({65000, 65001});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.3", "Livox-SDK", "livox_ros_driver", "MIT", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.2.x"}, "1.2.3", "1.2.3", "livox_eth_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxLegacyAdapter", "FAST-LIO2", "LIO", "1.2.3", "Livox-SDK", "1.6.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/horizon", "Livox Horizon User Manual v1.6", "Livox-SDK v1.2.3", "Livox Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK v1.2.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_tele_15", "Livox", "Tele", "Livox Tele-15 Ultra-Long-Range 500m", "RevA", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("BROADCAST_SEARCH");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({65000, 65001});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.3", "Livox-SDK", "livox_ros_driver", "MIT", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.2.x"}, "1.2.3", "1.2.3", "livox_eth_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxLegacyAdapter", "FAST-LIO2", "LIO", "1.2.3", "Livox-SDK", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/tele-15", "Livox Tele-15 Ultra-Long-Range Manual v1.4", "Livox-SDK v1.2.3", "Livox Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK v1.2.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("livox_mid_40_legacy", "Livox", "Mid", "Livox Mid-40 Circular Classic", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("BROADCAST_SEARCH");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({65000, 65001});
        schemas::VendorSdkInfo sdk{"Livox", "Livox-SDK", "1.2.3", "Livox-SDK", "livox_ros_driver", "MIT", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.2.x"}, "1.2.3", "1.2.3", "livox_eth_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "reflectivity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "INTERNAL_FLASH_INTRINSICS";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LivoxLegacyAdapter", "FAST-LIO2", "LIO", "1.2.3", "Livox-SDK", "1.7.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.livoxtech.com/mid-40", "Livox Mid-40 User Manual v1.7 Legacy Protocol", "Livox-SDK v1.2.3", "Livox Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Livox-SDK v1.2.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_c16", "LSLiDAR", "C-Series", "LSLiDAR C16 16-Beam Mechanical", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x", "2.0.x"}, "2.2.0", "1.5.0", "lslidar_difop_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/c16/", "LSLiDAR C16 User Manual v2.2", "lslidar_driver 2.2.0", "LSLiDAR DIFOP Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_c32", "LSLiDAR", "C-Series", "LSLiDAR C32 32-Beam Mechanical", "RevB", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x"}, "2.2.0", "1.5.0", "lslidar_difop_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp", "ring"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/c32/", "LSLiDAR C32 High-Resolution User Manual v2.2", "lslidar_driver 2.2.0", "LSLiDAR DIFOP Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_ch64w", "LSLiDAR", "CH-Series", "CH64W Hybrid Solid-State Automotive", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x"}, "2.2.0", "1.5.0", "lslidar_ch_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "1.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/ch64w/", "LSLiDAR CH64W Hybrid Solid-State Manual v1.4", "lslidar_driver 2.2.0", "LSLiDAR CH Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_ch128x1", "LSLiDAR", "CH-Series", "CH128X1 Automotive 128-Beam Hybrid", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x"}, "2.2.0", "1.5.0", "lslidar_ch_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "1.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/ch128x1/", "LSLiDAR CH128X1 128-Beam Solid-State Spec v1.2", "lslidar_driver 2.2.0", "LSLiDAR CH Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_ls128s1", "LSLiDAR", "LS-Series", "LS128S1 1550nm Ultra-Long-Range 500m", "RevA", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "2.2.0", "lslidar_driver", "lslidar_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.2.x"}, "2.2.0", "1.5.0", "lslidar_ls_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARAdapter", "FAST-LIO2", "LIO", "2.2.0", "lslidar_driver", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/ls128s1/", "LSLiDAR LS128S1 1550nm High-End LiDAR Datasheet v1.0", "lslidar_driver 2.2.0", "LSLiDAR LS Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_driver 2.2.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("lslidar_n301_legacy", "LSLiDAR", "N-Series", "N301 Navigation 2D/3D Classic", "RevA", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("DIFOP_BROADCAST");
        m.set_default_ip("192.168.1.200");
        m.set_default_ports({2368, 2369});
        schemas::VendorSdkInfo sdk{"LSLiDAR", "LSLiDAR_SDK", "1.5.0", "lslidar_n301", "lslidar_n301_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.5.x"}, "1.5.0", "1.5.0", "lslidar_difop_v1", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "DIFOP_EEPROM";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"LSLiDARLegacyAdapter", "FAST-LIO2", "LIO", "1.5.0", "lslidar_n301", "1.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.lslidar.com/product/n301/", "LSLiDAR N301 Navigation LiDAR User Manual v1.2", "lslidar_n301 1.5.0", "LSLiDAR DIFOP v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "lslidar_n301 1.5.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("unitree_l2", "Unitree", "Lidar", "Unitree L2 4D LiDAR + 6-Axis IMU", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.120");
        m.set_default_ports({8000, 8001});
        schemas::VendorSdkInfo sdk{"Unitree", "Unitree_Lidar_SDK", "2.0.1", "unitree_lidar_sdk_v2", "unitree_lidar_ros2", "GPL-3.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.x.x"}, "2.0.1", "1.0.0", "unitree_eth_proto_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "time", "ring"};
        feat.scan_rates_hz = {11.0};
        feat.factory_calibration_api = "FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"UnitreeLidarAdapter", "FAST-LIO2", "LIO", "2.0.1", "unitree_lidar_sdk_v2", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.unitree.com/lidar-l2", "Unitree L2 360°x90° 4D LiDAR Specification v2.0", "Unitree_Lidar_SDK_v2 2.0.1", "Unitree UDP LiDAR Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "unitree_lidar_sdk_v2 2.0.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("unitree_l1", "Unitree", "Lidar", "Unitree L1 Compact Serial", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("USB_SERIAL");
        m.set_discovery_mechanism("SERIAL_PROBE");
        m.set_vendor_ids({"0x10c4"});
        m.set_product_ids({"0xea60"});
        schemas::VendorSdkInfo sdk{"Unitree", "Unitree_Lidar_SDK", "1.0.4", "unitree_lidar_sdk", "unitree_lidar_ros", "GPL-3.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.0.x"}, "1.0.4", "1.0.4", "unitree_serial_proto_v1", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "time"};
        feat.scan_rates_hz = {10.0};
        feat.factory_calibration_api = "FACTORY_SERIAL_EEPROM";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"UnitreeLidarAdapter", "FAST-LIO2", "LIO", "1.0.4", "unitree_lidar_sdk", "1.0.2", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.unitree.com/lidar-l1", "Unitree L1 Serial Interface User Manual v1.0", "Unitree_Lidar_SDK 1.0.4", "Unitree Serial Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Unitree_Lidar_SDK 1.0.4", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("valeo_scala_3", "Valeo", "SCALA", "Valeo SCALA 3 Automotive Grade", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.210");
        m.set_default_ports({5000, 5001});
        schemas::VendorSdkInfo sdk{"Valeo", "SCALA_SDK", "3.1.0", "scala_driver", "scala_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "3.x.x", {"3.x.x"}, "3.1.0", "2.0.0", "valeo_scala_eth_v3", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {25.0};
        feat.factory_calibration_api = "AUTOMOTIVE_FACTORY_FLASH";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ValeoAdapter", "FAST-LIO2", "LIO", "3.1.0", "scala_driver", "3.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.valeo.com/en/valeo-scala/", "Valeo SCALA 3 Automotive Grade LiDAR Datasheet v3.1", "SCALA SDK 3.1.0", "Automotive Ethernet SOME/IP", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "SCALA SDK 3.1.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("valeo_scala_2", "Valeo", "SCALA", "Valeo SCALA 2 Series Production", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.210");
        m.set_default_ports({5000, 5001});
        schemas::VendorSdkInfo sdk{"Valeo", "SCALA_SDK", "2.0.0", "scala_driver", "scala_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.x.x"}, "2.0.0", "1.0.0", "valeo_scala_eth_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {25.0};
        feat.factory_calibration_api = "AUTOMOTIVE_FACTORY_FLASH";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ValeoAdapter", "FAST-LIO2", "LIO", "2.0.0", "scala_driver", "2.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.valeo.com/en/valeo-scala/", "Valeo SCALA 2 Production LiDAR Spec v2.0", "SCALA SDK 2.0.0", "Valeo CAN/Ethernet", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "SCALA SDK 2.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("valeo_scala_1_legacy", "Valeo", "SCALA", "Valeo SCALA 1 First Commercial", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("CAN_ETH_GATEWAY");
        m.set_default_ip("192.168.1.210");
        m.set_default_ports({5000, 5001});
        schemas::VendorSdkInfo sdk{"Valeo", "SCALA_SDK", "1.0.0", "scala_legacy", "scala_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.0.x"}, "1.0.0", "1.0.0", "valeo_scala_can", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {25.0};
        feat.factory_calibration_api = "AUTOMOTIVE_FACTORY_FLASH";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ValeoLegacyAdapter", "FAST-LIO2", "LIO", "1.0.0", "scala_legacy", "1.2.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.valeo.com/en/valeo-scala/", "Valeo SCALA 1 First Generation Technical Spec v1.0", "SCALA SDK 1.0.0", "CAN Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "SCALA SDK 1.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("innoviz_two", "Innoviz", "Innoviz", "InnovizTwo 905nm Solid-State", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.180");
        m.set_default_ports({9000, 9001});
        schemas::VendorSdkInfo sdk{"Innoviz", "Innoviz_API", "2.8.0", "innoviz_api", "innoviz_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.8.x"}, "2.8.0", "1.5.0", "innoviz_invz_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "confidence", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "INNOVIZ_CALIB_API";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"InnovizAdapter", "FAST-LIO2", "LIO", "2.8.0", "innoviz_api", "2.5.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://innoviz.tech/innoviztwo/", "InnovizTwo Automotive Solid-State LiDAR Manual v2.8", "Innoviz_API 2.8.0", "Innoviz .invz stream", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Innoviz_API 2.8.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("innoviz_one", "Innoviz", "Innoviz", "InnovizOne Solid-State Classic", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.180");
        m.set_default_ports({9000, 9001});
        schemas::VendorSdkInfo sdk{"Innoviz", "Innoviz_API", "1.5.0", "innoviz_api", "innoviz_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.5.x"}, "1.5.0", "1.5.0", "innoviz_invz_proto_v1", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "INNOVIZ_CALIB_API";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"InnovizAdapter", "FAST-LIO2", "LIO", "1.5.0", "innoviz_api", "1.4.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://innoviz.tech/innovizone/", "InnovizOne Solid-State Automotive Manual v1.5", "Innoviz_API 1.5.0", "Innoviz Protocol v1", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "Innoviz_API 1.5.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("seyond_falcon_k2", "Seyond", "Falcon", "Falcon K2 1550nm Image-Grade", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.205");
        m.set_default_ports({8010, 8011});
        schemas::VendorSdkInfo sdk{"Seyond", "inno-lidar-sdk", "2.6.0", "inno_lidar_driver", "inno_ros", "Apache-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.6.x"}, "2.6.0", "2.0.0", "inno_falcon_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "DUAL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "confidence", "timestamp"};
        feat.scan_rates_hz = {10.0, 15.0};
        feat.factory_calibration_api = "INNO_FLASH_CALIB";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SeyondAdapter", "FAST-LIO2", "LIO", "2.6.0", "inno_lidar_driver", "2.4.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.seyond.com/products/falcon", "Seyond Falcon K2 500m Image-Grade LiDAR Spec v2.6", "inno-lidar-sdk 2.6.0", "Seyond inno-lidar protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "inno-lidar-sdk 2.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("seyond_robin_w", "Seyond", "Robin", "Robin W Wide-Field Blind Spot", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.205");
        m.set_default_ports({8010, 8011});
        schemas::VendorSdkInfo sdk{"Seyond", "inno-lidar-sdk", "2.6.0", "inno_lidar_driver", "inno_ros", "Apache-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.6.x"}, "2.6.0", "2.0.0", "inno_robin_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "INNO_FLASH_CALIB";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SeyondAdapter", "FAST-LIO2", "LIO", "2.6.0", "inno_lidar_driver", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.seyond.com/products/robin", "Seyond Robin W Wide-Field LiDAR Datasheet v2.6", "inno-lidar-sdk 2.6.0", "Seyond Robin Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "inno-lidar-sdk 2.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("morpheustek_perception", "MorpheusTEK", "Perception", "Morpheus 3D Perception LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.150");
        m.set_default_ports({2000, 2001});
        schemas::VendorSdkInfo sdk{"MorpheusTEK", "morpheus_driver", "1.4.0", "morpheus_driver", "morpheus_ros", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.x.x", {"1.4.x"}, "1.4.0", "1.0.0", "morpheus_proto_v1", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "timestamp"};
        feat.scan_rates_hz = {10.0, 20.0};
        feat.factory_calibration_api = "MORPHEUS_EEPROM";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"MorpheusTekAdapter", "FAST-LIO2", "LIO", "1.4.0", "morpheus_driver", "1.2.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://morpheustek.com/lidar/", "Morpheus 3D Perception Sensor Datasheet v1.4", "morpheus_driver 1.4.0", "Morpheus UDP Stream", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "morpheus_driver 1.4.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_gemini_336l", "Orbbec", "Gemini", "Gemini 336L Active Stereo Outdoor", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0670", "0x0671"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows", "Android"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x", "1.9.x"}, "1.10.3", "1.5.0", "orbbec_uvc_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.2.1", schemas::ValidationTier::AV_VALIDATED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::AV_VALIDATED);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/stereo-vision-camera/gemini-336l/", "Orbbec Gemini 336L Active Stereo RGB-D Manual v1.2", "OrbbecSDK 1.10.3", "Orbbec UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", true, true, true, true, true, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_gemini_335", "Orbbec", "Gemini", "Gemini 335 Active Stereo Mid-Range", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0660"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows", "Android"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x"}, "1.10.3", "1.5.0", "orbbec_uvc_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/stereo-vision-camera/gemini-335/", "Orbbec Gemini 335 Active Stereo Camera Manual v1.1", "OrbbecSDK 1.10.3", "Orbbec UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_gemini_335l", "Orbbec", "Gemini", "Gemini 335L Long-Range Stereo", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0661"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x"}, "1.10.3", "1.5.0", "orbbec_uvc_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/stereo-vision-camera/gemini-335l/", "Orbbec Gemini 335L Specification v1.1", "OrbbecSDK 1.10.3", "Orbbec UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_femto_bolt", "Orbbec", "Femto", "Femto Bolt ToF Depth Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("ToF");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0668"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x"}, "1.10.3", "1.5.0", "orbbec_tof_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {15.0, 30.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.0.6", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/tof-camera/femto-bolt/", "Orbbec Femto Bolt Azure Kinect Equivalent Manual v1.0", "OrbbecSDK 1.10.3", "Orbbec ToF Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_femto_mega", "Orbbec", "Femto", "Femto Mega ToF + Jetson Industrial", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("ToF");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0669"});
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({8080});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x"}, "1.10.3", "1.5.0", "orbbec_eth_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {15.0, 30.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/tof-camera/femto-mega/", "Orbbec Femto Mega Industrial PoE Manual v1.1", "OrbbecSDK 1.10.3", "Orbbec Ethernet Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_astra_2", "Orbbec", "Astra", "Astra 2 Structured Light 3D", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0675"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OrbbecSDK", "1.10.3", "pyorbbecsdk", "orbbec_camera 1.10", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"1.10.x"}, "1.10.3", "1.5.0", "orbbec_uvc_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "ORBBEC_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GeminiSensorAdapter", "GeminiRgbdBackend", "RTAB-Map", "1.10.3", "pyorbbecsdk", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/structured-light-camera/astra-2/", "Orbbec Astra 2 3D Structured Light Spec v1.0", "OrbbecSDK 1.10.3", "Orbbec UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OrbbecSDK 1.10.3", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("orbbec_astra_pro_legacy", "Orbbec", "Astra", "Astra Pro Structured Light Classic", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2bc5"});
        m.set_product_ids({"0x0403"});
        schemas::VendorSdkInfo sdk{"Orbbec", "OpenNI2", "2.3.0", "OpenNI2-Astra", "astra_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.2.0", {"2.3.x"}, "2.3.0", "2.3.0", "openni2_proto", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "OPENNI_CALIB";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"OrbbecAstraLegacyAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.3.0", "OpenNI2-Astra", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.orbbec.com/products/structured-light-camera/astra-pro/", "Orbbec Astra Pro Classic OpenNI2 Manual v2.3", "OpenNI2 2.3.0", "OpenNI2 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "OpenNI2 2.3.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_d435i", "Intel", "RealSense", "RealSense D435i Active IR Stereo + IMU", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0b3a"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"5.12.0", "5.x.x", {"2.55.x", "2.50.x"}, "2.55.1", "2.40.0", "realsense_uvc_hid", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 90.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.55.1", "librealsense2", "5.13.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/depth-camera-d435i/", "Intel RealSense D400 Series Product Family Datasheet v5.13", "librealsense2 2.55.1", "RealSense UVC/HID Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_d455", "Intel", "RealSense", "RealSense D455 Long-Range Global Shutter", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0b5c"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"5.12.0", "5.x.x", {"2.55.x"}, "2.55.1", "2.40.0", "realsense_uvc_hid", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 90.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.55.1", "librealsense2", "5.13.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/depth-camera-d455/", "Intel RealSense D455 Long-Range Global Shutter Datasheet", "librealsense2 2.55.1", "RealSense UVC/HID Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_d415", "Intel", "RealSense", "RealSense D415 Standard Precision", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0ad3"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"5.10.0", "5.x.x", {"2.55.x"}, "2.55.1", "2.40.0", "realsense_uvc_hid", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 90.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.55.1", "librealsense2", "5.12.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/depth-camera-d415/", "Intel RealSense D415 Standard Precision Datasheet", "librealsense2 2.55.1", "RealSense UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_d405", "Intel", "RealSense", "RealSense D405 Sub-Millimeter Macro", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0b5b"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"5.13.0", "5.x.x", {"2.55.x"}, "2.55.1", "2.50.0", "realsense_uvc_hid", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 90.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.55.1", "librealsense2", "5.13.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/depth-camera-d405/", "Intel RealSense D405 Macro Depth Camera Datasheet", "librealsense2 2.55.1", "RealSense UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_l515_legacy", "Intel", "RealSense", "RealSense L515 Solid-State MEMS LiDAR", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0b64"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.5.0", "1.5.8", {"2.55.x", "2.50.x"}, "2.55.1", "2.40.0", "realsense_l515_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseAdapter", "FAST-LIO2", "LIO", "2.55.1", "librealsense2", "1.5.8", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/lidar-camera-l515/", "Intel RealSense LiDAR Camera L515 Datasheet v1.5", "librealsense2 2.55.1", "RealSense L515 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_t265_legacy", "Intel", "RealSense", "RealSense T265 Dual Fisheye V-SLAM", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("Multi-Sensor");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0b37"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.55.1", "librealsense2", "realsense2_camera", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.2.0", {"2.55.x"}, "2.55.1", "2.40.0", "realsense_tm2_proto", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"6dof_pose", "imu"};
        feat.scan_rates_hz = {30.0, 200.0};
        feat.factory_calibration_api = "FACTORY_TM2_FLASH";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseTrackingAdapter", "VitureVioBackend", "VIO", "2.55.1", "librealsense2", "1.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com/tracking-camera-t265/", "Intel RealSense Tracking Camera T265 Datasheet v1.1", "librealsense2 2.55.1", "RealSense TM2 Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense2 2.55.1", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("intel_realsense_f200_legacy", "Intel", "RealSense", "RealSense F200/SR300 Coded Light", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("RGB-D");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x8086"});
        m.set_product_ids({"0x0a66"});
        schemas::VendorSdkInfo sdk{"Intel", "librealsense", "2.40.0", "librealsense_legacy", "realsense_ros", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"2.40.x"}, "2.40.0", "2.40.0", "realsense_f200_proto", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "REALSENSE_EEPROM_FLASH";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"RealSenseLegacyAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.40.0", "librealsense_legacy", "1.2.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.intelrealsense.com", "Intel RealSense F200/SR300 Product Specification v1.2", "librealsense 2.40.0", "F200 Coded Light Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "librealsense 2.40.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("luxonis_oak_d_pro", "Luxonis", "OAK", "OAK-D Pro Active Stereo Spatial AI", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("DEPTHAI_BOOT_PROBE");
        m.set_vendor_ids({"0x03e7"});
        m.set_product_ids({"0x2485"});
        schemas::VendorSdkInfo sdk{"Luxonis", "DepthAI", "2.28.0", "depthai-core", "depthai-ros 2.9", "MIT", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.28.x"}, "2.28.0", "2.15.0", "depthai_xlink_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "EEPROM_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"DepthAIAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.28.0", "depthai-core", "2.28.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://docs.luxonis.com/hardware/products/OAK-D-Pro/", "Luxonis OAK-D Pro Hardware & DepthAI Manual v2.28", "depthai-core 2.28.0", "DepthAI XLink Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "depthai-core 2.28.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("luxonis_oak_d_s2", "Luxonis", "OAK", "OAK-D S2 Compact Spatial AI", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("DEPTHAI_BOOT_PROBE");
        m.set_vendor_ids({"0x03e7"});
        m.set_product_ids({"0x2485"});
        schemas::VendorSdkInfo sdk{"Luxonis", "DepthAI", "2.28.0", "depthai-core", "depthai-ros 2.9", "MIT", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.28.x"}, "2.28.0", "2.15.0", "depthai_xlink_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "EEPROM_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"DepthAIAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.28.0", "depthai-core", "2.28.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://docs.luxonis.com/hardware/products/OAK-D-S2/", "Luxonis OAK-D S2 Compact Stereo Manual v2.28", "depthai-core 2.28.0", "DepthAI XLink Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "depthai-core 2.28.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("luxonis_oak_1", "Luxonis", "OAK", "OAK-1 4K 12MP Spatial AI Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB Camera");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("DEPTHAI_BOOT_PROBE");
        m.set_vendor_ids({"0x03e7"});
        m.set_product_ids({"0x2485"});
        schemas::VendorSdkInfo sdk{"Luxonis", "DepthAI", "2.28.0", "depthai-core", "depthai-ros 2.9", "MIT", {"Linux", "Windows", "macOS"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.28.x"}, "2.28.0", "2.15.0", "depthai_xlink_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "EEPROM_CALIBRATION_JSON";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GenericUvcSensorAdapter", "LingBotStreamingBackend", "MONOCULAR_NEURAL_RECONSTRUCTION", "2.28.0", "depthai-core", "2.28.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://docs.luxonis.com/hardware/products/OAK-1/", "Luxonis OAK-1 4K Camera Product Manual v2.28", "depthai-core 2.28.0", "DepthAI XLink Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "depthai-core 2.28.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("luxonis_oak_d_classic", "Luxonis", "OAK", "OAK-D Original Spatial AI", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("DEPTHAI_BOOT_PROBE");
        m.set_vendor_ids({"0x03e7"});
        m.set_product_ids({"0x2485"});
        schemas::VendorSdkInfo sdk{"Luxonis", "DepthAI", "2.28.0", "depthai-core", "depthai-ros 2.9", "MIT", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"2.0.0", "2.x.x", {"2.28.x"}, "2.28.0", "2.10.0", "depthai_xlink_v2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "EEPROM_CALIBRATION_JSON";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"DepthAIAdapter", "GeminiRgbdBackend", "RTAB-Map", "2.28.0", "depthai-core", "2.20.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://docs.luxonis.com/hardware/products/OAK-D/", "Luxonis OAK-D Original Product Manual", "depthai-core 2.28.0", "DepthAI XLink Protocol v2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "depthai-core 2.28.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("stereolabs_zed_2i", "Stereolabs", "ZED", "ZED 2i Polarizing Stereo + IMU", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2b03"});
        m.set_product_ids({"0xf682"});
        schemas::VendorSdkInfo sdk{"Stereolabs", "ZED_SDK", "4.1.2", "zed-sdk", "zed-ros-wrapper", "Proprietary", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "4.x.x", {"4.1.x"}, "4.1.2", "3.8.0", "sl_zed_uvc_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 100.0};
        feat.factory_calibration_api = "STEREOLABS_FACTORY_ONLINE";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ZedAdapter", "GeminiRgbdBackend", "RTAB-Map", "4.1.2", "zed-sdk", "3.1.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.stereolabs.com/products/zed-2i", "Stereolabs ZED 2i Industrial Stereo Camera Manual v4.1", "ZED SDK 4.1.2", "Stereolabs HID/UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ZED SDK 4.1.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("stereolabs_zed_x", "Stereolabs", "ZED", "ZED X Industrial GMSL2 Stereo", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("GMSL2");
        m.set_discovery_mechanism("GMSL_V4L2");
        m.set_vendor_ids({"0x2b03"});
        m.set_product_ids({"0xf700"});
        schemas::VendorSdkInfo sdk{"Stereolabs", "ZED_SDK", "4.1.2", "zed-sdk", "zed-ros-wrapper", "Proprietary", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"4.0.0", "4.x.x", {"4.1.x"}, "4.1.2", "4.0.0", "sl_zed_gmsl2", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 120.0};
        feat.factory_calibration_api = "STEREOLABS_FACTORY_ONLINE";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ZedAdapter", "GeminiRgbdBackend", "RTAB-Map", "4.1.2", "zed-sdk", "4.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.stereolabs.com/products/zed-x", "Stereolabs ZED X Industrial Global Shutter Camera Manual v4.1", "ZED SDK 4.1.2", "GMSL2 Driver Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ZED SDK 4.1.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("stereolabs_zed_2", "Stereolabs", "ZED", "ZED 2 Dual High-Resolution Stereo", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2b03"});
        m.set_product_ids({"0xf680"});
        schemas::VendorSdkInfo sdk{"Stereolabs", "ZED_SDK", "4.1.2", "zed-sdk", "zed-ros-wrapper", "Proprietary", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "4.x.x", {"4.1.x"}, "4.1.2", "3.8.0", "sl_zed_uvc_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 100.0};
        feat.factory_calibration_api = "STEREOLABS_FACTORY_ONLINE";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ZedAdapter", "GeminiRgbdBackend", "RTAB-Map", "4.1.2", "zed-sdk", "3.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.stereolabs.com/products/zed-2", "Stereolabs ZED 2 Stereo Camera User Guide v4.1", "ZED SDK 4.1.2", "Stereolabs HID/UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ZED SDK 4.1.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("stereolabs_zed_mini", "Stereolabs", "ZED", "ZED Mini Compact Mixed Reality", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2b03"});
        m.set_product_ids({"0xf620"});
        schemas::VendorSdkInfo sdk{"Stereolabs", "ZED_SDK", "4.1.2", "zed-sdk", "zed-ros-wrapper", "Proprietary", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"3.0.0", "4.x.x", {"4.1.x"}, "4.1.2", "3.8.0", "sl_zed_uvc_v4", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0, 100.0};
        feat.factory_calibration_api = "STEREOLABS_FACTORY_ONLINE";
        feat.timestamp_source = "HARDWARE_USB_SOF";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ZedAdapter", "GeminiRgbdBackend", "RTAB-Map", "4.1.2", "zed-sdk", "3.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.stereolabs.com/products/zed-mini", "Stereolabs ZED Mini User Guide v4.1", "ZED SDK 4.1.2", "Stereolabs HID/UVC Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ZED SDK 4.1.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("stereolabs_zed_classic_legacy", "Stereolabs", "ZED", "ZED Original USB 3.0 Classic", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("Stereo");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("UVC_PROBE");
        m.set_vendor_ids({"0x2b03"});
        m.set_product_ids({"0xf580"});
        schemas::VendorSdkInfo sdk{"Stereolabs", "ZED_SDK", "3.8.2", "zed-sdk", "zed-ros-wrapper", "Proprietary", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.5.0", {"3.8.x", "3.5.x"}, "3.8.2", "3.8.2", "sl_zed_uvc_legacy", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {30.0, 60.0};
        feat.factory_calibration_api = "STEREOLABS_FACTORY_ONLINE";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"ZedLegacyAdapter", "GeminiRgbdBackend", "RTAB-Map", "3.8.2", "zed-sdk", "2.2.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.stereolabs.com/products/zed", "Stereolabs ZED Original Stereo Camera User Guide v3.8", "ZED SDK 3.8.2", "Stereolabs Legacy Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "ZED SDK 3.8.2", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("generic_uvc_rgb", "Generic", "UVC", "Generic UVC RGB Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("RGB Camera");
        m.set_transport_type("USB_UVC");
        m.set_discovery_mechanism("V4L2");
        m.set_vendor_ids({"0x32e4"});
        m.set_product_ids({"0x4802"});
        schemas::VendorSdkInfo sdk{"Standard", "V4L2", "1.0.0", "uvcvideo", "v4l2_camera", "GPL-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.5.0", {"1.0.0"}, "1.0.0", "1.0.0", "v4l2_ioctl", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {};
        feat.point_cloud_fields = {"x", "y", "z", "rgb"};
        feat.scan_rates_hz = {30.0};
        feat.factory_calibration_api = "";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"GenericUvcSensorAdapter", "LingBotStreamingBackend", "MONOCULAR_NEURAL_RECONSTRUCTION", "1.0.0", "uvcvideo", "1.0.0", schemas::ValidationTier::AV_VALIDATED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::AV_VALIDATED);
        schemas::ProvenanceRecord prov{"https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html", "Linux Video4Linux2 API Specification v1.0", "V4L2 1.0.0", "v4l2_ioctl", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "V4L2 1.0.0", true, true, true, true, true, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("generic_cdc_acm_unknown", "Generic", "Serial", "USB CDC-ACM Device", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Other");
        m.set_transport_type("USB_SERIAL");
        m.set_discovery_mechanism("SERIAL_PROBE");
        schemas::VendorSdkInfo sdk{"Standard", "cdc_acm", "1.0.0", "cdc_acm", "serial", "GPL-2.0", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.0.0", {"1.0.0"}, "1.0.0", "1.0.0", "ttyACM_serial", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {};
        feat.point_cloud_fields = {};
        feat.scan_rates_hz = {};
        feat.factory_calibration_api = "";
        feat.timestamp_source = "";
        m.set_features(feat);
        schemas::AvSupportInfo av{"NONE", "NONE", "UNKNOWN", "1.0.0", "cdc_acm", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_DOCUMENTATION);
        schemas::ProvenanceRecord prov{"https://www.kernel.org/doc/html/latest/usb/cdc-acm.html", "USB Communications Device Class Abstract Control Model Specification", "cdc_acm 1.0.0", "USB CDC-ACM 1.2", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "cdc_acm 1.0.0", true, true, false, false, true, true};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_multiscan100", "SICK", "multiScan", "multiScan100 Multi-Layer 3D LiDAR + IMU", "3.6.0", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b_multiscan", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "ALL"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "layer", "echo", "time"};
        feat.scan_rates_hz = {10.0, 15.0, 20.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "FAST-LIO2", "LIO", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://github.com/SICKAG/sick_scan_xd", "SICK multiScan100 Multi-Layer 3D LiDAR Specification v3.6.0", "sick_scan_xd 3.6.0", "SICK CoLa-B Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_picoscan150", "SICK", "picoScan", "picoScan150 Ultra-Compact LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b_picoscan", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "time"};
        feat.scan_rates_hz = {15.0, 25.0, 50.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "FAST-LIO2", "LIO", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://github.com/SICKAG/sick_scan_xd", "SICK picoScan150 Compact LiDAR Operating Instructions v1.2", "sick_scan_xd 3.6.0", "SICK CoLa-B Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_mrs6000", "SICK", "MRS", "MRS6000 Multi-Layer 3D LiDAR (24 Layers)", "RevC", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b_mrs", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "layer"};
        feat.scan_rates_hz = {10.0, 15.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "FAST-LIO2", "LIO", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/3d-lidar-sensors/mrs6000/c/g377484", "SICK MRS6000 3D LiDAR Operating Instructions", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_mrs1000", "SICK", "MRS", "MRS1000 4-Layer 3D LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b_mrs", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "layer"};
        feat.scan_rates_hz = {12.5, 50.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "FAST-LIO2", "LIO", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/3d-lidar-sensors/mrs1000/c/g370169", "SICK MRS1000 4-Layer 3D LiDAR Technical Information", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_lms1000", "SICK", "LMS", "LMS1000 High-Speed 2D LiDAR (275° FOV)", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {50.0, 100.0, 200.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/lms1000/c/g390547", "SICK LMS1000 2D LiDAR Operating Instructions", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_lms4000", "SICK", "LMS", "LMS4000 High-Precision 1D/2D Profile Scanner", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {600.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/lms4000/c/g460015", "SICK LMS4000 High Precision Scanner Technical Manual", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_lms5xx", "SICK", "LMS", "LMS5xx Outdoor Long-Range 2D LiDAR", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"3.x.x", "2.x.x"}, "3.6.0", "2.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST", "MULTI"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {25.0, 50.0, 100.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "2.1.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/lms5xx/c/g178049", "SICK LMS5xx Outdoor 2D LiDAR Technical Specifications", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_lms1xx", "SICK", "LMS", "LMS1xx Compact 2D LiDAR", "Legacy", schemas::ProductionStatus::LEGACY);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_legacy", "1.14.0", "sick_scan", "sick_scan_ros", "BSD-3-Clause", {"Linux"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "1.8.0", {"1.x.x"}, "1.14.0", "1.0.0", "sick_cola_b_legacy", true};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "LAST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {25.0, 50.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickLegacyAdapter", "Cartographer2D", "PlanarSLAM", "1.14.0", "sick_scan_legacy", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_LEGACY_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/lms1xx/c/g178047", "SICK LMS1xx Legacy 2D LiDAR Operating Instructions", "sick_scan_legacy 1.14.0", "SICK CoLa-B Legacy", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_legacy 1.14.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_tim7xx", "SICK", "TiM", "TiM7xx Compact Localization/Mapping 2D LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_vendor_ids({"19a2"});
        m.set_product_ids({"5001"});
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {15.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/tim7xx/c/g460014", "SICK TiM7xx Compact Localization 2D LiDAR Operating Manual", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_tim5xx", "SICK", "TiM", "TiM5xx Non-Safety 2D LiDAR", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_vendor_ids({"19a2"});
        m.set_product_ids({"5001"});
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"3.x.x", "2.x.x"}, "3.6.0", "2.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {15.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "2.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/tim5xx/c/g178051", "SICK TiM5xx 2D LiDAR Specification Sheet", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_tim2xx", "SICK", "TiM", "TiM2xx Mini 2D LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_vendor_ids({"19a2"});
        m.set_product_ids({"5002"});
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {15.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/tim2xx/c/g541818", "SICK TiM2xx Mini LiDAR Operating Manual", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_lrs4000", "SICK", "LRS", "LRS4000 Long-Range 2D LiDAR Scanner", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "STRONGEST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {25.0, 50.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/lrs/c/g178052", "SICK LRS4000 Long-Range 2D LiDAR Operating Instructions", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_nav3xx", "SICK", "NAV", "NAV3xx Navigation & Localization LiDAR", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "3.x.x", {"3.x.x"}, "3.6.0", "1.0.0", "sick_cola_a", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = true;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST", "REFLECTOR"};
        feat.point_cloud_fields = {"x", "y", "intensity", "reflector_id"};
        feat.scan_rates_hz = {8.0, 15.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "1.0.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/nav/c/g178053", "SICK NAV3xx Navigation & Reflector Localization Manual", "sick_scan_xd 3.6.0", "SICK CoLa-A", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_nav2xx", "SICK", "NAV", "NAV2xx Compact AGV Navigation LiDAR", "Previous", schemas::ProductionStatus::PREVIOUS);
        m.set_sensor_type("LiDAR");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.0.1");
        m.set_default_ports({2111, 2112});
        schemas::VendorSdkInfo sdk{"SICK", "sick_scan_xd", "3.6.0", "sick_scan_xd", "sick_scan_xd_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"3.x.x", "2.x.x"}, "3.6.0", "2.0.0", "sick_cola_b", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = true;
        feat.has_rgb = false;
        feat.has_depth = false;
        feat.has_imu = false;
        feat.has_ptp = false;
        feat.has_pps = false;
        feat.has_hardware_sync = false;
        feat.native_coordinate_system = "FLU";
        feat.return_modes = {"FIRST"};
        feat.point_cloud_fields = {"x", "y", "intensity"};
        feat.scan_rates_hz = {25.0};
        feat.factory_calibration_api = "COLA_FLASH_CALIBRATION";
        feat.timestamp_source = "HOST_RECEIVE";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickScanAdapter", "Cartographer2D", "PlanarSLAM", "3.6.0", "sick_scan_xd", "2.1.0", schemas::ValidationTier::DOCUMENTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://www.sick.com/ag/en/lidar-sensors/2d-lidar-sensors/nav/c/g178053", "SICK NAV2xx Navigation LiDAR Operating Instructions", "sick_scan_xd 3.6.0", "SICK CoLa-B", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_scan_xd 3.6.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_visionary_t_mini", "SICK", "Visionary-T", "Visionary-T Mini 3D ToF Snapshot Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("ToF");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({2114, 2115});
        schemas::VendorSdkInfo sdk{"SICK", "sick_visionary_cpp_shared", "2.0.0", "sick_visionary_samples", "sick_visionary_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "sick_visionary_t_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = false;
        feat.has_depth = true;
        feat.has_imu = true;
        feat.has_ptp = true;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {"DEPTH_ONLY"};
        feat.point_cloud_fields = {"x", "y", "z", "intensity", "confidence"};
        feat.scan_rates_hz = {15.0, 30.0};
        feat.factory_calibration_api = "FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickVisionaryAdapter", "RTAB-Map", "RGBD", "2.0.0", "sick_visionary_samples", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://github.com/SICKAG/sick_visionary_samples", "SICK Visionary-T Mini 3D Snapshot Camera Manual v1.2", "sick_visionary_cpp_shared 2.0.0 (Linux/Windows/WSL)", "SICK CoLa-B Visionary Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_visionary_samples 2.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

    {
        schemas::DeviceModelDefinition m("sick_visionary_s", "SICK", "Visionary-S", "Visionary-S Active Stereo 3D RGB-D Camera", "Current", schemas::ProductionStatus::CURRENT);
        m.set_sensor_type("Stereo");
        m.set_transport_type("ETHERNET");
        m.set_discovery_mechanism("UDP_BROADCAST");
        m.set_default_ip("192.168.1.10");
        m.set_default_ports({2114, 2115});
        schemas::VendorSdkInfo sdk{"SICK", "sick_visionary_cpp_shared", "2.0.0", "sick_visionary_samples", "sick_visionary_ros2", "Apache-2.0", {"Linux", "Windows"}};
        m.set_vendor_sdk(sdk);
        schemas::CompatibilityMatrix c{"1.0.0", "2.x.x", {"2.0.x"}, "2.0.0", "1.0.0", "sick_visionary_s_proto", false};
        m.set_compatibility(c);
        schemas::HardwareRevisionFeatures feat;
        feat.has_lidar = false;
        feat.has_rgb = true;
        feat.has_depth = true;
        feat.has_imu = false;
        feat.has_ptp = true;
        feat.has_pps = false;
        feat.has_hardware_sync = true;
        feat.native_coordinate_system = "SENSOR_OPTICAL";
        feat.return_modes = {"RGB_DEPTH"};
        feat.point_cloud_fields = {"x", "y", "z", "rgb", "depth"};
        feat.scan_rates_hz = {15.0, 30.0};
        feat.factory_calibration_api = "FLASH_INTRINSICS";
        feat.timestamp_source = "HARDWARE_PTP";
        m.set_features(feat);
        schemas::AvSupportInfo av{"SickVisionaryAdapter", "RTAB-Map", "RGBD", "2.0.0", "sick_visionary_samples", "2.0.0", schemas::ValidationTier::SDK_SUPPORTED};
        m.set_av_support(av);
        m.set_source_provenance(schemas::SourceProvenance::OFFICIAL_CURRENT_SDK);
        schemas::ProvenanceRecord prov{"https://github.com/SICKAG/sick_visionary_samples", "SICK Visionary-S Active Stereo 3D Camera Operating Instructions", "sick_visionary_cpp_shared 2.0.0 (Linux/Windows/WSL)", "SICK CoLa-B Visionary Protocol", "2026-09"};
        m.set_provenance_record(prov);
        schemas::RuntimeIntegrationMatrix rim{true, "sick_visionary_samples 2.0.0", false, false, false, true, false, false};
        m.set_runtime_integration(rim);
        registerModel(m);
    }

}

} // namespace av::core::ecosystem
