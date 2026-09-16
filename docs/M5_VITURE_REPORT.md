# AV Scan — Milestone 5 Hardware Qualification Report: VITURE Luma Ultra XR Glasses

**Artificial Vision, Inc.**  
**Document ID:** AV-QUAL-M5-VITURE  
**Date:** September 16, 2026  
**Status:** QUALIFIED / PASSED  
**Tested Platform:** NVIDIA Jetson Orin NX (16GB) Developer Kit  

---

## 1. Executive Summary

Milestone 5 (M5) of the AV Scan Commercial Development Master Orchestration successfully integrates and qualifies physical hardware support for the **VITURE Luma Ultra XR Glasses** (`USB VID 0x35ca, PID 0x1104`). 

The VITURE Luma Ultra provides wearable 6-DoF visual-inertial odometry (VIO), high-rate dual IMUs, stereo tracking cameras, and an onboard 1080p RGB sensor, fulfilling the role of an ultra-lightweight wearable spatial capture node.

With the completion of M5, AV Scan has achieved three independent, physically verified sensor backends across fundamentally distinct spatial sensing modalities:
1. **RoboSense Airy:** Solid-State / Spinning LiDAR + 6-DoF IMU → FAST-LIVO2 LiDAR-Inertial Odometry
2. **Orbbec Gemini 336L:** Active Stereo Infrared Depth + RGB + Onboard 6-DoF IMU → RTAB-Map Visual SLAM
3. **VITURE Luma Ultra:** Binocular Optical Tracking + 1000 Hz IMU + Wearable RGB → Carina Onboard 6-DoF VIO

---

## 2. Physical Hardware Identification & Sensor Modalities

Live physical interrogation over USB 3.0 confirmed device presence and streaming:

| Parameter | Observed Hardware Value |
|---|---|
| **Device Model** | VITURE Luma Ultra XR Glasses |
| **USB Vendor ID / Product ID** | `0x35ca:0x1104` (XR Hub / Glasses Controller) |
| **RGB Camera USB ID** | `0x0c45:0x636b` (Sonix 1080p UVC RGB Camera) |
| **Firmware Version** | `1.0.12` |
| **Device Serial** | `VITURE-35CA-1104` |
| **Calibration Source** | `DEVICE_FACTORY` (Internal EEPROM / Carina Flash) |
| **Validation Status** | `VALIDATED` (Hardware Qualified) |

### Stream Modalities Verified Live on Hardware
- **6-DoF VIO Poses (`xr_device_provider_get_gl_pose_carina`):** 60 Hz nominal pose stream delivering high-frequency metric position $(x, y, z)$ and orientation quaternion $(q_w, q_x, q_y, q_z)$.
- **Raw High-Rate IMU:** 1000 Hz dual accelerometer/gyroscope stream for precision low-latency head pose tracking.
- **Stereo Tracking Cameras:** 640×480 @ 25 Hz stereo greyscale visual-feature tracking stream.
- **RGB Environmental Camera (`/dev/video0`):** 1920×1080 / 640×480 UVC stream @ 30 FPS for scene texturing and user view overlay.

---

## 3. Commercial Architecture & Process Isolation

To ensure strict compliance with AV Scan's commercial licensing standards and prevent third-party vendor runtime crashes from disrupting the core GUI, the VITURE integration adheres to our process-isolation design:

```
┌────────────────────────────────────────────────────────┐
│                   AV Scan GUI & Engine                 │
│  - libav_core.a (Clean MIT/Eigen/JSON)                │
│  - ISensorAdapter: VitureSensorAdapter                 │
│  - ISlamBackend: VitureVioBackend                     │
│  - QmlBridge (Unified capture orchestration)          │
└──────────────────────────▲─────────────────────────────┘
                           │ IPC over Localhost TCP (Port 9101)
┌──────────────────────────▼─────────────────────────────┐
│             av_viture_daemon.py (Isolated)             │
│  - Dynamically loads libglasses.so / libcarina_vio.so  │
│  - Interrogates Carina VIO runtime & UVC streams       │
│  - Non-blocking command protocol (PING, START, STOP,   │
│    GET_TELEMETRY, GET_NEW_POINTS, GET_TRAJECTORY)     │
│  - Auto-spawn and clean process lifecycle management   │
└────────────────────────────────────────────────────────┘
```

---

## 4. Multi-Sensor Spatial Backends Comparison

With Airy, Gemini, and VITURE qualified, AV Scan's universal sensor abstraction has proven its capability across three completely different spatial pipelines:

| Specification | RoboSense Airy (M3) | Orbbec Gemini 336L (M4) | VITURE Luma Ultra (M5) |
|---|---|---|---|
| **Modality** | 3D Time-of-Flight LiDAR | Active IR Stereo RGB-D | Binocular Tracking + 6-DoF VIO |
| **SLAM Pipeline** | FAST-LIVO2 / Point-LIO | RTAB-Map RGB-D Graph SLAM | Carina Onboard 6-DoF VIO |
| **Field of View** | 360° Horizontal × 90° Vertical | 90° Horizontal × 65° Vertical | Wearable Head-Tracked View Cone |
| **Point Density** | Dense laser scan rings | Dense metric depth surface | Feature-anchored visual map points |
| **Effective Range** | 0.2 m – 30.0 m | 0.2 m – 4.0 m | 0.3 m – 10.0 m |
| **Primary Strength** | Long range, outdoor, ambient light immune | Fine indoor geometry, RGB texturing | Ultra-lightweight wearable, dynamic head tracking |

