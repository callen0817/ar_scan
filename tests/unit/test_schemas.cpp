#include <cassert>
#include <iostream>
#include "core/schemas/types.hpp"
#include "core/schemas/calibration_provenance.hpp"
#include "core/schemas/device_profile.hpp"
#include "core/schemas/scanner_config.hpp"
#include "core/schemas/sensor_instance.hpp"
#include "core/schemas/capture.hpp"
#include "core/schemas/scanner_node.hpp"
#include "core/schemas/session.hpp"
#include "core/schemas/project.hpp"
#include "core/schemas/map_metadata.hpp"

using namespace av::core::schemas;

void testCalibrationProvenance() {
    std::cout << "[TEST] CalibrationProvenance round-trip..." << std::endl;
    CalibrationProvenance prov("TEST_CALIB_VAL", CalibrationSource::DEVICE_FACTORY, 0.98, "SN123", "v1.0", "Factory jig", true);
    auto j = prov.to_json();
    assert(j["value"] == "TEST_CALIB_VAL");
    assert(j["source"] == "DEVICE_FACTORY");
    assert(j["confidence"] == 0.98);
    assert(j["device_serial"] == "SN123");
    assert(j["validated"] == true);

    auto recovered = CalibrationProvenance::from_json(j);
    assert(recovered.value() == prov.value());
    assert(recovered.source() == prov.source());
    assert(recovered.confidence() == prov.confidence());
    assert(recovered.device_serial() == prov.device_serial());
    assert(recovered.validated() == prov.validated());
    std::cout << "  -> PASS" << std::endl;
}

void testDeviceProfile() {
    std::cout << "[TEST] DeviceProfile round-trip..." << std::endl;
    DeviceProfile prof;
    prof.set_profile_id("test_lidar_01");
    prof.set_manufacturer("TestCorp");
    prof.set_model("Lidar-32");
    prof.set_device_family("LIDAR");
    prof.set_profile_status(ProfileStatus::VALIDATED);
    prof.set_slam_family("LIO");
    prof.set_slam_backend("FAST-LIO2");

    StreamInfo stream;
    stream.name = "points";
    stream.type = "POINT_CLOUD";
    stream.format = "XYZI";
    stream.nominal_rate_hz = 10.0;
    prof.set_streams({stream});

    auto j = prof.to_json();
    auto recovered = DeviceProfile::from_json(j);
    assert(recovered.profile_id() == "test_lidar_01");
    assert(recovered.manufacturer() == "TestCorp");
    assert(recovered.model() == "Lidar-32");
    assert(recovered.profile_status() == ProfileStatus::VALIDATED);
    assert(recovered.slam_backend() == "FAST-LIO2");
    assert(recovered.streams().size() == 1);
    assert(recovered.streams()[0].name == "points");
    std::cout << "  -> PASS" << std::endl;
}

void testScannerConfig() {
    std::cout << "[TEST] ScannerConfig round-trip..." << std::endl;
    ScannerConfig cfg("cfg_01", "Dual Sensor", "node_01", SensorRelationshipType::RIGID);
    cfg.add_sensor_instance("sensor_a");
    cfg.add_sensor_instance("sensor_b");
    cfg.set_extrinsic_status(ExtrinsicStatus::UNKNOWN);

    Transform3D tf;
    tf.translation = {0.1, 0.2, 0.3};
    tf.rotation = {0.0, 0.0, 0.0, 1.0};
    cfg.set_relative_transform("sensor_a_to_b", tf);

    auto j = cfg.to_json();
    auto recovered = ScannerConfig::from_json(j);
    assert(recovered.config_id() == "cfg_01");
    assert(recovered.name() == "Dual Sensor");
    assert(recovered.sensor_instances().size() == 2);
    assert(recovered.extrinsic_status() == ExtrinsicStatus::UNKNOWN);
    assert(recovered.relative_transforms().count("sensor_a_to_b") == 1);
    assert(recovered.relative_transforms().at("sensor_a_to_b").translation.x == 0.1);
    std::cout << "  -> PASS" << std::endl;
}

void testSensorInstance() {
    std::cout << "[TEST] SensorInstance round-trip..." << std::endl;
    SensorInstance inst("inst_01", "node_01", "profile_lidar", "SN9876", "192.168.1.200:2368", "FRONT");
    inst.set_status(SensorStatus::CONNECTED);

    auto j = inst.to_json();
    auto recovered = SensorInstance::from_json(j);
    assert(recovered.instance_id() == "inst_01");
    assert(recovered.scanner_node_id() == "node_01");
    assert(recovered.device_profile_id() == "profile_lidar");
    assert(recovered.serial_number() == "SN9876");
    assert(recovered.status() == SensorStatus::CONNECTED);
    assert(recovered.mount_label() == "FRONT");
    std::cout << "  -> PASS" << std::endl;
}

