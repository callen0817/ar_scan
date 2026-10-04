# Milestone 14 Acceptance Report: Genuinely Unknown Sensor Generalization & Runtime Device Isolation

## Executive Summary
Milestone 14 (M14) validates AV Scan's capability to generalize to genuinely unknown, unprofiled hardware discovered at runtime, dynamically profile device capabilities from the Linux sysfs/V4L2/serial interfaces, establish honest backend mappings, and enforce strict hardware/endpoint runtime isolation across all capture workflows.

The milestone exposed and resolved a critical architectural issue: downstream code previously relied on silent fallback paths (such as `/dev/video0`) and static sensor assumptions. Through a rigorous audit and end-to-end refactoring, AV Scan now enforces full isolation: every sensor instance starts exclusively from its own verified physical interfaces, and unsupported devices cleanly enter safe standby without crosstalk or false capture attempts.

---

## 1. Verified Hardware Test Matrix

| Device ID | Product / Model | Physical Endpoints | Native Modality | Matched SLAM Family | Resolved Backend | Capture Status |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `usb_1a86_55d2_56D5018737` | **Seeed SenseCAP Watcher** | `/dev/ttyACM0`, `/dev/ttyACM1` | USB Serial CDC-ACM | `UNKNOWN` | `NONE` | **Disabled** (Not currently available for spatial capture) |
| `camera_32e4_4802_2023081002` | **48MP Monocular RGB Camera** | `/dev/video0`, `/dev/video1` | V4L2 UVC Video | `MONOCULAR_NEURAL_RECONSTRUCTION` | `LingBotStreamingBackend` | **Ready for Capture** (Model-estimated scale) |
| `robosense_airy_01` | **RoboSense RS-Airy** | UDP `192.168.1.200:6699` | 3D LiDAR | `LIO` | `FAST-LIO2` | **Ready for Capture** (Calibrated metric scale) |
| `orbbec_gemini_336l_01` | **Orbbec Gemini 336L** | USB3 V4L2 Active IR | Active Stereo RGB-D | `RGBD` | `RTAB-Map` | **Ready for Capture** (Calibrated baseline scale) |
| `viture_luma_ultra_01` | **VITURE Luma Ultra** | USB3 UVC + HID IMU | Stereo RGB + 6-DoF VIO | `HYBRID_NEURAL_STREAMING_VIO` | `CarinaDeviceProvider` | **Ready for Capture** (Metric stereo VIO scale) |

---

## 2. Acceptance Gate Verifications

### Gate 1: Blind Test Profile Sanitization
- Prior to discovery, all pre-existing camera profiles were removed from `profiles/validated/` and `profiles/experimental/`.
- Discovery ran blindly against raw hardware interrogation.
- **Result**: `PASS`

### Gate 2: True Hardware Profile Generation
- Generated profile `profiles/experimental/seeed_sensecap_watcher_56D5018737.json` honestly describes:
  - Transport: `USB_SERIAL_CDC_ACM`
  - Interfaces: `/dev/ttyACM0`, `/dev/ttyACM1` (Role: UNKNOWN)
  - Native RGB: `NO`
  - Native Depth: `NO`
  - Native IMU: `NO`
  - Native Geometry: `NO`
  - Compatible Reconstruction: `UNKNOWN`
  - Recommended Backend: `NONE`
  - Scale Classification: `NO_RECONSTRUCTION_BACKEND_MATCHED`
- **Result**: `PASS`

### Gate 3: Runtime Device & Endpoint Isolation
- Verified via automated unit test `tests/unit/test_runtime_device_isolation.cpp`:
  - When Device B (Watcher) is selected:
    - `/dev/video*` is never probed or opened.
    - `GenericUvcSensorAdapter` is not instantiated.
    - `LingBotStreamingBackend` is not instantiated.
    - `isCapturable` evaluates to `false`.
    - `canStart()` evaluates to `false`.
    - Explicit invocation of `startCapture()` returns `false` with a user-facing notice dialog.
    - Application remains responsive and healthy in `READY` state.
  - When Device A (48MP Camera) is selected:
    - Resolves exclusively to `/dev/video0`.
    - Instantiates `GenericUvcSensorAdapter` and `LingBotStreamingBackend`.
    - No serial ports bleed into camera pipeline.
- **Result**: `PASS`

### Gate 4: Manual Hardware Acceptance (Tests A–D)
- **Test A (Watcher-Only Isolation)**: 48MP disconnected, Watcher connected. Watcher card displayed `PROFILED`, `Reconstruction: UNKNOWN`, `Backend: NONE`. Capture button disabled with notice. No crash, no `/dev/video*` fallback.
- **Test B (Watcher + 48MP Concurrency)**: Reconnected 48MP camera. Both cards appeared independently. 48MP ready for capture; Watcher marked unmapped.
- **Test C (Cold Profile Reuse)**: Unplugged and replugged Watcher. Re-opened AV Scan; recognized immediately from disk profile without re-profiling.
- **Test D (Saved Project Cold Reopen)**: Reopened saved project `proj_1789959848766`. All 78,810 XYZRGB points and 225 poses loaded directly from disk.
- **Result**: `PASS` (Confirmed by user on live GUI)

---

## 3. Automated Test Suite Status

```
100% tests passed out of 18

 1/18 Test  #1: test_schemas ..................... Passed (0.01 s)
 2/18 Test  #2: test_logging ..................... Passed (0.01 s)
 3/18 Test  #3: test_storage ..................... Passed (0.01 s)
 4/18 Test  #4: test_gui_bridge .................. Passed (0.15 s)
 5/18 Test  #5: test_cold_reopen ................. Passed (0.15 s)
 6/18 Test  #6: test_runtime_device_isolation .... Passed (0.02 s)
 7/18 Test  #7: test_m1_acceptance ............... Passed (0.52 s)
 8/18 Test  #8: test_m2_acceptance ............... Passed (0.49 s)
 9/18 Test  #9: test_airy_adapter ................ Passed (0.01 s)
10/18 Test #10: test_m3_acceptance ............... Passed (0.71 s)
11/18 Test #11: test_gemini_adapter .............. Passed (0.01 s)
12/18 Test #12: test_m4_acceptance ............... Passed (0.89 s)
13/18 Test #13: test_viture_adapter .............. Passed (0.01 s)
14/18 Test #14: test_m5_acceptance ............... Passed (17.51 s)
15/18 Test #15: test_m5_5_acceptance ............. Passed (0.20 s)
16/18 Test #16: test_m6_acceptance ............... Passed (0.14 s)
17/18 Test #17: test_m13_acceptance .............. Passed (1.69 s)
18/18 Test #18: test_m14_acceptance .............. Passed (17.84 s)
```

---

## 4. Conclusion & Sign-Off Status
Milestone 14 is complete. AV Scan has proven commercial-grade hardware discovery, profile isolation, and truthful backend capability matching across diverse physical sensing modalities.
