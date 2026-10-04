# AV Scan — Milestone 6: Modular Scanner Configuration & Multi-Sensor Concurrency Acceptance Report

**Artificial Vision, Inc.**  
**Document ID:** AV-M6-QUAL-001  
**Target Architecture:** Modular Scanner Configuration & $N$-Sensor Concurrency on One Scanner Node  
**Hardware Tested:**  
1. **RoboSense RS-Airy Mechanical LiDAR** (`robosense_airy_01`, S/N: `AIRY-2024-99812`)  
   - *SLAM Pipeline:* ROS 2 package `fast_livo`, executable `fastlivo_mapping` (`ros2 run fast_livo fastlivo_mapping`)  
   - *Orientation Status:* **PASS / FROZEN** (Physical orientation confirmed correct by user)  
2. **Orbbec Gemini 336L Active Stereo RGB-D Camera** (`orbbec_gemini_336l_01`, S/N: `CPC6463000XW`)  
   - *SLAM Pipeline:* RTAB-Map Active Stereo RGB-D Engine via Orbbec SDK  
   - *Orientation Status:* **FAIL** (Pending M5.5 Diagnostic Tool) | *RGB Status:* **FAIL** (Pending M5.5)  
3. **VITURE Luma Ultra XR Glasses** (`viture_luma_ultra_01`, S/N: `VITURE-35CA-1104`)  
   - *SLAM Pipeline:* Onboard 6-DoF VIO Engine via Carina XR SDK (`libcarina_vio.so`)  
   - *Orientation Status:* **FAIL** (Pending M5.5 Diagnostic Tool) | *RGB Status:* **FAIL** (Pending M5.5)  
**Host Platform:** NVIDIA Jetson Orin NX Developer Kit (16 GB, Linux 5.15.148-tegra, aarch64, Node ID: `scanar-01`)  
**Status:** **PASS / CLOSED — FROZEN** (Physical 3-Sensor Concurrent SLAM Validated on Hardware by User)  
**Date:** September 17, 2026 (Physical Acceptance: September 17, 2026, 22:30 EDT)  

---

## 1. Executive Summary & Architectural Scope

Milestone 6 resolves the foundational architecture for multi-sensor spatial computing on Jetson hardware:  
> **Can one AV Scan Scanner Node operate an arbitrary composition of physical sensors and their independent real SLAM pipelines simultaneously?**

### Key Principles Strictly Honored in M6:
- **Concurrency without Fusion:** All physical acquisition pipelines execute concurrently in isolated background daemon processes on the same Jetson host.
- **Strict Extrinsics Independence:** `extrinsic_status` is strictly `UNKNOWN`. There is **NO** CAD transform, **NO** sensor-to-sensor calibration, **NO** map registration, **NO** point cloud concatenation, and **NO** shared state estimator. Each map and trajectory is computed independently in its own sensor frame. Coordinate calibration and fusion belong exclusively to **M7** and **M8**.
- **Arbitrary Scanner Configuration Composition:** Rather than hardcoding fixed sensor pairs, AV Scan now provides a generic architecture allowing users to compose any combination of available `DeviceProfile` instances into a reusable named `ScannerConfig` via the GUI.
- **Data-Driven UI & Pipeline Execution:** Viewport switcher tabs, real-time bottom telemetry columns, capture initiation, and session persistence dynamically reflect the exact sensors selected in the active configuration. Changing the viewport never interrupts background SLAM.
- **Independent Session Persistence:** Every sensor in an active session is persisted in its own independent capture folder (`captures/cap_<ts>_<sensor_id>/`) with its own `map.pcd`, `trajectory.json`, `map_metadata.json`, and `capture.json`.
- **Cold Reopen Integrity:** Cold reopening any project inspects all session capture directories, restores independent sensor contexts, aggregates point counts, and enables immediate multi-sensor viewport inspection without data loss or fusion.

---

## 2. SLAM Engine Provenance & Execution Specification

To ensure total transparency and eliminate naming ambiguity, the exact packages, executables, and libraries powering each sensor backend are frozen as follows:

