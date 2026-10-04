#include "model_definition.hpp"
#include "device_profile.hpp"

namespace av::core::schemas {

nlohmann::json DeviceModelDefinition::to_json() const {
    return {
        {"model_id", model_id_},
        {"vendor", vendor_},
        {"family", family_},
        {"model", model_},
        {"hardware_revision", hardware_revision_},
        {"production_status", to_string(production_status_)},
        {"sensor_type", sensor_type_},
        {"transport_type", transport_type_},
        {"discovery_mechanism", discovery_mechanism_},
        {"vendor_ids", vendor_ids_},
        {"product_ids", product_ids_},
        {"default_ip", default_ip_},
        {"default_ports", default_ports_},
        {"vendor_sdk", vendor_sdk_.to_json()},
        {"compatibility", compatibility_.to_json()},
        {"features", features_.to_json()},
        {"av_support", av_support_.to_json()},
        {"source_provenance", to_string(source_provenance_)},
        {"provenance_record", provenance_record_.to_json()},
        {"runtime_integration", runtime_integration_.to_json()}
    };
}

DeviceModelDefinition DeviceModelDefinition::from_json(const nlohmann::json& j) {
    DeviceModelDefinition def;
    if (j.contains("model_id")) def.model_id_ = j["model_id"].get<std::string>();
    if (j.contains("vendor")) def.vendor_ = j["vendor"].get<std::string>();
    if (j.contains("family")) def.family_ = j["family"].get<std::string>();
    if (j.contains("model")) def.model_ = j["model"].get<std::string>();
    if (j.contains("hardware_revision")) def.hardware_revision_ = j["hardware_revision"].get<std::string>();
    if (j.contains("production_status")) def.production_status_ = production_status_from_string(j["production_status"].get<std::string>());
    if (j.contains("sensor_type")) def.sensor_type_ = j["sensor_type"].get<std::string>();
    if (j.contains("transport_type")) def.transport_type_ = j["transport_type"].get<std::string>();
    if (j.contains("discovery_mechanism")) def.discovery_mechanism_ = j["discovery_mechanism"].get<std::string>();
    if (j.contains("vendor_ids")) def.vendor_ids_ = j["vendor_ids"].get<std::vector<std::string>>();
    if (j.contains("product_ids")) def.product_ids_ = j["product_ids"].get<std::vector<std::string>>();
    if (j.contains("default_ip")) def.default_ip_ = j["default_ip"].get<std::string>();
    if (j.contains("default_ports")) def.default_ports_ = j["default_ports"].get<std::vector<int>>();
    if (j.contains("vendor_sdk")) def.vendor_sdk_ = VendorSdkInfo::from_json(j["vendor_sdk"]);
    if (j.contains("compatibility")) def.compatibility_ = CompatibilityMatrix::from_json(j["compatibility"]);
    if (j.contains("features")) def.features_ = HardwareRevisionFeatures::from_json(j["features"]);
    if (j.contains("av_support")) def.av_support_ = AvSupportInfo::from_json(j["av_support"]);
    if (j.contains("source_provenance")) def.source_provenance_ = source_provenance_from_string(j["source_provenance"].get<std::string>());
    if (j.contains("provenance_record")) def.provenance_record_ = ProvenanceRecord::from_json(j["provenance_record"]);
    if (j.contains("runtime_integration")) def.runtime_integration_ = RuntimeIntegrationMatrix::from_json(j["runtime_integration"]);
    return def;
}

DeviceProfile DeviceModelDefinition::seedDeviceProfile(bool is_template) const {
    DeviceProfile p;
    p.set_profile_id(model_id_ + (is_template ? "_template" : "_interrogated"));
    p.set_profile_version("1.0.0");
    p.set_manufacturer(vendor_);
    p.set_model(model_);
    p.set_device_family(family_);
    p.set_hardware_revision(hardware_revision_);
    p.set_firmware(is_template ? (compatibility_.preferred_sdk_version + " (Template)") : compatibility_.preferred_sdk_version);
    p.set_transport(transport_type_);
    p.set_driver(av_support_.adapter_class);
    p.set_sdk(vendor_sdk_.current_sdk);
    p.set_slam_family(av_support_.slam_family);
    p.set_slam_backend(av_support_.backend);

    if (is_template) {
        p.set_profile_status(ProfileStatus::EXPERIMENTAL);
    } else {
        p.set_profile_status(av_support_.validation_status == ValidationTier::AV_VALIDATED ? 
                             ProfileStatus::VALIDATED : ProfileStatus::EXPERIMENTAL);
    }

    std::vector<std::string> caps;
    std::vector<StreamInfo> streams;

    if (features_.has_lidar) {
        caps.push_back("LIDAR");
        caps.push_back("POINT_CLOUD");
        double rate = features_.scan_rates_hz.empty() ? 10.0 : features_.scan_rates_hz.front();
        streams.push_back(StreamInfo{"lidar_points", "POINT_CLOUD", "XYZI_PCL", rate, true});
    }
    if (features_.has_rgb) {
        caps.push_back("RGB");
        if (features_.has_lidar) caps.push_back("COLOR_POINT_CLOUD");
        streams.push_back(StreamInfo{"rgb_stream", "RGB", "RGB8", 30.0, true});
    }
    if (features_.has_depth) {
        caps.push_back("DEPTH");
        streams.push_back(StreamInfo{"depth_stream", "DEPTH", "DEPTH16", 30.0, true});
    }
    if (features_.has_imu) {
        caps.push_back("IMU");
        caps.push_back("6DOF_TRACKING");
        streams.push_back(StreamInfo{"imu_stream", "IMU", "6DOF_IMU", 200.0, true});
    }

    p.set_capabilities(caps);
    p.set_streams(streams);

    if (!features_.timestamp_source.empty()) {
        p.set_timestamp_sources({features_.timestamp_source});
    } else {
        p.set_timestamp_sources({"HOST_RECEIVE"});
    }

    p.set_coordinate_frames({
        {"native_frame", features_.native_coordinate_system},
        {"av_map_frame", "FLU (+X forward, +Y left, +Z up)"}
    });

    CalibrationProvenance cal;
    cal.set_source(is_template ? CalibrationSource::OFFICIAL_DOCUMENTATION : CalibrationSource::DEVICE_FACTORY);
    cal.set_evidence(is_template ? 
        "Model definition template parameters (uninterrogated physical hardware)" :
        "Interrogated hardware factory calibration parameters");
    cal.set_confidence(is_template ? 0.5 : 1.0);
    cal.set_validated(!is_template);
    p.set_calibration_sources({cal});
    p.set_provenance(cal);

    return p;
}

} // namespace av::core::schemas
