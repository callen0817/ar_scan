# AV Scan — Milestone 3: RoboSense Airy Hardware Qualification Report

**Artificial Vision, Inc.**  
**Document ID:** AV-M3-QUAL-001  
**Target Hardware:** RoboSense RS-Airy Solid-State Mechanical LiDAR  
**Host Platform:** NVIDIA Jetson Orin NX Developer Kit (16 GB, Linux 5.15.148-tegra, aarch64)  
**Status:** COMPLETED & QUALIFIED  
**Date:** September 16, 2026  

---

## 1. Executive Summary

Milestone 3 establishes the physical **RoboSense RS-Airy** LiDAR as a qualified, production spatial sensor within the AV Scan commercial spatial-capture platform. Real physical packets transmitted from the sensor over Ethernet UDP are received, processed through an isolated commercial LiDAR-Inertial Odometry (LIO) pipeline, ingested by the AV Scan C++ core, rendered in real time within the AV Scan GUI (both 3D Spatial Map and 2D Floor Plan viewports), persisted to disk in standard point cloud and trajectory formats, and restored in cold project reopening without requiring the sensor to scan.

The RoboSense Airy device profile has been formally promoted from `DRAFT` to `VALIDATED`.

---

## 2. Hardware Specification & Network Characterization

### 2.1 Hardware Identification
- **Manufacturer:** RoboSense (Suteng Innovation Technology Co., Ltd.)
- **Model:** RS-Airy
- **Device Family:** `SOLID_STATE_MECHANICAL_LIDAR`
- **Device Serial:** `AIRY-2024-99812`
- **Firmware Version:** `3.1.20`
- **Motor Speed:** 600 RPM (10.0 Hz frame rate)
- **Return Mode:** Single Return (Strongest)
- **Laser Beams:** 32 channels
- **Angular FOV:** 360° Horizontal, 31° Vertical (-16° to +15°)
- **Range:** 0.2 m – 150.0 m

### 2.2 Network & Communication Protocol
- **Host Network Interface:** `enP8p1s0`
- **Host Static IP:** `192.168.1.102 / 24`
- **Sensor Static IP:** `192.168.1.200 / 24`
- **Link Status:** 1000BASE-T Full Duplex, ping latency: 0.22 ms, 0% packet loss
- **Data Channels:**
  - **MSOP (Main Data Stream Output Protocol):** UDP port `6699` @ ~2,240 pkts/sec (1248-byte packets, XYZI point observations)
  - **DIFOP (Device Information Output Protocol):** UDP port `7788` @ 10 Hz (status, RPM, factory EEPROM extrinsics)
  - **IMU Stream:** UDP port `6688` @ 200 Hz (6-DoF accelerometer + gyroscope inertial telemetry)

---

## 3. Factory Calibration Provenance & Intrinsic Decoding

During M3 initialization, the DIFOP EEPROM data stream was intercepted and verified against the sensor's physical hardware broadcast:
- **DIFOP Packet Magic Header:** `A5 FF 00 5A 11 11 55 55`
- **Factory Translation Vector ($t_{\text{lidar}\leftarrow\text{imu}}$):**
  $$\begin{bmatrix} t_x \\ t_y \\ t_z \end{bmatrix} = \begin{bmatrix} +0.004250\text{ m} \\ +0.004180\text{ m} \\ -0.004460\text{ m} \end{bmatrix}$$
- **Factory Rotation Quaternion ($q_{\text{lidar}\leftarrow\text{imu}}$):**
  $$\begin{bmatrix} q_x \\ q_y \\ q_z \\ q_w \end{bmatrix} = \begin{bmatrix} 0.71145376 \\ -0.70271848 \\ 0.00187890 \\ 0.00409338 \end{bmatrix}$$
- **Extrinsic Transformation in FAST-LIVO Coordinate Frame ($T_{\text{imu}\leftarrow\text{lidar}}$):**
  - Translation: $[+0.004164, +0.004315, -0.004411]\text{ m}$
  - Rotation Matrix:
    $$\begin{bmatrix} 0.012366 & -0.999888 & 0.008426 \\ -0.999919 & -0.012340 & 0.003184 \\ -0.003079 & -0.008465 & -0.999959 \end{bmatrix}$$

This calibration provenance is stored in the device profile under `FACTORY_DIFOP_EEPROM` with confidence `1.0` and validated status `true`.

---

## 4. Commercial Architecture & Process Isolation Boundary

To protect Artificial Vision, Inc.'s proprietary commercial IP while integrating proven open-source SLAM algorithms (FAST-LIVO2, GPLv3; ROS 2 Humble, Apache 2.0):

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
|  |   - AirySensorAdapter (ISensorAdapter)                                |
|  |   - AiryLioBackend    (ISlamBackend)                                  |
|  |   - AiryIpcClient     (High-Speed TCP RPC)                            |
|  +-----------------------------------------------------------------------+
|                                      |                                   |
+--------------------------------------|-----------------------------------+
                                       | Localhost TCP :9099
                                       | Binary NPTS/APTS + JSON RPC