void testCapture() {
    std::cout << "[TEST] Capture round-trip..." << std::endl;
    Capture cap("cap_01", "sess_01", "inst_01", 1000000);
    cap.set_end_timestamp_ns(2000000);
    cap.set_observation_count(500);
    cap.set_raw_data_path("/data/cap_01.pcd");

    auto j = cap.to_json();
    auto recovered = Capture::from_json(j);
    assert(recovered.capture_id() == "cap_01");
    assert(recovered.session_id() == "sess_01");
    assert(recovered.sensor_instance_id() == "inst_01");
    assert(recovered.start_timestamp_ns() == 1000000);
    assert(recovered.end_timestamp_ns() == 2000000);
    assert(recovered.observation_count() == 500);
    assert(recovered.raw_data_path() == "/data/cap_01.pcd");
    std::cout << "  -> PASS" << std::endl;
}

void testScannerNode() {
    std::cout << "[TEST] ScannerNode round-trip..." << std::endl;
    ScannerNode node("node_jetson", "scanar-01", "JETSON_LINUX", "192.168.1.10", "PRIMARY_ORCHESTRATOR");
    SensorInstance s("s1", "node_jetson", "p1");
    node.add_sensor(s);

    auto j = node.to_json();
    auto recovered = ScannerNode::from_json(j);
    assert(recovered.node_id() == "node_jetson");
    assert(recovered.hostname() == "scanar-01");
    assert(recovered.platform() == "JETSON_LINUX");
    assert(recovered.attached_sensors().size() == 1);
    std::cout << "  -> PASS" << std::endl;
}

void testSession() {
    std::cout << "[TEST] Session round-trip..." << std::endl;
    Session sess("sess_100", "proj_01", "Morning Scan", 1234567);
    sess.add_scanner_node("node_jetson");
    Capture cap("cap_1", "sess_100", "sensor_1");
    sess.add_capture(cap);

    auto j = sess.to_json();
    auto recovered = Session::from_json(j);
    assert(recovered.session_id() == "sess_100");
    assert(recovered.project_id() == "proj_01");
    assert(recovered.name() == "Morning Scan");
    assert(recovered.scanner_nodes().size() == 1);
    assert(recovered.captures().size() == 1);
    std::cout << "  -> PASS" << std::endl;
}

void testProject() {
    std::cout << "[TEST] Project round-trip..." << std::endl;
    Project proj("proj_001", "Site A Survey", "/data/projects/proj_001", "Initial project test");
    proj.add_session("sess_100");
    proj.set_created_at_ns(999999);

    auto j = proj.to_json();
    auto recovered = Project::from_json(j);
    assert(recovered.project_id() == "proj_001");
    assert(recovered.name() == "Site A Survey");
    assert(recovered.sessions().size() == 1);
    assert(recovered.sessions()[0] == "sess_100");
    std::cout << "  -> PASS" << std::endl;
}

void testMapMetadata() {
    std::cout << "[TEST] MapMetadata round-trip..." << std::endl;
    MapMetadata meta("map_01", "sess_100", "sensor_airy", "node_jetson");
    meta.set_timestamp_ns(55555);
    meta.set_point_count(1048576);
    meta.set_tracking_state(TrackingState::TRACKING_OK);

    Pose3D pose;
    pose.timestamp_ns = 55555;
    pose.position = {1.0, 2.0, 3.0};
    pose.orientation = {0.0, 0.0, 0.0, 1.0};
    meta.set_current_pose(pose);
    meta.add_trajectory_pose(pose);

    auto j = meta.to_json();
    auto recovered = MapMetadata::from_json(j);
    assert(recovered.map_id() == "map_01");
    assert(recovered.session_id() == "sess_100");
    assert(recovered.point_count() == 1048576);
    assert(recovered.tracking_state() == TrackingState::TRACKING_OK);
    assert(recovered.current_pose().position.x == 1.0);
    assert(recovered.trajectory().size() == 1);
    std::cout << "  -> PASS" << std::endl;
}

void testVersionException() {
    std::cout << "[TEST] Schema version rejection for future version 999..." << std::endl;
    nlohmann::json invalid_version_json = {
        {"schema_version", 999},
        {"project_id", "future_proj"}
    };
    bool caught = false;
    try {
        Project::from_json(invalid_version_json);
    } catch (const SchemaVersionException& ex) {
        caught = true;
        assert(ex.found_version() == 999);
        assert(ex.expected_version() == CURRENT_SCHEMA_VERSION);
    }
    assert(caught);
    (void)caught;
    std::cout << "  -> PASS" << std::endl;
}

int main() {
    std::cout << "=== Running Schema Unit Tests ===" << std::endl;
    testCalibrationProvenance();
    testDeviceProfile();
    testScannerConfig();
    testSensorInstance();
    testCapture();
    testScannerNode();
    testSession();
    testProject();
    testMapMetadata();
    testVersionException();
    std::cout << "=== All 10 Schema Tests PASSED ===" << std::endl;
    return 0;
}
