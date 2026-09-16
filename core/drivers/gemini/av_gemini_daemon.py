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

    def start_acquisition(self):
        with self.lock:
            if self.is_capturing:
                return True, "Already capturing"

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
                "rtabmap_args:=\"--delete_db_on_start --Mem/IncrementalMemory true --RGBD/LinearUpdate 0.05 --RGBD/AngularUpdate 0.05\" "
                "rgb_topic:=/camera/color/image_raw "
                "depth_topic:=/camera/depth/image_raw "
                "camera_info_topic:=/camera/color/camera_info "
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
            yaw = quat_to_yaw(o.x, o.y, o.z, o.w)

            with self.lock:
                self.current_pose = {
                    "x": float(p.x),
                    "y": float(p.y),
                    "z": float(p.z),
                    "qx": float(o.x),
                    "qy": float(o.y),
                    "qz": float(o.z),
                    "qw": float(o.w),
                    "yaw": float(yaw),
                    "timestamp_ns": int(t_ns)
                }
                self.tracking_state = "TRACKING_OK"

                # Append trajectory if motion exceeds threshold
                if not self.trajectory:
                    self.trajectory.append((t_ns, p.x, p.y, p.z, o.x, o.y, o.z, o.w, yaw))
                else:
                    last = self.trajectory[-1]
                    dist_sq = (p.x - last[1])**2 + (p.y - last[2])**2 + (p.z - last[3])**2
                    angle_diff = abs(yaw - last[8])
                    if dist_sq >= 0.0004 or angle_diff >= 0.015:  # 2cm or ~0.8 deg
                        self.trajectory.append((t_ns, p.x, p.y, p.z, o.x, o.y, o.z, o.w, yaw))

        def cloud_callback(msg: PointCloud2):
            if not self.is_capturing:
                return
            try:
                # Read 3D points and real RGB from map frame
                field_names = [f.name for f in msg.fields]
                has_rgb = "rgb" in field_names
                if has_rgb:
                    gen = pc2.read_points(msg, field_names=("x", "y", "z", "rgb"), skip_nans=True)
                else:
                    gen = pc2.read_points(msg, field_names=("x", "y", "z"), skip_nans=True)

                v_size = self.voxel_size
                new_pts = []

                with self.lock:
                    for p in gen:
                        x, y, z = float(p[0]), float(p[1]), float(p[2])
                        if has_rgb:
                            rgb_val = p[3]
                            rgb_int = struct.unpack("I", struct.pack("f", float(rgb_val)))[0]
                            r = (rgb_int >> 16) & 0xFF
                            g = (rgb_int >> 8) & 0xFF
                            b = rgb_int & 0xFF
                            has_c = 1
                        else:
                            r, g, b, has_c = 255, 255, 255, 0

                        gx = int(math.floor(x / v_size))
                        gy = int(math.floor(y / v_size))
                        gz = int(math.floor(z / v_size))
                        key = (gx, gy, gz)
                        if key not in self.global_voxels:
                            intensity = 1.0
                            pt_tuple = (x, y, z, intensity, r, g, b, has_c)
                            self.global_voxels[key] = pt_tuple
                            new_pts.append(pt_tuple)

                    if new_pts:
                        self.new_points_buffer.extend(new_pts)
            except Exception as e:
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