| Sensor | Subsystem | Executable / Library | Launch Mechanism / Protocol |
| :--- | :--- | :--- | :--- |
| **RoboSense RS-Airy** | LiDAR-Inertial Odometry | **Package:** `fast_livo`<br>**Executable:** `fastlivo_mapping` | `ros2 run fast_livo fastlivo_mapping`<br>Monitored via `core/drivers/airy/av_airy_daemon.py` over UDP `192.168.1.200:6699` |
| **Orbbec Gemini 336L** | Active Stereo RGB-D SLAM | **Library:** `libOrbbecSDK.so`<br>**Engine:** RTAB-Map RGB-D | Managed via `core/drivers/gemini/av_gemini_daemon.py` over USB 3.2 Gen 1 |
| **VITURE Luma Ultra** | Visual-Inertial Odometry | **Libraries:** `libcarina_vio.so`, `libglasses.so`<br>**Engine:** Carina XR 6-DoF VIO | Managed via `core/drivers/viture/av_viture_daemon.py` over USB 3 UVC/HID |

---

## 3. Generalized System Architecture

```
                                  scanar-01
                             ONE SCANNER NODE
                                    │
           ┌────────────────────────┼────────────────────────┐
           │                        │                        │
       ROBOSENSE AIRY        ORBBEC GEMINI 336L      VITURE LUMA ULTRA
           │                        │                        │
      LiDAR + IMU              RGB-D + IMU              Stereo + IMU
           │                        │                        │
       FAST-LIVO                RTAB-Map                 Carina VIO
  (fastlivo_mapping)        (libOrbbecSDK)           (libcarina_vio.so)
           │                        │                        │
      Trajectory A             Trajectory B             Trajectory C
     Point Cloud A            Point Cloud B            Point Cloud C
           │                        │                        │
           └──────────────── SAME SESSION ───────────────────┘

                          EXTRINSICS = UNKNOWN
                               NO FUSION
```

### 3.1 QmlBridge Dynamic Sensor Model
The previous hardcoded dual-sensor properties have been replaced by a generic runtime model:
```cpp
struct SensorRuntimeContext {
    std::string id;                  // "AIRY", "GEMINI", "VITURE"
    std::string instance_id;         // e.g. "robosense_airy_01"
    std::string name;                // "RoboSense Airy LiDAR"
    std::string short_name;          // "Airy"
    std::string icon;                // "📡" or "📷"
    std::string device_status;       // "CONNECTED", "DISCONNECTED"
    std::string acquisition_status;  // "STREAMING", "STOPPED"
    std::string tracking_status;     // "TRACKING_OK", "INITIALIZING"
    uint64_t point_count{0};
    double acquisition_rate{0.0};
    double slam_rate{0.0};
    std::vector<core::schemas::PointXYZI> map_points;
    std::vector<TrajectoryPose> trajectory;
};
```
- `activeSensors_`: Dynamic vector representing the active scanner configuration.
- `startCapture()`: Iterates over all active sensors with failure isolation (if any sensor fails during startup, all active pipelines abort cleanly to avoid partial captures).
- `stopCapture()`: Iterates over all active sensors, commands clean stop, and collects independent point clouds and trajectories.
- `saveCapture()`: Serializes a multi-sensor `ScannerNode` and writes separate capture directories under `captures/`.
- `setActiveDisplaySensor(sensorId)`: Seamlessly routes the selected sensor's point cloud and trajectory to the 3D viewport without affecting background SLAM threads.

---

## 4. UI Enhancements: Scanner Configuration Composition Dialog

1. **`[ ⚙️ Configure Sensors ]` Button:** Located next to the Scanner dropdown in the control panel.
2. **Modular Configuration Dialog:**
   - Dynamically loads all available physical `DeviceProfile` entries from the storage catalog (`RoboSense Airy`, `Orbbec Gemini 336L`, `VITURE Luma Ultra`).
   - Live multi-select checkboxes for arbitrary composition ($N=1, 2, 3$).
   - Text input for naming custom scanner configurations (e.g., `"Tri-Sensor Rig (Airy + Gemini + Luma)"`).
   - "Save & Activate Configuration" button: Serializes the new `ScannerConfig` to disk with `extrinsic_status = UNKNOWN` and immediately activates it.
3. **Data-Driven Viewport Switching:**
   - Top of 3D canvas renders dynamic tabs `[ 📡 Airy (X pts) ]`, `[ 📷 Gemini 336L (Y pts) ]`, `[ 📷 Luma Ultra (Z pts) ]`.
   - Viewport switching is instant and strictly non-destructive.
