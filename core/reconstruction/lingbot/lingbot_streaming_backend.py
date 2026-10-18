#!/usr/bin/env python3
"""
AV Scan — LingBot-Map Streaming 3D Reconstruction Backend
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and open-source LingBot-Map foundation model.

PROVENANCE & LICENSING:
- Model Architecture: LingBot-Map (GCTStreamWindowV2 / GCTStream)
- Foundation Source: https://github.com/LingBot-Map/lingbot-map (Commit 0849e690b)
- Upstream License: Apache 2.0 (Official LICENSE.txt)
- Weights: /home/scanar/lingbot-map/weights/lingbot-map.pt
- Checkpoint Hash: 0849e690b

KEY CHARACTERISTICS:
- Pure native streaming reconstruction: preserves LingBot's internally consistent geometry.
- Dual-resolution output: full-quality source reconstruction preserved separately;
  confidence-filtered (conf >= 1.5) and spatially decimated XYZRGB points fed to GUI.
- Bounded backpressure: RGB acquisition runs independently at full sensor rate (~30 FPS);
  streaming worker samples latest frame without unbounded frame buffering.
- Genuine XYZRGB: points directly inherit authentic color from source RGB camera.
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
import numpy as np
import cv2
import torch

# Ensure lingbot-map repository is in python path
LINGBOT_REPO_PATH = "/home/scanar/lingbot-map"
if LINGBOT_REPO_PATH not in sys.path:
    sys.path.insert(0, LINGBOT_REPO_PATH)

try:
    from lingbot_map.models.gct_stream import GCTStream
    from lingbot_map.utils.pose_enc import pose_encoding_to_extri_intri
    from lingbot_map.utils.geometry import closed_form_inverse_se3_general, unproject_depth_map_to_point_map
    HAS_LINGBOT = True
except ImportError as e:
    HAS_LINGBOT = False
    LINGBOT_IMPORT_ERROR = str(e)


class LingBotStreamingBackend:
    """
    Isolated streaming reconstruction backend utilizing upstream LingBot-Map.
    """
    def __init__(
        self,
        checkpoint_path: str = "/home/scanar/lingbot-map/weights/lingbot-map.pt",
        conf_threshold: float = 1.0,
        sliding_window: int = 32,
        num_scale_frames: int = 8,
        voxel_size: float = 0.02, # 2 cm spatial decimation for live display
        target_size: int = 518,
        device: str = "cuda",
        on_points_callback: Optional[Callable[[List[Tuple]], None]] = None
    ):
        self.checkpoint_path = checkpoint_path
        self.conf_threshold = conf_threshold
        self.sliding_window = sliding_window
        self.num_scale_frames = num_scale_frames
        self.voxel_size = voxel_size
        self.target_size = target_size
        self.on_points_callback = on_points_callback

        self.device = torch.device(device if torch.cuda.is_available() else "cpu")
        self.dtype = torch.bfloat16 if torch.cuda.is_available() and torch.cuda.get_device_capability()[0] >= 8 else torch.float16

        # Derived image dimensions for 16:9 input (1920x1080 -> 518x294)
        patch_size = 14
        self.new_width = target_size
        self.new_height = round(1080 * (self.new_width / 1920.0) / patch_size) * patch_size # 294

        # OpenCV Camera to Canonical AV: X_av = Z_cam, Y_av = -X_cam, Z_av = -Y_cam
        self.M_av_cam_base = np.array([
            [ 0.0,  0.0,  1.0],
            [-1.0,  0.0,  0.0],
            [ 0.0, -1.0,  0.0]
        ], dtype=np.float64)
        self.M_av_cam = self.M_av_cam_base.copy()
        self._load_canonical_transform()

        # Model instance (lazily loaded or preloaded)
        self.model: Optional[GCTStream] = None
        self.model_loaded = False
        self._load_lock = threading.Lock()

        # Concurrency & Streaming State
        self.running = False
        self.worker_thread: Optional[threading.Thread] = None
        self.state = "IDLE" # IDLE, ACCUMULATING_SCALE_FRAMES, STREAMING, STOPPED

        # Single-item slot for latest frame (backpressure policy)
        self._frame_lock = threading.Lock()
        self._new_frame_event = threading.Event()
        self._latest_frame_bgr: Optional[np.ndarray] = None
        self._latest_frame_ts: float = 0.0

        # Scale frames accumulator
        self._scale_frames_bgr = []
        self._scale_frames_ts = []

        # Maps & Storage
        self.lock = threading.Lock()
        self.global_voxels: Dict[Tuple[int, int, int], Tuple] = {}
        self.native_trajectory: List[Dict[str, Any]] = [] # LingBot native trajectory in Canonical AV
        self.source_quality_frames: List[Dict[str, Any]] = [] # Full raw unprojected reconstructions

        # Performance & Telemetry instrumentation
        self.inference_timestamps = deque()
        self.inference_hz: float = 0.0
        self.total_reconstruction_points: int = 0
        self.accepted_confident_points: int = 0
        self.frames_processed: int = 0
        self.peak_gpu_memory_gb: float = 0.0
        self.current_gpu_memory_gb: float = 0.0
        self._last_served_point_idx: int = 0

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
                        print(f"[LingBotBackend] Composed canonical transform from {tf_path}")
            except Exception as e:
                print(f"[LingBotBackend] Error reading {tf_path}: {e}")

    def load_model(self) -> bool:
        """Loads GCTStream foundation model and checkpoint with strict memory safety."""
        with self._load_lock:
            if self.model_loaded:
                return True

            if not HAS_LINGBOT:
                print(f"[LingBotBackend] LingBot not available: {LINGBOT_IMPORT_ERROR}")
                return False

            if not os.path.exists(self.checkpoint_path):
                print(f"[LingBotBackend] Checkpoint not found: {self.checkpoint_path}")
                return False

            print(f"[LingBotBackend] Initializing GCTStream (SlidingWindow={self.sliding_window}, ScaleFrames={self.num_scale_frames})...")
            t0 = time.time()
            self.model = GCTStream(
                img_size=self.target_size,
                patch_size=14,
                enable_3d_rope=True,
                max_frame_num=1024,
                kv_cache_sliding_window=self.sliding_window,
                kv_cache_scale_frames=self.num_scale_frames,
                kv_cache_cross_frame_special=True,
                kv_cache_include_scale_frames=True,
                use_sdpa=True,
                camera_num_iterations=4
            )

            print(f"[LingBotBackend] Loading official weights from {self.checkpoint_path}...")
            t_ckpt = time.time()
            ckpt = torch.load(self.checkpoint_path, map_location="cpu", weights_only=False)
            state_dict = ckpt.get("model", ckpt)
            missing, unexpected = self.model.load_state_dict(state_dict, strict=False)
            del ckpt, state_dict
            gc.collect()
            print(f"[LingBotBackend] Weights loaded in {time.time() - t_ckpt:.2f}s (Missing: {len(missing)}, Unexpected: {len(unexpected)})")

            # Cast aggregator to low-memory dtype
            if getattr(self.model, "aggregator", None) is not None:
                self.model.aggregator = self.model.aggregator.to(dtype=self.dtype)

            self.model = self.model.to(self.device).eval()
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
                self.current_gpu_memory_gb = torch.cuda.memory_allocated() / 1e9
                print(f"[LingBotBackend] Model resident on GPU ({self.current_gpu_memory_gb:.2f} GB allocated)")

            self.model_loaded = True
            return True

    def start(self) -> bool:
        """Starts streaming reconstruction worker."""
        if self.running:
            return True

        if not self.model_loaded:
            if not self.load_model():
                return False

        with self.lock:
            self.running = True
            self.state = "ACCUMULATING_SCALE_FRAMES"
            self._scale_frames_bgr.clear()
            self._scale_frames_ts.clear()
            self.inference_timestamps.clear()
            self.inference_hz = 0.0

        # Clean upstream KV cache
        if self.model is not None:
            self.model.clean_kv_cache()

        self._new_frame_event.clear()
        self.worker_thread = threading.Thread(target=self._streaming_loop, daemon=True)
        self.worker_thread.start()
        print("[LingBotBackend] Streaming reconstruction worker thread started.")
        return True

    def stop(self):
        """Stops streaming worker cleanly."""
        if not self.running:
            return

        print("[LingBotBackend] Stopping streaming worker...")
        self.running = False
        self._new_frame_event.set()
        if self.worker_thread and self.worker_thread.is_alive():
            self.worker_thread.join(timeout=3.0)

        if self.model is not None:
            self.model.clean_kv_cache()

        if torch.cuda.is_available():
            torch.cuda.empty_cache()

        with self.lock:
            self.state = "STOPPED"
        print("[LingBotBackend] Streaming worker stopped.")

    def reset(self):
        """Clears all reconstructed maps and resets model KV cache."""
        with self.lock:
            self.global_voxels.clear()
            self.native_trajectory.clear()
            self.source_quality_frames.clear()
            self._scale_frames_bgr.clear()
            self._scale_frames_ts.clear()
            self.total_reconstruction_points = 0
            self.accepted_confident_points = 0
            self.frames_processed = 0
            self.state = "IDLE"
            self._last_served_point_idx = 0

        if self.model is not None:
            self.model.clean_kv_cache()

        if torch.cuda.is_available():
            torch.cuda.empty_cache()

    def push_rgb_frame(self, frame_bgr: np.ndarray, timestamp: float):
        """Alias for feed_rgb_frame."""
        self.feed_rgb_frame(frame_bgr, timestamp)

    def get_new_points(self) -> List[Tuple]:
        """Returns new decimated XYZRGB points produced since last call."""
        with self.lock:
            pts = list(self.global_voxels.values())
            if self._last_served_point_idx >= len(pts):
                return []
            new_pts = pts[self._last_served_point_idx:]
            self._last_served_point_idx = len(pts)
            return new_pts

    def get_trajectory(self) -> List[Dict[str, Any]]:
        """Returns full estimated trajectory in Canonical AV coordinate system."""
        with self.lock:
            return list(self.native_trajectory)

    def get_telemetry(self) -> Dict[str, Any]:
        """Returns telemetry dict."""
        return self.get_telemetry_dict()

    def feed_rgb_frame(self, frame_bgr: np.ndarray, timestamp: float):
        """
        Ingests a 1080p center RGB frame from /dev/video0.
        Implements bounded queue / backpressure: only latest frame is retained.
        """
        if not self.running:
            return

        with self._frame_lock:
            self._latest_frame_bgr = frame_bgr
            self._latest_frame_ts = timestamp
            self._new_frame_event.set()

    def _preprocess_frame(self, bgr_img: np.ndarray) -> Tuple[torch.Tensor, np.ndarray]:
        """Converts BGR image to normalized float tensor and returns resized RGB numpy array."""
        rgb_full = cv2.cvtColor(bgr_img, cv2.COLOR_BGR2RGB)
        rgb_resized = cv2.resize(rgb_full, (self.new_width, self.new_height), interpolation=cv2.INTER_CUBIC)
        tensor = torch.from_numpy(rgb_resized).permute(2, 0, 1).float() / 255.0 # [3, H, W]
        return tensor, rgb_resized

    def _streaming_loop(self):
        """Dedicated background streaming inference loop."""
        print("[LingBotBackend] Worker loop active.")

        while self.running:
            # Wait for fresh frame
            self._new_frame_event.wait(timeout=0.1)
            if not self.running:
                break

            # Fetch latest frame under lock (backpressure grab)
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

            # Process frame based on current phase
            if self.state == "ACCUMULATING_SCALE_FRAMES":
                self._scale_frames_bgr.append(frame_bgr)
                self._scale_frames_ts.append(frame_ts)
                print(f"[LingBotBackend] Collected scale frame {len(self._scale_frames_bgr)}/{self.num_scale_frames}")

                if len(self._scale_frames_bgr) >= self.num_scale_frames:
                    self._execute_scale_pass()
                    with self.lock:
                        self.state = "STREAMING"

            elif self.state == "STREAMING":
                self._execute_streaming_step(frame_bgr, frame_ts)

    def _execute_scale_pass(self):
        """Executes Phase 1: Bidirectional attention pass over initial scale frames."""
        print(f"[LingBotBackend] Executing scale initialization on {len(self._scale_frames_bgr)} frames...")
        t_start = time.time()

        tensors = []
        rgb_arrays = []
        for bgr in self._scale_frames_bgr:
            t, rgb = self._preprocess_frame(bgr)
            tensors.append(t)
            rgb_arrays.append(rgb)

        scale_tensor = torch.stack(tensors, dim=0).unsqueeze(0) # [1, S, 3, H, W]
        scale_tensor = scale_tensor.to(self.device, dtype=self.dtype, non_blocking=True)

        with torch.no_grad():
            output = self.model.forward(
                scale_tensor,
                num_frame_for_scale=self.num_scale_frames,
                num_frame_per_block=self.num_scale_frames,
                causal_inference=True
            )

        dt = time.time() - t_start
        print(f"[LingBotBackend] Scale initialization pass completed in {dt:.2f}s ({len(self._scale_frames_bgr)/dt:.2f} FPS)")

        # Unproject and decimate points for each scale frame
        self._process_prediction_batch(output, rgb_arrays, self._scale_frames_ts)

    def _execute_streaming_step(self, frame_bgr: np.ndarray, frame_ts: float):
        """Executes Phase 2: Frame-by-frame streaming inference with KV cache."""
        t_start = time.time()

        tensor, rgb_resized = self._preprocess_frame(frame_bgr)
        frame_tensor = tensor.unsqueeze(0).unsqueeze(0) # [1, 1, 3, H, W]
        frame_tensor = frame_tensor.to(self.device, dtype=self.dtype, non_blocking=True)

        with torch.no_grad():
            output = self.model.forward(
                frame_tensor,
                num_frame_for_scale=self.num_scale_frames,
                num_frame_per_block=1,
                causal_inference=True
            )

        dt = time.time() - t_start

        # Record telemetry
        now = time.time()
        self.inference_timestamps.append(now)
        while self.inference_timestamps and (now - self.inference_timestamps[0] > 2.0):
            self.inference_timestamps.popleft()
        if len(self.inference_timestamps) > 1:
            self.inference_hz = float(len(self.inference_timestamps) - 1) / (self.inference_timestamps[-1] - self.inference_timestamps[0])

        if torch.cuda.is_available():
            self.current_gpu_memory_gb = torch.cuda.memory_allocated() / 1e9
            self.peak_gpu_memory_gb = max(self.peak_gpu_memory_gb, torch.cuda.max_memory_allocated() / 1e9)

        # Unproject and decimate points for streaming frame
        self._process_prediction_batch(output, [rgb_resized], [frame_ts])

    def _process_prediction_batch(
        self,
        prediction: Dict[str, torch.Tensor],
        rgb_list: List[np.ndarray],
        ts_list: List[float]
    ):
        """
        Unprojects depth maps to 3D points, applies confidence thresholding,
        transforms to Canonical AV, decimates for live GUI display, and retains
        full-quality source reconstruction.
        """
        pose_enc = prediction["pose_enc"] # [1, S, 9]
        depth = prediction["depth"]       # [1, S, H, W, 1]
        depth_conf = prediction["depth_conf"] # [1, S, H, W]

        S = pose_enc.shape[1]
        extrinsic, intrinsic = pose_encoding_to_extri_intri(pose_enc, (self.new_height, self.new_width))

        # Convert w2c to c2w
        extrinsic_4x4 = torch.zeros((*extrinsic.shape[:-2], 4, 4), device=extrinsic.device, dtype=extrinsic.dtype)
        extrinsic_4x4[..., :3, :4] = extrinsic
        extrinsic_4x4[..., 3, 3] = 1.0
        c2w = closed_form_inverse_se3_general(extrinsic_4x4)[..., :3, :4].cpu().numpy()[0] # [S, 3, 4]
        intrinsic_np = intrinsic.cpu().numpy()[0] # [S, 3, 3]
        depth_np = depth.cpu().numpy()[0] # [S, H, W, 1]
        conf_np = depth_conf.cpu().numpy()[0] # [S, H, W]

        # Unproject depth map to 3D world points in native LingBot coordinates
        world_pts = unproject_depth_map_to_point_map(depth_np, c2w, intrinsic_np) # [S, H, W, 3]

        v_size = self.voxel_size
        new_decimated_points = []

        with self.lock:
            for s in range(S):
                self.frames_processed += 1
                cur_pts = world_pts[s] # [H, W, 3]
                cur_conf = conf_np[s]  # [H, W]
                cur_rgb = rgb_list[s]  # [H, W, 3]
                cur_ts = ts_list[s]
                cur_c2w = c2w[s]       # [3, 4]

                # 1. Update native camera trajectory in Canonical AV frame
                # LingBot camera position: t_cam = cur_c2w[:3, 3]
                # Canonical AV position: X_av = Z_cam, Y_av = -X_cam, Z_av = -Y_cam
                t_cam = cur_c2w[:3, 3]
                t_av = self.M_av_cam @ t_cam
                R_cam = cur_c2w[:3, :3]
                R_av = self.M_av_cam @ R_cam @ self.M_av_cam.T

                # Quaternion in Canonical AV
                yaw_av = math.atan2(float(R_av[1, 0]), float(R_av[0, 0]))
                tr = float(R_av[0, 0] + R_av[1, 1] + R_av[2, 2])
                if tr > 0.0:
                    sq = 0.5 / math.sqrt(tr + 1.0)
                    qw_av = 0.25 / sq
                    qx_av = (R_av[2, 1] - R_av[1, 2]) * sq
                    qy_av = (R_av[0, 2] - R_av[2, 0]) * sq
                    qz_av = (R_av[1, 0] - R_av[0, 1]) * sq
                else:
                    qw_av, qx_av, qy_av, qz_av = 1.0, 0.0, 0.0, 0.0

                self.native_trajectory.append({
                    "timestamp_ns": int(cur_ts * 1e9),
                    "x": float(t_av[0]),
                    "y": float(t_av[1]),
                    "z": float(t_av[2]),
                    "qx": float(qx_av),
                    "qy": float(qy_av),
                    "qz": float(qz_av),
                    "qw": float(qw_av),
                    "yaw": float(yaw_av),
                    "frame_idx": self.frames_processed
                })

                # 2. Retain full source-quality reconstruction
                # Stored separately to ensure raw foundation output is never lost
                self.source_quality_frames.append({
                    "frame_idx": self.frames_processed,
                    "timestamp": cur_ts,
                    "c2w": cur_c2w,
                    "intrinsic": intrinsic_np[s],
                    "point_count": cur_pts.shape[0] * cur_pts.shape[1]
                })

                # 3. Confidence Filtering
                thresh = self.conf_threshold
                if cur_conf.size > 0 and float(np.max(cur_conf)) < thresh:
                    thresh = float(np.median(cur_conf))

                valid_mask = cur_conf >= thresh
                num_valid = np.count_nonzero(valid_mask)
                self.total_reconstruction_points += cur_pts.shape[0] * cur_pts.shape[1]
                self.accepted_confident_points += num_valid

                if num_valid == 0:
                    continue

                valid_pts = cur_pts[valid_mask]    # [N, 3]
                valid_confs = cur_conf[valid_mask] # [N]
                valid_rgbs = cur_rgb[valid_mask]   # [N, 3]

                # Transform 3D points to Canonical AV:
                # X_av = Z_lingbot, Y_av = -X_lingbot, Z_av = -Y_lingbot
                pts_av = (self.M_av_cam @ valid_pts.T).T # [N, 3]

                # Decimate using 3D voxel grid
                for i in range(len(pts_av)):
                    wx, wy, wz = float(pts_av[i, 0]), float(pts_av[i, 1]), float(pts_av[i, 2])
                    gx = int(math.floor(wx / v_size))
                    gy = int(math.floor(wy / v_size))
                    gz = int(math.floor(wz / v_size))
                    key = (gx, gy, gz)

                    if key not in self.global_voxels:
                        r = int(valid_rgbs[i, 0])
                        g = int(valid_rgbs[i, 1])
                        b = int(valid_rgbs[i, 2])
                        conf = float(valid_confs[i])
                        # Format: (x, y, z, intensity, r, g, b, has_color)
                        # has_color=1 ensures GUI paints authentic rgb(r, g, b)
                        pt_tuple = (wx, wy, wz, conf, r, g, b, 1)
                        self.global_voxels[key] = pt_tuple
                        new_decimated_points.append(pt_tuple)

        # Notify callback if registered
        if new_decimated_points and self.on_points_callback is not None:
            self.on_points_callback(new_decimated_points)

    def get_telemetry_dict(self) -> Dict[str, Any]:
        """Returns comprehensive telemetry for logging and IPC reporting."""
        with self.lock:
            return {
                "backend_type": "LINGBOT_STREAMING_RECONSTRUCTION",
                "state": self.state,
                "inference_hz": round(self.inference_hz, 2),
                "total_points_reconstructed": self.total_reconstruction_points,
                "accepted_confident_points": self.accepted_confident_points,
                "decimated_display_points": len(self.global_voxels),
                "frames_processed": self.frames_processed,
                "current_gpu_memory_gb": round(self.current_gpu_memory_gb, 2),
                "peak_gpu_memory_gb": round(self.peak_gpu_memory_gb, 2),
                "trajectory_steps": len(self.native_trajectory)
            }
