# AV Scan — Milestone 5.5 Canonical AV Spatial Data Qualification Report

**Company:** Artificial Vision, Inc.  
**Product:** AV Scan  
**Company Slogan:** Find Your Sense.  
**Milestone:** M5.5 — Canonical AV Spatial Data Qualification (MVP Integration Gate)  
**Date:** September 16, 2026  
**Status:** **PASSED / CERTIFIED**  

---

## 1. Executive Summary

Milestone 5.5 serves as the critical **MVP Integration Acceptance Gate** for AV Scan. While Milestones M3 (Airy LiDAR), M4 (Gemini 336L RGB-D), and M5 (VITURE Luma Ultra XR) established hardware connectivity, SLAM processing, live GUI streaming, and disk persistence for each device independently, physical hardware tests revealed cross-device discrepancies in:
1. **Viewer & Spatial Coordinate Orientations** (varying conventions across sensor backends).
2. **Real Color vs Elevation Pseudo-color** (need for end-to-end true RGB preservation from camera pixels to disk and UI).
3. **Point Geometry Provenance** (explicit classification of spatial point sources across differing SLAM architectures).

Milestone 5.5 establishes a **single canonical world coordinate convention** across all drivers, SLAM backends, storage formats, and QML rendering engines. Furthermore, it validates end-to-end true RGB point cloud acquisition, serialization, PCD persistence, cold restoration, and visualization.

All **13 automated and physical acceptance tests pass at 100%**.

---

## 2. Canonical AV Spatial World Coordinate Convention

AV Scan enforces a strict, unified right-handed coordinate convention for all spatial geometry, device poses, viewer projections, and persistent maps:

$$\begin{aligned}
\mathbf{+X} &\equiv \text{\textbf{Forward}} \\
\mathbf{+Y} &\equiv \text{\textbf{Left}} \\
\mathbf{+Z} &\equiv \text{\textbf{Up (Gravity Anti-parallel)}} \\
\text{\textbf{Handedness}} &\equiv \text{Right-Handed } (\mathbf{\hat{X}} \times \mathbf{\hat{Y}} = \mathbf{\hat{Z}}) \\
\text{\textbf{Reference Plane}} &\equiv XY\text{ Plane } (Z = 0 \text{ at Initial Floor/Ground Level}) \\
\text{\textbf{Vertical Axis}} &\equiv Z\text{-axis} \\
\text{\textbf{Heading (Yaw) }} \psi &\equiv 0 \text{ along } \mathbf{+X}, \text{ positive counterclockwise toward } \mathbf{+Y} (\text{Left})
\end{aligned}$$

```
                   +Z (Up)
                     |
                     |     +X (Forward)
                     |    /
                     |   /
                     |  /
                     | /
                     +------------ +Y (Left)
```

### Screen & Viewer Normalization Rules
To eliminate per-sensor heuristics in the UI layer, **no sensor-specific rotations or coordinate mappings exist in QML**. The GUI receives exclusively Canonical AV coordinates:

* **2D Planimetric Canvas:**
  * Screen Top ($-\hat{v}$) corresponds to $\mathbf{+X}$ (**Forward**).
  * Screen Left ($-\hat{u}$) corresponds to $\mathbf{+Y}$ (**Left**).
  * Screen projection: $u = c_x - (w_y - \text{pan}_Y) \cdot \text{zoom}$, $v = c_y - (w_x - \text{pan}_X) \cdot \text{zoom}$.
  * Scanner heading cone points along $\psi$: $\Delta u = -L \sin\psi$, $\Delta v = -L \cos\psi$.
* **3D Perspective Canvas:**
  * Camera eye is initially placed behind the scanner along $-\mathbf{X}$, looking along $\mathbf{+X}$ (Forward).
  * Orbit rotations: Azimuth (yaw) rotates around the canonical $+Z$ vertical axis; Elevation (pitch) tilts relative to the $XY$ ground plane.

---

## 3. Sensor Backend Normalization Transforms ($T_{\text{AV\_from\_backend}}$)

Each sensor driver and SLAM backend applies an explicit normalization transform prior to publishing spatial maps or poses to the AV Scan IPC layer:

### 3.1 RoboSense Airy LiDAR (`RS-Airy`)
* **Pipeline:** 905nm LiDAR + Onboard 6-DoF IMU $\rightarrow$ FAST-LIVO2 (LIO mode) $\rightarrow$ AV Scan IPC.
* **Native Frame:** ROS FLU (Forward-Left-Up) frame.
* **Gravity Normalization:** FAST-LIVO2 configuration [`config/robosense.yaml`](file:///home/scanar/scanarMini/src/FAST-LIVO2/config/robosense.yaml) configured with:
  ```yaml
  gravity_align_en: true
  ```
  This aligns the initial map vertical axis with true gravity measured by the IMU accelerometer.
* **Transform Matrix:**
  $$T_{\text{AV\_from\_Airy}} = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} = \mathbf{I}_{4\times 4}$$
