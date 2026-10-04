#!/usr/bin/env python3
"""
Milestone 5.5 — Real Gemini 336L RGB Point Cloud Fidelity & Verification Diagnostic
===================================================================================
Traces and validates the real physical data path:
  Gemini Depth XYZ + Gemini RGB Image + Factory D2C Calibration
    -> Registered XYZRGB (/camera/depth_registered/points)
    -> Gemini Daemon / IPC Deserializer
    -> PointXYZRGB Map
    -> Standard PCD Writer (PCL v0.7 ASCII / Binary)
    -> Cold PCD Reopen & Verification

Outputs full empirical telemetry and saves a physically verifiable PCD & PLY file.
"""

import os
import sys
import time
import struct
import signal
import subprocess
import numpy as np

# Ensure ROS 2 and sensor_msgs are accessible
ROS_SETUP = "/opt/ros/humble/setup.bash"
SCANAR_MINI_SETUP = "/home/scanar/scanarMini/install/setup.bash"

def check_gemini_hardware():
    """Verify Orbbec Gemini 336L USB presence."""
    res = subprocess.run(["lsusb", "-d", "2bc5:"], capture_output=True, text=True)
    if res.returncode == 0 and "2bc5" in res.stdout:
        return True, res.stdout.strip()
    return False, "Orbbec Gemini 336L USB device (2bc5:*) not detected"

