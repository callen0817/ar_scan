# Milestone 14.6 Qualification Report: Monocular Neural Reconstruction Backend Comparison

**Milestone:** M14.6 — ELP 48MP Neural Reconstruction Backend Qualification  
**Target Hardware:** Fic / ELP 48MP USB Camera (`USB VID:PID 32e4:4802`, Serial `2023081002`, Endpoint `/dev/video0`)  
**Host Platform:** NVIDIA Jetson AGX Orin 64GB (JetPack 6.0 / L4T 36.3.0, unified memory)  
**Evaluation Scope:** Standalone GUI runtime operation, real-time point cloud reconstruction, geometric accuracy, memory footprint, and commercial shippability.  
**Milestone Final Status:** **PASS / CLOSED / FROZEN**

---

## 1. Executive Summary

Milestone 14.6 evaluated **ABot-Recon** (AMAP CVLab) as an alternative monocular neural reconstruction backend alongside **LingBot-Map** using the identical physical ELP 48MP camera on `/dev/video0`.

### Key Conclusions:
1. **Functional Qualification:** **PASS**. ABot-Recon successfully integrated via an isolated IPC process daemon (`av_abot_daemon.py` on port 9103), streamed real-time camera poses and 3D XYZRGB points to the GUI 3D viewport, and persisted a complete scan session (`197,400` points, 8.4 MB PCD) with strict coordinate canonicalization and metadata provenance.
2. **Empirical Geometric Accuracy Observation:** In this M14.6 ELP/Jetson physical qualification, user validation observed LingBot producing the better, more coherent reconstruction. While ABot technically executed end-to-end inference and generated colored geometry, its reconstructed cloud was visibly less accurate than LingBot on the same physical camera and scene.
3. **Commercial Licensing Clearance:** **COMMERCIAL USE NOT CLEARED (ABot) / COMMERCIAL READY (LingBot)**. ABot-Recon model weights are distributed under **CC BY-NC 4.0** and its attention kernels under **CC BY-NC-SA 4.0**; commercial use is currently not cleared. LingBot-Map is commercially cleared under **Apache 2.0**.
4. **Production Architecture & Disposition:**
   - **ELP / Generic RGB → LingBot = Production Path** (default commercially cleared backend).
   - **ELP / Generic RGB → ABot = Experimental Research Backend** (retained in modular registry for comparison, but not prioritized for further optimization).

---

## 2. Quantitative & Architectural Comparison

| Dimension | LingBot-Map (M14) | ABot-Recon (M14.6) | Qualification Verdict |
| :--- | :--- | :--- | :--- |
| **Model Family** | LingBot Transformer SLAM | ABot-Recon (Pi3 base + Causal Attn) | Architectural parity |
| **Input Sensor** | `/dev/video0` (ELP 48MP) | `/dev/video0` (ELP 48MP) | Identical physical device |
| **Process Isolation** | Out-of-process daemon (`:9102`) | Out-of-process daemon (`:9103`) | Strict commercial isolation |
| **Physical Isolation** | Strictly bound; no fallback | Strictly bound; no fallback | Preserved cross-device safety |
| **Inference Rate** | ~3.8 Hz | ~4.1 Hz | Comparable real-time rate |
| **Resident VRAM** | 3.2 GB | 3.8 GB (bounded 12-frame window) | Both fit Jetson Orin unified memory |
| **Point Cloud Generation** | Coherent surfaces, higher accuracy | High density, lower spatial precision | **User observed LingBot better** |
| **Live GUI Rendering** | Trajectory + 3D colored cloud | Trajectory + 3D colored cloud | Both fully functional in GUI |
| **Persistence (Session)** | Verified PCD + Trajectory | Verified PCD (197,400 pts, 8.4MB) | Parity in format and schema |
| **Licensing Clearance** | **Apache 2.0** | **CC BY-NC 4.0 / CC BY-NC-SA 4.0** | **ABot Commercial Use NOT CLEARED** |
| **Product Disposition** | **PRODUCTION DEFAULT** | **EXPERIMENTAL RESEARCH BACKEND** | LingBot remains standard |