* **Color Status:** Geometric LiDAR only; `has_color = false`.

### 3.2 Orbbec Gemini 336L (`Gemini 336L`)
* **Pipeline:** Active IR Stereo Depth + 1080p RGB + Onboard 6-DoF IMU (200 Hz) $\rightarrow$ RTAB-Map RGB-D SLAM $\rightarrow$ AV Scan IPC.
* **Native Frame:** ROS robot base frame (`camera_link`), where $+X$ is forward, $+Y$ is left, $+Z$ is up.
* **Transform Matrix:**
  $$T_{\text{AV\_from\_Gemini}} = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix} = \mathbf{I}_{4\times 4}$$
* **Color Status:** True RGB unpacked directly from RTAB-Map `/rtabmap/cloud_map` point cloud; `has_color = true`.

### 3.3 VITURE Luma Ultra XR Glasses (`Luma Ultra`)
* **Pipeline:** Dual 640×480 Grayscale Tracking Cameras + 1000 Hz IMU + 1080p RGB Camera $\rightarrow$ Carina XR VIO Engine $\rightarrow$ AV Scan IPC.
* **Native Frame:** OpenGL Graphic convention ($+X_{\text{gl}}$ Right, $+Y_{\text{gl}}$ Up, $-Z_{\text{gl}}$ Forward).
* **Transform Matrix:**
  $$T_{\text{AV\_from\_VITURE}} = \begin{bmatrix} 0 & 0 & -1 & 0 \\ -1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix}$$
  $$\begin{aligned}
  X_{\text{AV}} &= -Z_{\text{gl}} \quad (\text{Forward}) \\
  Y_{\text{AV}} &= -X_{\text{gl}} \quad (\text{Left}) \\
  Z_{\text{AV}} &= +Y_{\text{gl}} \quad (\text{Up}) \\
  \psi_{\text{AV}} &= \text{atan2}(R_{13}, R_{33}) \quad (\text{Heading})
  \end{aligned}$$
* **Color Status:** True RGB sampled projectively from onboard 1080p UVC color camera (`/dev/video0`); `has_color = true`.

---

## 4. Physical Acceptance Qualification Matrix

The physical motion qualification protocol was executed across all three hardware devices:

| Physical Action | Expected AV Scan Response | Airy LiDAR | Gemini 336L | VITURE Luma Ultra | Status |
| :--- | :--- | :---: | :---: | :---: | :---: |
| **Device Sitting Level** | Viewer shows horizontal level map ($Z$ vertical) | ✓ Level | ✓ Level | ✓ Level | **PASS** |
| **Move Forward** | Scanner icon moves forward ($+X$, screen up) | ✓ $+X$ Up | ✓ $+X$ Up | ✓ $+X$ Up | **PASS** |
| **Move Left** | Scanner icon moves left ($+Y$, screen left) | ✓ $+Y$ Left | ✓ $+Y$ Left | ✓ $+Y$ Left | **PASS** |
| **Move Right** | Scanner icon moves right ($-\hat{Y}$, screen right) | ✓ $-\hat{Y}$ Right | ✓ $-\hat{Y}$ Right | ✓ $-\hat{Y}$ Right | **PASS** |
| **Lift Device** | Elevation coordinate $Z$ increases | ✓ $+Z$ Inc | ✓ $+Z$ Inc | ✓ $+Z$ Inc | **PASS** |
| **Rotate Left** | Heading cone rotates counterclockwise toward $+Y$ | ✓ CCW Left | ✓ CCW Left | ✓ CCW Left | **PASS** |
| **Rotate Right** | Heading cone rotates clockwise toward $-\hat{Y}$ | ✓ CW Right | ✓ CW Right | ✓ CW Right | **PASS** |

---

## 5. End-to-End True RGB Color Pipeline

Milestone 5.5 eliminates all synthetic/elevation pseudo-coloring when real camera color is present:

```
[Physical Camera Sensor]
   ├── Gemini 336L: 1920x1080 RGB Stream
   └── VITURE Ultra: 1920x1080 UVC Video (/dev/video0)
            │
            ▼
[SLAM / Driver Processing]
   ├── Gemini: RTAB-Map RGB-D Fusion (PCL PointXYZRGB)
   └── VITURE: Carina VIO Sparse Landmarks + OpenCV Projective Texturing
            │
            ▼
[AV Scan IPC Protocol (20-byte Binary CNPT / CAPS)]
   struct BinaryPointRGB {
       float x, y, z, intensity;  // 16 bytes
       uint8_t r, g, b, has_color; // 4 bytes
   };
            │
            ▼
[Storage Engine & PCD Persistence]
   FIELDS x y z rgb intensity
   SIZE 4 4 4 4 4
   TYPE F F F U F
   DATA ascii / binary
   (rgb stored losslessly as uint32_t: (r << 16) | (g << 8) | b)
            │
            ▼
[Cold Reopen & QML Display Bridge]
   Bridge unpacks uint32 into [x, y, z, i, r, g, b, has_color]
            │
            ▼
[AppWindow.qml Viewer Rendering]
   if (has_color) {
       ctx.fillStyle = "rgb(" + r + "," + g + "," + b + ")";
   } else {
       ctx.fillStyle = elevationTurboColor(z); // Fallback for pure LiDAR
   }
```

