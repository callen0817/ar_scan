#pragma once

#include "ecosystem_types.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace av::core::schemas {
class DeviceProfile;

class DeviceModelDefinition {
public:
    DeviceModelDefinition() = default;
    DeviceModelDefinition(
        const std::string& model_id,
        const std::string& vendor,
        const std::string& family,
        const std::string& model,
        const std::string& hardware_revision,
        ProductionStatus status
    ) : model_id_(model_id), vendor_(vendor), family_(family),
        model_(model), hardware_revision_(hardware_revision),
        production_status_(status) {}

    // Getters
    const std::string& model_id() const noexcept { return model_id_; }
    const std::string& vendor() const noexcept { return vendor_; }
    const std::string& family() const noexcept { return family_; }
    const std::string& model() const noexcept { return model_; }
    const std::string& hardware_revision() const noexcept { return hardware_revision_; }
    ProductionStatus production_status() const noexcept { return production_status_; }
    const std::string& transport_type() const noexcept { return transport_type_; }
    const std::string& discovery_mechanism() const noexcept { return discovery_mechanism_; }
    const std::vector<std::string>& vendor_ids() const noexcept { return vendor_ids_; }
    const std::vector<std::string>& product_ids() const noexcept { return product_ids_; }
    const std::string& default_ip() const noexcept { return default_ip_; }
    const std::vector<int>& default_ports() const noexcept { return default_ports_; }
    const VendorSdkInfo& vendor_sdk() const noexcept { return vendor_sdk_; }
    const CompatibilityMatrix& compatibility() const noexcept { return compatibility_; }
    const HardwareRevisionFeatures& features() const noexcept { return features_; }
    const AvSupportInfo& av_support() const noexcept { return av_support_; }
    SourceProvenance source_provenance() const noexcept { return source_provenance_; }

    const std::string& sensor_type() const noexcept { return sensor_type_; }
    const ProvenanceRecord& provenance_record() const noexcept { return provenance_record_; }
    const RuntimeIntegrationMatrix& runtime_integration() const noexcept { return runtime_integration_; }

    // Setters
    void set_model_id(const std::string& id) { model_id_ = id; }
    void set_vendor(const std::string& v) { vendor_ = v; }
    void set_family(const std::string& f) { family_ = f; }
    void set_model(const std::string& m) { model_ = m; }
    void set_hardware_revision(const std::string& r) { hardware_revision_ = r; }
    void set_production_status(ProductionStatus s) { production_status_ = s; }
    void set_sensor_type(const std::string& st) { sensor_type_ = st; }
    void set_transport_type(const std::string& t) { transport_type_ = t; }
    void set_discovery_mechanism(const std::string& d) { discovery_mechanism_ = d; }
    void set_vendor_ids(const std::vector<std::string>& vids) { vendor_ids_ = vids; }
    void set_product_ids(const std::vector<std::string>& pids) { product_ids_ = pids; }
    void set_default_ip(const std::string& ip) { default_ip_ = ip; }
    void set_default_ports(const std::vector<int>& ports) { default_ports_ = ports; }
    void set_vendor_sdk(const VendorSdkInfo& sdk) { vendor_sdk_ = sdk; }
    void set_compatibility(const CompatibilityMatrix& c) { compatibility_ = c; }
    void set_features(const HardwareRevisionFeatures& f) { features_ = f; }
    void set_av_support(const AvSupportInfo& sup) { av_support_ = sup; }
    void set_source_provenance(SourceProvenance p) { source_provenance_ = p; }
    void set_provenance_record(const ProvenanceRecord& pr) { provenance_record_ = pr; }
    void set_runtime_integration(const RuntimeIntegrationMatrix& rim) { runtime_integration_ = rim; }

    // Profile generation from authoritative model definition
    DeviceProfile seedDeviceProfile(bool is_template = true) const;

    nlohmann::json to_json() const;
    static DeviceModelDefinition from_json(const nlohmann::json& j);

private:
    std::string model_id_;
    std::string vendor_;
    std::string family_;
    std::string model_;
    std::string hardware_revision_;
    ProductionStatus production_status_{ProductionStatus::CURRENT};
    std::string transport_type_{"ETHERNET"};
    std::string discovery_mechanism_{"UDP_BROADCAST"};
    std::vector<std::string> vendor_ids_;
    std::vector<std::string> product_ids_;
    std::string default_ip_;
    std::vector<int> default_ports_;
    VendorSdkInfo vendor_sdk_;
    CompatibilityMatrix compatibility_;
    HardwareRevisionFeatures features_;
    AvSupportInfo av_support_;
    SourceProvenance source_provenance_{SourceProvenance::OFFICIAL_DOCUMENTATION};
    std::string sensor_type_{"LiDAR"};
    ProvenanceRecord provenance_record_;
    RuntimeIntegrationMatrix runtime_integration_;
};

} // namespace av::core::schemas
