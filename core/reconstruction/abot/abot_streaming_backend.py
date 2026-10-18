#!/usr/bin/env python3
"""
AV Scan — ABot-Recon Streaming 3D Reconstruction Backend
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and open-source ABot-Recon foundation model.

QUALIFICATION STATE: RESEARCH / EXPERIMENTAL
COMMERCIAL USE: NOT CLEARED / SEPARATE COMMERCIAL LICENSE REQUIRED

PROVENANCE:
- Repository: https://github.com/amap-cvlab/ABot-Recon (Commit 81a773722)
- Upstream Code License: Apache-2.0
- Model Weights License: CC BY-NC 4.0
- cuRoPE2D / CroCo / DUSt3R: CC BY-NC-SA 4.0
- Pi3 Base Weights: CC BY-NC 4.0

CHARACTERISTICS:
- Fixed 12-frame local attention context window (constant VRAM vs sequence length).
- Streaming adjacent relative camera pose composition.
- Dense local point map generation per keyframe.
- Authentic RGB color assignment from physical camera stream.
- Strict canonical AV coordinate transformation (+X forward, +Y left, +Z up).
"""

import os
import sys

os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

import gc
import time
import math
import threading
from collections import deque
from typing import Optional, Callable, Tuple, List, Dict, Any
from pathlib import Path

import numpy as np
import cv2
import torch
import torchvision.transforms.functional as tvf

# Ensure abot_recon repository is in python path
AV_SCAN_ROOT = Path("/home/scanar/av_scan")
ABOT_REPO_PATH = AV_SCAN_ROOT / "third_party" / "abot_recon"
if str(ABOT_REPO_PATH) not in sys.path:
    sys.path.insert(0, str(ABOT_REPO_PATH))

try:
    from abot_recon import ABotRecon
    from abot_recon.config import InferenceConfig
    from abot_recon.modeling.streaming.network import detach_carry
    HAS_ABOT = True
except ImportError as e:
    HAS_ABOT = False
    ABOT_IMPORT_ERROR = str(e)


