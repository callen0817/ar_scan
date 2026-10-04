# Milestone 10: Multi-Node Session Infrastructure Implementation Report

**Milestone Identifier:** M10 — Multi-Node Session Infrastructure  
**Implementation Date:** 2026-10-01  
**Target Hardware:** NVIDIA Jetson Orin NX (Linux ARM64 reference node)  
**Acceptance Test Suite:** `tests/acceptance/test_m10_acceptance.cpp` (All 7 Gates Passed)  
**Milestone Status:** **IMPLEMENTATION COMPLETE / DEFERRED SIGN-OFF (OPEN / AWAITING PHYSICAL CROSS-PLATFORM VALIDATION IN M15)**

---

## Executive Summary

Milestone 10 establishes the core infrastructure for multiple independent AV Scan installations and devices to participate in the same logical AV Scan session. M10 governs **coordination, persistent identity, multi-node persistence, synchronization metadata, and independent capture orchestration**.

Per explicit architectural requirements, M10 does **NOT** perform cross-node point cloud registration, spatial map fusion, shared SLAM state, distributed pose-graph optimization, or automatic coordinate frame alignment. Each scanner node strictly captures and reconstructs its own spatial data in its own independent coordinate frame.

In strict accordance with the user directives:
> *"Acceptance Gate: M10 implementation may be completed now, but M10 must remain OPEN / AWAITING PHYSICAL CROSS-PLATFORM VALIDATION. Do not declare M10 PASS merely because unit tests pass or localhost tests pass. The final physical acceptance is deliberately deferred until M15 gives us the second AV Scan desktop platform (WINDOWS AV SCAN NODE <-> real network <-> LINUX/JETSON AV SCAN NODE)."*

This report documents the completed schemas, storage architecture, network wire protocols, standalone GUI mesh dialog, and the 7-gate acceptance test verification.

---

## 1. Architectural Architecture & Invariants

```
PROJECT
└── SESSION
    ├── SCANNER_NODE A (Jetson Orin NX)
    │   ├── SENSOR_INSTANCE (Physical Devices / Profiles)
    │   └── CAPTURE A (owner_node_uuid: A, independent local frame)
    └── SCANNER_NODE B (Windows Desktop / Secondary Node)
        ├── SENSOR_INSTANCE (Physical Devices / Profiles)
        └── CAPTURE B (owner_node_uuid: B, independent local frame)
```

### Key Invariants Enforced
1. **Persistent Node Identity:** Each AV Scan installation maintains a permanent RFC 4122 UUIDv4 saved to `~/.av_scan/node_identity.json`. Identity persists across restarts, reboots, network interface changes, IP address updates, and sensor swaps.
2. **Session-Specific Roles:** Roles (`SESSION_COORDINATOR`, `SESSION_PARTICIPANT`) are scoped strictly per-session, not baked into device identity.
3. **Capture Ownership Invariance:** Every capture possesses an immutable `owner_node_uuid_`. A node cannot consume or re-label another node's sensor or capture.
4. **Network Resilience Invariant:** Once a capture is started locally, immediate or prolonged network disconnects do NOT abort or corrupt the local capture. Reconnecting resumes session membership and syncs metadata without interrupting scanning.
5. **Strict Spatial Coordinate Segregation:** Point clouds and SLAM graphs are preserved within the capturing node's local frame. Merging/fusion is deferred to M11.

---

## 2. Implemented Subsystems & Modules

