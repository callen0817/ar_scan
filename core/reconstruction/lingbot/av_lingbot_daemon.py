#!/usr/bin/env python3
"""
AV Scan — Generic LingBot Neural Reconstruction Daemon
Port 9102

Hosts the generic LingBotStreamingBackend foundation model reconstruction engine
for monocular spatial scanning. Provides telemetry, pose trajectory estimation,
and real XYZRGB spatial point streaming with scale status tracking.
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
import cv2

sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

# Ensure repository root is in python path
AV_SCAN_ROOT = "/home/scanar/av_scan"
if AV_SCAN_ROOT not in sys.path:
    sys.path.insert(0, AV_SCAN_ROOT)

# Import generic LingBot backend
from core.reconstruction.lingbot.lingbot_streaming_backend import LingBotStreamingBackend

class LingBotDaemon:
    def __init__(self, host="127.0.0.1", port=9102, default_video_dev="/dev/video2"):
        self.host = host
        self.port = port
        self.default_video_dev = default_video_dev
        self.video_dev = default_video_dev

        self.backend = LingBotStreamingBackend()
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
        print("[LingBotDaemon] Preloading LingBot foundation model weights in background...")
        threading.Thread(target=self.backend.load_model, daemon=True).start()

    def start_camera(self, dev_path=None):
        if dev_path:
            self.video_dev = dev_path

        dev_idx = 2
        if isinstance(self.video_dev, int):
            dev_idx = self.video_dev
        elif self.video_dev.startswith("/dev/video"):
            try:
                dev_idx = int(self.video_dev.replace("/dev/video", ""))
            except ValueError:
                dev_idx = 2

        if not isinstance(self.video_dev, int) and not str(self.video_dev).startswith("/dev/video"):
            print(f"[LingBotDaemon] Error: Requested device '{self.video_dev}' is not a V4L2 video device node (e.g. /dev/video*). Refusing to hijack an alternate camera.")
            self.cap = None
            return False

        print(f"[LingBotDaemon] Opening V4L2 video capture on {self.video_dev} (index {dev_idx})...")
        self.cap = cv2.VideoCapture(dev_idx, cv2.CAP_V4L2)
        if not self.cap or not self.cap.isOpened():
            print(f"[LingBotDaemon] Error: Failed to open V4L2 device node: {self.video_dev} (index {dev_idx})")
            self.cap = None
            return False

        self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        self.cap.set(cv2.CAP_PROP_FPS, 30.0)
        print(f"[LingBotDaemon] Camera successfully opened: {self.video_dev} (1920x1080 @ 30 FPS)")
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
                print("[LingBotDaemon] Waiting for LingBot model to finish loading...")
                time.sleep(1.0)

            # Start backend foundation model worker
            if not self.backend.start():
                print("[LingBotDaemon] Error: Failed to start LingBotStreamingBackend")
                self.running = False
                return False

            # Start camera capture
            if not self.start_camera(dev_path):
                print(f"[LingBotDaemon] Error: Failed to start camera capture on {dev_path}")
                self.backend.stop()
                self.running = False
                return False
            self.capture_running = True
            self.capture_thread = threading.Thread(target=self.capture_loop, daemon=True)
            self.capture_thread.start()
            print("[LingBotDaemon] Acquisition started successfully.")
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
            print("[LingBotDaemon] Acquisition stopped.")
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
            "memory_gb": round(b_tel.get("current_gpu_gb", 0.0), 2)
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
                        "backend": "LingBotStreamingBackend",
                        "model": "lingbot-map",
                        "version": "1.0.0",
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
                    # Stream new XYZRGB points
                    pts = self.backend.get_new_points()
                    if pts:
                        # Pack binary CNPT packet
                        # Magic CNPT (4 bytes), count (uint32)
                        # Each point: float32 x, y, z, intensity (4f), uint8 r, g, b, flag (4B) = 20 bytes
                        count = len(pts)
                        header = b"CNPT" + struct.pack("<I", count)
                        body = bytearray()
                        for p in pts:
                            # p is (x, y, z, r, g, b) or (x, y, z, conf, r, g, b)
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
            print(f"[LingBotDaemon] Client error: {e}")
        finally:
            client_sock.close()

    def run(self):
        self.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_sock.bind((self.host, self.port))
        self.server_sock.listen(5)
        print(f"[LingBotDaemon] Server listening on {self.host}:{self.port}")

        while True:
            try:
                client_sock, addr = self.server_sock.accept()
                t = threading.Thread(target=self.handle_client, args=(client_sock,), daemon=True)
                t.start()
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"[LingBotDaemon] Accept error: {e}")
                time.sleep(0.1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="AV Scan LingBot Reconstruction Daemon")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9102)
    parser.add_argument("--video-dev", default="/dev/video2")
    args = parser.parse_args()

    daemon = LingBotDaemon(host=args.host, port=args.port, default_video_dev=args.video_dev)
    try:
        daemon.run()
    finally:
        daemon.stop_acquisition()