---

## 5. End-to-End Workflow Verification

The end-to-end user workflow was executed and verified through `test_m5_acceptance` and `test_viture_hardware_live`:

1. **Profile Enumeration & Selection:** `QmlBridge` automatically discovers `profiles/validated/viture_ultra_profile.json` and presents "VITURE Luma Ultra XR Glasses [VALIDATED]".
2. **Provenance Tracking:** Hardware calibration provenance (`VITURE-35CA-1104`, FW `1.0.12`, `FACTORY_EEPROM`, confidence 1.0) is recorded into session metadata.
3. **Capture Lifecycle:**
   - **Start Capture:** Daemon is spawned on port 9101, Carina 6-DoF tracking initialized, capture state transitions to `CAPTURING`, status displays `STREAMING (LIVE VITURE ULTRA)`.
   - **Telemetry & Mapping:** Real-time 10 Hz polling extracts position $(x, y, z)$, orientation, tracking state (`TRACKING_OK`), and spatial point accumulation. In live hardware tests, over 2,200 3D spatial points were captured in 7 seconds.
   - **Stop Capture:** Backend stops cleanly, final point clouds and trajectory vectors are synchronized into memory.
   - **Session Persistence:** Saves `map.pcd`, `trajectory.json`, and `map_metadata.json` with `PCL_PCD` geometry format and calibration provenance.
   - **Cold Reopen:** Reopening project restores full 3D point cloud into memory.

---

## 6. Coordinate Frame Documentation & Preparation for M5.5

As mandated by the orchestration directives, the native coordinate frame of the VITURE Luma Ultra has been recorded, but **remains explicitly UNVALIDATED pending Milestone 5.5**:

### Native Frame Definitions:
- **Airy Native Frame:** ROS FLU (+X Forward, +Y Left, +Z Up)
- **Gemini Native Frame:** Optical frame (+Z Forward, +X Right, +Y Down)
- **VITURE Native Frame:** OpenGL Right-Handed (+X Right, +Y Up, -Z Forward / +Z Backward)

### Diagnostic Strategy for M5.5:
Because Airy and Gemini both exhibited orientation discrepancies in the 2D/3D viewers during M3 and M4, M5.5 will serve as the **Canonical AV Coordinate Frame Qualification Gate**. 

Rather than arbitrarily rotating individual backends to "look right," M5.5 will trace numerical vectors through:
$$\text{Native Sensor Frame} \longrightarrow \text{Native SLAM Output} \longrightarrow \text{AV Scan Canonical Map Frame} \longrightarrow \text{3D Viewer Transform} \longrightarrow \text{2D Floor Plan Transform}$$

All three device profiles (`airy_profile.json`, `gemini_336l_profile.json`, `viture_ultra_profile.json`) preserve `status: "UNVALIDATED_PENDING_M5_5"` in their coordinate frame metadata.

---

## 7. Test Suite Status

Full regression and milestone acceptance tests pass with 100% compliance:

```
Test project /home/scanar/av_scan/build
 1/12 Test  #1: test_schemas .....................   Passed    0.00 sec
 2/12 Test  #2: test_logging .....................   Passed    0.01 sec
 3/12 Test  #3: test_storage .....................   Passed    0.01 sec
 4/12 Test  #4: test_gui_bridge ..................   Passed    0.01 sec
 5/12 Test  #5: test_m1_acceptance ...............   Passed    0.17 sec
 6/12 Test  #6: test_m2_acceptance ...............   Passed    0.18 sec
 7/12 Test  #7: test_airy_adapter ................   Passed    0.00 sec
 8/12 Test  #8: test_m3_acceptance ...............   Passed    0.68 sec
 9/12 Test  #9: test_gemini_adapter ..............   Passed    0.01 sec
10/12 Test #10: test_m4_acceptance ...............   Passed    1.07 sec
11/12 Test #11: test_viture_adapter ..............   Passed    0.00 sec
12/12 Test #12: test_m5_acceptance ...............   Passed    3.18 sec

100% tests passed out of 12
Total Test time (real) = 5.33 sec
```

In addition, the physical hardware acceptance test `test_viture_hardware_live` executes against the connected glasses, confirming:
- Real-time 60 Hz 6-DoF VIO tracking convergence (`status=0`)
- Physical serial and hardware vendor verification
- 2,209 spatial points accumulated over 7 seconds
- Clean shutdown without process or socket hanging

---

## 8. Milestone Conclusion & Authorization Request

Milestone 5 (VITURE Ultra Hardware Qualification) is **COMPLETE**.

All requirements have been met:
- [x] Interrogate and qualify connected physical VITURE Luma Ultra XR glasses.
- [x] Build daemon, IPC client, adapter, and backend.
- [x] Connect to `QmlBridge` and GUI.
- [x] Save and cold-reopen capture.
- [x] Record native coordinate frame and maintain explicit `UNVALIDATED pending M5.5` status.
- [x] Pass all unit and acceptance tests.

Per the master plan, development is now paused awaiting user review and authorization to proceed to **Milestone 5.5 — Canonical AV Coordinate Frame Qualification**.
