# AV Scan — Jetson Host Environment Audit

**Document Version:** 1.0.0  
**Milestone:** M1 — Foundation  
**Target Hostname:** `scanar-01`  
**Date:** September 16, 2026  
**Auditor:** AV Scan Engineering  

---

## 1. Executive Summary

This document establishes the official pre-integration hardware, software, and peripheral audit of the primary deployment target for **AV Scan**: an NVIDIA Jetson Orin NX (16GB) computing platform.

The audit was conducted strictly non-destructively:
- No system-wide upgrades were performed.
- Existing working installations (`scan_ar`, `scanAR Cam`, `scanarMini`, `rslidar_sdk`) were preserved completely intact.
- Hardware, firmware, networking, peripheral bus topologies, and installed libraries were inventoried to validate compatibility with the AV Scan commercial architecture (C++17, Qt Quick/QML, CMake, Eigen, PCL, and ROS 2 Humble backend adapters).

---

## 2. Hardware Specification

| Component | Audit Finding | Details / Sysfs Identification |
| :--- | :--- | :--- |
| **SoC / Platform** | NVIDIA Tegra234 (Orin) | SoC Family: Tegra, Chip ID: `0x23` |
| **Compute Module** | NVIDIA Jetson Orin NX (16GB) | P-Number: `p3767-0000`, 8-core ARM Cortex-A78AE @ 1.98 GHz |
| **Carrier Board** | Seeed Studio reComputer J4012 / DevKit Reference | Compatible Spec: `p3768` reference (`TNSPEC: 3767-301-0000-G.1-1-1-jetson-orin-nano-devkit-`) |
| **GPU Architecture** | NVIDIA Ampere Architecture | 1024 NVIDIA CUDA cores with 32 Tensor Cores, Max Power: 25W (MAXN) |
| **System Memory** | 15.6 GiB Unified LPDDR5 RAM | 128-bit bus, ~102.4 GB/s bandwidth; Swap: 7.6 GiB configured on NVMe |
| **Storage Subsystem** | 915 GiB PCIe NVMe SSD | `/dev/nvme0n1p1` mounted on `/`, 575 GiB used, **294 GiB available** (67% utilization) |

---

## 3. Operating System & Kernel

* **Distribution:** Ubuntu 22.04.5 LTS (Jammy Jellyfish)
* **Kernel Version:** `5.15.148-tegra` (`#1 SMP PREEMPT Thu Sep 18 15:08:33 PDT 2025 aarch64`)
* **L4T (Linux for Tegra):** `R36.4.7` (Release 36, Revision 4.7, GCID 42132812)
* **JetPack Equivalent:** JetPack 6.1.1 / 6.2 component level
* **Init System:** Systemd 249.11
* **Docker:** Docker version 29.7.2, build a7dcaa6

---

## 4. Accelerated Compute & Toolchains

* **CUDA Toolkit:** `12.6.68` (`/usr/local/cuda-12.6`, Driver / nvcc V12.6.68)
* **cuDNN:** `9.3.0.75`
* **TensorRT:** `10.3.0.30`
* **VPI (Vision Programming Interface):** `3.2.4`
* **Vulkan:** `1.3.204`
* **C++ Compiler:** GCC / G++ `11.4.0` (Standard Ubuntu Jammy toolchain, supporting C++17 and C++20)
* **CMake:** `4.4.2` (Modern CMake available in `/usr/local/bin` / `/usr/bin`)
* **Python Runtime:** Python `3.10.12`

---

## 5. Robotics & Spatial Libraries

| Library | Version | Location / Status | Relevance to AV Scan |
| :--- | :--- | :--- | :--- |
| **ROS 2** | Humble Hawksbill | `/opt/ros/humble` | Available for optional sensor/SLAM backend transport; AV Core remains ROS-free |
| **Eigen3** | `3.4.0` | `/usr/include/eigen3` | Core linear algebra, transformations, rotations, state estimation |
| **PCL** | `1.12.1` | `/usr/include/pcl-1.12`, `libpcl-dev` | Established 3D point cloud filtering, KD-trees, registration, ICP |
| **GTSAM** | `4.2.0` | `/opt/ros/humble/include/gtsam` | Pose graph optimization, factor graph SLAM, iSAM2 |
| **OpenCV** | `4.5.4` / `4.8.0` | System / OpenCV 4.8 runtime | Image acquisition, feature detection, camera projection |
| **Open3D** | Header only in RTAB-Map | Not installed system-wide | PCL and Eigen satisfy M1–M4 spatial algorithms without requiring Open3D bloat |
| **nlohmann/json** | `3.10.5` | `/usr/include/nlohmann` | Commercial JSON schemas, profile serialization, project/session persistence |

---

## 6. Graphical User Interface (GUI) Audit

