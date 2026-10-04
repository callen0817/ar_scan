# AV Scan Upstream Patch: ABot-Recon Jetson Orin Unified Memory & Allocator Fixes

**Target Upstream Repository:** `https://github.com/amap-cvlab/ABot-Recon`  
**Base Commit Hash:** `81a773722782bcc41655ea06792fa8a74e90b507`  
**Milestone:** M14.6 — ELP 48MP Neural Reconstruction Backend Qualification  

---

## 1. Rationale & Problem Description

When running ABot-Recon's streaming inference (`memory_mode="streaming"`) on NVIDIA Jetson AGX Orin with unified host/device memory under PyTorch:
1. `StreamingKVState.clone_buffers()` called Python's standard `copy.deepcopy(self)`. On Jetson's unified memory, deepcopying complex PyTorch tensor storage objects invokes CUDA memory allocation hooks that trigger Tegra heap exhaustion (`NvMapMemAllocInternalTagged: error 12`) and internal PyTorch NVML assertion errors (`NVML_SUCCESS == r INTERNAL ASSERT FAILED at CUDACachingAllocator.cpp:1131`).
2. `ABotReconNetwork._postprocess_stream_carry()` called `kv.clone_buffers()` a second time per frame, immediately after `sdpa.py` had already cloned the buffer and appended the committed frame. This doubled the transient KV allocations per step to ~2.5 GB.

---

## 2. Changes in `0001-jetson-orin-unified-memory-allocator-and-carry-optimization.patch`

1. **`abot_recon/modeling/streaming/kv_state.py`**:
   - Replaced `copy.deepcopy(self)` in `clone_buffers()` with an explicit `StreamingKVState(...)` initialization that explicitly calls `.clone()` on active tensor buffers (`reference_pk`, `reference_pv`, `summary_pk`, `summary_pv`, `window_pk`, `window_pv`).
2. **`abot_recon/modeling/streaming/network.py`**:
   - In `_postprocess_stream_carry()`, replaced `out.append(kv.clone_buffers())` with `out.append(kv)` to retain the already-committed carry state without creating redundant tensor clones.

---

## 3. How to Apply

```bash
cd third_party/abot_recon
git checkout 81a773722782bcc41655ea06792fa8a74e90b507
git apply ../../patches/abot_recon/0001-jetson-orin-unified-memory-allocator-and-carry-optimization.patch
```
