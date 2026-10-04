#pragma once

#include "core/schemas/ecosystem_types.hpp"
#include "core/schemas/model_definition.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace av::core::ecosystem {

class VendorEcosystemRegistry {
public:
    static VendorEcosystemRegistry& instance();

    VendorEcosystemRegistry();

    /**
     * Retrieve all indexed model definitions across all 14+ vendors and generations.
     */
    const std::vector<schemas::DeviceModelDefinition>& getAllModels() const noexcept {
        return models_;
    }

    /**
     * Find model definition by model ID (e.g. "ouster_os1_rev8_rgb").
     */
    const schemas::DeviceModelDefinition* findModelById(const std::string& model_id) const;

    /**
     * Look up known model definition by physical hardware signature.
     * Matches USB VID:PID, sysfs strings, MAC/IP, or vendor/model strings.
     */
    const schemas::DeviceModelDefinition* findModelByHardware(
        const std::string& vendor_id,
        const std::string& product_id,
        const std::string& vendor_name = "",
        const std::string& product_name = "",
        const std::string& transport_address = ""
    ) const;

    /**
     * Filter models by vendor.
     */
    std::vector<schemas::DeviceModelDefinition> getModelsByVendor(const std::string& vendor) const;

    /**
     * Filter models by sensor type (e.g. "LiDAR", "RGB-D", "Stereo", "RGB Camera", "ToF").
     */
    std::vector<schemas::DeviceModelDefinition> getModelsBySensorType(const std::string& type) const;

    /**
     * Filter models by production generation status (CURRENT, PREVIOUS, LEGACY).
     */
    std::vector<schemas::DeviceModelDefinition> getModelsByStatus(schemas::ProductionStatus status) const;

    /**
     * Retrieve unique list of all indexed manufacturers.
     */
    std::vector<std::string> getManufacturers() const;

    /**
     * Retrieve unique list of all sensor types.
     */
    std::vector<std::string> getSensorTypes() const;

    /**
     * Retrieve sensor types supported by a specific manufacturer.
     */
    std::vector<std::string> getSensorTypesForManufacturer(const std::string& vendor) const;

    /**
     * Filter models hierarchically: Manufacturer -> Sensor Type.
     */
    std::vector<schemas::DeviceModelDefinition> getModels(const std::string& vendor, const std::string& sensor_type = "") const;

    /**
     * Free-text search across manufacturer, model name, family, revision, and capabilities.
     */
    std::vector<schemas::DeviceModelDefinition> searchModels(const std::string& query) const;

    /**
     * Automatically generates the authoritative capability matrix JSON
     * directly from the model definitions and vendor SDK specifications.
     */
    nlohmann::json generateCapabilityMatrixJson() const;

private:
    void registerModel(const schemas::DeviceModelDefinition& def);
    void populateAuthoritativeEcosystem();

    std::vector<schemas::DeviceModelDefinition> models_;
    std::map<std::string, size_t> id_index_;
};

} // namespace av::core::ecosystem