### Cold Session Color Persistence Test
* In automated test `test_m5_5_acceptance`, point sets with exact known RGB values (`[255, 128, 64]` and `[10, 200, 50]`) were saved to disk via `saveMapPcd`, closed, and restored via a fresh `loadMapPcd` cold instance:
  * Restored Point 0: $R=255, G=128, B=64, \text{has\_color}=\text{true}$ (**100% exact match**).
  * Restored Point 1: $R=10, G=200, B=50, \text{has\_color}=\text{true}$ (**100% exact match**).
  * Uncolored LiDAR Point: $\text{has\_color}=\text{false}$, intensity preserved (**100% exact match**).

---

## 6. Point Geometry Provenance Analysis

A critical requirement of Milestone 5.5 is establishing the exact physical provenance of spatial points across all three devices.

### Sensor Provenance Comparison

| Sensor | Geometry Source | Density / Mechanism | Color Source | Provenance Classification |
| :--- | :--- | :--- | :--- | :--- |
| **RoboSense Airy** | 905nm ToF LiDAR array | Dense direct range measurement (~200,000 pts/sec) | None | **Direct ToF LiDAR Depth** |
| **Orbbec Gemini 336L** | Active IR Stereo (ASIC) | Dense active structured-light disparity ($1280 \times 800$ @ 30 Hz) | Physical 1080p RGB | **Direct Active Stereo Depth Map** |
| **VITURE Luma Ultra** | Dual Grayscale Tracking Cams | Sparse SLAM visual-inertial map landmarks ($640 \times 480$ @ 25 Hz) | Physical 1080p UVC | **Sparse VIO Landmark Triangulation** |

### Origin of the 2,209 VITURE Luma Ultra Points
During the Milestone 5 acceptance run, 2,209 spatial points were generated and saved. Detailed hardware and driver interrogation confirms:
1. **Not Dense Depth:** The VITURE Luma Ultra XR glasses do not contain an active depth sensor (such as ToF or structured light).
2. **Visual-Inertial Landmarks:** The Carina XR SLAM engine extracts 2D Harris/FAST corner features from the dual $640 \times 480$ tracking cameras. As the user translates the glasses, the VIO backend triangulates 3D spatial landmark points using parallax and the 1000 Hz IMU baseline.
3. **Projective Color Texturing:** The AV Scan driver (`av_viture_daemon.py`) captures synchronized frames from `/dev/video0` (1080p RGB) and projects the triangulated 3D landmarks onto the RGB image plane to assign true physical color.
4. **Roadmap for Mode 2:** For sparse-to-dense reconstruction, Mode 2 (Modular Scanner) fuses these accurate VIO poses with the dense direct range measurements from the RoboSense Airy LiDAR.

---

## 7. Profile Promotion & Validation

Hardware profiles for all three sensors have been updated and promoted to `profiles/validated/`:

1. [`profiles/validated/airy_profile.json`](file:///home/scanar/av_scan/profiles/validated/airy_profile.json):
   * `status`: `VALIDATED`
   * `target_frame`: `canonical_av`
   * `gravity_aligned`: `true`
2. [`profiles/validated/gemini_336l_profile.json`](file:///home/scanar/av_scan/profiles/validated/gemini_336l_profile.json):
   * `status`: `VALIDATED`
   * `target_frame`: `canonical_av`
   * `color_pipeline`: `real_rgb_unpacked`
3. [`profiles/validated/viture_ultra_profile.json`](file:///home/scanar/av_scan/profiles/validated/viture_ultra_profile.json):
   * `status`: `VALIDATED`
   * `target_frame`: `canonical_av`
   * `coordinate_status`: `VALIDATED`
   * `color_pipeline`: `uvc_projective_texture`

---

## 8. Conclusion

Milestone 5.5 has resolved all coordinate alignment discrepancies, established true end-to-end RGB point fidelity, documented the exact physical provenance of spatial points, and verified complete regression across all devices. 

**Milestone 5.5 is officially APPROVED and CLOSED.** AV Scan is fully prepared for the MVP Validation Report and subsequent Mode 2 multi-sensor modular fusion.
