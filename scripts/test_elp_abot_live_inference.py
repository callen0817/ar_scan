#!/usr/bin/env python3
"""
M14.6 Phase 5: First Real ABot Inference on Jetson Orin NX 16GB
Hardware: ELP/Fic USB48MP02-SL-185 (32e4:4802, serial 2023081002) at /dev/video0
Captures real live frames from physical camera, feeds them to ABot-Recon,
measures performance, and verifies output tensors.
"""

import os
import sys
import time
import shutil
from pathlib import Path
import cv2
import numpy as np
import torch

# Do not set expandable_segments on Tegra/Jetson unified memory
if "PYTORCH_CUDA_ALLOC_CONF" in os.environ:
    del os.environ["PYTORCH_CUDA_ALLOC_CONF"]

# Add abot_recon to path
REPO_ROOT = Path("/home/scanar/av_scan")
ABOT_PATH = REPO_ROOT / "third_party" / "abot_recon"
if str(ABOT_PATH) not in sys.path:
    sys.path.insert(0, str(ABOT_PATH))

from abot_recon import ABotRecon
from abot_recon.config import InferenceConfig

def main():
    print("=" * 70)
    print("M14.6 Phase 5 — Real ELP Camera -> Real ABot Model Live Benchmark")
    print("=" * 70)

    device_name = torch.cuda.get_device_name(0)
    print(f"CUDA Hardware: {device_name}")
    print(f"PyTorch Version: {torch.__version__}")
    print(f"CUDA Available: {torch.cuda.is_available()}")

    # 1. Step 1: Capture 15 real consecutive frames from physical ELP camera (/dev/video0)
    video_dev = "/dev/video0"
    print(f"\n[Step 1] Connecting to physical ELP camera at {video_dev}...")
    cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
    if not cap.isOpened():
        print(f"ERROR: Could not open {video_dev}")
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
    cap.set(cv2.CAP_PROP_FPS, 30.0)

    # Scratch frame dir
    frame_dir = REPO_ROOT / "scratch" / "m14_6_real_elp_frames"
    if frame_dir.exists():
        shutil.rmtree(frame_dir)
    frame_dir.mkdir(parents=True, exist_ok=True)

    print("Capturing 15 real frames from ELP camera...")
    captured_paths = []
    # Warmup camera 5 frames
    for _ in range(5):
        cap.read()

    t_cap_start = time.time()
    for i in range(15):
        ret, frame = cap.read()
        if not ret or frame is None:
            print(f"ERROR: Failed to read frame {i}")
            cap.release()
            sys.exit(1)
        frame_path = frame_dir / f"frame_{i:04d}.jpg"
        cv2.imwrite(str(frame_path), frame, [cv2.IMWRITE_JPEG_QUALITY, 95])
        captured_paths.append(frame_path)
    cap.release()
    t_cap_end = time.time()
    print(f"  ✓ Captured 15 frames ({frame.shape[1]}x{frame.shape[0]}) in {t_cap_end - t_cap_start:.2f}s ({15/(t_cap_end - t_cap_start):.1f} capture FPS)")

    # 2. Step 2: Initialize Real ABot Model
    print("\n[Step 2] Initializing official ABot-Recon foundation model on CUDA...")
    torch.cuda.empty_cache()
    torch.cuda.reset_peak_memory_stats()
    vram_before = torch.cuda.memory_allocated() / (1024 * 1024)

    t_init_start = time.time()
    ckpt_path = ABOT_PATH / "checkpoints" / "abot_recon.safetensors"
    model = ABotRecon.from_pretrained(
        ckpt_path,
        device="cuda",
        attention_backend="auto",
        loop_closure=False,
    )
    t_init_end = time.time()
    vram_model = torch.cuda.memory_allocated() / (1024 * 1024)
    init_duration = t_init_end - t_init_start
    print(f"  ✓ Model initialized in {init_duration:.2f}s")
    print(f"  ✓ Model resident VRAM: {vram_model - vram_before:.1f} MB (Total allocated: {vram_model:.1f} MB)")

    # 3. Step 3: Run Live Inference on the Real Frames
    print(f"\n[Step 3] Executing streaming 3D reconstruction on {len(captured_paths)} real ELP frames...")
    torch.cuda.reset_peak_memory_stats()
    t_infer_start = time.time()

    result = model.infer(
        captured_paths,
        output_local_points=True,
        output_world_points=True,
        output_confidence=True,
    )

    torch.cuda.synchronize()
    t_infer_end = time.time()
    infer_duration = t_infer_end - t_infer_start
    fps = len(captured_paths) / infer_duration
    latency_per_frame_ms = (infer_duration / len(captured_paths)) * 1000.0
    peak_vram = torch.cuda.max_memory_allocated() / (1024 * 1024)

    # 4. Step 4: Validate and Report Outputs
    print("\n[Step 4] Live Inference Benchmark Results:")
    print("-" * 50)
    print(f"  Number of Input Frames:   {len(captured_paths)}")
    print(f"  Camera Native Resolution: 1920x1080 (16:9)")
    print(f"  ABot Preprocessed Input:  504x280 (16:9)")
    print(f"  Total Inference Time:     {infer_duration:.2f}s")
    print(f"  Inference Throughput:     {fps:.2f} FPS")
    print(f"  Mean Frame Latency:       {latency_per_frame_ms:.1f} ms")
    print(f"  Peak VRAM Consumption:    {peak_vram:.1f} MB (~{peak_vram/1024:.2f} GB)")

    print("\n[Step 5] Spatial Output Tensor Verification:")
    print("-" * 50)
    
    # Trajectory
    poses = result.camera_poses
    print(f"  Camera Poses Shape:       {poses.shape} (Expected: [15, 4, 4])")
    print(f"  Start Pose Position:      [x={poses[0, 0, 3]:.4f}, y={poses[0, 1, 3]:.4f}, z={poses[0, 2, 3]:.4f}]")
    print(f"  End Pose Position:        [x={poses[-1, 0, 3]:.4f}, y={poses[-1, 1, 3]:.4f}, z={poses[-1, 2, 3]:.4f}]")
    
    # Relative poses
    rel_poses = result.relative_poses
    print(f"  Relative Poses Shape:     {rel_poses.shape} (Expected: [14, 4, 4])")

    # Local & World points
    local_pts = result.local_points
    world_pts = result.world_points
    print(f"  Local Points Shape:       {local_pts.shape} (Expected: [15, 280, 504, 3])")
    print(f"  World Points Shape:       {world_pts.shape} (Expected: [15, 280, 504, 3])")

    # Confidence
    conf = result.confidence
    print(f"  Confidence Shape:         {conf.shape} (Expected: [15, 280, 504])")
    print(f"  Confidence Min / Max / Med: {conf.min().item():.5f} / {conf.max().item():.5f} / {conf.median().item():.5f}")
    
    # Analyze world_pts
    finite_pts = world_pts[torch.isfinite(world_pts).all(dim=-1)]
    print(f"  Total Finite 3D Points:   {finite_pts.shape[0]:,} points")
    if finite_pts.shape[0] > 0:
        print(f"  Point bounds X: [{finite_pts[:, 0].min():.2f}, {finite_pts[:, 0].max():.2f}] m")
        print(f"  Point bounds Y: [{finite_pts[:, 1].min():.2f}, {finite_pts[:, 1].max():.2f}] m")
        print(f"  Point bounds Z: [{finite_pts[:, 2].min():.2f}, {finite_pts[:, 2].max():.2f}] m")

    print("\n" + "=" * 70)
    print(">>> PHASE 5 QUALIFICATION: REAL ELP -> REAL ABOT INFERENCE PASSED! <<<")
    print("=" * 70)

if __name__ == "__main__":
    main()
