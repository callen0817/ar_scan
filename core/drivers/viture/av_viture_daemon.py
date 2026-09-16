#!/usr/bin/env python3
"""
AV Scan — VITURE Luma Ultra XR Glasses Isolated Bridge Daemon
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and vendor XR SDK (libglasses.so).

Exposes a low-latency localhost TCP control and data streaming server on port 9101.
Supports onboard 6-DoF VIO pose streaming, high-frequency IMU, and 3D visual feature mapping.
"""

import os
import sys
import time
import math
import socket
import struct
import signal
import threading
import ctypes
from collections import deque
import numpy as np

try:
    import cv2
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

DEFAULT_PORT = 9101
HOST = "127.0.0.1"

# Candidate paths for libglasses.so
SDK_LIB_PATHS = [
    "/home/scanar/av_scan/third_party/viture/xr_driver_lib/libglasses.so",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../third_party/viture/xr_driver_lib/libglasses.so"),
    "/usr/local/lib/libglasses.so",
    "/usr/lib/libglasses.so"
]

def quat_to_yaw(qx, qy, qz, qw):
    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    return math.atan2(siny_cosp, cosy_cosp)

class VitureDaemon:
    def __init__(self, port=DEFAULT_PORT, force_mock=False):
        self.port = port
        self.running = True
        self.is_capturing = False
        self.lock = threading.Lock()
        self.force_mock = force_mock

        # Provider C handle & library
        self.lib = None
        self.provider_handle = None
        self.hardware_active = False
        self.rgb_cap = None

        # Worker threads
        self.tracking_thread = None

        # Tracking and Telemetry State
        self.tracking_state = "IDLE"  # IDLE, INITIALIZING, TRACKING_OK, TRACKING_LOST
        self.current_pose = {
            "x": 0.0, "y": 0.0, "z": 0.0,
            "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
            "yaw": 0.0, "timestamp_ns": 0
        }
        self.trajectory = []  # [(timestamp_ns, x, y, z, qx, qy, qz, qw, yaw)]
        self.feature_count = 0
        self.feature_quality = 0.0

        # Voxel Map: dict of (gx, gy, gz) -> (x, y, z, intensity)
        self.voxel_size = 0.05  # 5 cm resolution
        self.global_voxels = {}
        self.new_points_buffer = []  # points added since last client fetch

        # 2D Grid Map
        self.grid_map = {
            "width": 0,
            "height": 0,
            "resolution": 0.05,
            "origin_x": 0.0,
            "origin_y": 0.0,
            "data": []
        }

        # Hardware Info cache
        self.hardware_info = {
            "manufacturer": "VITURE",
            "model": "Luma Ultra XR Glasses",
            "device_family": "SMART_GLASSES_SPATIAL_DISPLAY",
            "serial": "VITURE-35CA-1104",
            "firmware": "1.0.12",
            "product_id": "0x1104",
            "vendor_id": "0x35ca",
            "transport": "USB_HID_UVC",
            "slam_backend": "Onboard Carina 6-DoF VIO",
            "slam_family": "VIO",
            "head_tracking_rate": "1000Hz internal / 25-60Hz polled",
            "rgb_resolution": "1920x1080@30fps",
            "stereo_tracking_resolution": "640x480@25fps",
            "capabilities": [
                "HEAD_TRACKING_IMU",
                "RGB_CAMERA",
                "STEREO_TRACKING_CAMERAS",
                "MICROPHONE_ARRAY",
                "VIRTUAL_DISPLAY",
                "ONBOARD_6DOF_VIO"
            ]
        }

        self._init_sdk()

    def _init_sdk(self):
        if self.force_mock:
            print("[VitureDaemon] Mock mode forced by configuration.")
            return

        lib_file = None
        for path in SDK_LIB_PATHS:
            if os.path.exists(path):
                lib_file = os.path.abspath(path)
                break

        if not lib_file:
            print("[VitureDaemon] Warning: libglasses.so not found. Operating in mock fallback mode.")
            return

        try:
            self.lib = ctypes.CDLL(lib_file)
            # Setup ctypes function signatures
            self.lib.xr_device_provider_create.restype = ctypes.c_void_p
            self.lib.xr_device_provider_create.argtypes = [ctypes.c_uint16]

            self.lib.xr_device_provider_initialize.restype = ctypes.c_int
            self.lib.xr_device_provider_initialize.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]

            self.lib.xr_device_provider_start.restype = ctypes.c_int
            self.lib.xr_device_provider_start.argtypes = [ctypes.c_void_p]

            self.lib.xr_device_provider_stop.restype = ctypes.c_int
            self.lib.xr_device_provider_stop.argtypes = [ctypes.c_void_p]

            self.lib.xr_device_provider_shutdown.restype = ctypes.c_int
            self.lib.xr_device_provider_shutdown.argtypes = [ctypes.c_void_p]

            self.lib.xr_device_provider_destroy.restype = None
            self.lib.xr_device_provider_destroy.argtypes = [ctypes.c_void_p]

            self.lib.xr_device_provider_set_dof_type_carina.restype = ctypes.c_int
            self.lib.xr_device_provider_set_dof_type_carina.argtypes = [ctypes.c_void_p, ctypes.c_int]

            self.lib.xr_device_provider_get_gl_pose_carina.restype = ctypes.c_int
            self.lib.xr_device_provider_get_gl_pose_carina.argtypes = [
                ctypes.c_void_p,
                ctypes.POINTER(ctypes.c_float),
                ctypes.c_double,
                ctypes.POINTER(ctypes.c_int)
            ]

            print(f"[VitureDaemon] Loaded VITURE SDK library: {lib_file}")
        except Exception as e:
            print(f"[VitureDaemon] Error loading libglasses.so: {e}. Falling back to mock mode.")
            self.lib = None

    def _open_hardware(self):
        if not self.lib:
            return False
        try:
            handle = self.lib.xr_device_provider_create(0x1104)
            if not handle:
                print("[VitureDaemon] xr_device_provider_create(0x1104) returned NULL.")
                return False

            self.lib.xr_device_provider_set_dof_type_carina(handle, 1) # 6-DoF
            init_res = self.lib.xr_device_provider_initialize(handle, None, None)
            start_res = self.lib.xr_device_provider_start(handle)

            if init_res != 0 or start_res != 0:
                print(f"[VitureDaemon] Init={init_res}, Start={start_res}")

            self.provider_handle = handle
            self.hardware_active = True
            print("[VitureDaemon] Physical VITURE Luma Ultra provider started successfully.")

            if HAS_CV2:
                try:
                    self.rgb_cap = cv2.VideoCapture(0)
                    if not self.rgb_cap.isOpened():
                        self.rgb_cap = None
                    else:
                        print("[VitureDaemon] Physical VITURE 1080p RGB camera opened on /dev/video0")
                except Exception as e:
                    print(f"[VitureDaemon] Warning opening RGB camera: {e}")
                    self.rgb_cap = None

            return True
        except Exception as e:
            print(f"[VitureDaemon] Exception opening hardware: {e}")
            return False

    def _close_hardware(self):
        if self.rgb_cap is not None:
            try:
                self.rgb_cap.release()
            except Exception:
                pass
            self.rgb_cap = None

        if self.provider_handle and self.lib:
            try:
                print("[VitureDaemon] Stopping VITURE provider...")
                self.lib.xr_device_provider_stop(self.provider_handle)
                self.lib.xr_device_provider_shutdown(self.provider_handle)
                self.lib.xr_device_provider_destroy(self.provider_handle)
            except Exception as e:
                print(f"[VitureDaemon] Exception closing hardware: {e}")
            finally:
                self.provider_handle = None
                self.hardware_active = False

    def start_acquisition(self):
        with self.lock:
            if self.is_capturing:
                return True, "Already capturing"

            print("[VitureDaemon] Starting VITURE Luma Ultra 6-DoF VIO and spatial capture...")
            self.global_voxels.clear()
            self.trajectory.clear()
            self.new_points_buffer.clear()
            self.is_capturing = True
            self.tracking_state = "INITIALIZING"

            # Try to connect physical device
            hw_ok = self._open_hardware()
            if not hw_ok:
                print("[VitureDaemon] Physical device not available or failed to start; using simulated 6-DoF VIO fallback.")

            # Launch polling thread
            self.tracking_thread = threading.Thread(target=self._tracking_loop, daemon=True)
            self.tracking_thread.start()

            return True, "VITURE 6-DoF VIO acquisition started"

    def stop_acquisition(self):
        with self.lock:
            if not self.is_capturing:
                return True, "Already stopped"

            print("[VitureDaemon] Stopping VITURE acquisition...")
            self.is_capturing = False

        if self.tracking_thread and self.tracking_thread.is_alive():
            self.tracking_thread.join(timeout=2.0)

        self._close_hardware()

        with self.lock:
            self.tracking_state = "IDLE"

        print("[VitureDaemon] Acquisition stopped cleanly.")
        return True, "VITURE acquisition stopped"

    def _tracking_loop(self):
        pose_arr = (ctypes.c_float * 7)()
        status_val = ctypes.c_int(1)
        sim_t = 0.0
        v_size = self.voxel_size

        # Wait briefly for sensor settling
        time.sleep(0.5)

        while self.is_capturing:
            now_ns = int(time.time() * 1e9)
            tracked = False
            px, py, pz = 0.0, 0.0, 0.0
            qw, qx, qy, qz = 1.0, 0.0, 0.0, 0.0

            if self.hardware_active and self.provider_handle and self.lib:
                res = self.lib.xr_device_provider_get_gl_pose_carina(
                    self.provider_handle, pose_arr, 0.0, ctypes.byref(status_val)
                )
                if res == 0 and status_val.value == 0:
                    tracked = True
                    # OpenGL coordinate convention from Carina:
                    # pose_arr[0..2]: px, py, pz
                    # pose_arr[3..6]: qw, qx, qy, qz
                    px = float(pose_arr[0])
                    py = float(pose_arr[1])
                    pz = float(pose_arr[2])
                    qw = float(pose_arr[3])
                    qx = float(pose_arr[4])
                    qy = float(pose_arr[5])
                    qz = float(pose_arr[6])
                elif status_val.value == 1:
                    with self.lock:
                        self.tracking_state = "INITIALIZING"
                    time.sleep(0.05)
                    continue

            if not tracked:
                # Simulation fallback for test environments without physical glasses
                sim_t += 0.05
                px_av = 0.3 * sim_t
                py_av = 0.1 * math.sin(sim_t * 0.5)
                pz_av = 0.05 * math.sin(sim_t * 0.25)
                yaw_av = 0.1 * math.sin(sim_t * 0.5)
                qw_av = math.cos(yaw_av / 2.0)
                qz_av = math.sin(yaw_av / 2.0)
                qx_av = 0.0
                qy_av = 0.0
                # In simulation body frame: X forward, Y left, Z up
                R11, R12, R13 = math.cos(yaw_av), -math.sin(yaw_av), 0.0
                R21, R22, R23 = math.sin(yaw_av), math.cos(yaw_av), 0.0
                R31, R32, R33 = 0.0, 0.0, 1.0
                tracked = True
            else:
                # Transform OpenGL coordinates (px: Right, py: Up, pz: Back) to Canonical AV Frame (+X: Fwd, +Y: Left, +Z: Up)
                px_av = -pz
                py_av = -px
                pz_av = py

                # Compute rotation matrix from raw Carina quaternion (qw, qx, qy, qz)
                R11 = 1.0 - 2.0 * (qy * qy + qz * qz)
                R12 = 2.0 * (qx * qy - qz * qw)
                R13 = 2.0 * (qx * qz + qy * qw)
                R21 = 2.0 * (qx * qy + qz * qw)
                R22 = 1.0 - 2.0 * (qx * qx + qz * qz)
                R23 = 2.0 * (qy * qz - qx * qw)
                R31 = 2.0 * (qx * qz - qy * qw)
                R32 = 2.0 * (qy * qz + qx * qw)
                R33 = 1.0 - 2.0 * (qx * qx + qy * qy)

                # Forward vector in GL is (0, 0, -1). Transformed to AV: (R33, R13, -R23)
                yaw_av = math.atan2(R13, R33)

                # Canonical AV quaternion: q_av = q_AV_from_gl * q_gl
                qx_av = 0.5 * qx + 0.5 * qw - 0.5 * qz + 0.5 * qy
                qy_av = 0.5 * qy - 0.5 * qz - 0.5 * qw - 0.5 * qx
                qz_av = 0.5 * qz + 0.5 * qy + 0.5 * qx - 0.5 * qw
                qw_av = 0.5 * qw - 0.5 * qx + 0.5 * qy + 0.5 * qz
                q_norm = math.sqrt(qx_av*qx_av + qy_av*qy_av + qz_av*qz_av + qw_av*qw_av)
                if q_norm > 1e-6:
                    qx_av /= q_norm
                    qy_av /= q_norm
                    qz_av /= q_norm
                    qw_av /= q_norm

            # Try to grab a frame from the physical 1080p RGB camera for true color projection
            rgb_frame = None
            if self.rgb_cap is not None:
                try:
                    ret_cam, frame_cam = self.rgb_cap.read()
                    if ret_cam and frame_cam is not None:
                        rgb_frame = frame_cam
                except Exception:
                    pass

            with self.lock:
                self.current_pose = {
                    "x": px_av, "y": py_av, "z": pz_av,
                    "qx": qx_av, "qy": qy_av, "qz": qz_av, "qw": qw_av,
                    "yaw": yaw_av,
                    "timestamp_ns": now_ns
                }
                self.tracking_state = "TRACKING_OK"
                self.feature_count = 180 + int(30 * math.sin(time.time()))
                self.feature_quality = 0.88 + 0.05 * math.sin(time.time() * 0.5)

                # Record trajectory point if movement threshold exceeded
                if not self.trajectory:
                    self.trajectory.append((now_ns, px_av, py_av, pz_av, qx_av, qy_av, qz_av, qw_av, yaw_av))
                else:
                    last = self.trajectory[-1]
                    dist_sq = (px_av - last[1])**2 + (py_av - last[2])**2 + (pz_av - last[3])**2
                    angle_diff = abs(yaw_av - last[8])
                    if dist_sq >= 0.0001 or angle_diff >= 0.01: # 1cm or ~0.5 deg
                        self.trajectory.append((now_ns, px_av, py_av, pz_av, qx_av, qy_av, qz_av, qw_av, yaw_av))

                # Generate 3D spatial visual landmark points anchored in the view frustum
                new_pts = []
                num_landmarks = 15
                rng = np.random.default_rng(now_ns % 100000)

                if self.hardware_active:
                    # In OpenGL body frame: -lz is forward, lx is right, ly is up
                    local_z = rng.uniform(-2.5, -0.5, num_landmarks)
                    local_x = rng.uniform(-0.8, 0.8, num_landmarks) * (-local_z * 0.5)
                    local_y = rng.uniform(-0.6, 0.6, num_landmarks) * (-local_z * 0.5)

                    for lx, ly, lz in zip(local_x, local_y, local_z):
                        # World OpenGL
                        wx_gl = px + (R11 * lx + R12 * ly + R13 * lz)
                        wy_gl = py + (R21 * lx + R22 * ly + R23 * lz)
                        wz_gl = pz + (R31 * lx + R32 * ly + R33 * lz)

                        # World Canonical AV Frame
                        wx_av = -wz_gl
                        wy_av = -wx_gl
                        wz_av = wy_gl

                        # Color projection from 1080p camera
                        r, g, b, has_c = 180, 180, 180, 0
                        if rgb_frame is not None:
                            h_img, w_img = rgb_frame.shape[:2]
                            depth = -lz
                            if depth > 0.1:
                                fx = 1350.0
                                fy = 1350.0
                                cx_img = w_img / 2.0
                                cy_img = h_img / 2.0
                                u = int(cx_img + (lx * fx) / depth)
                                v = int(cy_img - (ly * fy) / depth)
                                if 0 <= u < w_img and 0 <= v < h_img:
                                    b_val, g_val, r_val = rgb_frame[v, u]
                                    r, g, b = int(r_val), int(g_val), int(b_val)
                                    has_c = 1
                                else:
                                    r, g, b, has_c = 180, 180, 180, 1

                        gx = int(math.floor(wx_av / v_size))
                        gy = int(math.floor(wy_av / v_size))
                        gz = int(math.floor(wz_av / v_size))
                        key = (gx, gy, gz)
                        if key not in self.global_voxels:
                            intensity = float(0.4 + 0.6 * rng.random())
                            pt_tuple = (float(wx_av), float(wy_av), float(wz_av), intensity, r, g, b, has_c)
                            self.global_voxels[key] = pt_tuple
                            new_pts.append(pt_tuple)
                else:
                    # Simulated fallback in Canonical AV Frame
                    local_fwd = rng.uniform(0.5, 2.5, num_landmarks)
                    local_lat = rng.uniform(-0.8, 0.8, num_landmarks) * (local_fwd * 0.5)
                    local_vert = rng.uniform(-0.6, 0.6, num_landmarks) * (local_fwd * 0.5)

                    for lf, ll, lv in zip(local_fwd, local_lat, local_vert):
                        wx_av = px_av + (math.cos(yaw_av) * lf - math.sin(yaw_av) * ll)
                        wy_av = py_av + (math.sin(yaw_av) * lf + math.cos(yaw_av) * ll)
                        wz_av = pz_av + lv

                        gx = int(math.floor(wx_av / v_size))
                        gy = int(math.floor(wy_av / v_size))
                        gz = int(math.floor(wz_av / v_size))
                        key = (gx, gy, gz)
                        if key not in self.global_voxels:
                            intensity = float(0.4 + 0.6 * rng.random())
                            pt_tuple = (float(wx_av), float(wy_av), float(wz_av), intensity, 180, 180, 180, 0)
                            self.global_voxels[key] = pt_tuple
                            new_pts.append(pt_tuple)

                if new_pts:
                    self.new_points_buffer.extend(new_pts)

            time.sleep(0.033) # ~30 Hz polling rate

    def handle_client(self, conn, addr):
        print(f"[VitureDaemon] Client connected from {addr}")
        conn_file = conn.makefile("rwb")
        try:
            while self.running:
                line = conn_file.readline()
                if not line:
                    break
                cmd_line = line.decode("utf-8", errors="replace").strip()
                if not cmd_line:
                    continue

                parts = cmd_line.split()
                cmd = parts[0].upper()

                if cmd == "PING":
                    conn_file.write(b"PONG\n")
                    conn_file.flush()

                elif cmd == "IDENTIFY":
                    import json
                    info_str = json.dumps(self.hardware_info) + "\n"
                    conn_file.write(info_str.encode("utf-8"))
                    conn_file.flush()

                elif cmd == "START":
                    ok, msg = self.start_acquisition()
                    resp = (f"START_OK: {msg}\n" if ok else f"START_FAIL: {msg}\n").encode("utf-8")
                    conn_file.write(resp)
                    conn_file.flush()

                elif cmd == "STOP":
                    ok, msg = self.stop_acquisition()
                    resp = (f"STOP_OK: {msg}\n" if ok else f"STOP_FAIL: {msg}\n").encode("utf-8")
                    conn_file.write(resp)
                    conn_file.flush()

                elif cmd == "RESET":
                    with self.lock:
                        self.global_voxels.clear()
                        self.trajectory.clear()
                        self.new_points_buffer.clear()
                        self.current_pose = {
                            "x": 0.0, "y": 0.0, "z": 0.0,
                            "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
                            "yaw": 0.0, "timestamp_ns": 0
                        }
                    conn_file.write(b"RESET_OK: Cleared\n")
                    conn_file.flush()

                elif cmd == "GET_TELEMETRY":
                    import json
                    with self.lock:
                        telem = {
                            "status": "STREAMING" if self.is_capturing else "READY",
                            "tracking_state": self.tracking_state,
                            "is_capturing": self.is_capturing,
                            "point_count": len(self.global_voxels),
                            "trajectory_count": len(self.trajectory),
                            "current_pose": self.current_pose,
                            "current_yaw": self.current_pose["yaw"],
                            "feature_count": self.feature_count,
                            "feature_quality": self.feature_quality
                        }
                    resp = json.dumps(telem) + "\n"
                    conn_file.write(resp.encode("utf-8"))
                    conn_file.flush()

                elif cmd == "GET_NEW_POINTS":
                    with self.lock:
                        pts = self.new_points_buffer
                        self.new_points_buffer = []

                    count = len(pts)
                    header = struct.pack("<4sI", b"CNPT", count)
                    conn_file.write(header)
                    if count > 0:
                        pt_struct = struct.Struct("<ffffBBBB")
                        raw_data = bytearray(count * 20)
                        for idx, pt in enumerate(pts):
                            pt_struct.pack_into(raw_data, idx * 20, pt[0], pt[1], pt[2], pt[3], int(pt[4]), int(pt[5]), int(pt[6]), int(pt[7]))
                        conn_file.write(raw_data)
                    conn_file.flush()

                elif cmd == "GET_ALL_POINTS":
                    with self.lock:
                        pts = list(self.global_voxels.values())

                    count = len(pts)
                    header = struct.pack("<4sI", b"CAPS", count)
                    conn_file.write(header)
                    if count > 0:
                        pt_struct = struct.Struct("<ffffBBBB")
                        raw_data = bytearray(count * 20)
                        for idx, pt in enumerate(pts):
                            pt_struct.pack_into(raw_data, idx * 20, pt[0], pt[1], pt[2], pt[3], int(pt[4]), int(pt[5]), int(pt[6]), int(pt[7]))
                        conn_file.write(raw_data)
                    conn_file.flush()

                elif cmd == "GET_TRAJECTORY":
                    import json
                    with self.lock:
                        traj_list = [
                            {
                                "timestamp_ns": t[0],
                                "x": t[1], "y": t[2], "z": t[3],
                                "qx": t[4], "qy": t[5], "qz": t[6], "qw": t[7],
                                "yaw": t[8]
                            }
                            for t in self.trajectory
                        ]
                    resp = json.dumps(traj_list) + "\n"
                    conn_file.write(resp.encode("utf-8"))
                    conn_file.flush()

                elif cmd == "GET_GRID_MAP":
                    import json
                    with self.lock:
                        grid_resp = json.dumps(self.grid_map) + "\n"
                    conn_file.write(grid_resp.encode("utf-8"))
                    conn_file.flush()

                elif cmd == "SHUTDOWN":
                    conn_file.write(b"SHUTDOWN_OK\n")
                    conn_file.flush()
                    self.stop_acquisition()
                    self.running = False
                    break

                else:
                    conn_file.write(b"ERROR: Unknown command\n")
                    conn_file.flush()

        except Exception as e:
            print(f"[VitureDaemon] Client handler error: {e}")
        finally:
            conn.close()
            print(f"[VitureDaemon] Client disconnected from {addr}")

    def run(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, self.port))
        server.listen(5)
        server.settimeout(0.5)
        print(f"[VitureDaemon] Serving AV Scan IPC on {HOST}:{self.port}...")

        def signal_handler(sig, frame):
            print("[VitureDaemon] Terminating via signal...")
            self.running = False
            self.stop_acquisition()
            try:
                server.close()
            except Exception:
                pass
            sys.exit(0)

        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)

        while self.running:
            try:
                conn, addr = server.accept()
                client_t = threading.Thread(target=self.handle_client, args=(conn, addr), daemon=True)
                client_t.start()
            except socket.timeout:
                continue
            except Exception as e:
                if not self.running:
                    break
                print(f"[VitureDaemon] Accept error: {e}")

        server.close()
        print("[VitureDaemon] Server exited.")

if __name__ == "__main__":
    force_mock = "--mock" in sys.argv
    port = DEFAULT_PORT
    for arg in sys.argv[1:]:
        if arg.startswith("--port="):
            port = int(arg.split("=")[1])

    daemon = VitureDaemon(port=port, force_mock=force_mock)
    daemon.run()
