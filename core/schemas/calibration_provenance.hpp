#pragma once

#include "types.hpp"
#include <string>
#include <nlohmann/json.hpp>

namespace av::core::schemas {

class CalibrationProvenance {
public:
    CalibrationProvenance() = default;

    CalibrationProvenance(const std::string& value,
                          CalibrationSource source,
                          double confidence,
                          const std::string& device_serial = "",
                          const std::string& firmware = "",
                          const std::string& evidence = "",
                          bool validated = false)
        : schema_version_(CURRENT_SCHEMA_VERSION),
          value_(value),
          source_(source),
          confidence_(confidence),
          device_serial_(device_serial),
          firmware_(firmware),
          evidence_(evidence),
          validated_(validated) {}

    // Getters
    int schema_version() const noexcept { return schema_version_; }
    const std::string& value() const noexcept { return value_; }
    CalibrationSource source() const noexcept { return source_; }
    double confidence() const noexcept { return confidence_; }
    const std::string& device_serial() const noexcept { return device_serial_; }
    const std::string& firmware() const noexcept { return firmware_; }
    const std::string& evidence() const noexcept { return evidence_; }
    bool validated() const noexcept { return validated_; }

    // Setters
    void set_value(const std::string& val) { value_ = val; }
    void set_source(CalibrationSource src) { source_ = src; }
    void set_confidence(double conf) { confidence_ = conf; }
    void set_device_serial(const std::string& s) { device_serial_ = s; }
    void set_firmware(const std::string& fw) { firmware_ = fw; }
    void set_evidence(const std::string& ev) { evidence_ = ev; }
    void set_validated(bool val) { validated_ = val; }

    // Serialization
    nlohmann::json to_json() const;
    static CalibrationProvenance from_json(const nlohmann::json& j);

private:
    int schema_version_{CURRENT_SCHEMA_VERSION};
    std::string value_;
    CalibrationSource source_{CalibrationSource::UNKNOWN};
    double confidence_{0.0};
    std::string device_serial_;
    std::string firmware_;
    std::string evidence_;
    bool validated_{false};
};

} // namespace av::core::schemas