### 2.1 Persistent Node Identity Manager
- **Location:** [`core/multi_node/node_identity_manager.hpp`](file:///home/scanar/av_scan/core/multi_node/node_identity_manager.hpp), [`core/multi_node/node_identity_manager.cpp`](file:///home/scanar/av_scan/core/multi_node/node_identity_manager.cpp)
- **Features:**
  - Thread-safe singleton managing the local node identity.
  - Automatically interrogates OS platform (`LINUX_ARM64`, `LINUX_X64`, `WINDOWS_X64`, `MACOS_ARM64`, `MACOS_X64`, `IOS_ARM64`, `UNKNOWN`).
  - Queries hardware model dynamically (e.g. `NVIDIA Jetson Orin NX Engineering Reference Developer Kit` on Linux Tegra).
  - Maintains configurable display name and updates primary IP endpoint on network changes while strictly holding UUID invariant.

### 2.2 Core Schemas & Storage Hierarchy
- **Location:** [`core/schemas/scanner_node.hpp`](file:///home/scanar/av_scan/core/schemas/scanner_node.hpp), [`core/schemas/capture.hpp`](file:///home/scanar/av_scan/core/schemas/capture.hpp), [`core/schemas/session.hpp`](file:///home/scanar/av_scan/core/schemas/session.hpp), [`core/storage/storage_engine.cpp`](file:///home/scanar/av_scan/core/storage/storage_engine.cpp)
- **Features:**
  - Augmented `ScannerNode` with `node_uuid_`, `hardware_model_`, `software_version_`, `supported_transports_`, `clock_sync_source_`, and `clock_uncertainty_ms_`.
  - Augmented `Capture` with explicit `owner_node_uuid_`, `requested_start_time_ns_`, and `actual_local_start_time_ns_`.
  - Preserved complete backward compatibility with pre-M10 capture files.
  - Implemented session-scoped node persistence:
    `projects/<proj_id>/sessions/<sess_id>/scanner_nodes/<node_uuid>.json`

### 2.3 LAN Discovery & Routed Directed Unicast (UDP Port 9110)
- **Location:** [`core/multi_node/discovery_service.hpp`](file:///home/scanar/av_scan/core/multi_node/discovery_service.hpp), [`core/multi_node/discovery_service.cpp`](file:///home/scanar/av_scan/core/multi_node/discovery_service.cpp)
- **Features:**
  - Periodic background UDP beaconing on port 9110 broadcasting node identity, role, and coordination port.
  - Non-blocking socket listener with duplicate suppression and peer expiration tracking.
  - Dedicated support for non-broadcast/routed networks (e.g. Tailscale, WireGuard, corporate VPNs) via `addKnownPeer(ip, port)`.

### 2.4 TCP Coordination Service (Port 9111)
- **Location:** [`core/multi_node/coordination_service.hpp`](file:///home/scanar/av_scan/core/multi_node/coordination_service.hpp), [`core/multi_node/coordination_service.cpp`](file:///home/scanar/av_scan/core/multi_node/coordination_service.cpp)
- **Wire Protocol:** Delimited JSON stream over TCP port 9111 implementing Protocol Version 1.0.
- **Message Types:** `HELLO`, `INVITE_SESSION`, `JOIN_SESSION`, `LEAVE_SESSION`, `PREPARE_CAPTURE`, `READY_FOR_CAPTURE`, `START_CAPTURE`, `STOP_CAPTURE`, `CAPTURE_STATUS`, `SESSION_STATUS`, `HEARTBEAT`.
- **Two-Phase Capture:** `PREPARE_CAPTURE` -> nodes acknowledge `READY_FOR_CAPTURE` -> coordinator issues `START_CAPTURE`. Commands are strictly idempotent.
- **Heartbeat & Clock Offset Tracking:** Exchanges monotonic timestamps to calculate and record relative clock offsets as synchronization metadata without synthetic fabrication.

### 2.5 Multi-Node Session Orchestrator & GUI Bridge
- **Location:** [`core/multi_node/multi_node_session_orchestrator.hpp`](file:///home/scanar/av_scan/core/multi_node/multi_node_session_orchestrator.hpp), [`gui/qml_bridge.hpp`](file:///home/scanar/av_scan/gui/qml_bridge.hpp), [`gui/qml_bridge.cpp`](file:///home/scanar/av_scan/gui/qml_bridge.cpp)
- **UI Components ([`gui/qml/AppWindow.qml`](file:///home/scanar/av_scan/gui/qml/AppWindow.qml)):**
  - **Header Bar Button:** `🌐 Multi-Node Mesh` reflecting live connection status.
  - **Viewport Mesh Bar:** Displays active session role and prominent notice: `MESH: <ROLE> | INDEPENDENT COORDINATE FRAMES (NON-FUSED)`.
  - **Multi-Node Mesh Dialog:** Complete management interface with:
    - Persistent Node Identity card (UUID, Platform, Hardware, Name).
    - Session Coordinator / Participant status and controls.
    - Routed / Tailscale manual peer discovery input.
    - Discovered nodes table with Invite and Join capabilities.
    - Connected session members table with real-time status and clock offset.
    - Coordinated Start and Stop controls.
    - Clear spatial coordinate segregation notice.

---

## 3. Acceptance Test Results (`test_m10_acceptance`)

All 7 gates of the comprehensive M10 acceptance test suite were executed and verified on the reference Jetson platform:

| Gate | Description | Verified Criteria | Result |
|---|---|---|---|
| **Gate 1** | Node Identity & Platform Interrogation | Persistent UUIDv4 generated, saved, and loaded; hardware model (`Jetson Orin NX`) detected; IP change leaves UUID invariant; enums validated. | **PASS** |
| **Gate 2** | Schemas & Backward Compatibility | `ScannerNode`, `Capture` (explicit `owner_node_uuid`), `Session` schema roundtrips; legacy capture JSON loads without regression. | **PASS** |
| **Gate 3** | Session-Scoped Node Persistence | Storage engine saves nodes to `projects/.../sessions/.../scanner_nodes/<uuid>.json` without file collisions; reloads cleanly. | **PASS** |
| **Gate 4** | Discovery Service | UDP beaconing on port 9110, broadcast reception, directed unicast for routed networks (Tailscale/VPNs) verified between independent nodes. | **PASS** |
| **Gate 5** | Coordination Wire Protocol | TCP 9111 handshake (`HELLO`, `JOIN_SESSION`, `SESSION_STATUS`), Two-Phase Capture (`PREPARE_CAPTURE`, `READY_FOR_CAPTURE`, `START_CAPTURE`), server-side idempotency, telemetry. | **PASS** |
| **Gate 6** | Network Loss Resilience Invariant | Abrupt TCP socket severance during active capture does NOT abort or corrupt local capture; scanner state remains undisturbed. | **PASS** |
| **Gate 7** | Spatial Frame Independence | Captures retain independent `owner_node_uuid` values; strict spatial non-fusion invariant verified. | **PASS** |

### Complete Regression Suite Status
```
100% tests passed out of 20
Total Test time (real) = 66.06 sec
Zero regressions across all existing M1 through M14.6 test suites.
```

---

## 4. Acceptance Gate Decision & Roadmap

In adherence to the roadmap:
- **Milestone 10 Implementation:** **COMPLETE & VERIFIED**
- **Milestone 10 Milestone Status:** **DEFERRED SIGN-OFF (OPEN / AWAITING PHYSICAL CROSS-PLATFORM VALIDATION IN M15)**
- **Next Milestone:** **Milestone 5.5 — Canonical AV Spatial Data Qualification (Physical Verification of Real World Geometry, Scale, and Coordinate Conventions)** followed by **Milestone 15 (Windows Desktop Portability)** for the final physical cross-platform multi-node sign-off.
