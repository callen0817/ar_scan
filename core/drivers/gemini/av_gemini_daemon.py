#!/usr/bin/env python3
"""
AV Scan — Orbbec Gemini 336L Hardware & RTAB-Map RGB-D SLAM Isolated Bridge Daemon
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and open-source Orbbec SDK / RTAB-Map SLAM stack.

Exposes a low-latency localhost TCP control and data streaming server on port 9100.
"""

import os
import sys
import time
import math
import json
import socket
import struct
import signal
import subprocess
import threading
from collections import deque
import numpy as np

# Default ROS 2 domain if not set
if "ROS_DOMAIN_ID" not in os.environ:
    os.environ["ROS_DOMAIN_ID"] = "0"

DEFAULT_PORT = 9100
HOST = "127.0.0.1"

def quat_to_yaw(qx, qy, qz, qw):
    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    return math.atan2(siny_cosp, cosy_cosp)

class GeminiSlamDaemon:
    def __init__(self, port=DEFAULT_PORT):
        self.port = port
        self.running = True
        self.is_capturing = False
        self.lock = threading.Lock()

        # Processes
        self.camera_proc = None
        self.slam_proc = None
        self.ros_thread = None
        self.ros_node = None

        # SLAM and Telemetry State
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
        self.canonical_R = None
        self._load_canonical_transform()
        self.cloud_rate_hz = 0.0
        self.odom_rate_hz = 0.0
        self._cloud_count = 0
        self._odom_count = 0
        self._rate_calc_time = time.time()

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
            "manufacturer": "Orbbec",
            "model": "Gemini 336L",
            "device_family": "ACTIVE_STEREO_RGBD_CAMERA",
            "serial": "CPC6463000XW",
            "firmware": "1.4.60",
            "product_id": "0x0807",
            "transport": "USB3_V4L2",
            "slam_backend": "RTAB-Map",
            "slam_family": "RGBD_SLAM",
            "depth_resolution": "640x400@30fps",
            "color_resolution": "640x400@30fps",
            "imu_rate": "200Hz",
            "internal_extrinsics": {
                "translation": [-0.00042455, -0.02373056, 0.00009320],
                "quaternion": [-0.0006584, -0.0001741, -0.0011888, 0.999999]
            }
        }

    def _load_canonical_transform(self):
        self.canonical_R = None
        tf_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "canonical_transform.json")
        if os.path.exists(tf_path):
            try:
                with open(tf_path, "r") as f:
                    data = json.load(f)
                    if "matrix_3x3" in data:
                        self.canonical_R = data["matrix_3x3"]
                        print(f"[GeminiDaemon] Loaded canonical transform from {tf_path}")
                        return
            except Exception as e:
                print(f"[GeminiDaemon] Error reading {tf_path}: {e}")

        prof_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../profiles/validated/gemini_336l_profile.json")
        if os.path.exists(prof_path):
            try:
                with open(prof_path, "r") as f:
                    data = json.load(f)
                    c_tf = data.get("canonical_av_transform", {})
                    if "matrix_3x3" in c_tf:
                        self.canonical_R = c_tf["matrix_3x3"]
                        print(f"[GeminiDaemon] Loaded canonical transform from {prof_path}")
            except Exception as e:
                print(f"[GeminiDaemon] Error reading {prof_path}: {e}")

    def start_acquisition(self):
        with self.lock:
            if self.is_capturing:
                return True, "Already capturing"

            self._load_canonical_transform()
            print("[GeminiDaemon] Starting Orbbec Gemini 336L driver and RTAB-Map RGB-D SLAM...")
            self.global_voxels.clear()
            self.trajectory.clear()
            self.new_points_buffer.clear()
            self.current_pose = {
                "x": 0.0, "y": 0.0, "z": 0.0,
                "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
                "yaw": 0.0, "timestamp_ns": int(time.time() * 1e9)
            }
            self.tracking_state = "INITIALIZING"

            # 1. Launch Orbbec Camera Driver
            cmd_cam = [
                "bash", "-c",
                "source /opt/ros/humble/setup.bash && "
                "source /home/scanar/scanarMini/install/setup.bash && "
                "exec ros2 launch orbbec_camera gemini_330_series.launch.py "
                "camera_name:=camera "
                "publish_tf:=true "
                "enable_color:=true "
                "enable_depth:=true "
                "depth_registration:=true "
                "enable_point_cloud:=true "
                "enable_colored_point_cloud:=true "
                "enable_accel:=true "
                "enable_gyro:=true "
                "unite_imu_method:=copy "
                "color_width:=640 color_height:=400 color_fps:=30 "
                "depth_width:=640 depth_height:=400 depth_fps:=30"
            ]
            self.camera_proc = subprocess.Popen(
                cmd_cam,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                preexec_fn=os.setsid
            )

            # 2. Launch RTAB-Map RGB-D SLAM
            cmd_slam = [
                "bash", "-c",
                "source /opt/ros/humble/setup.bash && "
                "exec ros2 launch rtabmap_launch rtabmap.launch.py "
                "rtabmap_args:=\"--delete_db_on_start --Mem/IncrementalMemory true --RGBD/LinearUpdate 0.05 --RGBD/AngularUpdate 0.05 --Optimizer/GravitySigma 0.3\" "
                "rgb_topic:=/camera/color/image_raw "
                "depth_topic:=/camera/depth/image_raw "
                "camera_info_topic:=/camera/color/camera_info "
                "imu_topic:=/camera/gyro_accel/sample "
                "wait_imu_to_init:=false "
                "frame_id:=camera_link "
                "approx_sync:=true "
                "visual_odometry:=true "
                "rtabmap_viz:=false "
                "rviz:=false "
                "qos:=1"
            ]
            self.slam_proc = subprocess.Popen(
                cmd_slam,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                preexec_fn=os.setsid
            )

            self.is_capturing = True

            # 3. Start ROS 2 subscriber thread
            self.ros_thread = threading.Thread(target=self._ros_subscriber_loop, daemon=True)
            self.ros_thread.start()

            print(f"[GeminiDaemon] Acquisition started. Camera PID: {self.camera_proc.pid}, SLAM PID: {self.slam_proc.pid}")
            return True, "Acquisition and RTAB-Map SLAM started"

    def stop_acquisition(self):
        with self.lock:
            if not self.is_capturing:
                return True, "Already stopped"

            print("[GeminiDaemon] Stopping Gemini 336L camera and RTAB-Map SLAM...")
            self.is_capturing = False
            self.tracking_state = "STOPPED"

            # Terminate child processes safely
            for proc, name in [(self.slam_proc, "rtabmap"), (self.camera_proc, "orbbec_camera")]:
                if proc and proc.poll() is None:
                    try:
                        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
                        time.sleep(0.4)
                        if proc.poll() is None:
                            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                            proc.wait(timeout=1.5)
                    except Exception:
                        try:
                            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                        except Exception:
                            pass
            self.slam_proc = None
            self.camera_proc = None

            print(f"[GeminiDaemon] Acquisition stopped. Final map points: {len(self.global_voxels)}, Trajectory: {len(self.trajectory)}")
            return True, "Acquisition stopped"

    def _ros_subscriber_loop(self):
        """ROS 2 Node loop running in background thread."""
        try:
            import rclpy
            from rclpy.node import Node
            from rclpy.qos import qos_profile_sensor_data, QoSProfile, ReliabilityPolicy, HistoryPolicy
            from nav_msgs.msg import Odometry, OccupancyGrid
            from sensor_msgs.msg import PointCloud2
            import sensor_msgs_py.point_cloud2 as pc2
        except ImportError as e:
            print(f"[GeminiDaemon] Error importing ROS 2 modules: {e}")
            return

        try:
            rclpy.init(args=None)
        except Exception:
            pass

        node = Node("av_gemini_bridge_subscriber")
        self.ros_node = node

        qos = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=10
        )

        def odom_callback(msg: Odometry):
            if not self.is_capturing:
                return
            t_ns = msg.header.stamp.sec * 1000000000 + msg.header.stamp.nanosec
            p = msg.pose.pose.position
            o = msg.pose.pose.orientation
            qx, qy, qz, qw = float(o.x), float(o.y), float(o.z), float(o.w)
            yaw = quat_to_yaw(qx, qy, qz, qw)

            with self.lock:
                if self.canonical_R:
                    px = self.canonical_R[0][0] * p.x + self.canonical_R[0][1] * p.y + self.canonical_R[0][2] * p.z
                    py = self.canonical_R[1][0] * p.x + self.canonical_R[1][1] * p.y + self.canonical_R[1][2] * p.z
                    pz = self.canonical_R[2][0] * p.x + self.canonical_R[2][1] * p.y + self.canonical_R[2][2] * p.z
                    # Rotate 3x3 orientation matrix by canonical_R
                    R_q = np.array([
                        [1 - 2*(qy*qy + qz*qz), 2*(qx*qy - qz*qw),     2*(qx*qz + qy*qw)],
                        [2*(qx*qy + qz*qw),     1 - 2*(qx*qx + qz*qz), 2*(qy*qz - qx*qw)],
                        [2*(qx*qz - qy*qw),     2*(qy*qz + qx*qw),     1 - 2*(qx*qx + qy*qy)]
                    ], dtype=np.float64)
                    R_c = np.array(self.canonical_R, dtype=np.float64) @ R_q
                    tr = R_c[0, 0] + R_c[1, 1] + R_c[2, 2]
                    if tr > 0:
                        S = math.sqrt(tr + 1.0) * 2.0
                        qw = 0.25 * S
                        qx = (R_c[2, 1] - R_c[1, 2]) / S
                        qy = (R_c[0, 2] - R_c[2, 0]) / S
                        qz = (R_c[1, 0] - R_c[0, 1]) / S
                    elif (R_c[0, 0] > R_c[1, 1]) and (R_c[0, 0] > R_c[2, 2]):
                        S = math.sqrt(1.0 + R_c[0, 0] - R_c[1, 1] - R_c[2, 2]) * 2.0
                        qw = (R_c[2, 1] - R_c[1, 2]) / S
                        qx = 0.25 * S
                        qy = (R_c[0, 1] + R_c[1, 0]) / S
                        qz = (R_c[0, 2] + R_c[2, 0]) / S
                    elif R_c[1, 1] > R_c[2, 2]:
                        S = math.sqrt(1.0 + R_c[1, 1] - R_c[0, 0] - R_c[2, 2]) * 2.0
                        qw = (R_c[0, 2] - R_c[2, 0]) / S
                        qx = (R_c[0, 1] + R_c[1, 0]) / S
                        qy = 0.25 * S
                        qz = (R_c[1, 2] + R_c[2, 1]) / S
                    else:
                        S = math.sqrt(1.0 + R_c[2, 2] - R_c[0, 0] - R_c[1, 1]) * 2.0
                        qw = (R_c[1, 0] - R_c[0, 1]) / S
                        qx = (R_c[0, 2] + R_c[2, 0]) / S
                        qy = (R_c[1, 2] + R_c[2, 1]) / S
                        qz = 0.25 * S
                    yaw = quat_to_yaw(qx, qy, qz, qw)
                else:
                    px, py, pz = float(p.x), float(p.y), float(p.z)

                self.current_pose = {
                    "x": float(px),
                    "y": float(py),
                    "z": float(pz),
                    "qx": float(qx),
                    "qy": float(qy),
                    "qz": float(qz),
                    "qw": float(qw),
                    "yaw": float(yaw),
                    "timestamp_ns": int(t_ns)
                }
                self.tracking_state = "TRACKING_OK"
                self._odom_count += 1
                now = time.time()
                elapsed = now - self._rate_calc_time
                if elapsed >= 1.0:
                    self.cloud_rate_hz = round(self._cloud_count / elapsed, 1)
                    self.odom_rate_hz = round(self._odom_count / elapsed, 1)
                    self._cloud_count = 0
                    self._odom_count = 0
                    self._rate_calc_time = now

                # Append trajectory if motion exceeds threshold
                if not self.trajectory:
                    self.trajectory.append((t_ns, px, py, pz, qx, qy, qz, qw, yaw))
                else:
                    last = self.trajectory[-1]
                    dist_sq = (px - last[1])**2 + (py - last[2])**2 + (pz - last[3])**2
                    angle_diff = abs(yaw - last[8])
                    if dist_sq >= 0.0004 or angle_diff >= 0.015:  # 2cm or ~0.8 deg
                        self.trajectory.append((t_ns, px, py, pz, qx, qy, qz, qw, yaw))

        def cloud_callback(msg: PointCloud2):
            if not self.is_capturing:
                return
            num_points = msg.width * msg.height
            if num_points == 0 or msg.point_step < 12:
                return
            with self.lock:
                self._cloud_count += 1
                R_canon = np.array(self.canonical_R, dtype=np.float32) if self.canonical_R else None
                pose_xyz = (
                    float(self.current_pose.get("x", 0.0)),
                    float(self.current_pose.get("y", 0.0)),
                    float(self.current_pose.get("z", 0.0))
                )
            try:
                raw = bytes(msg.data)
                field_map = {f.name: f.offset for f in msg.fields}
                if "x" not in field_map or "y" not in field_map or "z" not in field_map:
                    return

                dt_names = ["x", "y", "z"]
                dt_formats = ["<f4", "<f4", "<f4"]
                dt_offsets = [field_map["x"], field_map["y"], field_map["z"]]
                has_rgb = ("rgb" in field_map) or ("rgba" in field_map)
                if has_rgb:
                    rgb_off = field_map.get("rgb", field_map.get("rgba"))
                    dt_names.append("rgb")
                    dt_formats.append("<u4")
                    dt_offsets.append(rgb_off)

                dt = np.dtype({
                    "names": dt_names,
                    "formats": dt_formats,
                    "offsets": dt_offsets,
                    "itemsize": msg.point_step
                })
                pts = np.frombuffer(raw, dtype=dt, count=num_points)
                valid = np.isfinite(pts["x"]) & np.isfinite(pts["y"]) & np.isfinite(pts["z"])
                pts_valid = pts[valid]
                if len(pts_valid) == 0:
                    return

                coords = np.column_stack((pts_valid["x"], pts_valid["y"], pts_valid["z"])).astype(np.float32, copy=False)
                if R_canon is not None:
                    coords = coords @ R_canon.T

                # Range gate relative to current pose (0.15m minimum blind-spot filter, 12.0m max global stereo horizon)
                dx = coords[:, 0] - pose_xyz[0]
                dy = coords[:, 1] - pose_xyz[1]
                dz = coords[:, 2] - pose_xyz[2]
                dist_sq = dx * dx + dy * dy + dz * dz
                range_mask = (dist_sq >= 0.0225) & (dist_sq <= 144.0)
                if not np.all(range_mask):
                    coords = coords[range_mask]
                    pts_valid = pts_valid[range_mask]
                    if len(coords) == 0:
                        return

                if has_rgb:
                    rgb_u32 = pts_valid["rgb"]
                    rs = ((rgb_u32 >> 16) & 0xFF).astype(np.uint8)
                    gs = ((rgb_u32 >> 8) & 0xFF).astype(np.uint8)
                    bs = (rgb_u32 & 0xFF).astype(np.uint8)
                    has_c = 1
                else:
                    rs = np.full(len(coords), 255, dtype=np.uint8)
                    gs = np.full(len(coords), 255, dtype=np.uint8)
                    bs = np.full(len(coords), 255, dtype=np.uint8)
                    has_c = 0

                v_size = self.voxel_size
                gxs = np.floor(coords[:, 0] / v_size).astype(np.int64)
                gys = np.floor(coords[:, 1] / v_size).astype(np.int64)
                gzs = np.floor(coords[:, 2] / v_size).astype(np.int64)

                # Deduplicate within current frame using packed 64-bit spatial hash before taking lock
                keys64 = ((gxs & 0x1FFFFF) << 42) | ((gys & 0x1FFFFF) << 21) | (gzs & 0x1FFFFF)
                _, unique_idx = np.unique(keys64, return_index=True)

                u_coords = coords[unique_idx]
                u_rs = rs[unique_idx]
                u_gs = gs[unique_idx]
                u_bs = bs[unique_idx]
                u_gxs = gxs[unique_idx]
                u_gys = gys[unique_idx]
                u_gzs = gzs[unique_idx]

                new_pts = []
                with self.lock:
                    voxels = self.global_voxels
                    for i in range(len(unique_idx)):
                        key = (int(u_gxs[i]), int(u_gys[i]), int(u_gzs[i]))
                        x, y, z = float(u_coords[i, 0]), float(u_coords[i, 1]), float(u_coords[i, 2])
                        r, g, b = int(u_rs[i]), int(u_gs[i]), int(u_bs[i])
                        prev = voxels.get(key)
                        if prev is None:
                            pt_tuple = (x, y, z, 1.0, r, g, b, has_c)
                            voxels[key] = pt_tuple
                            new_pts.append(pt_tuple)
                        else:
                            # Running exponential centroid & RGB refinement (0.75 prior + 0.25 new observation)
                            voxels[key] = (
                                0.75 * prev[0] + 0.25 * x,
                                0.75 * prev[1] + 0.25 * y,
                                0.75 * prev[2] + 0.25 * z,
                                1.0,
                                int(0.75 * prev[4] + 0.25 * r),
                                int(0.75 * prev[5] + 0.25 * g),
                                int(0.75 * prev[6] + 0.25 * b),
                                has_c
                            )

                    if new_pts:
                        self.new_points_buffer.extend(new_pts)
            except Exception:
                pass

        def grid_callback(msg: OccupancyGrid):
            if not self.is_capturing:
                return
            with self.lock:
                self.grid_map["width"] = msg.info.width
                self.grid_map["height"] = msg.info.height
                self.grid_map["resolution"] = msg.info.resolution
                self.grid_map["origin_x"] = msg.info.origin.position.x
                self.grid_map["origin_y"] = msg.info.origin.position.y
                self.grid_map["data"] = list(msg.data[::4]) if len(msg.data) > 10000 else list(msg.data)

        node.create_subscription(Odometry, "/rtabmap/odom", odom_callback, qos)
        node.create_subscription(PointCloud2, "/rtabmap/cloud_map", cloud_callback, 10)
        node.create_subscription(OccupancyGrid, "/rtabmap/map", grid_callback, 10)

        print("[GeminiDaemon] ROS 2 subscriber loop spinning...")
        while self.is_capturing and rclpy.ok():
            try:
                rclpy.spin_once(node, timeout_sec=0.1)
            except Exception:
                break

        node.destroy_node()
        print("[GeminiDaemon] ROS 2 subscriber loop ended.")

    def handle_client(self, conn, addr):
        print(f"[GeminiDaemon] Client connected from {addr}")
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
                            "feature_quality": self.feature_quality,
                            "acquisition_rate_hz": self.cloud_rate_hz,
                            "slam_rate_hz": self.odom_rate_hz
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
            print(f"[GeminiDaemon] Client handler error: {e}")
        finally:
            conn.close()
            print(f"[GeminiDaemon] Client disconnected from {addr}")

    def run(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, self.port))
        server.listen(5)
        print(f"[GeminiDaemon] Serving AV Scan IPC on {HOST}:{self.port}...")

        def signal_handler(sig, frame):
            print("[GeminiDaemon] Terminating via signal...")
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
                t = threading.Thread(target=self.handle_client, args=(conn, addr), daemon=True)
                t.start()
            except Exception:
                if not self.running:
                    break

def main():
    daemon = GeminiSlamDaemon(port=DEFAULT_PORT)
    daemon.run()

if __name__ == "__main__":
    main()
