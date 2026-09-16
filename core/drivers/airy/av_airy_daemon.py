#!/usr/bin/env python3
"""
AV Scan — RoboSense Airy Hardware & LIO SLAM Isolated Bridge Daemon
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and open-source FAST-LIO2 / ROS 2 driver stack.

Exposes a low-latency localhost TCP control and data streaming server.
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

# Configuration constants
DEFAULT_PORT = 9099
HOST = "127.0.0.1"
LIDAR_HOST_IP = "192.168.1.102"
LIDAR_IP = "192.168.1.200"
DIFOP_PORT = 7788
MSOP_PORT = 6699
IMU_PORT = 6688

RSLIDAR_CONFIG = "/home/scanar/scanarMini/src/rslidar_sdk/config/drone_config.yaml"
FASTLIVO_ROBOSENSE_CONFIG = "/home/scanar/scanarMini/src/FAST-LIVO2/config/robosense.yaml"
FASTLIVO_CAMERA_CONFIG = "/home/scanar/scanarMini/src/FAST-LIVO2/config/camera_robosense.yaml"

def quat_to_yaw(qx, qy, qz, qw):
    siny_cosp = 2.0 * (qw * qz + qx * qy)
    cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz)
    return math.atan2(siny_cosp, cosy_cosp)

class AirySlamDaemon:
    def __init__(self, port=DEFAULT_PORT):
        self.port = port
        self.running = True
        self.is_capturing = False
        self.lock = threading.Lock()

        # Processes
        self.lidar_proc = None
        self.slam_proc = None
        self.ros_thread = None

        # SLAM and Telemetry State
        self.tracking_state = "IDLE" # IDLE, INITIALIZING, TRACKING_OK, TRACKING_LOST
        self.current_pose = {
            "x": 0.0, "y": 0.0, "z": 0.0,
            "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
            "yaw": 0.0, "timestamp_ns": 0
        }
        self.trajectory = [] # [(timestamp_ns, x, y, z, qx, qy, qz, qw, yaw)]
        
        # Voxel Map: dict of (gx, gy, gz) -> (x, y, z, intensity)
        self.voxel_size = 0.05 # 5 cm resolution
        self.global_voxels = {}
        self.new_points_buffer = [] # points added since last client fetch

        # Hardware Info cache
        self.hardware_info = {
            "manufacturer": "RoboSense",
            "model": "RS-Airy",
            "lidar_ip": LIDAR_IP,
            "host_ip": LIDAR_HOST_IP,
            "rpm": 600,
            "return_mode": "SINGLE RETURN (Strongest)",
            "difop_received": False,
            "serial": "AIRY-2024-99812",
            "firmware": "3.1.20",
            "lidar_to_imu": {
                "translation": [0.004250, 0.004180, -0.004460],
                "quaternion": [0.71145376, -0.70271848, 0.0018789, 0.00409338]
            }
        }

    def query_hardware_difop(self, timeout_sec=1.5):
        """Interrogates live DIFOP packet directly from physical sensor."""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.bind((LIDAR_HOST_IP, DIFOP_PORT))
            sock.settimeout(timeout_sec)
            data, addr = sock.recvfrom(2048)
            sock.close()

            if len(data) >= 1248:
                rpm = struct.unpack(">H", data[8:10])[0]
                self.hardware_info["rpm"] = rpm
                self.hardware_info["difop_received"] = True
                return True
        except Exception as e:
            pass
        return False

    def start_acquisition(self):
        with self.lock:
            if self.is_capturing:
                return True, "Already capturing"

            print("[AiryDaemon] Starting RoboSense Airy driver and FAST-LIVO SLAM...")
            self.global_voxels.clear()
            self.trajectory.clear()
            self.new_points_buffer.clear()
            self.current_pose = {
                "x": 0.0, "y": 0.0, "z": 0.0,
                "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
                "yaw": 0.0, "timestamp_ns": int(time.time() * 1e9)
            }
            self.tracking_state = "INITIALIZING"

            # 1. Launch rslidar_sdk_node
            cmd_lidar = [
                "bash", "-c",
                f"source /opt/ros/humble/setup.bash && "
                f"source /home/scanar/scanarMini/install/setup.bash && "
                f"exec ros2 run rslidar_sdk rslidar_sdk_node --ros-args -p config_path:={RSLIDAR_CONFIG}"
            ]
            self.lidar_proc = subprocess.Popen(
                cmd_lidar,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                preexec_fn=os.setsid
            )

            # 2. Launch fastlivo_mapping
            cmd_slam = [
                "bash", "-c",
                f"source /opt/ros/humble/setup.bash && "
                f"source /home/scanar/scanarMini/install/setup.bash && "
                f"exec ros2 run fast_livo fastlivo_mapping --ros-args "
                f"--params-file {FASTLIVO_ROBOSENSE_CONFIG} "
                f"--params-file {FASTLIVO_CAMERA_CONFIG}"
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

            print(f"[AiryDaemon] Acquisition started. Driver PID: {self.lidar_proc.pid}, SLAM PID: {self.slam_proc.pid}")
            return True, "Acquisition and LIO mapping started"

    def stop_acquisition(self):
        with self.lock:
            if not self.is_capturing:
                return True, "Already stopped"

            print("[AiryDaemon] Stopping RoboSense Airy driver and SLAM...")
            self.is_capturing = False
            self.tracking_state = "STOPPED"

            # Terminate child processes safely
            for proc, name in [(self.slam_proc, "fast_livo"), (self.lidar_proc, "rslidar_sdk")]:
                if proc and proc.poll() is None:
                    try:
                        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
                        time.sleep(0.3)
                        if proc.poll() is None:
                            os.killpg(os.getpgid(proc.pid), signal.SIGTERM)
                            proc.wait(timeout=1.0)
                    except Exception:
                        try:
                            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
                        except Exception:
                            pass
            self.slam_proc = None
            self.lidar_proc = None

            print(f"[AiryDaemon] Acquisition stopped. Final map points: {len(self.global_voxels)}, Trajectory: {len(self.trajectory)}")
            return True, "Acquisition stopped"

    def _ros_subscriber_loop(self):
        """ROS 2 Node loop running in background thread."""
        try:
            import rclpy
            from rclpy.node import Node
            from rclpy.qos import qos_profile_sensor_data
            from nav_msgs.msg import Odometry, Path
            from sensor_msgs.msg import PointCloud2
        except ImportError as e:
            print(f"[AiryDaemon] Error importing ROS 2 modules: {e}")
            return

        if not rclpy.ok():
            rclpy.init()

        node = Node("av_scan_airy_bridge_subscriber")
        print("[AiryDaemon] ROS 2 Subscriber Node created.")

        def odom_callback(msg: Odometry):
            p = msg.pose.pose.position
            q = msg.pose.pose.orientation
            sec = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
            ts_ns = int(sec * 1e9)

            if math.isnan(p.x) or math.isnan(p.y) or math.isnan(p.z):
                return

            yaw = quat_to_yaw(q.x, q.y, q.z, q.w)

            with self.lock:
                self.current_pose = {
                    "x": float(p.x), "y": float(p.y), "z": float(p.z),
                    "qx": float(q.x), "qy": float(q.y), "qz": float(q.z), "qw": float(q.w),
                    "yaw": float(yaw), "timestamp_ns": ts_ns
                }
                self.tracking_state = "TRACKING_OK"

                # Append to trajectory if moved >= 2cm or rotated >= 1 deg, or first pose
                if len(self.trajectory) == 0:
                    self.trajectory.append((ts_ns, p.x, p.y, p.z, q.x, q.y, q.z, q.w, yaw))
                else:
                    last = self.trajectory[-1]
                    dist_sq = (p.x - last[1])**2 + (p.y - last[2])**2 + (p.z - last[3])**2
                    angle_diff = abs(yaw - last[8])
                    if dist_sq >= 0.0004 or angle_diff >= 0.017:
                        self.trajectory.append((ts_ns, p.x, p.y, p.z, q.x, q.y, q.z, q.w, yaw))

        def cloud_callback(msg: PointCloud2):
            num_points = msg.width * msg.height
            if num_points == 0 or msg.point_step < 12:
                return

            try:
                raw = bytes(msg.data)
                dt_fields = [('x', '<f4'), ('y', '<f4'), ('z', '<f4')]
                dt_offsets = [0, 4, 8]
                if msg.point_step >= 36:
                    dt_fields.append(('intensity', '<f4'))
                    dt_offsets.append(32)

                dt = np.dtype({
                    'names': [f[0] for f in dt_fields],
                    'formats': [f[1] for f in dt_fields],
                    'offsets': dt_offsets,
                    'itemsize': msg.point_step
                })
                pts = np.frombuffer(raw, dtype=dt)
                valid = np.isfinite(pts['x']) & np.isfinite(pts['y']) & np.isfinite(pts['z'])
                pts_valid = pts[valid]
                if len(pts_valid) == 0:
                    return

                xs = pts_valid['x']
                ys = pts_valid['y']
                zs = pts_valid['z']
                intensities = pts_valid['intensity'] if 'intensity' in dt.names else np.zeros_like(xs)

                # Subsample / Voxelize into global map (5cm voxels)
                v_size = self.voxel_size
                gxs = np.floor(xs / v_size).astype(np.int32)
                gys = np.floor(ys / v_size).astype(np.int32)
                gzs = np.floor(zs / v_size).astype(np.int32)

                new_batch = []
                with self.lock:
                    for i in range(len(pts_valid)):
                        key = (int(gxs[i]), int(gys[i]), int(gzs[i]))
                        if key not in self.global_voxels:
                            pt = (float(xs[i]), float(ys[i]), float(zs[i]), float(intensities[i]))
                            self.global_voxels[key] = pt
                            new_batch.append(pt)
                    self.new_points_buffer.extend(new_batch)

            except Exception as e:
                pass

        node.create_subscription(Odometry, "/aft_mapped_to_init", odom_callback, qos_profile_sensor_data)
        node.create_subscription(PointCloud2, "/cloud_registered", cloud_callback, qos_profile_sensor_data)

        while self.is_capturing and rclpy.ok():
            try:
                rclpy.spin_once(node, timeout_sec=0.05)
            except Exception:
                break

        node.destroy_node()

    def handle_client(self, conn):
        """Processes binary/text RPC protocol commands from AV Scan core."""
        buffer = ""
        while self.running:
            try:
                data = conn.recv(4096)
                if not data:
                    break
                buffer += data.decode("utf-8", errors="ignore")
                
                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)
                    line = line.strip()
                    if not line:
                        continue
                    
                    parts = line.split(" ")
                    cmd = parts[0].upper()

                    if cmd == "PING":
                        conn.sendall(b"PONG\n")

                    elif cmd == "IDENTIFY":
                        self.query_hardware_difop(timeout_sec=0.5)
                        import json
                        resp = json.dumps(self.hardware_info) + "\n"
                        conn.sendall(resp.encode("utf-8"))

                    elif cmd == "START":
                        ok, msg = self.start_acquisition()
                        resp = f"START_OK:{msg}\n" if ok else f"START_ERR:{msg}\n"
                        conn.sendall(resp.encode("utf-8"))

                    elif cmd == "STOP":
                        ok, msg = self.stop_acquisition()
                        resp = f"STOP_OK:{msg}\n" if ok else f"STOP_ERR:{msg}\n"
                        conn.sendall(resp.encode("utf-8"))

                    elif cmd == "RESET":
                        with self.lock:
                            self.global_voxels.clear()
                            self.trajectory.clear()
                            self.new_points_buffer.clear()
                        conn.sendall(b"RESET_OK\n")

                    elif cmd == "GET_TELEMETRY":
                        with self.lock:
                            telem = {
                                "status": "OK",
                                "tracking_state": self.tracking_state,
                                "is_capturing": self.is_capturing,
                                "current_pose": dict(self.current_pose),
                                "point_count": len(self.global_voxels),
                                "trajectory_count": len(self.trajectory)
                            }
                        import json
                        resp = json.dumps(telem) + "\n"
                        conn.sendall(resp.encode("utf-8"))

                    elif cmd == "GET_NEW_POINTS":
                        with self.lock:
                            pts = list(self.new_points_buffer)
                            self.new_points_buffer.clear()
                        count = len(pts)
                        header = struct.pack("<4sI", b"NPTS", count)
                        if count > 0:
                            arr = np.array(pts, dtype=np.float32).flatten()
                            payload = header + arr.tobytes()
                        else:
                            payload = header
                        conn.sendall(payload)

                    elif cmd == "GET_ALL_POINTS":
                        with self.lock:
                            pts = list(self.global_voxels.values())
                        count = len(pts)
                        header = struct.pack("<4sI", b"APTS", count)
                        if count > 0:
                            arr = np.array(pts, dtype=np.float32).flatten()
                            payload = header + arr.tobytes()
                        else:
                            payload = header
                        conn.sendall(payload)

                    elif cmd == "GET_TRAJECTORY":
                        with self.lock:
                            traj = list(self.trajectory)
                        import json
                        resp = json.dumps(traj) + "\n"
                        conn.sendall(resp.encode("utf-8"))

                    elif cmd == "SHUTDOWN":
                        self.stop_acquisition()
                        conn.sendall(b"SHUTDOWN_OK\n")
                        self.running = False
                        break

            except Exception as e:
                break
        conn.close()

    def run(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, self.port))
        server.listen(5)
        print(f"[AiryDaemon] Listening for AV Scan IPC commands on {HOST}:{self.port}...")

        self.query_hardware_difop(timeout_sec=1.0)

        while self.running:
            try:
                server.settimeout(1.0)
                conn, addr = server.accept()
                t = threading.Thread(target=self.handle_client, args=(conn,), daemon=True)
                t.start()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[AiryDaemon] Accept error: {e}")
                break

        server.close()
        self.stop_acquisition()
        print("[AiryDaemon] Server exited.")

if __name__ == '__main__':
    daemon = AirySlamDaemon()
    def sig_handler(sig, frame):
        print("[AiryDaemon] Interrupted, shutting down...")
        daemon.running = False
        daemon.stop_acquisition()
        sys.exit(0)

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)
    daemon.run()
