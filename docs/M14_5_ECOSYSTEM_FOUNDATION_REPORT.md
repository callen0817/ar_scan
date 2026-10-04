# AV Scan — Milestone 14.5 Acceptance Report
## Universal Sensor Ecosystem & Compatibility Foundation

**Date:** 2026-09-22  
**Status:** **PASS / CLOSED**  
**Milestone:** M14.5  
**Authoritative Matrix Artifact:** [`docs/ecosystem_capability_matrix.json`](file:///home/scanar/av_scan/docs/ecosystem_capability_matrix.json)

---

### Executive Summary

Milestone 14.5 establishes the authoritative **Universal Sensor Ecosystem & Compatibility Foundation** for AV Scan. Rather than treating sensor onboarding as ad-hoc driver invocations, AV Scan now features a principled, multi-generational hardware knowledge base, an automated capability matrix generator, and a strict **6-tier hierarchical sensor resolution pipeline**.

Hardware support is formally decoupled:
$$\text{Hardware Support} \neq \text{Firmware Support} \neq \text{SDK Support} \neq \text{AV Scan Support}$$

Older devices are supported cleanly through dedicated legacy adapters without forcing firmware upgrades or downgrading modern production sensors.

---

### Key Vendor Ecosystems & Generational Scope

AV Scan indexes and models hardware across **15 vendor families** and 24 device models across **Current**, **Previous**, and **Legacy** lifecycles:

| Vendor | Models Indexed | Current Generation | Previous Generation | Legacy Generation | Primary Transport | Recommended Backend |
|---|---|---|---|---|---|---|
| **Ouster** | 3 | Rev8 Native RGB (OS1 Max/128 RGB, FW 4.0+) | Rev7 (OS1-64/128 non-RGB, FW 3.x) | Gen1 (OS1-16/64 RevC/D/5/6, FW 2.x) | UDP Ethernet | `FAST-LIO2` |
| **RoboSense** | 3 | RS-Airy, RS-Helios-16/32 | RS-Ruby, RS-Bpearl | RS-LiDAR-16 / 32 | UDP Ethernet | `FAST-LIO2` |
| **Unitree** | 2 | Unitree L2 (4D LiDAR + 6-axis IMU) | Unitree L1 | — | UDP Eth / Serial | `FAST-LIO2` |
| **Hesai** | 2 | AT128 Solid-State, OT128 | Pandar XT32 | Pandar 40P | UDP Ethernet | `FAST-LIO2` |
| **Livox** | 2 | Mid-360 (SDK2, built-in IMU), HAP | Horizon | Mid-40 / Mid-100 (SDK1) | UDP Ethernet | `FAST-LIO2` |
| **LSLiDAR** | 1 | C16 / C32 Mechanical, CX series | N301 | — | UDP Ethernet | `FAST-LIO2` |
| **Valeo** | 1 | SCALA 3 Automotive | SCALA 2 | SCALA 1 | Automotive Ethernet | `FAST-LIO2` |
| **Innoviz** | 1 | InnovizTwo | InnovizOne | — | TCP/UDP (.invz) | `FAST-LIO2` |
| **Seyond** | 1 | Falcon K2 / Robin (`inno-lidar-sdk`) | Falcon Kinetic | — | UDP Ethernet | `FAST-LIO2` |
| **MorpheusTEK** | 1 | Morpheus 3D Perception LiDAR | — | — | UDP Ethernet | `FAST-LIO2` |
| **Orbbec** | 2 | Gemini 336L Active Stereo, Femto Bolt | Astra+ | Astra Mini / Pro (OpenNI2) | USB UVC | `RTAB-Map` |
| **Intel** | 1 | RealSense D435i, D455 | RealSense L515 MEMS | F200 / SR300 | USB UVC | `RTAB-Map` |
| **Luxonis** | 1 | OAK-D Pro (Stereo + IR + IMU) | OAK-1 | — | USB UVC | `RTAB-Map` |
| **Stereolabs** | 1 | ZED 2i, ZED X | ZED Mini | Original ZED USB 3.0 | USB UVC | `RTAB-Map` |
| **Generic** | 2 | Generic UVC RGB Camera | — | Generic CDC-ACM Serial | V4L2 / USB Serial | `LingBotStreamingBackend` / `NONE` |

---

### Architectural Rules Enforced

#### 1. Decoupling Hardware, Firmware, and SDK Support
AV Scan begins with the latest official production SDK for each manufacturer. When hardware running older firmware (e.g. Ouster Gen1 running FW 2.2) is connected:
- AV Scan does **not** fail discovery.
- AV Scan does **not** force a destructive firmware upgrade on the user.
- AV Scan activates the designated legacy adapter (`OusterLegacyAdapter`) and driver protocol branch while leaving Rev8 devices routed to modern Rev8 pipelines.

#### 2. Critical Ecosystem Disambiguations
- **Ouster Rev8 vs Rev7**: Rev8 introduces native single/dual-return color (`has_rgb = true`, FW 4.0+, SDK 0.16+). Rev7 OS0/OS1/OSDome are non-RGB (`has_rgb = false`, FW 3.x).
- **Unitree L1 vs L2**: Unitree maintains separate official SDKs. L2 exposes 4D point clouds + 6-axis IMU over UDP Ethernet, while L1 operates over serial. Both are distinct model definitions.
- **Seyond vs Innoviz**: `inno-lidar-sdk` belongs to Seyond (supporting Falcon and Robin). Innoviz operates its own proprietary SDK/API for InnovizOne and InnovizTwo.

#### 3. Strict 6-Tier Hierarchical Sensor Resolution Pipeline
```
CONNECTED SENSOR
      ↓
[Tier 1] Exact AV-validated profile on disk?
      ├─ YES → Use exact validated profile (AV_VALIDATED)
      ↓ NO
[Tier 2] Known model definition in ecosystem registry?
      ├─ YES → Interrogate hardware with vendor adapter
      ↓ NO
[Tier 3] Recognized vendor SDK?
      ├─ YES → Vendor SDK interrogates hardware -> generate EXPERIMENTAL profile
      ↓ NO
[Tier 4] Recognized standard transport? (UVC video, USB CDC-ACM serial, Ethernet)
      ↓
[Tier 5] Generic M14 profiler interrogation
      ↓
[Tier 6] Enough verified spatial capture capability?
      ├─ YES → Match compatible spatial backend
      └─ NO  → Safe fallback: PROFILED / UNKNOWN / NONE (UNSUPPORTED)
```

---

### Test & Qualification Results

All 19 test suites in `ctest` passed with 100% success rate:

```
Test project /home/scanar/av_scan/build
      Start  1: test_schemas
 1/19 Test  #1: test_schemas .....................   Passed    0.01 sec
      Start  2: test_logging
 2/19 Test  #2: test_logging .....................   Passed    0.01 sec
      Start  3: test_storage
 3/19 Test  #3: test_storage .....................   Passed    0.01 sec
      Start  4: test_gui_bridge
 4/19 Test  #4: test_gui_bridge ..................   Passed    0.19 sec
      Start  5: test_cold_reopen
 5/19 Test  #5: test_cold_reopen .................   Passed    0.18 sec
      Start  6: test_runtime_device_isolation
 6/19 Test  #6: test_runtime_device_isolation ....   Passed    0.02 sec
      Start  7: test_sensor_ecosystem
 7/19 Test  #7: test_sensor_ecosystem ............   Passed    0.01 sec
      Start  8: test_m1_acceptance
 8/19 Test  #8: test_m1_acceptance ...............   Passed    0.52 sec
      Start  9: test_m2_acceptance
 9/19 Test  #9: test_m2_acceptance ...............   Passed    0.36 sec
      Start 10: test_airy_adapter
10/19 Test #10: test_airy_adapter ................   Passed    0.01 sec
      Start 11: test_m3_acceptance
11/19 Test #11: test_m3_acceptance ...............   Passed    0.71 sec
      Start 12: test_gemini_adapter
12/19 Test #12: test_gemini_adapter ..............   Passed    0.01 sec
      Start 13: test_m4_acceptance
13/19 Test #13: test_m4_acceptance ...............   Passed    0.90 sec
      Start 14: test_viture_adapter
14/19 Test #14: test_viture_adapter ..............   Passed    0.01 sec
      Start 15: test_m5_acceptance
15/19 Test #15: test_m5_acceptance ...............   Passed   17.27 sec
      Start 16: test_m5_5_acceptance
16/19 Test #16: test_m5_5_acceptance .............   Passed    0.20 sec
      Start 17: test_m6_acceptance
17/19 Test #17: test_m6_acceptance ...............   Passed    0.11 sec
      Start 18: test_m13_acceptance
18/19 Test #18: test_m13_acceptance ..............   Passed    1.72 sec
      Start 19: test_m14_acceptance
19/19 Test #19: test_m14_acceptance ..............   Passed    2.83 sec

100% tests passed out of 19
Total Test time (real) = 25.06 sec
```

---

### Conclusion & Next Roadmap Step

Milestone 14.5 is **PASS / CLOSED**. The universal ecosystem foundation and capability matrix provide the concrete backing required for multi-node operations and platform portability.

Following the sequence:
$$\text{M14.5} \ \checkmark \longrightarrow \textbf{M10 (Multi-Node Session Foundation)} \longrightarrow \textbf{M5.5 (Canonical Spatial Conventions)} \longrightarrow \textbf{M15 (Windows Desktop Portability)} \longrightarrow \textbf{Joint M10 + M15 Acceptance}$$

We are ready to proceed to **Milestone 10: Multi-Node Session Infrastructure**.
