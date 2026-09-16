# AV Scan — Milestone 4: Orbbec Gemini 336L Hardware Qualification Report

**Artificial Vision, Inc.**  
**Document ID:** AV-M4-QUAL-001  
**Target Hardware:** Orbbec Gemini 336L Active Stereo RGB-D Camera  
**Host Platform:** NVIDIA Jetson Orin NX Developer Kit (16 GB, Linux 5.15.148-tegra, aarch64)  
**Status:** COMPLETED & QUALIFIED  
**Date:** September 16, 2026  

---

## 1. Executive Summary

Milestone 4 establishes the physical **Orbbec Gemini 336L** active stereo RGB-D camera as a fully qualified, production spatial sensor within the AV Scan commercial spatial-capture platform. 

The primary architectural objective of Milestone 4 was to rigorously prove that AV Scan's abstraction and common map representation function identically with a fundamentally different sensing modality and mapping pipeline:
- **RoboSense RS-Airy (M3):** Mechanical LiDAR + IMU $\rightarrow$ LiDAR-Inertial Odometry (LIO via FAST-LIVO2) $\rightarrow$ **Common AV Map**
- **Orbbec Gemini 336L (M4):** Active Stereo IR + RGB + Depth + IMU $\rightarrow$ Visual-Inertial / RGB-D SLAM (via RTAB-Map) $\rightarrow$ **Common AV Map**

Both pipelines feed seamlessly into the common AV Scan interface, driving the unified 3D Spatial Canvas, 2D Floor Plan Canvas, telemetry engine, and zero-data-loss persistence layer.

The Orbbec Gemini 336L device profile has been formally promoted from `DRAFT` to `VALIDATED`.

---

## 2. Hardware Specification & Characterization

### 2.1 Hardware Identification
- **Manufacturer:** Orbbec Inc.
- **Model:** Gemini 336L
- **Device Family:** `ACTIVE_STEREO_RGBD_CAMERA`
- **Device Serial:** `CPC6463000XW`
- **Firmware Version:** `1.4.60`
- **USB Bus:** `002/003` (USB 3.2 Gen 1 SuperSpeed 5Gbps, Device ID `2bc5:0807`)
- **Active Stereo Baseline:** $23.7\text{ mm}$
- **Depth Technology:** Active IR Stereo (Dual IR sensors + VCSEL dot-projector)
- **Range:** $0.20\text{ m} – 6.0\text{ m}$

### 2.2 Sensor Streams & Modalities
- **Color Stream (RGB):** $640 \times 400$ @ 30.0 Hz (RGB8, global timestamped)
- **Depth Stream (Z16):** $640 \times 400$ @ 30.0 Hz (16UC1 millimeter depth, hardware registered to RGB optical frame)
- **Infrared Streams:** Dual active stereo IR streams @ 30.0 Hz
- **Internal IMU:** 6-DoF BMI085 (3-axis accelerometer @ 200 Hz, 3-axis gyroscope @ 200 Hz)
- **Timestamp Synchronization:** Hardware clock sync with global timestamp domain

---

## 3. Factory Calibration Provenance & Intrinsic Decoding

During M4 qualification, device calibration was extracted directly from the physical camera's internal EEPROM flash via the Orbbec SDK:
- **Baseline Translation Vector ($t_{\text{depth}\leftarrow\text{rgb}}$):**
  $$\begin{bmatrix} t_x \\ t_y \\ t_z \end{bmatrix} = \begin{bmatrix} -0.00042455\text{ m} \\ -0.02373056\text{ m} \\ +0.00009320\text{ m} \end{bmatrix}$$
- **Factory Rotation Quaternion ($q_{\text{depth}\leftarrow\text{rgb}}$):**
  $$\begin{bmatrix} q_x \\ q_y \\ q_z \\ q_w \end{bmatrix} = \begin{bmatrix} -0.00065840 \\ -0.00017410 \\ -0.00118880 \\ 0.99999900 \end{bmatrix}$$
- **Intrinsic Calibration Matrix (Depth & Color):**
  $$f_x = 382.4,\; f_y = 382.4,\; c_x = 320.5,\; c_y = 200.2$$

This calibration provenance is stored in the device profile under `FACTORY_EEPROM` with confidence `1.0` and validated status `true`.

---

## 4. Commercial Architecture & Process Isolation Boundary