4. **Data-Driven Telemetry Bar:**
   - Bottom telemetry panel dynamically allocates columns for each sensor in the active configuration displaying real-time acquisition Hz, SLAM Hz, tracking status, and point counts.

---

## 5. Storage Hierarchy for Multi-Capture Sessions

Sessions configured with 3 sensors store artifacts side-by-side:
```
projects/<project_id>/sessions/<session_id>/
  scanner_node.json                  <-- 1 Scanner Node hosting 3 attached sensors
  session.json                       <-- Session metadata referencing all captures
  captures/
    cap_<ts>_airy/
      capture.json                   <-- RoboSense Airy metadata (Extrinsics: UNKNOWN)
      map.pcd                        <-- Independent mechanical LiDAR point cloud
      trajectory.json                <-- Independent FAST-LIVO trajectory
      map_metadata.json
    cap_<ts>_gemini/
      capture.json                   <-- Orbbec Gemini 336L metadata (Extrinsics: UNKNOWN)
      map.pcd                        <-- Independent RGB-D point cloud
      trajectory.json                <-- Independent RTAB-Map trajectory
      map_metadata.json
    cap_<ts>_viture/
      capture.json                   <-- VITURE Luma Ultra metadata (Extrinsics: UNKNOWN)
      map.pcd                        <-- Independent VIO landmark point cloud
      trajectory.json                <-- Independent Carina VIO trajectory
      map_metadata.json
```

---

## 6. Automated Qualification Results

The full test suite passes with 100% success rate:
```
Test project /home/scanar/av_scan/build
      Start  1: test_schemas .....................   Passed    0.00 sec
      Start  2: test_logging .....................   Passed    0.01 sec
      Start  3: test_storage .....................   Passed    0.00 sec
      Start  4: test_gui_bridge ..................   Passed    0.01 sec
      Start  5: test_m1_acceptance ...............   Passed    0.25 sec
      Start  6: test_m2_acceptance ...............   Passed    0.27 sec
      Start  7: test_airy_adapter ................   Passed    0.00 sec
      Start  8: test_m3_acceptance ...............   Passed    0.71 sec
      Start  9: test_gemini_adapter ..............   Passed    0.01 sec
      Start 10: test_m4_acceptance ...............   Passed    0.88 sec
      Start 11: test_viture_adapter ..............   Passed    0.01 sec
      Start 12: test_m5_acceptance ...............   Passed    5.81 sec
      Start 13: test_m5_5_acceptance .............   Passed    0.01 sec
      Start 14: test_m6_acceptance ...............   Passed    0.08 sec

100% tests passed out of 14 (Total Test time: 8.07 sec)
```

### Milestone 6 Acceptance Suite (`test_m6_acceptance`) Steps:
- **Step 1:** Verifying `ScannerNode` Model and Storage Engine Persistence — **PASSED**
- **Step 2:** Verifying Scanner Configuration Extrinsics Status — **PASSED** (`extrinsic_status` is strictly `UNKNOWN`)
- **Step 3:** Verifying Host System Telemetry (CPU & GPU Load) — **PASSED**
- **Step 4:** Verifying `QmlBridge` Multi-Sensor Operations — **PASSED**
- **Step 5:** Verifying Multi-Capture Storage Organization under One Session — **PASSED**
- **Step 6:** Verifying Cold Reopen of Dual-Sensor Session — **PASSED**
- **Step 7:** Verifying Arbitrary Composition of Device Profiles into Scanner Configuration — **PASSED** (Discovered 3 profiles, composed 3-sensor rig, verified schema)
- **Step 8:** Verifying 3-Sensor Independent Persistence and Cold Reopen — **PASSED** (Persisted 3 independent captures, cold reloaded all 3 maps and trajectories without fusion)

---

## 7. Operator Physical Validation & Resolution of Sensor-Display Switching

The physical validation of Milestone 6 was conducted on physical hardware by the operator with all 3 concurrent hardware sensors actively streaming on Jetson Orin NX node `scanar-01`.

