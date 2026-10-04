# ABot-Recon Provenance & Commercial Licensing Audit Record

**Date:** 2026-09-29  
**Milestone:** M14.6 — ELP 48MP Neural Reconstruction Backend Qualification  
**Qualification State:** `RESEARCH / EXPERIMENTAL`  
**Commercial Release Clearance:** `NOT CLEARED / COMMERCIAL USE PROHIBITED WITHOUT SEPARATE LICENSING`

---

## 1. Upstream Repository & Checkpoint Provenance

| Item | Details |
| :--- | :--- |
| **Project Name** | ABot-Recon (AMAP CVLab / Alibaba Group) |
| **Repository URL** | `https://github.com/amap-cvlab/ABot-Recon` |
| **Exact Git Commit SHA** | `81a773722782bcc41655ea06792fa8a74e90b507` |
| **Branch / Ref** | `main` (commit date: 2026-09-25 22:51:04 +0800) |
| **Top-Level Code License** | Apache License 2.0 (stated in `LICENSE`) |
| **Model Weights Source** | Hugging Face Hub: `acvlab/ABot-Recon` |
| **Model Commit SHA** | `c69c26ca1853afc9c9212014459e729d7339ecdf` |
| **Primary Checkpoint File** | `abot_recon.safetensors` (3.82 GB / 4,003,866,352 bytes) |
| **Weights License** | **CC BY-NC 4.0** (Creative Commons Attribution-NonCommercial 4.0 International) |

---

## 2. Complete Dependency & Licensing Chain Audit

While the repository root contains an Apache-2.0 `LICENSE` file, the core runtime dependencies, derived code, and model weights have non-commercial and copyleft constraints that legally block commercial product distribution:

```
ABot-Recon (Apache 2.0 wrapper)
├── Model Weights (abot_recon.safetensors)
│   └── License: CC BY-NC 4.0 [NON-COMMERCIAL ONLY]
│       └── Derived from: Pi3 released weights (CC BY-NC 4.0)
│
├── Core Geometric Reconstruction & Image Encoding
│   ├── Derived from: Pi3 (https://github.com/yyfz/Pi3)
│   │   ├── Code License: BSD 3-Clause
│   │   └── Released Model Weights: CC BY-NC 4.0 [NON-COMMERCIAL]
│   └── cuRoPE2D / CroCo / DUSt3R
│       ├── Source location: `abot_recon/modeling/pi3/models/curope/`
│       ├── Origin: NAVER Corporation (CroCo / DUSt3R)
│       └── License: CC BY-NC-SA 4.0 [NON-COMMERCIAL + SHARE-ALIKE]
│
├── Optional Loop-Closure Module (Not used for baseline streaming)
│   ├── DINOv2: Apache 2.0 (Meta AI)
│   └── SALAD: Independent research checkpoint
│
└── Attention & Inference Engine
    ├── PyTorch Native SDPA: BSD 3-Clause
    └── FlashInfer (Optional): Apache 2.0
```

### Critical Findings:
1. **Model Weights (`CC BY-NC 4.0`)**:
   Under `MODEL_LICENSE.md`, the released weights are derived from Pi3 and are explicitly licensed under Creative Commons Attribution-NonCommercial 4.0. Section 2 states:
   > *"The source-code license in LICENSE does not grant commercial rights to the model weights. Commercial use requires separate written authorization from all relevant rights holders."*
2. **CUDA Kernels (`CC BY-NC-SA 4.0`)**:
   Under `THIRD_PARTY_NOTICES.md`, CUDA sources under `abot_recon/modeling/pi3/models/curope/` retain upstream notices from NAVER Corporation and are licensed under **CC BY-NC-SA 4.0** (Non-Commercial, Share-Alike).
3. **Product Clearance Verdict**:
   **ABot-Recon CANNOT be shipped as a commercial feature in AV Scan without commercial licensing from Alibaba/AMAP CVLab, the Pi3 authors, and NAVER Corporation.**
   It is admitted into AV Scan strictly as a **RESEARCH / EXPERIMENTAL** qualification target to benchmark local-context transformer SLAM on the Jetson Orin NX.

---

## 3. Metadata & User Presentation Invariant

Any UI element, telemetry string, or documentation in AV Scan referencing ABot-Recon must adhere to:

```json
{
  "backend_name": "ABotStreamingBackend",
  "display_name": "ABot-Recon (AMAP CVLab) [EXPERIMENTAL]",
  "qualification_tier": "RESEARCH_EXPERIMENTAL",
  "commercial_status": "NOT CLEARED / NON-COMMERCIAL LICENSE",
  "license_flags": ["CC-BY-NC-4.0", "CC-BY-NC-SA-4.0"],
  "scale_classification": "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED"
}
```