class ABotStreamingBackend:
    """
    Isolated streaming reconstruction backend utilizing ABot-Recon.
    """
    def __init__(
        self,
        checkpoint_path: str = "/home/scanar/av_scan/third_party/abot_recon/checkpoints/abot_recon.safetensors",
        conf_threshold: float = 0.02,
        target_width: int = 504,
        target_height: int = 280,
        voxel_size: float = 0.02, # 2 cm spatial decimation for live display
        device: str = "cuda",
        on_points_callback: Optional[Callable[[List[Tuple]], None]] = None
    ):
        self.checkpoint_path = checkpoint_path
        self.conf_threshold = conf_threshold
        self.target_width = target_width
        self.target_height = target_height
        self.voxel_size = voxel_size
        self.on_points_callback = on_points_callback

        self.device = torch.device(device if torch.cuda.is_available() else "cpu")
        self.dtype = torch.bfloat16 if torch.cuda.is_available() and torch.cuda.get_device_capability()[0] >= 8 else torch.float32

        # OpenCV Camera to Canonical AV: X_av = Z_cam, Y_av = -X_cam, Z_av = -Y_cam
        self.M_av_cam_base = np.array([
            [ 0.0,  0.0,  1.0],
            [-1.0,  0.0,  0.0],
            [ 0.0, -1.0,  0.0]
        ], dtype=np.float64)
        self.M_av_cam = self.M_av_cam_base.copy()
        self._load_canonical_transform()

        # Model instance
        self.model: Optional[ABotRecon] = None
        self.network = None
        self.model_loaded = False
        self._load_lock = threading.Lock()

        # Concurrency & Streaming State
        self.running = False
        self.worker_thread: Optional[threading.Thread] = None
        self.state = "IDLE" # IDLE, STREAMING, STOPPED

        # Thread-safe frame passing (Backpressure grab)
        self._latest_frame_bgr: Optional[np.ndarray] = None
        self._latest_frame_ts: float = 0.0
        self._frame_lock = threading.Lock()
        self._new_frame_event = threading.Event()

        # Decimated Voxel Cache for GUI
        self.voxel_grid: Dict[Tuple[int, int, int], Tuple[float, float, float, float, int, int, int]] = {}
        self.new_points_buffer: List[Tuple[float, float, float, float, int, int, int]] = []
        self.lock = threading.Lock()

        # Trajectory storage in Canonical AV frame
        self.trajectory_av: List[Dict[str, Any]] = []
        self.current_c2w: np.ndarray = np.eye(4, dtype=np.float64)

        # Streaming model state carries
        self._carry = None
        self._ref_h = None
        self._camera_state = None
        self._frame_index = 0

        # Performance & Telemetry
        self.total_processed_frames = 0
        self.total_points_reconstructed = 0
        self.inference_hz = 0.0
        self.last_step_duration = 0.0
        self.current_gpu_gb = 0.0

    def _load_canonical_transform(self):
        """Loads optional UVC canonical_transform.json to compose user orientation with M_av_cam_base."""
        import json
        self.M_av_cam = self.M_av_cam_base.copy()
        tf_path = "/home/scanar/av_scan/core/drivers/uvc/canonical_transform.json"
        if os.path.exists(tf_path):
            try:
                with open(tf_path, "r") as f:
                    data = json.load(f)
                    if "matrix_3x3" in data:
                        R_user = np.array(data["matrix_3x3"], dtype=np.float64)
                        self.M_av_cam = R_user @ self.M_av_cam_base
                        print(f"[ABotBackend] Composed canonical transform from {tf_path}")
            except Exception as e:
                print(f"[ABotBackend] Error reading {tf_path}: {e}")

    def load_model(self) -> bool:
        """Loads official ABot-Recon checkpoint onto target GPU device."""
        if not HAS_ABOT:
            print(f"[ABotBackend] Error: ABot-Recon module unavailable: {ABOT_IMPORT_ERROR}")
            return False

        with self._load_lock:
            if self.model_loaded:
                return True

            print(f"[ABotBackend] Loading ABot-Recon checkpoint from {self.checkpoint_path}...")
            t0 = time.time()
            try:
                ckpt = Path(self.checkpoint_path)
                if not ckpt.exists():
                    print(f"[ABotBackend] Error: Checkpoint file not found: {ckpt}")
                    return False

                self.model = ABotRecon.from_pretrained(
                    ckpt,
                    device=str(self.device),
                    attention_backend="auto",
                    loop_closure=False,
                )
                self.network = self.model.model.network
                self.model_loaded = True
                dt = time.time() - t0
                vram_mb = torch.cuda.memory_allocated() / (1024 * 1024)
                print(f"[ABotBackend] ABot-Recon loaded in {dt:.2f}s (Resident VRAM: {vram_mb:.1f} MB)")
                return True
            except Exception as e:
                print(f"[ABotBackend] Failed to load ABot-Recon: {e}")
                import traceback
                traceback.print_exc()
                return False

    def start(self) -> bool:
        """Starts worker thread for streaming inference."""
        if self.running:
            return True

        if not self.model_loaded:
            if not self.load_model():
                return False

        # Reset streaming carries
        self._carry = None
        self._ref_h = None
        self._camera_state = None
        self._frame_index = 0
        self.current_c2w = np.eye(4, dtype=np.float64)

        with self.lock:
            self.voxel_grid.clear()
            self.new_points_buffer.clear()
            self.trajectory_av.clear()
            self.total_processed_frames = 0
            self.total_points_reconstructed = 0
            self.state = "STREAMING"

        # Record initial origin pose in canonical AV frame
        self._record_pose(self.current_c2w, time.time())

        self.running = True
        self.worker_thread = threading.Thread(target=self._streaming_loop, daemon=True)
        self.worker_thread.start()
        print("[ABotBackend] Started streaming worker.")
        return True

    def stop(self) -> bool:
        """Stops streaming worker thread."""
        self.running = False
        self._new_frame_event.set()
        if self.worker_thread:
            self.worker_thread.join(timeout=2.0)
            self.worker_thread = None

        with self.lock:
            self.state = "STOPPED"

        # Free intermediate tensors
        self._carry = None
        self._ref_h = None
        self._camera_state = None
        if torch.cuda.is_available():
            torch.cuda.empty_cache()

        print("[ABotBackend] Stopped streaming worker.")
        return True

    def push_rgb_frame(self, frame_bgr: np.ndarray, timestamp: float):
        """Buffers the latest incoming RGB frame under lock (single slot, non-blocking)."""
        with self._frame_lock:
            self._latest_frame_bgr = frame_bgr
            self._latest_frame_ts = timestamp
            self._new_frame_event.set()

    def _preprocess_frame(self, bgr_img: np.ndarray) -> Tuple[torch.Tensor, np.ndarray]:
        """Converts BGR to RGB, resizes to target (504x280), and returns normalized float tensor."""
        rgb_full = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
        rgb_resized = cv2.resize(rgb_full, (self.target_width, self.target_height), interpolation=cv2.INTER_CUBIC)
        tensor = torch.from_numpy(rgb_resized).permute(2, 0, 1).float() / 255.0 # [3, H, W]
        return tensor, rgb_resized

    def _streaming_loop(self):
        """Dedicated background streaming worker loop."""
        print("[ABotBackend] Worker loop active.")

        while self.running:
            self._new_frame_event.wait(timeout=0.1)
            if not self.running:
                break

            frame_bgr = None
            frame_ts = 0.0
            with self._frame_lock:
                if self._latest_frame_bgr is not None:
                    frame_bgr = self._latest_frame_bgr
                    frame_ts = self._latest_frame_ts
                    self._latest_frame_bgr = None
                    self._new_frame_event.clear()

            if frame_bgr is None:
                continue

            self._execute_streaming_step(frame_bgr, frame_ts)

    def _execute_streaming_step(self, frame_bgr: np.ndarray, frame_ts: float):
        """Executes one causal streaming step through ABotReconNetwork."""
        t_start = time.time()

        tensor, rgb_resized = self._preprocess_frame(frame_bgr)
        # Input tensor: [1, 1, 3, H, W]
        frame_tensor = tensor.unsqueeze(0).unsqueeze(0).to(self.device, dtype=self.dtype, non_blocking=True)

        autocast_enabled = (self.device.type == "cuda" and self.dtype != torch.float32)
        try:
            with torch.inference_mode(), torch.autocast(device_type=self.device.type, dtype=self.dtype, enabled=autocast_enabled):
                pred = self.network.forward(
                    frame_tensor,
                    causal_global_attn=True,
                    long_sequence_parallel=True,
                    streaming_inference=True,
                    past_key_values=self._carry,
                    ref_hidden=self._ref_h,
                    camera_state=self._camera_state,
                )

                # Post-process carry for bounded 12-frame KV window
                self._carry = self.network._postprocess_stream_carry(
                    detach_carry(pred.get("past_key_values")),
                    spatial_hw=(self.target_height, self.target_width),
                )
                if pred.get("ref_hidden") is not None:
                    self._ref_h = pred["ref_hidden"]
                self._camera_state = pred.get("camera_state", self._camera_state)

                # Extract camera pose: [1, 1, 4, 4]
                pose_tensor = pred["camera_poses"][0, 0].detach().cpu().to(torch.float32).numpy()
                self.current_c2w = pose_tensor.astype(np.float64)

                # Extract local points: [1, 1, H, W, 3] and conf: [1, 1, H, W, 1]
                local_pts = pred["local_points"][0, 0].detach().cpu().to(torch.float32).numpy()
                conf_pts = pred["conf"][0, 0, ..., 0].detach().cpu().to(torch.float32).numpy() if pred.get("conf") is not None else None

            # Record pose in canonical AV frame
            self._record_pose(self.current_c2w, frame_ts)

            # Process and decimate 3D point cloud
            self._process_point_cloud(local_pts, conf_pts, rgb_resized)

            dt = time.time() - t_start
            self.last_step_duration = dt
            self.inference_hz = 1.0 / max(dt, 1e-4)
            self.total_processed_frames += 1

            if torch.cuda.is_available():
                self.current_gpu_gb = torch.cuda.memory_allocated() / (1024 ** 3)

        except Exception as e:
            print(f"[ABotBackend] Error during streaming step: {e}")
            import traceback
            traceback.print_exc()

    def _record_pose(self, c2w_cam: np.ndarray, timestamp: float):
        """Converts camera-to-world pose into Canonical AV frame and stores trajectory."""
        # T_av = M_av_cam * c2w_cam * M_av_cam^T
        R_cam = c2w_cam[:3, :3]
        t_cam = c2w_cam[:3, 3]

        R_av = self.M_av_cam @ R_cam @ self.M_av_cam.T
        t_av = self.M_av_cam @ t_cam

        # Convert rotation matrix to quaternion (w, x, y, z)
        tr = R_av[0, 0] + R_av[1, 1] + R_av[2, 2]
        if tr > 0:
            S = math.sqrt(tr + 1.0) * 2
            qw = 0.25 * S
            qx = (R_av[2, 1] - R_av[1, 2]) / S
            qy = (R_av[0, 2] - R_av[2, 0]) / S
            qz = (R_av[1, 0] - R_av[0, 1]) / S
        elif (R_av[0, 0] > R_av[1, 1]) and (R_av[0, 0] > R_av[2, 2]):
            S = math.sqrt(1.0 + R_av[0, 0] - R_av[1, 1] - R_av[2, 2]) * 2
            qw = (R_av[2, 1] - R_av[1, 2]) / S
            qx = 0.25 * S
            qy = (R_av[0, 1] + R_av[1, 0]) / S
            qz = (R_av[0, 2] + R_av[2, 0]) / S
        elif R_av[1, 1] > R_av[2, 2]:
            S = math.sqrt(1.0 + R_av[1, 1] - R_av[0, 0] - R_av[2, 2]) * 2
            qw = (R_av[0, 2] - R_av[2, 0]) / S
            qx = (R_av[0, 1] + R_av[1, 0]) / S
            qy = 0.25 * S
            qz = (R_av[1, 2] + R_av[2, 1]) / S
        else:
            S = math.sqrt(1.0 + R_av[2, 2] - R_av[0, 0] - R_av[1, 1]) * 2
            qw = (R_av[1, 0] - R_av[0, 1]) / S
            qx = (R_av[0, 2] + R_av[2, 0]) / S
            qy = (R_av[1, 2] + R_av[2, 1]) / S
            qz = 0.25 * S

        pose_dict = {
            "timestamp": timestamp,
            "timestamp_ns": int(timestamp * 1e9),
            "x": float(t_av[0]), "y": float(t_av[1]), "z": float(t_av[2]),
            "qw": float(qw), "qx": float(qx), "qy": float(qy), "qz": float(qz),
            "position": {"x": float(t_av[0]), "y": float(t_av[1]), "z": float(t_av[2])},
            "orientation": {"w": float(qw), "x": float(qx), "y": float(qy), "z": float(qz)},
            "source_device": "camera_32e4_4802_2023081002",
            "confidence": 0.95
        }

        with self.lock:
            self.trajectory_av.append(pose_dict)

    def _process_point_cloud(self, local_pts: np.ndarray, conf_pts: Optional[np.ndarray], rgb_img: np.ndarray):
        """Transforms points to world coordinates, maps to AV canonical, and applies voxel decimation."""
        # local_pts: [H, W, 3] in camera coordinate frame
        H, W, _ = local_pts.shape
        flat_pts_cam = local_pts.reshape(-1, 3) # [N, 3]
        flat_rgb = rgb_img.reshape(-1, 3)       # [N, 3]

        if conf_pts is not None:
            # conf_pts is raw logits from network; apply sigmoid to get [0, 1] confidence
            raw_conf = conf_pts.reshape(-1).astype(np.float32)
            flat_conf = 1.0 / (1.0 + np.exp(-np.clip(raw_conf, -20.0, 20.0)))
        else:
            flat_conf = np.ones(H * W, dtype=np.float32)

        # Adaptive thresholding if scene confidence is uniformly lower than threshold
        thresh = self.conf_threshold
        if conf_pts is not None and flat_conf.size > 0 and float(np.max(flat_conf)) < thresh:
            thresh = max(float(np.median(flat_conf)), 0.001)

        # Filter valid depth and confidence
        valid_mask = np.isfinite(flat_pts_cam).all(axis=1) & (flat_pts_cam[:, 2] > 0.01) & (flat_pts_cam[:, 2] < 30.0)
        if conf_pts is not None:
            valid_mask &= (flat_conf >= thresh)

        if not np.any(valid_mask):
            return

        valid_cam = flat_pts_cam[valid_mask]
        valid_rgb = flat_rgb[valid_mask]
        valid_conf = flat_conf[valid_mask]

        # Transform to world camera coordinates: P_world_cam = R_c2w * P_local + t_c2w
        R_c2w = self.current_c2w[:3, :3]
        t_c2w = self.current_c2w[:3, 3]
        pts_world_cam = (valid_cam @ R_c2w.T) + t_c2w

        # Transform to Canonical AV coordinates: P_av = M_av_cam * P_world_cam
        pts_av = pts_world_cam @ self.M_av_cam.T

        # Subsample to keep processing fast (stride of 2)
        step = 2
        pts_sub = pts_av[::step]
        rgb_sub = valid_rgb[::step]
        conf_sub = valid_conf[::step]

        inv_voxel = 1.0 / self.voxel_size
        new_points = []

        with self.lock:
            for pt, col, c in zip(pts_sub, rgb_sub, conf_sub):
                vx = int(math.floor(pt[0] * inv_voxel))
                vy = int(math.floor(pt[1] * inv_voxel))
                vz = int(math.floor(pt[2] * inv_voxel))
                vkey = (vx, vy, vz)

                if vkey not in self.voxel_grid:
                    entry = (float(pt[0]), float(pt[1]), float(pt[2]), float(c), int(col[0]), int(col[1]), int(col[2]))
                    self.voxel_grid[vkey] = entry
                    new_points.append(entry)

            self.new_points_buffer.extend(new_points)
            self.total_points_reconstructed += len(new_points)

        if self.on_points_callback and new_points:
            try:
                self.on_points_callback(new_points)
            except Exception as e:
                print(f"[ABotBackend] Error in points callback: {e}")

    def get_new_points(self) -> List[Tuple[float, float, float, float, int, int, int]]:
        """Drains the newly accumulated XYZRGB points buffer for streaming output."""
        with self.lock:
            pts = list(self.new_points_buffer)
            self.new_points_buffer.clear()
            return pts

    def get_trajectory(self) -> List[Dict[str, Any]]:
        """Returns the full 6DOF camera trajectory in Canonical AV frame."""
        with self.lock:
            return list(self.trajectory_av)

    def get_telemetry(self) -> Dict[str, Any]:
        """Returns runtime performance telemetry."""
        with self.lock:
            return {
                "status": self.state,
                "inference_hz": self.inference_hz,
                "total_frames": self.total_processed_frames,
                "total_points": self.total_points_reconstructed,
                "accepted_points": len(self.voxel_grid),
                "last_step_ms": self.last_step_duration * 1000.0,
                "current_gpu_gb": self.current_gpu_gb,
                "backend": "ABotStreamingBackend",
                "model": "ABot-Recon (AMAP CVLab)",
                "qualification": "RESEARCH_EXPERIMENTAL",
                "commercial_status": "NOT CLEARED / NON-COMMERCIAL"
            }