### 7.1 Sensor Switcher Hit Responsiveness & Delegate Thrashing Resolution
During initial multi-sensor physical capture, switching between sensor viewports required multiple clicks or mashing because clicks were intermittently dropped.
- **Root Cause:** In `QmlBridge::updateLiveTelemetry()`, `emit activeSensorsChanged()` was being fired on every 100 ms telemetry tick. In QML, `Repeater { model: bridge.activeSensors }` responded by tearing down and rebuilding all button delegates 10 times a second. If mouse-down or release occurred as the delegate reset, Qt Quick cancelled the click event.
- **Resolution Implemented:**
  1. Removed `activeSensorsChanged` emissions from the 10 Hz telemetry loop; the active sensor configuration model remains permanently instantiated and stable throughout capture.
  2. Telemetry and point counts now update reactively through isolated property bindings (`airyPointsCaptured`, `geminiPointsCaptured`, `viturePointsCaptured`).
  3. Replaced standard QtQuick Controls `Button.onClicked` with a custom `Rectangle` + `MouseArea { onPressed: ... }`. Switching now triggers instantaneously upon mouse-down.
  4. Enlarged button hit area (28px height, generous padding), added hover styling, and added hand cursor (`Qt.PointingHandCursor`).
  5. Made the bottom telemetry cards also interactive, allowing sensor view switching by clicking either the top tab buttons or bottom sensor cards.
- **Retest Result:** Operator re-tested physically and confirmed: *"the buttons switch displays alot better now. i would say we now have a pass."*

### 7.2 Physical Multi-Sensor Acceptance Checklist
- [x] **Configure Sensors Workflow:** Compose, name, and activate custom 3-sensor rig (`Tri-Sensor Rig (Airy + Gemini + Luma)`). — **PASS**
- [x] **Hardware Concurrency:** Simultaneous real execution of FAST-LIVO (Airy UDP), RTAB-Map (Gemini USB3), and Carina VIO (Luma USB3). — **PASS**
- [x] **Independent SLAM:** All 3 pipelines maintain independent tracking states and point clouds under `extrinsic_status = UNKNOWN`. — **PASS**
- [x] **Real-Time Display Switching:** Responsive single-click switching between Airy, Gemini, and Luma without SLAM interruption. — **PASS**
- [x] **Persistence & Cold Reopen:** Clean stop, save, and cold reopen restoring all 3 captures independently. — **PASS**

**FINAL VERDICT: MILESTONE 6 — PASS / CLOSED & FROZEN.**

---

## 8. Milestone Sequencing Matrix & Next Steps

```
┌────────────────────────────────────────────────────────┐
│                   MILESTONE STATUS MATRIX              │
├────────┬────────────────────────────────┬──────────────┤
│ M1     │ Project & Workspace Management │ PASS         │
│ M2     │ UI Layout & Hardware Metrics   │ PASS         │
│ M3     │ RoboSense Airy LiDAR SLAM      │ PASS         │
│ M4     │ Orbbec Gemini 336L RGB-D SLAM  │ PASS         │
│ M5     │ VITURE Luma Ultra VIO          │ PASS         │
│ M5.5   │ Orientation & Real RGB Fidelity│ OPEN         │
│        │  ├── Airy Orientation         │ PASS / FROZEN│
│        │  ├── Gemini 336L Orientation   │ FAIL / OPEN  │
│        │  ├── Gemini 336L RGB Fidelity  │ FAIL / OPEN  │
│        │  ├── Luma Ultra Orientation    │ FAIL / OPEN  │
│        │  └── Luma Ultra RGB Fidelity   │ FAIL / OPEN  │
│ M6     │ Modular Multi-Sensor Concur.   │ PASS / CLOSED│
│ M7     │ Unknown Rigid Calibration      │ LOCKED       │
└────────┴────────────────────────────────┴──────────────┘
```

### Directives for Milestone 5.5:
1. **M6 Architecture Frozen:** Do not modify the generic $N$-sensor composition, storage schema, or GUI architecture unless an actual regression is found.
2. **Airy Orientation Frozen:** Do **NOT** modify the RoboSense RS-Airy canonical transform. It is physically validated and will serve as the ground-truth visual reference.
3. **M7 Strict Lock:** Under no circumstances should rigid extrinsics calibration or multi-sensor fusion (M7/M8) be started.
4. **Immediate Focus (M5.5):**
   - Address Gemini 336L canonical orientation correction and real RGB point cloud colorization.
   - Address VITURE Luma Ultra canonical orientation correction and real RGB point cloud colorization.
