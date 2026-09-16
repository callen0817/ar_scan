# AV Scan — Technology Decisions

**Document Version:** 1.0.0  
**Milestone:** M1 — Foundation  
**Date:** September 16, 2026  
**Status:** Approved for Implementation  

---

## 1. Context & Objectives

AV Scan is a commercial spatial-capture platform designed to capture, process, register, and fuse heterogeneous 3D observations across diverse sensor modalities (LiDAR, RGB-D, Stereo VIO, XR glasses) and compute nodes.

Per the master orchestration guidelines:
- **Commercial Quality:** Reliable, maintainable, testable, and real.
- **Avoid Paralysis by Analysis:** Select proven, robust solutions that work directly on the target host without endless speculative debates.
- **Platform Strategy:** The Portable Core must remain platform-agnostic and ROS-independent. Jetson Linux is the initial target, but architecture must support Windows, desktop Linux, and iOS without redesign.

---

## 2. Core Architecture Decisions

### 2.1 Core Language: Modern C++ (C++17)
* **Decision:** Implement all core data schemas, mathematical abstractions, interfaces, pipeline orchestration, storage, and logging in **Modern C++ (C++17)**.
* **Rationale:**
  - High deterministic throughput required for real-time point cloud pipelines (10–30 Hz point cloud frames, up to millions of points/sec).
  - Native interoperability with high-performance spatial libraries: Eigen3, PCL, GTSAM, OpenCV, and manufacturer SDKs.
  - Zero-overhead memory management, RAII concurrency control, and predictable latency on embedded ARM64 hardware.
  - Native cross-platform compilation target across Linux (GCC/Clang), Windows (MSVC), and iOS (Apple Clang).

### 2.2 GUI Framework: Qt Quick / QML (Qt 5.15 LTS)
* **Decision:** Build the primary capture operator interface using **Qt Quick / QML with a C++ ViewModel backend**.
* **Rationale:**
  - Hardware-accelerated scene-graph rendering using OpenGL ES / EGL on Tegra Orin GPU.
  - Clean separation between business logic (C++ core engine) and user presentation (declarative QML).
  - Effortless cross-platform portability across Linux, Windows, macOS, and iOS without altering UI code.
  - Tested and validated during Host Audit: isolated QML runtime plugins in `third_party/qml/` load cleanly without requiring system modification or root privileges.

### 2.3 Build System: Modern CMake (>= 3.22, host has 4.4.2)
* **Decision:** Standardize on **Modern target-based CMake**.
* **Rationale:**
  - Industry standard for commercial C++ applications.
  - Granular modular targets (`av_core`, `av_gui`, `av_platform`, `av_sensors`, `av_slam`, `av_scan_app`, unit/integration test targets).
  - Explicit dependency propagation (`target_link_libraries`, `target_include_directories`).

### 2.4 Separation from ROS (Robot Operating System)
* **Decision:** **AV Scan Core is strictly ROS-independent**.
* **Rationale:**
  - Commercial enterprise deployment cannot require operators to maintain ROS environments, manage ROS master/DDS discovery daemon configurations, or troubleshoot ROS topic graphs.
  - ROS 2 Humble may be leveraged optionally inside specific Linux sensor adapters (such as bridging `rslidar_sdk` or `orbbec_camera` streams) where official manufacturer drivers provide ROS nodes, but these are isolated behind the clean, abstract `ISensorAdapter` C++ interface.
  - Core algorithms (fusion, registration, project storage, session management) consume strictly native AV Scan data types.

### 2.5 Serialization & Schemas: nlohmann/json with Explicit Versioning
* **Decision:** Utilize **nlohmann/json (version 3.10.5)** with explicit `schema_version` attributes for all projects, sessions, device profiles, scanner configs, and metadata.
* **Rationale:**
  - Human-readable, easily inspected, git-friendly, and validated industry standard.
  - Supports backward and forward schema compatibility without heavy external code-generation toolchains (such as protobuf or flatbuffers) during M1–M5.
  - Pre-installed and verified on the Jetson host.

### 2.6 Linear Algebra & Geometry: Eigen3 & PCL
* **Decision:** Standardize on **Eigen3 (3.4.0)** for internal transforms, poses, and matrices, and **PCL (1.12.1)** for spatial filtering, KD-trees, and point-cloud data structures.
* **Rationale:**
  - Established, mature, highly optimized SIMD implementations (ARM NEON accelerated).
  - Fully pre-installed and verified on the host system.

---

## 3. Layered System Architecture

```
┌────────────────────────────────────────────────────────┐
│                   AV Scan Application                  │
│       (Main Application Lifecycle, Orchestrator)       │
└──────────────────────────┬─────────────────────────────┘
                           │
       ┌───────────────────┴───────────────────┐
       ▼                                       ▼
┌─────────────────────────┐         ┌─────────────────────────┐
│     AV Scan QML GUI     │         │      Portable Core      │
│  (Qt Quick / ViewModels)│         │ (Projects, Sessions,    │
└────────────┬────────────┘         │  Profiles, Configs)     │
             │                      └────────────┬────────────┘
             │                                   │
             ▼                                   ▼
┌─────────────────────────┐         ┌─────────────────────────┐
│    Platform Adapters    │         │    Interface Layer      │
│ (Linux/Jetson, Win, iOS)│         │ ISensorAdapter          │
└─────────────────────────┘         │ ISlamBackend            │
                                    │ IMapProvider            │
                                    │ IStorageEngine          │
                                    └────────────┬────────────┘
                                                 │
                     ┌───────────────────────────┴───────────────────────────┐
                     ▼                                                       ▼
        ┌─────────────────────────┐                             ┌─────────────────────────┐
        │     Sensor Adapters     │                             │      SLAM Backends      │
        │ • RoboSense Airy        │                             │ • LIO (FAST-LIO2)       │
        │ • Orbbec Gemini 336L    │                             │ • RGB-D SLAM            │
        │ • VITURE Ultra          │                             │ • Stereo VIO            │
        └─────────────────────────┘                             └─────────────────────────┘
```

---

## 4. Execution Guidance

1. Implement real C++ classes and headers for all nine required schemas in `core/schemas/`.
2. Implement pure virtual interfaces with complete contracts in `core/interfaces/`.
3. Implement structured thread-safe logging in `core/logging/`.
4. Implement storage engine in `core/storage/`.
5. Implement platform adapter for Linux in `platform/linux/`.
6. Implement QML application shell in `app/` and `gui/`.
7. Build, test, and package on Jetson Orin NX.