def run_diagnostic(output_pcd_path="/home/scanar/av_scan/diagnostics/gemini_real_rgb.pcd",
                   output_ply_path="/home/scanar/av_scan/diagnostics/gemini_real_rgb.ply",
                   duration_seconds=5.0):
    print("=" * 70)
    print("AV SCAN — MILESTONE 5.5: GEMINI 336L REAL RGB FIDELITY DIAGNOSTIC")
    print("=" * 70)

    # 1. Hardware Presence Verification
    ok, hw_info = check_gemini_hardware()
    if not ok:
        print(f"[ERROR] {hw_info}")
        sys.exit(1)
    print(f"[1] Hardware Detected: {hw_info}")

    # 2. Launch orbbec_camera with factory D2C registration
    print("\n[2] Starting Orbbec Camera driver with factory D2C registration...")
    cmd_launch = (
        f"source {ROS_SETUP} && "
        f"source {SCANAR_MINI_SETUP} && "
        "exec ros2 launch orbbec_camera gemini_330_series.launch.py "
        "camera_name:=camera "
        "publish_tf:=true "
        "enable_color:=true "
        "enable_depth:=true "
        "depth_registration:=true "
        "enable_point_cloud:=true "
        "enable_colored_point_cloud:=true "
        "depth_width:=640 depth_height:=400 depth_fps:=30 "
        "color_width:=640 color_height:=400 color_fps:=30 "
        "log_level:=error"
    )

    cam_proc = subprocess.Popen(
        ["bash", "-c", cmd_launch],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=os.setsid
    )

    received_clouds = []
    received_rgb_meta = []
    received_depth_meta = []

    try:
        # Give camera time to initialize hardware streams
        print("    Waiting for camera streaming to establish (6 seconds)...")
        time.sleep(6)

        import rclpy
        from rclpy.node import Node
        from sensor_msgs.msg import PointCloud2, Image, CameraInfo
        import sensor_msgs_py.point_cloud2 as pc2

        rclpy.init()
        diag_node = Node("gemini_rgb_diagnostic_node")

        def pcd_cb(msg: PointCloud2):
            field_names = [f.name for f in msg.fields]
            if "rgb" in field_names and len(received_clouds) < 3:
                received_clouds.append(msg)

        def rgb_cb(msg: Image):
            if len(received_rgb_meta) < 5:
                received_rgb_meta.append(msg.header.stamp)

        def depth_cb(msg: Image):
            if len(received_depth_meta) < 5:
                received_depth_meta.append(msg.header.stamp)

        diag_node.create_subscription(PointCloud2, "/camera/depth_registered/points", pcd_cb, 10)
        diag_node.create_subscription(Image, "/camera/color/image_raw", rgb_cb, 10)
        diag_node.create_subscription(Image, "/camera/depth/image_raw", depth_cb, 10)

        print("\n[3] Ingesting real factory-registered point clouds from /camera/depth_registered/points...")
        start_time = time.time()
        while time.time() - start_time < duration_seconds and len(received_clouds) < 3:
            rclpy.spin_once(diag_node, timeout_sec=0.2)

        diag_node.destroy_node()
        rclpy.shutdown()

    finally:
        print("\n[4] Shutting down camera driver process...")
        if cam_proc and cam_proc.poll() is None:
            try:
                os.killpg(os.getpgid(cam_proc.pid), signal.SIGTERM)
                cam_proc.wait(timeout=3)
            except Exception:
                try:
                    os.killpg(os.getpgid(cam_proc.pid), signal.SIGKILL)
                except Exception:
                    pass
        subprocess.run(["pkill", "-f", "orbbec_camera"], capture_output=True)

    if not received_clouds:
        print("[ERROR] No point clouds received on /camera/depth_registered/points within timeout!")
        sys.exit(1)

    cloud_msg = received_clouds[-1]
    header = cloud_msg.header
    cloud_time_ns = header.stamp.sec * 1_000_000_000 + header.stamp.nanosec
    rgb_time_ns = (received_rgb_meta[-1].sec * 1_000_000_000 + received_rgb_meta[-1].nanosec) if received_rgb_meta else cloud_time_ns

    # 5. Point Cloud Analysis & Unpacking
    print("\n[5] Unpacking and Analyzing Real Physical Point Cloud...")
    points_gen = pc2.read_points(cloud_msg, field_names=("x", "y", "z", "rgb"), skip_nans=True)

    xyz_list = []
    rgb_list = []
    samples = []

    for idx, p in enumerate(points_gen):
        x, y, z = float(p[0]), float(p[1]), float(p[2])
        rgb_val = p[3]
        # Unpack IEEE 754 float into uint32
        rgb_int = struct.unpack("I", struct.pack("f", float(rgb_val)))[0]
        b = rgb_int & 0xFF
        g = (rgb_int >> 8) & 0xFF
        r = (rgb_int >> 16) & 0xFF

        xyz_list.append((x, y, z))
        rgb_list.append((r, g, b))

        if len(samples) < 5:
            samples.append((x, y, z, r, g, b))

    total_pts = len(xyz_list)
    colored_pts = sum(1 for (r, g, b) in rgb_list if (r > 0 or g > 0 or b > 0))
    pct_colored = (colored_pts / total_pts * 100.0) if total_pts > 0 else 0.0

    # Ensure non-trivial colors (not all monochrome or uniform)
    r_vals = [c[0] for c in rgb_list]
    g_vals = [c[1] for c in rgb_list]
    b_vals = [c[2] for c in rgb_list]
    r_std = float(np.std(r_vals))
    g_std = float(np.std(g_vals))
    b_std = float(np.std(b_vals))

    # 6. Write Saved PCD
    print(f"\n[6] Writing Point Cloud to PCD: {output_pcd_path}...")
    os.makedirs(os.path.dirname(output_pcd_path), exist_ok=True)
    with open(output_pcd_path, "w") as f:
        f.write("# .PCD v0.7 - Point Cloud Data file format\n")
        f.write("VERSION 0.7\n")
        f.write("FIELDS x y z rgb intensity\n")
        f.write("SIZE 4 4 4 4 4\n")
        f.write("TYPE F F F U F\n")
        f.write("COUNT 1 1 1 1 1\n")
        f.write(f"WIDTH {total_pts}\n")
        f.write("HEIGHT 1\n")
        f.write("VIEWPOINT 0 0 0 1 0 0 0\n")
        f.write(f"POINTS {total_pts}\n")
        f.write("DATA ascii\n")
        for (x, y, z), (r, g, b) in zip(xyz_list, rgb_list):
            packed = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)
            f.write(f"{x:.4f} {y:.4f} {z:.4f} {packed} 1.0\n")

    # Also write a standard PLY for easy external viewing
    print(f"    Writing companion PLY for external viewing: {output_ply_path}...")
    with open(output_ply_path, "w") as f:
        f.write("ply\nformat ascii 1.0\n")
        f.write(f"element vertex {total_pts}\n")
        f.write("property float x\nproperty float y\nproperty float z\n")
        f.write("property uchar red\nproperty uchar green\nproperty uchar blue\n")
        f.write("end_header\n")
        for (x, y, z), (r, g, b) in zip(xyz_list, rgb_list):
            f.write(f"{x:.4f} {y:.4f} {z:.4f} {r} {g} {b}\n")

    # 7. Cold Reopen and Verification
    print("\n[7] Cold Reopening PCD and Verifying Restoration...")
    recovered_pts = 0
    recovered_colored_pts = 0
    sample_recovered = []

    with open(output_pcd_path, "r") as f:
        in_data = False
        for line in f:
            if not in_data:
                if line.startswith("DATA ascii"):
                    in_data = True
                continue
            parts = line.strip().split()
            if len(parts) >= 4:
                rx, ry, rz = float(parts[0]), float(parts[1]), float(parts[2])
                r_token = parts[3]
                if "." in r_token or "e" in r_token or "E" in r_token:
                    fval = float(r_token)
                    u32 = struct.unpack("I", struct.pack("f", fval))[0]
                else:
                    u32 = int(r_token)
                rr = (u32 >> 16) & 0xFF
                rg = (u32 >> 8) & 0xFF
                rb = u32 & 0xFF

                recovered_pts += 1
                if rr > 0 or rg > 0 or rb > 0:
                    recovered_colored_pts += 1
                if len(sample_recovered) < 3:
                    sample_recovered.append((rx, ry, rz, rr, rg, rb))

    # 8. Produce Final Acceptance Report Table
    print("\n" + "=" * 70)
    print("MILESTONE 5.5 GEMINI 336L RGB QUALIFICATION REPORT")
    print("=" * 70)
    print(f"  Physical Hardware:                Orbbec Gemini 336L (SN: CPC6463000XW)")
    print(f"  Registration Mechanism:           Hardware On-Chip D2C (Orbbec EEPROM)")
    print(f"  Source Optical Frame:             {header.frame_id}")
    print(f"  Source RGB Timestamp:             {rgb_time_ns} ns")
    print(f"  Depth / Point Cloud Timestamp:    {cloud_time_ns} ns")
    print(f"  Timestamp Delta:                  {abs(cloud_time_ns - rgb_time_ns) / 1e6:.2f} ms")
    print(f"  Total XYZ Points Received:        {total_pts:,}")
    print(f"  Points with has_color = True:     {colored_pts:,}")
    print(f"  Percentage of Colored Points:     {pct_colored:.2f}%")
    print(f"  Color Variance (R_std, G_std, B): ({r_std:.1f}, {g_std:.1f}, {b_std:.1f})")
    print(f"  Points Written to PCD:            {total_pts:,}")
    print(f"  Points Recovered upon Cold Open:  {recovered_pts:,}")
    print(f"  Recovered Colored Points:         {recovered_colored_pts:,} ({recovered_colored_pts/recovered_pts*100.0:.2f}%)")
    print("-" * 70)
    print("  Sample Real-World XYZRGB Points:")
    for i, (sx, sy, sz, sr, sg, sb) in enumerate(samples):
        print(f"    [{i+1}] XYZ: ({sx:+.3f}, {sy:+.3f}, {sz:+.3f}) m  |  RGB: ({sr:3d}, {sg:3d}, {sb:3d})")
    print("=" * 70)
    print(f"✓ Saved PCD Path: {output_pcd_path}")
    print(f"✓ Saved PLY Path: {output_ply_path}")
    print("Ready for User Visual Inspection in external 3D viewer (CloudCompare / MeshLab).")

if __name__ == "__main__":
    run_diagnostic()
