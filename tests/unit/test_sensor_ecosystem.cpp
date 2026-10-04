#include "core/ecosystem/vendor_ecosystem.hpp"
#include "core/ecosystem/compatibility_resolver.hpp"
#include "core/schemas/ecosystem_types.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdlib>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at line " << __LINE__ << ": " << (msg) << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace av::core::ecosystem;
using namespace av::core::schemas;

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << "[M14.5 TEST] Universal Sensor Ecosystem & Compatibility" << std::endl;
    std::cout << "============================================================" << std::endl;

    auto& reg = VendorEcosystemRegistry::instance();
    auto& resolver = CompatibilityResolver::instance();

    // -------------------------------------------------------------------------
    // 1. Verify 14+ Vendor Families Indexed
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 1] Verifying 14+ Vendor Families Indexed..." << std::endl;
    std::vector<std::string> expected_vendors = {
        "Ouster", "RoboSense", "Unitree", "Hesai", "Livox", "LSLiDAR",
        "Valeo", "Innoviz", "Seyond", "MorpheusTEK", "Orbbec", "Intel",
        "Luxonis", "Stereolabs", "Generic", "SICK"
    };

    for (const auto& v : expected_vendors) {
        auto models = reg.getModelsByVendor(v);
        TEST_ASSERT(!models.empty(), ("Vendor family missing: " + v).c_str());
        std::cout << "  ✓ Vendor " << v << ": " << models.size() << " model(s) indexed." << std::endl;
    }

    // -------------------------------------------------------------------------
    // 2. Generational Breakdown (Current, Previous, Legacy)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 2] Verifying Generational Lifecycle Breakdown..." << std::endl;
    auto current_models = reg.getModelsByStatus(ProductionStatus::CURRENT);
    auto previous_models = reg.getModelsByStatus(ProductionStatus::PREVIOUS);
    auto legacy_models = reg.getModelsByStatus(ProductionStatus::LEGACY);

    TEST_ASSERT(!current_models.empty(), "No CURRENT generation models registered");
    TEST_ASSERT(!previous_models.empty(), "No PREVIOUS generation models registered");
    TEST_ASSERT(!legacy_models.empty(), "No LEGACY generation models registered");

    std::cout << "  ✓ CURRENT generation models:  " << current_models.size() << std::endl;
    std::cout << "  ✓ PREVIOUS generation models: " << previous_models.size() << std::endl;
    std::cout << "  ✓ LEGACY generation models:   " << legacy_models.size() << std::endl;

    // -------------------------------------------------------------------------
    // 3. Ouster Rev8 Native RGB vs Rev7 vs Gen1 Legacy Compatibility
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 3] Verifying Ouster Rev8 Native RGB vs Rev7 vs Gen1 Legacy..." << std::endl;
    const auto* ouster_rev8 = reg.findModelById("ouster_os1_rev8_rgb");
    const auto* ouster_rev7 = reg.findModelById("ouster_os1_rev7");
    const auto* ouster_legacy = reg.findModelById("ouster_os1_gen1_legacy");

    TEST_ASSERT(ouster_rev8 != nullptr, "ouster_os1_rev8_rgb not found");
    TEST_ASSERT(ouster_rev7 != nullptr, "ouster_os1_rev7 not found");
    TEST_ASSERT(ouster_legacy != nullptr, "ouster_os1_gen1_legacy not found");

    // Critical distinction: Rev8 has native RGB, Rev7 does not
    TEST_ASSERT(ouster_rev8->features().has_rgb == true, "Ouster Rev8 must have native RGB flag set");
    TEST_ASSERT(ouster_rev7->features().has_rgb == false, "Ouster Rev7 must NOT have native RGB flag set");
    TEST_ASSERT(ouster_rev8->features().has_lidar == true, "Ouster Rev8 must have LiDAR");
    TEST_ASSERT(ouster_rev8->features().has_imu == true, "Ouster Rev8 must have IMU");

    // Firmware resolution test: FW 2.2 on legacy Ouster Gen1 hardware
    std::string adapter, sdk_ver;
    bool req_legacy = false;
    bool fw_ok = resolver.resolveFirmwareCompatibility(*ouster_legacy, "2.2.0", adapter, sdk_ver, req_legacy);
    TEST_ASSERT(fw_ok == true, "Legacy FW 2.2.0 must be supported without forcing upgrade");
    TEST_ASSERT(adapter == "OusterLegacyAdapter", "Legacy FW 2.2.0 must route to OusterLegacyAdapter");
    TEST_ASSERT(req_legacy == true, "req_legacy must be true for Gen1 FW 2.2.0");
    std::cout << "  ✓ Ouster Rev8 Native RGB: verified." << std::endl;
    std::cout << "  ✓ Ouster Rev7 non-RGB: verified." << std::endl;
    std::cout << "  ✓ Ouster FW 2.2 Legacy resolved cleanly to OusterLegacyAdapter without forced firmware upgrade." << std::endl;

    // -------------------------------------------------------------------------
    // 4. Unitree L1 vs L2 Official SDK Distinction
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 4] Verifying Unitree L1 vs L2 SDK Distinction..." << std::endl;
    const auto* unitree_l2 = reg.findModelById("unitree_l2");
    const auto* unitree_l1 = reg.findModelById("unitree_l1");

    TEST_ASSERT(unitree_l2 != nullptr, "unitree_l2 not found");
    TEST_ASSERT(unitree_l1 != nullptr, "unitree_l1 not found");
    TEST_ASSERT(unitree_l2->transport_type() == "ETHERNET", "Unitree L2 must support Ethernet");
    TEST_ASSERT(unitree_l1->transport_type() == "USB_SERIAL", "Unitree L1 uses Serial");
    TEST_ASSERT(unitree_l2->features().has_imu == true, "Unitree L2 exposes 6-axis IMU");
    TEST_ASSERT(unitree_l2->vendor_sdk().current_sdk.rfind("2.", 0) == 0, "Unitree L2 SDK version must be 2.x");
    std::cout << "  ✓ Unitree L1 & L2 official SDK distinctions verified." << std::endl;

    // -------------------------------------------------------------------------
    // 5. Innoviz vs Seyond Disambiguation
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 5] Verifying Innoviz vs Seyond (inno-lidar-sdk) Distinction..." << std::endl;
    const auto* innoviz = reg.findModelById("innoviz_two");
    const auto* seyond = reg.findModelById("seyond_falcon_k2");

    TEST_ASSERT(innoviz != nullptr, "innoviz_two not found");
    TEST_ASSERT(seyond != nullptr, "seyond_falcon_k2 not found");
    TEST_ASSERT(innoviz->vendor() == "Innoviz", "Innoviz vendor mismatch");
    TEST_ASSERT(seyond->vendor() == "Seyond", "Seyond vendor mismatch");
    TEST_ASSERT(seyond->vendor_sdk().family_name == "inno-lidar-sdk", "Seyond must use inno-lidar-sdk");
    TEST_ASSERT(innoviz->vendor_sdk().family_name != "inno-lidar-sdk", "Innoviz must NOT use inno-lidar-sdk");
    std::cout << "  ✓ Innoviz (Innoviz API) and Seyond (inno-lidar-sdk) cleanly disambiguated." << std::endl;

    // -------------------------------------------------------------------------
    // 6. 6-Tier Hierarchical Lookup Execution
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 6] Verifying 6-Tier Hierarchical Sensor Resolution..." << std::endl;
    
    // Case A: Tier 2 Known Model (RoboSense RS-Airy)
    HardwareInterrogationData hw_airy;
    hw_airy.vendor_name = "RoboSense";
    hw_airy.product_name = "RS-Airy";
    hw_airy.transport_type = "ETHERNET";
    hw_airy.primary_endpoint = "192.168.1.200:6699";
    hw_airy.has_verified_lidar = true;
    hw_airy.has_verified_imu = true;
    hw_airy.firmware_version = "1.2.0";

    auto res_airy = resolver.resolveHardware(hw_airy);
    TEST_ASSERT(res_airy.resolution_tier == ResolutionTier::TIER2_KNOWN_MODEL_DEFINITION, "RS-Airy must resolve to Tier 2");
    TEST_ASSERT(res_airy.is_capturable == true, "RS-Airy must be capturable");
    TEST_ASSERT(res_airy.recommended_backend == "FAST-LIO2", "RS-Airy must use FAST-LIO2");
    std::cout << "  ✓ Case A: RS-Airy resolved to Tier 2 (" << to_string(res_airy.resolution_tier) << ")." << std::endl;

    // Case B: Tier 4 Standard UVC Video Stream
    HardwareInterrogationData hw_uvc;
    hw_uvc.vendor_id = "0x9999"; // uncataloged vendor
    hw_uvc.product_id = "0x8888";
    hw_uvc.transport_type = "USB_UVC";
    hw_uvc.primary_endpoint = "/dev/video4";
    hw_uvc.has_verified_rgb = true;

    auto res_uvc = resolver.resolveHardware(hw_uvc);
    TEST_ASSERT(res_uvc.resolution_tier == ResolutionTier::TIER4_RECOGNIZED_STANDARD_TRANSPORT, "UVC must resolve to Tier 4");
    TEST_ASSERT(res_uvc.is_capturable == true, "UVC stream must be capturable");
    TEST_ASSERT(res_uvc.selected_adapter == "GenericUvcSensorAdapter", "UVC must select GenericUvcSensorAdapter");
    std::cout << "  ✓ Case B: Unknown UVC webcam resolved to Tier 4 (" << to_string(res_uvc.resolution_tier) << ")." << std::endl;

    // Case C: Tier 6 Unsupported Device (Seeed SenseCAP Watcher)
    HardwareInterrogationData hw_watcher;
    hw_watcher.vendor_id = "0x1a86";
    hw_watcher.product_id = "0x55d2";
    hw_watcher.transport_type = "USB_SERIAL";
    hw_watcher.primary_endpoint = "/dev/ttyACM0";
    hw_watcher.has_verified_rgb = false;
    hw_watcher.has_verified_depth = false;
    hw_watcher.has_verified_lidar = false;

    auto res_watcher = resolver.resolveHardware(hw_watcher);
    TEST_ASSERT(res_watcher.resolution_tier == ResolutionTier::TIER6_UNSUPPORTED_SPATIAL_CAPTURE, "Watcher must resolve to Tier 6 UNSUPPORTED");
    TEST_ASSERT(res_watcher.is_capturable == false, "Watcher must NOT be capturable");
    TEST_ASSERT(res_watcher.recommended_backend == "NONE", "Watcher backend must be NONE");
    TEST_ASSERT(res_watcher.slam_family == "UNKNOWN", "Watcher SLAM family must be UNKNOWN");
    std::cout << "  ✓ Case C: Seeed SenseCAP Watcher safely resolved to Tier 6 (" << to_string(res_watcher.resolution_tier) << ") [NONE/UNKNOWN]." << std::endl;

    // -------------------------------------------------------------------------
    // 7. Capability Matrix JSON Generation & Persistence
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 7] Verifying Automated Capability Matrix JSON Export..." << std::endl;
    nlohmann::json matrix = reg.generateCapabilityMatrixJson();
    TEST_ASSERT(matrix.contains("schema_version"), "Capability matrix must have schema_version");
    TEST_ASSERT(matrix.contains("ecosystem"), "Capability matrix must have ecosystem object");
    TEST_ASSERT(matrix["total_models"].get<int>() >= 15, "Capability matrix must contain >= 15 models");
    std::cout << "  ✓ Capability matrix automatically generated (" << matrix["total_models"] << " total models)." << std::endl;

    std::filesystem::create_directories("docs");
    std::ofstream ofs("docs/ecosystem_capability_matrix.json");
    if (ofs.is_open()) {
        ofs << matrix.dump(2) << std::endl;
        std::cout << "  ✓ Saved authoritative matrix to docs/ecosystem_capability_matrix.json" << std::endl;
    }

    // -------------------------------------------------------------------------
    // 8. Hierarchical Queries & Multi-Keyword Search
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 8] Verifying Hierarchical Filtering & Fast Search..." << std::endl;
    auto manufacturers = reg.getManufacturers();
    TEST_ASSERT(manufacturers.size() >= 16, "Expected >= 16 manufacturers in registry");
    
    auto ouster_types = reg.getSensorTypesForManufacturer("Ouster");
    TEST_ASSERT(!ouster_types.empty(), "Ouster must have sensor types registered");
    TEST_ASSERT(ouster_types[0] == "LiDAR", "Ouster primary sensor type must be LiDAR");

    auto orbbec_types = reg.getSensorTypesForManufacturer("Orbbec");
    TEST_ASSERT(!orbbec_types.empty(), "Orbbec must have sensor types registered");

    auto sick_types = reg.getSensorTypesForManufacturer("SICK");
    TEST_ASSERT(sick_types.size() >= 2, "SICK must have both LiDAR and 3D Vision types registered");

    auto search_airy = reg.searchModels("Airy");
    TEST_ASSERT(!search_airy.empty(), "Search for 'Airy' must find RoboSense RS-Airy");
    TEST_ASSERT(search_airy[0].model_id() == "robosense_airy", "Search result mismatch for Airy");

    auto search_gemini = reg.searchModels("Gemini");
    TEST_ASSERT(search_gemini.size() >= 3, "Search for 'Gemini' must find >= 3 Orbbec Gemini models");

    auto search_pandar = reg.searchModels("Pandar");
    TEST_ASSERT(search_pandar.size() >= 4, "Search for 'Pandar' must find >= 4 Hesai Pandar models");

    auto search_multiscan = reg.searchModels("multiScan");
    TEST_ASSERT(!search_multiscan.empty(), "Search for 'multiScan' must find SICK multiScan models");

    auto search_visionary = reg.searchModels("Visionary");
    TEST_ASSERT(!search_visionary.empty(), "Search for 'Visionary' must find SICK Visionary models");

    auto models_lidar = reg.getModels("Hesai", "LiDAR");
    TEST_ASSERT(!models_lidar.empty(), "Expected LiDAR models for Hesai");
    auto sick_models = reg.getModelsByVendor("SICK");
    TEST_ASSERT(sick_models.size() >= 16, "Expected >= 16 SICK models");

    std::cout << "  ✓ Hierarchical filtering (Manufacturer -> Sensor Type -> Model) verified." << std::endl;
    std::cout << "  ✓ SICK ecosystem verified: " << sick_models.size() << " models across " << sick_types.size() << " sensor types." << std::endl;
    std::cout << "  ✓ Fast multi-keyword search verified for Airy, Gemini, Pandar, multiScan, Visionary." << std::endl;

    // -------------------------------------------------------------------------
    // 9. Template Profile Seeding (Distinct Model Definition vs Profile Concept)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 9] Verifying Model Definition -> Template Profile Seeding..." << std::endl;
    const auto* hesai_xt32 = reg.findModelById("hesai_pandar_xt32");
    TEST_ASSERT(hesai_xt32 != nullptr, "hesai_pandar_xt32 not found");

    auto template_prof = hesai_xt32->seedDeviceProfile(true);
    TEST_ASSERT(template_prof.profile_id() == "hesai_pandar_xt32_template", "Template profile ID mismatch");
    TEST_ASSERT(template_prof.profile_status() == av::core::schemas::ProfileStatus::EXPERIMENTAL, "Template profile status must be EXPERIMENTAL");
    TEST_ASSERT(template_prof.manufacturer() == "Hesai", "Manufacturer mismatch");
    TEST_ASSERT(template_prof.model() == "PandarXT-32 High Precision", "Model mismatch");
    TEST_ASSERT(template_prof.provenance().source() == av::core::schemas::CalibrationSource::OFFICIAL_DOCUMENTATION, "Template calibration source must be OFFICIAL_DOCUMENTATION");
    TEST_ASSERT(template_prof.provenance().confidence() == 0.5, "Template confidence score must be 0.5");
    TEST_ASSERT(!template_prof.provenance().validated(), "Template scale must NOT be validated without physical test");
    TEST_ASSERT(template_prof.provenance().evidence().find("uninterrogated physical hardware") != std::string::npos, "Evidence must record uninterrogated hardware");

    std::cout << "  ✓ Template profile seeded with default nominal parameters & unverified status." << std::endl;
    std::cout << "  ✓ Dual profile concepts strictly preserved (Model Definition != Device Profile)." << std::endl;

    // -------------------------------------------------------------------------
    // 10. Livox Mid-360L Truthful Runtime Integration Ladder (M14.5B)
    // -------------------------------------------------------------------------
    std::cout << "\n[TEST 10] Verifying Livox Mid-360L Truthful Runtime Ladder..." << std::endl;
    const auto* mid360l = reg.findModelById("livox_mid_360l");
    TEST_ASSERT(mid360l != nullptr, "livox_mid_360l must be registered in catalog");
    TEST_ASSERT(mid360l->vendor() == "Livox", "Mid-360L vendor must be Livox");
    TEST_ASSERT(mid360l->sensor_type() == "LiDAR", "Mid-360L sensor type must be LiDAR");
    TEST_ASSERT(mid360l->av_support().validation_status == ValidationTier::DOCUMENTED, "Mid-360L must be tier DOCUMENTED (untested)");
    
    // Check 8-point runtime ladder: only A is verified, B is target, C-H are pending (false)
    const auto& rim = mid360l->runtime_integration();
    TEST_ASSERT(rim.catalog_knowledge_exists == true, "A. Catalog knowledge must exist");
    TEST_ASSERT(!rim.official_sdk_identified.empty(), "B. Official SDK identified");
    TEST_ASSERT(rim.sdk_integrated == false, "C. SDK integration must be pending (false)");
    TEST_ASSERT(rim.hardware_interrogation_implemented == false, "D. HW interrogation must be pending (false)");
    TEST_ASSERT(rim.runtime_adapter_implemented == false, "E. Streaming adapter must be pending (false)");
    TEST_ASSERT(rim.slam_backend_implemented == false, "F. SLAM backend must be pending (false)");
    TEST_ASSERT(rim.physically_tested == false, "G. Physical test must be pending (false)");
    TEST_ASSERT(rim.gui_validated == false, "H. GUI validation must be pending (false)");
    std::cout << "  ✓ Livox Mid-360L truthful ladder states verified (Catalog ✓, Runtime/Hardware pending)." << std::endl;

    std::cout << "\n>>> ALL M14.5 UNIT & QUALIFICATION TESTS PASSED! <<<" << std::endl;
    return 0;
}