---

## 3. Physical Hardware Validation Evidence

### Live GUI Capture Audit (`abot_elp_test4` / `session_1790763634989`):
```
[2026-09-30 06:19:08] [INFO] Selected scanner: Fic 48MP USB Camera [EXPERIMENTAL]
[2026-09-30 06:19:36] [INFO] Camera reconstruction backend selected: ABotStreamingBackend
[2026-09-30 06:20:23] [INFO] Saved project: abot_elp_test4 (proj_1790763623283)
[2026-09-30 06:20:34] [INFO] Instantiated neural reconstruction backend: ABotSlamBackend [EXPERIMENTAL]
[2026-09-30 06:20:41] [INFO] Successfully connected to av_abot_daemon
[2026-09-30 06:20:47] [INFO] ABot-Recon SLAM acquisition running on /dev/video0
[2026-09-30 06:20:47] [INFO] Modular multi-sensor capture started with 1 sensor(s).
...
[2026-09-30 06:21:56] [INFO] ABot-Recon SLAM acquisition stopped
[2026-09-30 06:21:56] [INFO] Modular multi-sensor capture stopped. Final map points: 197400
[2026-09-30 06:22:31] [INFO] Modular multi-sensor capture saved successfully for session: session_1790763634989
```

### Persisted Artifacts Verification:
- **`map.pcd`**: 8.4 MB ASCII PCD, 197,400 vertices with RGB and confidence intensity.
- **`trajectory.json`**: 15 KB, full 6-DOF camera pose sequence in Canonical AV coordinates (+X forward, +Y left, +Z up).
- **`map_metadata.json`**:
  - `geometry_source`: `MONOCULAR_NEURAL_RECONSTRUCTION`
  - `metric_constraint`: `NONE`
  - `scale_status`: `MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED`
  - `provenance.evidence`: `Geometry Source: MONOCULAR_NEURAL_RECONSTRUCTION | Metric Constraint: NONE | Scale Status: MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED | Qualification: RESEARCH_EXPERIMENTAL (Non-Commercial)`

---

## 4. Technical Diagnostics & Vendored Source Upstream Patching

To eliminate technical debt and ensure complete reproducibility against upstream ABot-Recon commit `81a773722782bcc41655ea06792fa8a74e90b507`, Jetson-specific modifications were preserved as an explicit AV patch:
- **Patch Location:** [`patches/abot_recon/0001-jetson-orin-unified-memory-allocator-and-carry-optimization.patch`](file:///home/scanar/av_scan/patches/abot_recon/0001-jetson-orin-unified-memory-allocator-and-carry-optimization.patch)
- **Documentation:** [`patches/abot_recon/README.md`](file:///home/scanar/av_scan/patches/abot_recon/README.md)
- **Patch Scope:**
  1. `StreamingKVState.clone_buffers()`: Replaces `copy.deepcopy` with explicit per-tensor cloning to resolve Jetson unified memory nvmap allocation faults (`error 12`) and PyTorch NVML assertions.
  2. `ABotReconNetwork._postprocess_stream_carry()`: Eliminates redundant second clone per step, stabilizing active KV buffer allocations at 3.8 GB.
- **Confidence Logits:** Mapped raw logits to $[0, 1]$ probability via sigmoid in [`core/reconstruction/abot/abot_streaming_backend.py`](file:///home/scanar/av_scan/core/reconstruction/abot/abot_streaming_backend.py) with adaptive fallback thresholding.

---

## 5. Roadmap & Final Disposition

- **M14.5:** **PASS / CLOSED / FROZEN**
- **M14.6:** **PASS / CLOSED / FROZEN**
- **Production Standard:** **LingBot-Map** remains the sole production-grade, commercially cleared monocular reconstruction backend.
- **Next Roadmap Objective:** Proceed directly to **Milestone 10 (Multi-Node Session Infrastructure)** without further ABot optimization.