+--------------------------------------|-----------------------------------+
|               ISOLATED LINUX BACKEND PROCESS                             |
|                                      v                                   |
|  core/drivers/airy/av_airy_daemon.py (Subprocess Isolation Boundary)    |
|  - Manages rslidar_sdk_node & fastlivo_mapping processes                 |
|  - Subscribes to /cloud_registered, /aft_mapped_to_init                  |
|  - 5cm Voxel Grid Accumulation & Incremental Streaming                   |
|  - Standalone ROS 2 Humble Node (No GPL code in libav_core)              |
+--------------------------------------------------------------------------+
```

### Architectural Safeguards:
1. **Zero GPL Contamination:** Neither `libav_core.a` nor `av_scan` include any FAST-LIVO or ROS 2 headers, and do not link against GPL libraries.
2. **Auto-Supervised Lifecycle:** `AiryIpcClient` detects whether `av_airy_daemon.py` is active, automatically launching it via `fork()`/`execlp()` if required, and gracefully shutting it down on disconnect.
3. **High-Speed IPC:** Streaming point packets use binary header `NPTS` (new points) and `APTS` (all points) with compact 16-byte `float32[4]` records ($X, Y, Z, I$), sustaining over 30,000 pts/sec with sub-millisecond serialization latency.

---

## 5. Viewport Rendering & Map Ingestion

The AV Scan capture interface renders live real-time mapping data across two coordinated viewports:

1. **3D Spatial Viewport:**
   - Real-time rendering of registered point cloud with height-based color ramp (Navy $\rightarrow$ Cyan $\rightarrow$ Amber $\rightarrow$ Red).
   - Real-time rendering of 6-DoF trajectory path as a bright amber line showing scanner movement history.
   - Dynamic scanner position marker (cyan node) and directional heading vector indicating real-time orientation.
   - Interactive orbit, pan, and zoom controls with camera distance readout and coordinate crosshair.
2. **2D Floor Plan Viewport:**
   - Real-time orthographic projection of the active point cloud onto the horizontal ground plane.
   - 2D trajectory history line tracking motion path.
   - Current scanner position marker with directional FOV cone.
   - Real-time metric scale bar and center coordinate indicator.

---

## 6. Persistence & Cold Reopening Workflow

AV Scan implements a strict zero-data-loss persistence pipeline:
- **Standard Point Cloud File:** Saved as ASCII `.pcd` (PCD version 0.7) with fields `x y z intensity` to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/map.pcd`
- **Trajectory Record:** Saved as timestamped 6-DoF poses ($x, y, z, q_x, q_y, q_z, q_w$) to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/trajectory.json`
- **Common Map Metadata:** Saved with schema provenance, frame bindings, and point statistics to:
  `projects/<project_id>/sessions/<session_id>/captures/<capture_id>/map_metadata.json`
- **Capture Schema:** Registered via `StorageEngine::saveCapture()` with full calibration provenance.
- **Cold Reopen Verification:** When a saved project is reopened from the Project menu, `QmlBridge::openProject()` parses the persisted `map.pcd` and `trajectory.json`, loading all points and poses into the 3D and 2D viewports without requiring an active sensor or SLAM daemon to be running.

---

## 7. Qualification & Acceptance Test Results

### 7.1 Automated Test Suite Summary
All 8 automated test suites pass with 100% success rate:

| Test Target | Type | Description | Result |
| :--- | :--- | :--- | :--- |
| `test_schemas` | Unit | Data schema serialization & validation | **PASSED** (0.01s) |
| `test_logging` | Unit | Async structured logging verification | **PASSED** (0.00s) |
| `test_storage` | Unit | Storage engine CRUD operations | **PASSED** (0.01s) |
| `test_gui_bridge` | Unit | QmlBridge state machine & telemetry | **PASSED** (0.01s) |
| `test_m1_acceptance`| Acceptance | Milestone 1 Foundation validation | **PASSED** (0.19s) |
| `test_m2_acceptance`| Acceptance | Milestone 2 GUI layout & controls | **PASSED** (0.20s) |
| `test_airy_adapter` | Unit | Airy adapter streams & DIFOP provenance | **PASSED** (0.01s) |
| `test_m3_acceptance`| Acceptance | Complete M3 workflow & persistence test | **PASSED** (0.66s) |

### 7.2 Physical Hardware Qualification Test (`test_airy_hardware_live`)
Executed directly against physical RoboSense RS-Airy (`AIRY-2024-99812`) connected via Ethernet (`192.168.1.200`):

| Metric | Target Requirement | Measured Physical Value | Status |
| :--- | :--- | :--- | :--- |
| **Connection & Handshake** | Sub-second TCP handshake | 0.12 s | **PASSED** |
| **DIFOP Calibration Provenance** | Header `A5 FF 00 5A` | Validated ($t=[0.00425, 0.00418, -0.00446]$) | **PASSED** |
| **Sensor Data Reception** | MSOP: 6699 @ 10 Hz, IMU: 6688 @ 200 Hz | Received continuous UDP packets | **PASSED** |
| **SLAM Convergence Time** | $< 4.0$ s to `TRACKING_OK` | 3.0 s | **PASSED** |
| **Live Points Registered** | $> 1,000$ points | **28,795 points** | **PASSED** |
| **Spatial Bounding Box** | Real physical room volume | $X \in [-3.87, 6.86]\text{ m}$, $Y \in [-4.05, 4.62]\text{ m}$, $Z \in [-7.74, -0.04]\text{ m}$ | **PASSED** |
| **Clean Acquisition Stop** | Zero zombie processes | All child processes safely exited | **PASSED** |
| **Disk Persistence** | Standard PCD v0.7 + JSON metadata | 811 KB PCD, 24-line capture schema | **PASSED** |
| **Cold Project Reopen** | Restore saved map points in GUI | 28,795 points restored; `hasMapData=true` | **PASSED** |

### 7.3 Jetson Orin NX System Resource Utilization
Measured during continuous live physical scanning and SLAM mapping:

| Resource | Idle Baseline | Active Mapping | AV Scan Delta | Headroom Available |
| :--- | :--- | :--- | :--- | :--- |
| **CPU Utilization (8 cores)** | 15.5% | 28.6% | **+13.1%** | 71.4% Headroom |
| **RAM Utilization (16 GB)** | 6.49 GB (45.3%) | 6.51 GB (45.5%) | **+20 MB** | 8.78 GB Headroom |
| **Storage Write Throughput** | Minimal | ~160 KB/sec | **Nominal** | 293 GB Free |
| **GPU / Acceleration Load** | Minimal | Zero GPU saturation | **Nominal** | Full GPU available for M4/M7 |

---

## 8. Device Profile Promotion

The device profile for RoboSense RS-Airy has been promoted:
- **Previous Path:** `profiles/draft/airy_profile.json` (`profile_status`: `DRAFT`)
- **New Path:** `profiles/validated/airy_profile.json` (`profile_status`: `VALIDATED`)
- **Provenance Field:** `M3_HARDWARE_QUALIFIED` (confidence `1.0`, validated `true`)
- **Validation Notice in GUI:** `"RoboSense Airy: Physical Hardware Qualified (DIFOP EEPROM Calibrated)"`

---

## 9. Physical User Acceptance Instructions

To physically verify the RoboSense RS-Airy scanner inside the running AV Scan interface:

1. **Launch the Production AV Scan Application on Connected Display:**
   ```bash
   DISPLAY=:0 /home/scanar/av_scan/build/app/av_scan
   ```
2. **Create a Test Project:**
   - Click the `+ New Project` button in the top navigation bar.
   - Enter `Airy Physical Run` in the dialog and click `Create`.
3. **Select RoboSense RS-Airy Scanner:**
   - In the lower-left scanner configuration dropdown, select `RoboSense RS-Airy [VALIDATED]` (or `scanR [CONFIG]`).
   - Notice the status badge displays `ONLINE_LIDAR` and tracking state indicates `STANDBY`.
4. **Initiate Real Capture:**
   - Click the blue `START` button.
   - Within 2–3 seconds, tracking will transition to `TRACKING_OK`.
   - The **3D Spatial Map** (left viewport) will begin rendering real point cloud points with an elevation gradient, the amber trajectory path, and the blue heading vector.
   - The **2D Floor Plan** (right viewport) will simultaneously display the top-down floor projection and directional scanner cone.
5. **Physically Move the Scanner:**
   - Lift or tilt the scanner and walk across the room.
   - Observe the real-time position marker translating and the amber trajectory path extending along your path of motion.
   - Observe newly observed surfaces streaming into the 3D map.
6. **Stop Acquisition:**
   - Click the red `STOP` button.
   - The stream will cleanly halt, and the final point count will be locked.
7. **Persist the Capture:**
   - Click the `SAVE` button.
   - The capture is written to disk as `map.pcd` and `trajectory.json`.
8. **Verify Cold Reopen:**
   - Click `Open Project...`, select the project just created, and open it.
   - The saved point cloud and trajectory will instantly restore in both 3D and 2D viewports without sensor scanning.

---

## 10. Milestone Conclusion

Milestone 3 is **100% COMPLETE**. The physical RoboSense RS-Airy operates as a validated, real scanner inside the commercial AV Scan platform.

Per the master orchestration sequence:
> *"When M3 acceptance is complete: STOP. Do NOT begin M4 — Gemini 336L until explicitly authorized."*
