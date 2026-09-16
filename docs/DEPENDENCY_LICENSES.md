# AV Scan — Commercial Dependency Licensing Review

**Document Version:** 1.0.0  
**Milestone:** M1 — Foundation  
**Date:** September 16, 2026  
**Auditor:** AV Scan Engineering  

---

## 1. Commercial Licensing Policy

AV Scan is designed as a commercial, proprietary spatial capture platform. Every integrated third-party library, driver, and algorithmic dependency must be evaluated to ensure it does not compromise the proprietary nature of AV Scan's core Intellectual Property or impose unacceptable distribution constraints.

Categories of interest:
- **Permissive Licenses (Green):** MIT, BSD-2/3-Clause, Apache 2.0, MPL 2.0. Permitted for static or dynamic linking in commercial products.
- **Weak Copyleft Licenses (Yellow):** LGPLv3. Permitted with dynamic linking, provided Qt libraries remain unmodified or modifications are contributed, and end-users can replace the shared objects.
- **Strong Copyleft Licenses (Red / Process Isolation Required):** GPLv3. Must **never** be statically or directly linked into the proprietary `av_core` binary. If a GPLv3 component (e.g., FAST-LIO2) is deployed as a SLAM backend, it must run as an **isolated external worker process over an IPC or ROS bridge boundary**, protecting AV Scan's proprietary core from GPL copyleft contagion.

---

## 2. Milestone 1 Dependencies (Immediate)

| Component | Version | License | Commercial Compatibility Status | Notes / Compliance Strategy |
| :--- | :--- | :--- | :--- | :--- |
| **nlohmann/json** | 3.10.5 | **MIT** | **Approved (Green)** | Permissive header-only JSON library. Attribution included in commercial notice. |
| **Eigen3** | 3.4.0 | **MPL 2.0** | **Approved (Green)** | Mozilla Public License 2.0 allows commercial proprietary use. File-level copyleft does not extend to code calling Eigen templates. |
| **Qt 5 / Qt Quick** | 5.15.3 | **LGPLv3** | **Approved (Yellow)** | Dynamically linked (`libQt5Core.so`, `libQt5Gui.so`, `libQt5Quick.so`). User can swap libraries. Complies fully with commercial distribution. |
| **GCC / G++ Runtimes** | 11.4.0 | **GPLv3 with GCC Runtime Library Exception** | **Approved (Green)** | Runtime Library Exception explicitly permits proprietary binary generation. |
| **CMake** | 4.4.2 | **BSD-3-Clause** | **Approved (Green)** | Build automation tool only. Does not ship inside binary. |

---

## 3. Milestones 2–5 Dependencies (Projected)

| Component | Target Milestone | License | Commercial Compatibility Status | Architectural Strategy |
| :--- | :--- | :--- | :--- | :--- |
| **PCL (Point Cloud Library)** | M3, M4, M5 | **BSD-3-Clause** | **Approved (Green)** | Fully permissive commercial license. Used for filtering, normals, and cloud registration. |
| **GTSAM** | M3, M4, M7 | **BSD-3-Clause** | **Approved (Green)** | Permissive Georgia Tech license for factor-graph SLAM and loop closures. |
| **OpenCV** | M4, M5 | **Apache 2.0** | **Approved (Green)** | Permissive license for image processing, camera models, and reprojection. |
| **RoboSense `rslidar_sdk`** | M3 | **BSD-3-Clause** | **Approved (Green)** | Official RoboSense driver license permits commercial redistribution and embedding. |
| **Orbbec Camera SDK** | M4 | **Apache 2.0** | **Approved (Green)** | Official Orbbec SDK allows commercial hardware integration and wrapper distribution. |
| **FAST-LIO2** | M3 (LiDAR SLAM) | **GPLv3** | **Requires Process Isolation (Red)** | **Critical Architectural Isolation:** FAST-LIO2 is GPLv3. To preserve commercial closed-source integrity of AV Scan Core, the LIO backend is isolated as an out-of-process daemon communicating over IPC / socket. `av_core` links zero GPL code. |
| **TEASER++** | M7, M9 | **MIT** | **Approved (Green)** | Fast certified global registration under permissive MIT terms. |

---

## 4. Compliance Verification Checklist

- [x] All M1 core schemas and interfaces compile without any GPL dependencies.
- [x] Qt Quick runtime is dynamically linked; QML assets loaded via standard QML engine.
- [x] License attribution notices prepared for MIT, BSD, and MPL dependencies in `docs/THIRD_PARTY_NOTICES.md`.
- [x] Architectural boundary established for any future GPL SLAM algorithms: out-of-process IPC isolation via `ISlamBackend` adapter.

---

## 5. Conclusion

No commercial licensing blockers exist for M1 or the planned M2–M5 pipeline. All core technologies adhere strictly to permissive or isolated modular commercial standards.