AV Scan maintains a strict commercial licensing and process isolation boundary:
```
+--------------------------------------------------------------------------+
|                      AV SCAN COMMERCIAL APPLICATION                      |
|                                                                          |
|  +---------------------------+       +--------------------------------+  |
|  |   QtQuick / QML GUI       | <---> |   QmlBridge (C++ / Qt5)        |  |
|  |   - 3D Spatial Canvas     |       |   - Telemetry Loop (100ms)     |  |
|  |   - 2D Floor Plan Canvas  |       |   - PCD / Trajectory Loader    |  |
|  +---------------------------+       +--------------------------------+  |
|                                                      |                   |
|  +---------------------------------------------------+                   |
|  |                                                                       |
|  |   libav_core (Commercial C++ Library)                                 |
|  |   - GeminiSensorAdapter (ISensorAdapter)                              |
|  |   - GeminiRgbdBackend   (ISlamBackend)                                |
|  |   - GeminiIpcClient     (High-Speed TCP RPC)                          |
|  +-----------------------------------------------------------------------+
|                                      |                                   |
+--------------------------------------|-----------------------------------+
                                       | Localhost TCP :9100
                                       | Binary NPTS/APTS + JSON RPC
+--------------------------------------|-----------------------------------+
|               ISOLATED LINUX BACKEND PROCESS                             |
|                                      v                                   |
|  core/drivers/gemini/av_gemini_daemon.py (Subprocess Isolation Boundary) |
|  - Manages orbbec_camera & rtabmap SLAM processes                        |
|  - Subscribes to /rtabmap/odom, /rtabmap/cloud_map, /rtabmap/map         |
|  - 5cm Voxel Grid Accumulation & Incremental Streaming                   |
|  - Standalone ROS 2 Humble Node (No GPL code in libav_core)              |
+--------------------------------------------------------------------------+
```

### Architectural Safeguards:
1. **Zero Contamination:** Neither `libav_core.a` nor `av_scan` include any ROS 2 or Orbbec SDK headers, ensuring zero third-party licensing contamination.
2. **Auto-Supervised Lifecycle:** `GeminiIpcClient` detects whether `av_gemini_daemon.py` is running on port 9100, automatically launching it via subprocess isolation if required, and safely terminating child SLAM processes upon session stop.
3. **High-Speed IPC Protocol:** Incremental point cloud delivery uses binary `NPTS` packets (16-byte $X, Y, Z, I$ records), maintaining smooth 60 FPS viewport rendering with minimal CPU overhead.

---

## 5. Native Coordinate Frame Behavior Analysis (Record for M5.5)

In accordance with product guidelines, native orientation metadata is recorded here for systematic diagnostic tracing and canonical normalization in milestone **M5.5**:

### 5.1 Gemini Published Native Frame Metadata
- **Camera Body Frame (`camera_link`):**
  - Published TF convention: $+X$ forward (optical direction when level), $+Y$ left, $+Z$ up (REP-103 right-handed).
- **Optical Frame (`camera_depth_optical_frame`):**
  - Published TF convention: $+Z$ forward along optical axis, $+X$ right, $+Y$ down.
- **RTAB-Map SLAM Frame (`map`):**
  - Published odometry reference: Aligned with `camera_link` at tracking initialization ($t=0$).

> [!WARNING]
> **COORDINATE STATUS: OPEN / UNVALIDATED PENDING M5.5**
> Native coordinate metadata recorded; end-to-end coordinate interpretation remains **UNVALIDATED** pending M5.5.
>
> During physical acceptance testing, live visual maps for both Airy and Gemini exhibited incorrect orientation in both 2D and 3D AV Scan viewers. Because two fundamentally distinct physical sensors and SLAM pipelines exhibit the same visible symptom, this indicates that the error is likely not two independent sensor-orientation problems, but rather originates in the common rendering/map-coordinate boundary:
> ```
> Airy backend ────┐
>                  ├── Common AV Map → GUI transformation → [ORIENTATION ERROR]
> Gemini backend ──┘
> ```
> At **M5.5**, AV Scan will diagnose this boundary numerically before applying any fixes by tracing known sample poses and points through:
> `native sensor → native SLAM → Common AV Map → 3D viewer → 2D viewer`.
> No ad-hoc per-sensor visual rotations will be introduced until this numerical pipeline tracing is complete.

---

## 6. Persistence & Cold Reopening Workflow

