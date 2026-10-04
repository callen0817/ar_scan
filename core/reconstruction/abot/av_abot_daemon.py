#!/usr/bin/env python3
"""
AV Scan — ABot-Recon Streaming Reconstruction Daemon
Port 9103

Hosts the ABot-Recon foundation model streaming reconstruction engine.
Provides telemetry, pose trajectory estimation, and genuine XYZRGB spatial
point streaming in Canonical AV frame with scale status tracking.

QUALIFICATION STATE: RESEARCH / EXPERIMENTAL
COMMERCIAL USE: NOT CLEARED / NON-COMMERCIAL
"""

import os
import sys

os.environ["PYTORCH_CUDA_ALLOC_CONF"] = "expandable_segments:True"

import time
import socket
import struct
import threading
import json
import argparse
from pathlib import Path
import cv2

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

# Ensure repository root is in python path
AV_SCAN_ROOT = "/home/scanar/av_scan"
if AV_SCAN_ROOT not in sys.path:
    sys.path.insert(0, AV_SCAN_ROOT)

from core.reconstruction.abot.abot_streaming_backend import ABotStreamingBackend


class ABotDaemon:
    def __init__(self, host="127.0.0.1", port=9103, default_video_dev="/dev/video0"):
        self.host = host
        self.port = port
        self.default_video_dev = default_video_dev
        self.video_dev = default_video_dev

        self.backend = ABotStreamingBackend()
        self.running = False
        self.capture_running = False
        self.capture_thread = None

        self.cap = None
        self.camera_fps = 0.0
        self.frame_count = 0
        self.last_pts_idx = 0

        self.server_sock = None
        self.lock = threading.Lock()

        # Preload foundation model weights immediately in background
        print("[ABotDaemon] Preloading ABot-Recon foundation model weights in background...")
        threading.Thread(target=self.backend.load_model, daemon=True).start()

    def start_camera(self, dev_path=None):
        if dev_path:
            self.video_dev = dev_path

        # STRICT DEVICE ISOLATION INVARIANT:
        # Must be an explicit /dev/video* node. Never fall back to arbitrary device index or first camera.
        if not str(self.video_dev).startswith("/dev/video"):
            print(f"[ABotDaemon] Error: Requested device '{self.video_dev}' is NOT a V4L2 video device node (e.g. /dev/video*). Refusing to hijack an alternate camera.")
            self.cap = None
            return False

        if not os.path.exists(self.video_dev):
            print(f"[ABotDaemon] Error: Device node '{self.video_dev}' does not exist on host filesystem.")
            self.cap = None
            return False

        try:
            dev_idx = int(self.video_dev.replace("/dev/video", ""))
        except ValueError:
            print(f"[ABotDaemon] Error: Cannot parse integer V4L2 device index from '{self.video_dev}'")
            return False

        print(f"[ABotDaemon] Opening V4L2 video capture on {self.video_dev} (index {dev_idx})...")
        self.cap = cv2.VideoCapture(dev_idx, cv2.CAP_V4L2)
        if not self.cap or not self.cap.isOpened():
            print(f"[ABotDaemon] Error: Failed to open V4L2 device node: {self.video_dev} (index {dev_idx})")
            self.cap = None
            return False

        self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        self.cap.set(cv2.CAP_PROP_FPS, 30.0)
        print(f"[ABotDaemon] Camera successfully opened: {self.video_dev} (1920x1080 @ 30 FPS)")
        return True

    def capture_loop(self):
        fps_t0 = time.time()
        fps_frames = 0
        while self.capture_running:
            if self.cap and self.cap.isOpened():
                ret, frame = self.cap.read()
                if ret and frame is not None:
                    ts = time.time()
                    self.backend.push_rgb_frame(frame, ts)
                    self.frame_count += 1
                    fps_frames += 1
                    now = time.time()
                    if now - fps_t0 >= 1.0:
                        self.camera_fps = fps_frames / (now - fps_t0)
                        fps_frames = 0
                        fps_t0 = now
                else:
                    time.sleep(0.01)
            else:
                time.sleep(0.05)

    def start_acquisition(self, dev_path=None):
        with self.lock:
            if self.running:
                return True
            self.running = True
            self.last_pts_idx = 0
            self.frame_count = 0

            # Wait for background model preload if in progress
            t_wait_start = time.time()
            while not self.backend.model_loaded and (time.time() - t_wait_start < 60.0):
                print("[ABotDaemon] Waiting for ABot-Recon model to finish loading...")
                time.sleep(1.0)

            # Start backend foundation model worker
            if not self.backend.start():
                print("[ABotDaemon] Error: Failed to start ABotStreamingBackend")
                self.running = False
                return False

            # Start camera capture
            target_dev = dev_path if dev_path else self.video_dev
            if not self.start_camera(target_dev):
                print(f"[ABotDaemon] Error: Failed to start camera capture on {target_dev}")
                self.backend.stop()
                self.running = False
                return False

            self.capture_running = True
            self.capture_thread = threading.Thread(target=self.capture_loop, daemon=True)
            self.capture_thread.start()
            print("[ABotDaemon] Acquisition started successfully.")
            return True

    def stop_acquisition(self):
        with self.lock:
            self.capture_running = False
            if self.capture_thread:
                self.capture_thread.join(timeout=1.0)
                self.capture_thread = None

            if self.cap and self.cap.isOpened():
                self.cap.release()
                self.cap = None

            self.backend.stop()
            self.running = False
            print("[ABotDaemon] Acquisition stopped.")
            return True

    def get_telemetry(self):
        b_tel = self.backend.get_telemetry()
        return {
            "status": "STREAMING" if self.running else "READY",
            "camera_fps": round(self.camera_fps, 1),
            "inference_fps": round(b_tel.get("inference_hz", 0.0), 2),
            "processed_frames": self.frame_count,
            "reconstructed_points": b_tel.get("total_points", 0),
            "displayed_points": b_tel.get("accepted_points", 0),
            "tracking_state": "OK" if self.running else "IDLE",
            "scale_status": "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED",
            "geometry_source": "MONOCULAR_NEURAL_RECONSTRUCTION",
            "metric_constraint": "NONE",
            "memory_gb": round(b_tel.get("current_gpu_gb", 0.0), 2),
            "backend": "ABotStreamingBackend",
            "qualification": "RESEARCH_EXPERIMENTAL",
            "commercial_status": "NOT CLEARED / NON-COMMERCIAL"
        }

    def handle_client(self, client_sock):
        try:
            client_file = client_sock.makefile("r", buffering=1)
            while True:
                line = client_file.readline()
                if not line:
                    break
                cmd = line.strip()
                if not cmd:
                    continue

                if cmd == "PING":
                    client_sock.sendall(b"PONG\n")
                elif cmd == "IDENTIFY":
                    info = {
                        "backend": "ABotStreamingBackend",
                        "model": "ABot-Recon (AMAP CVLab)",
                        "version": "1.0.0",
                        "qualification": "RESEARCH_EXPERIMENTAL",
                        "commercial_status": "NOT CLEARED / NON-COMMERCIAL",
                        "scale_status": "MODEL_ESTIMATED / NOT INDEPENDENTLY VERIFIED",
                        "geometry_source": "MONOCULAR_NEURAL_RECONSTRUCTION",
                        "metric_constraint": "NONE",
                        "status": "STREAMING" if self.running else "READY"
                    }
                    client_sock.sendall((json.dumps(info) + "\n").encode("utf-8"))
                elif cmd.startswith("START_ACQUISITION"):
                    parts = cmd.split(maxsplit=1)
                    dev = parts[1] if len(parts) > 1 else self.video_dev
                    ok = self.start_acquisition(dev)
                    client_sock.sendall(b"OK\n" if ok else b"ERROR\n")
                elif cmd == "STOP_ACQUISITION":
                    self.stop_acquisition()
                    client_sock.sendall(b"OK\n")
                elif cmd == "GET_TELEMETRY":
                    tel = self.get_telemetry()
                    client_sock.sendall((json.dumps(tel) + "\n").encode("utf-8"))
                elif cmd == "GET_NEW_POINTS":
                    pts = self.backend.get_new_points()
                    if pts:
                        # Pack binary CNPT packet
                        # Magic CNPT (4 bytes), count (uint32)
                        # Each point: float32 x, y, z, intensity (4f), uint8 r, g, b, flag (4B) = 20 bytes
                        count = len(pts)
                        header = b"CNPT" + struct.pack("<I", count)
                        body = bytearray()
                        for p in pts:
                            if len(p) >= 7:
                                x, y, z, conf, r, g, b = p[:7]
                            elif len(p) == 6:
                                x, y, z, r, g, b = p
                                conf = 1.0
                            else:
                                x, y, z = p[:3]
                                conf, r, g, b = 1.0, 200, 200, 200
                            body.extend(struct.pack("<ffffBBBB", float(x), float(y), float(z), float(conf), int(r), int(g), int(b), 1))
                        client_sock.sendall(header + body)
                    else:
                        client_sock.sendall(b"CNPT\x00\x00\x00\x00")
                elif cmd == "GET_TRAJECTORY":
                    traj = self.backend.get_trajectory()
                    client_sock.sendall((json.dumps(traj) + "\n").encode("utf-8"))
                elif cmd == "RESET":
                    self.stop_acquisition()
                    client_sock.sendall(b"OK\n")
                else:
                    client_sock.sendall(b"UNKNOWN_COMMAND\n")
        except Exception as e:
            print(f"[ABotDaemon] Client error: {e}")
        finally:
            client_sock.close()

    def run(self):
        self.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_sock.bind((self.host, self.port))
        self.server_sock.listen(5)
        print(f"[ABotDaemon] Server listening on {self.host}:{self.port}")

        while True:
            try:
                client_sock, addr = self.server_sock.accept()
                t = threading.Thread(target=self.handle_client, args=(client_sock,), daemon=True)
                t.start()
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"[ABotDaemon] Accept error: {e}")
                time.sleep(0.1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="AV Scan ABot Reconstruction Daemon")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9103)
    parser.add_argument("--video-dev", default="/dev/video0")
    args = parser.parse_args()

    daemon = ABotDaemon(host=args.host, port=args.port, default_video_dev=args.video_dev)
    try:
        daemon.run()
    finally:
        daemon.stop_acquisition()