* **Qt Framework:** Qt `5.15.3` (`qtbase5-dev`, `qtdeclarative5-dev`, `libqt5quick5`, `libqt5widgets5`, `libqt5opengl5-dev`)
* **PySide6 (Qt 6):** `6.8.0.2` installed in user site-packages (`/home/scanar/.local/lib/python3.10/site-packages/PySide6`)
* **QML Runtime Isolation:**
  - Standard Ubuntu 22.04 deb packages provide Qt Quick 2, Qt Quick Controls 2, and Qt Quick Layouts.
  - To prevent system perturbation and avoid requiring root privileges, the required Qt 5.15 QML runtime plugins (`QtQuick.2`, `QtQuick/Controls.2`, `QtQuick/Layouts`, `QtQuick/Window.2`, `QtGraphicalEffects`) were extracted into `third_party/qml/`.
  - Verified: C++ Qt Quick applications initialize and load QML scenes flawlessly with offscreen and X11/EGL platforms via `engine.addImportPath("third_party/qml")`.

---

## 7. Network Interfaces

| Interface | Type | IP / Configuration | Hardware Binding | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| `wlP1p1s0` | Wi-Fi (802.11ac/ax) | `10.0.0.80/24` | `74:04:f1:c2:4d:72` | Primary local network / management / telemetry |
| `enP8p1s0` | 1GbE/2.5GbE RJ45 | `192.168.1.10/24` (static) | `3c:6d:66:1f:61:6e` | Dedicated LiDAR Ethernet stream (RoboSense Airy & PTP grandmaster) |
| `tailscale0` | VPN Tunnel | Overlay IP | Virtual interface | Remote diagnostics and multi-node testing |
| `can0` | CAN Bus | Down | Native Tegra CAN | Available for future vehicle/odometry integrations |
| `docker0` | Bridge | `172.17.0.1/16` | Virtual bridge | Container network |
| `lo` | Loopback | `127.0.0.1/8` | Internal | IPC and local telemetry |

---

## 8. USB & Sensor Peripherals

Audited USB hierarchy (`lsusb -t` / `lsusb`):
1. **Bus 002 (USB 3.1 SuperSpeed Root Hub):**
   - Port 1: VIA Labs USB 3.1 Hub (`2109:0822`)
2. **Bus 001 (USB 2.0 HighSpeed Root Hub):**
   - Port 1: VIA Labs USB 2.0 Hub (`2109:2822`)
   - Port 2: QinHeng Electronics USB HUB (`1a86:8091`)
   - Attached devices:
     - **Microdia USB 2.0 Camera:** `0c45:636b` (`/dev/video0`, `/dev/video1`)
     - **VITURE Microphone:** `35ca:1102`
     - **VITURE Luma Ultra XR Glasses:** `35ca:1104` (HID interfaces `/dev/hidraw5`, `/dev/hidraw6`, `/dev/hidraw7`)
     - **Intel Bluetooth Wireless:** `8087:0a2b`
     - **Logitech Unifying Receiver:** `046d:c52b`
     - **Elecom MR-K013 Multicard Reader:** `25a7:fa61`

---

## 9. Existing Sensor Software & Preserved Environments

The host contains prior experimental packages developed under the ScanAR initiative. Per section M1.1 of the master specification, **these environments remain completely untouched and isolated**:

1. `/home/scanar/rslidar_sdk`:
   - Validated driver source for RoboSense LiDARs (Airy, Helios, Ruby).
   - Provides packet decoding, point cloud assembly, and coordinate transforms.
2. `/home/scanar/scan_ar`:
   - Contains `run_lidar.sh` with working PTP configuration (`gptp_master.conf` using `ptp4l` over `enP8p1s0`).
   - Contains `fast_lio2` and `FAST-LIVO2` integrations.
3. `/home/scanar/scanarMini` and `/home/scanar/scanAR Cam`:
   - Contains Orbbec camera SDK and ROS 2 wrappers (`orbbec_camera`, `orbbec_camera_msgs`).
   - Validated configurations for Orbbec Gemini 336L stereo depth camera.

---

## 10. Audit Conclusion & Feasibility Assessment

* **Compute & Storage:** Jetson Orin NX (16GB) with 294 GB available SSD storage exceeds all requirements for M1 through M5.
* **Compiler & Build System:** GCC 11.4.0 and CMake 4.4.2 natively support modern C++17/C++20 standards.
* **Core Libraries:** Eigen 3.4.0, PCL 1.12.1, and nlohmann/json 3.10.5 are pre-installed in standard system locations.
* **GUI Engine:** Qt 5.15.3 Quick/QML operates successfully with local project-isolated QML modules (`third_party/qml/`), without requiring system-wide changes or root credentials.
* **Genuine Blockers:** **NONE.**
* **Verdict:** The Jetson host environment is fully validated. Proceed immediately to M1 implementation.