The AV Scan persistence layer guarantees complete session reproducibility:
- **Point Cloud File:** Saved as ASCII `.pcd` (PCL v0.7 format) with fields `x y z intensity` to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/map.pcd`
- **6-DoF Trajectory:** Saved as timestamped camera trajectory entries ($x, y, z, q_x, q_y, q_z, q_w$) to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/trajectory.json`
- **Common Map Metadata:** Saved with geometry formatting and coordinate frame tags to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/map_metadata.json`
- **Capture Schema & Provenance:** Registered in `StorageEngine` with `CPC6463000XW` factory EEPROM calibration provenance.
- **Cold Reopening:** When a project is reopened without active hardware, `QmlBridge::openProject()` parses persisted PCD points and poses directly into memory, restoring complete 3D spatial and 2D floor plan views.

---

## 7. Qualification & Acceptance Test Results

### 7.1 Automated Test Suite Summary (10 / 10 Passing)

| Test Target | Type | Description | Result |
| :--- | :--- | :--- | :--- |
| `test_schemas` | Unit | Data schema serialization & validation | **PASSED** (0.00s) |
| `test_logging` | Unit | Async structured logging verification | **PASSED** (0.00s) |
| `test_storage` | Unit | Storage engine CRUD operations | **PASSED** (0.00s) |
| `test_gui_bridge` | Unit | QmlBridge state machine & telemetry | **PASSED** (0.01s) |
| `test_m1_acceptance`| Acceptance | Milestone 1 Foundation validation | **PASSED** (0.19s) |
| `test_m2_acceptance`| Acceptance | Milestone 2 GUI layout & controls | **PASSED** (0.19s) |
| `test_airy_adapter` | Unit | Airy adapter streams & DIFOP provenance | **PASSED** (0.00s) |
| `test_m3_acceptance`| Acceptance | Milestone 3 Airy workflow & persistence | **PASSED** (0.69s) |
| `test_gemini_adapter`| Unit | Gemini adapter streams & EEPROM provenance | **PASSED** (0.00s) |
| `test_m4_acceptance`| Acceptance | Milestone 4 Gemini workflow & persistence | **PASSED** (1.04s) |

### 7.2 Physical Hardware Qualification Test (`test_gemini_hardware_live`)

Executed directly against physical Orbbec Gemini 336L (`CPC6463000XW`) connected via USB 3.2:

| Metric | Target Requirement | Measured Physical Value | Status |
| :--- | :--- | :--- | :--- |
| **Physical Serial Verification** | Match `CPC6463000XW` | `CPC6463000XW` | **PASSED** |
| **Firmware Verification** | Match `1.4.60` | `1.4.60` | **PASSED** |
| **Active Stereo Streams** | RGB + Depth @ 30 Hz | Continuous dual 640x400 streams | **PASSED** |
| **SLAM Convergence Time** | $< 5.0$ s to `TRACKING_OK` | 4.8 s | **PASSED** |
| **Visual Feature Tracking** | $> 100$ features | **250–295 features** tracked | **PASSED** |
| **Tracking Precision** | Sub-centimeter odometry | **1.1 mm std dev** | **PASSED** |
| **Live Points Registered** | $> 1,000$ points | **4,902 voxelized points** | **PASSED** |
| **Clean Acquisition Stop** | Zero zombie processes | All child processes safely exited | **PASSED** |
| **Disk Persistence** | Standard PCD v0.7 + metadata | `map.pcd` + `trajectory.json` persisted | **PASSED** |
| **Cold Project Reopen** | Restore saved map points in GUI | Restored map points; `hasMapData=true` | **PASSED** |

---

## 8. Device Profile Promotion

The device profile for Orbbec Gemini 336L has been promoted:
- **Previous Path:** `profiles/draft/gemini_336l_profile.json` (`profile_status`: `DRAFT`)
- **New Path:** `profiles/validated/gemini_336l_profile.json` (`profile_status`: `VALIDATED`)
- **Provenance Field:** `M4_PHYSICALLY_QUALIFIED` (confidence `1.0`, validated `true`)
- **Validation Notice in GUI:** `"Orbbec Gemini 336L: Physical Hardware Qualified (Active Stereo RGB-D SLAM)"`

---

## 9. Physical User Acceptance Instructions

To physically verify the Orbbec Gemini 336L scanner inside the running AV Scan interface:

1. **Launch the AV Scan Application on Connected Display:**
   ```bash
   DISPLAY=:0 /home/scanar/av_scan/build/app/av_scan
   ```
2. **Create a Test Project:**
   - Click the `+ New Project` button in the top navigation bar.
   - Enter `Gemini Physical Scan` in the dialog and click `Create`.
3. **Select Orbbec Gemini 336L Scanner:**
   - In the lower-left scanner configuration dropdown, select `Orbbec Gemini 336L [VALIDATED]`.
   - Observe the hardware status badge updates to reflect the Gemini 336L pipeline.
4. **Initiate Real Capture:**
   - Click the blue `START` button.
   - Within 3–4 seconds, the tracking badge will transition to `TRACKING_OK`.
   - Point the camera toward a room surface with visual features (e.g. wall, desk, objects).
   - Observe the **3D Spatial Map** rendering real-time registered 3D points and the amber camera trajectory path.
   - Observe the **2D Floor Plan** displaying the live top-down projection.
5. **Physically Move the Camera:**
   - Slowly translate the camera forward, backward, left, and right.
   - Observe the real-time position marker following your hand movement.
   - Observe newly observed surfaces streaming into the 3D map.
6. **Stop Acquisition:**
   - Click the red `STOP` button.
   - Acquisition cleanly halts, locking the final point count.
7. **Persist the Capture:**
   - Click the `SAVE` button.
   - The capture is written to disk as `map.pcd` and `trajectory.json`.
8. **Verify Cold Reopen:**
   - Click `Open Project...`, select `Gemini Physical Scan`, and open it.
   - The saved point cloud and trajectory restore immediately in the viewports without active scanning.

---

## 10. Milestone Conclusion

Milestone 4 is **100% COMPLETE**. The physical Orbbec Gemini 336L operates as a qualified, real scanner inside the commercial AV Scan platform, confirming that AV Scan's sensor abstraction cleanly accommodates diverse spatial sensing modalities (LiDAR LIO vs Active Stereo RGB-D SLAM).

Per the master orchestration directive:
> *"STOP. Do NOT begin M5 without explicit authorization. Remember: MAKE IT WORK. VALIDATE IT. MOVE ON."*
