#include "calibration_provenance.hpp"

namespace av::core::schemas {

nlohmann::json CalibrationProvenance::to_json() const {
    return {
        {"schema_version", schema_version_},
        {"value", value_},
        {"source", to_string(source_)},
        {"confidence", confidence_},
        {"device_serial", device_serial_},
        {"firmware", firmware_},
        {"evidence", evidence_},
        {"validated", validated_}
    };
}

CalibrationProvenance CalibrationProvenance::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::invalid_argument("Expected JSON object for CalibrationProvenance");
    }

    int version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
    if (version > CURRENT_SCHEMA_VERSION) {
        throw SchemaVersionException("CalibrationProvenance", version, CURRENT_SCHEMA_VERSION);
    }

    CalibrationProvenance prov;
    prov.schema_version_ = version;
    prov.value_ = j.value("value", "");
    prov.source_ = calibration_source_from_string(j.value("source", "UNKNOWN"));
    prov.confidence_ = j.value("confidence", 0.0);
    prov.device_serial_ = j.value("device_serial", "");
    prov.firmware_ = j.value("firmware", "");
    prov.evidence_ = j.value("evidence", "");
    prov.validated_ = j.value("validated", false);
    return prov;
}

} // namespace av::core::schemas
