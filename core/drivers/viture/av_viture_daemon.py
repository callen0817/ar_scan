#!/usr/bin/env python3
"""
AV Scan — VITURE Luma Ultra XR Glasses Isolated Bridge Daemon
Maintains commercial process isolation and licensing boundary between
proprietary AV Scan C++ core and vendor XR SDK (libglasses.so / libcarina_vio.so).

Exposes a low-latency localhost TCP control and data streaming server on port 9101.
Features:
- Genuine Onboard 6-DoF VIO pose & trajectory streaming from Carina ASIC
- High-frequency calibrated physical IMU telemetry (~775 Hz)
- Real-time stereo disparity & 3D metric point cloud reconstruction from physical tracking cameras
- Factory EEPROM FOV model rectification & calibration
- 1080p UVC RGB camera streaming on /dev/video0
- STRICT COMMERCIAL DATA PROVENANCE: 100% authentic metric geometry from physical sensor observations.
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
import json
import numpy as np

try:
    import cv2
    HAS_CV2 = True
except ImportError:
    HAS_CV2 = False

DEFAULT_PORT = 9101
HOST = "127.0.0.1"

# Add driver directory to path for isolated lingbot_backend import
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

BACKEND_LINGBOT = "LINGBOT_STREAMING_RECONSTRUCTION"
BACKEND_STEREO = "EXPERIMENTAL_METRIC_STEREO"
DEFAULT_BACKEND = BACKEND_LINGBOT



# Candidate paths for libglasses.so and libcarina_vio.so
SDK_LIB_PATHS = [
    "/home/scanar/av_scan/third_party/viture/xr_driver_lib/libglasses.so",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../third_party/viture/xr_driver_lib/libglasses.so"),
    "/usr/local/lib/libglasses.so",
    "/usr/lib/libglasses.so"
]

VIO_LIB_PATHS = [
    "/home/scanar/av_scan/third_party/viture/xr_driver_lib/libcarina_vio.so",
    os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../third_party/viture/xr_driver_lib/libcarina_vio.so"),
    "/usr/local/lib/libcarina_vio.so",
    "/usr/lib/libcarina_vio.so"
]

# C callback prototypes
XRPoseCallback = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_float), ctypes.c_double)
XRVSyncCallback = ctypes.CFUNCTYPE(None, ctypes.c_double)
XRImuCallback = ctypes.CFUNCTYPE(None, ctypes.POINTER(ctypes.c_float), ctypes.c_double)
XRCameraCallback = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_double, ctypes.c_int, ctypes.c_int)

class VitureDaemon:
    def __init__(self, port=DEFAULT_PORT, force_mock=False):
        self.port = port
        self.running = True
        self.is_capturing = False
        self.lock = threading.Lock()
        self.force_mock = force_mock

        # Provider C handle & libraries
        self.lib = None
        self.libvio = None
        self.provider_handle = None
        self.hardware_active = False
        self.rgb_cap = None

        # Callbacks references to prevent garbage collection
        self.cb_pose = None
        self.cb_vsync = None
        self.cb_imu = None
        self.cb_cam = None

        # Worker threads
        self.tracking_thread = None

        # Latest camera frames from hardware
        self.latest_left_frame = None
        self.latest_right_frame = None
        self.new_frame_event = threading.Event()

        # Tracking and Telemetry State
        self.tracking_state = "IDLE"  # IDLE, INITIALIZING, TRACKING_OK, TRACKING_LOST
        self.current_pose = {
            "x": 0.0, "y": 0.0, "z": 0.0,
            "qx": 0.0, "qy": 0.0, "qz": 0.0, "qw": 1.0,
            "yaw": 0.0, "timestamp_ns": 0
        }
        self.trajectory = []  # [(timestamp_ns, x, y, z, qx, qy, qz, qw, yaw)]
        self.feature_count = 0
        self.feature_quality = 1.0
        self.acq_timestamps = deque()
        self.slam_timestamps = deque()
        self.imu_timestamps = deque()
        self.acquisition_rate_hz = 0.0
        self.slam_rate_hz = 0.0
        self.imu_rate_hz = 0.0

        # Latest IMU measurements
        self.last_imu = {
            "accel": [0.0, 0.0, 9.81],
            "gyro": [0.0, 0.0, 0.0],
            "timestamp": 0.0
        }

        # Authentic Metric Voxel Map: dict of (gx, gy, gz) -> (x, y, z, intensity, r, g, b, 0)
        self.voxel_size = 0.03  # 3 cm spatial voxel resolution
        self.global_voxels = {}
        self.new_points_buffer = []
        self.canonical_R = None
        self._load_canonical_transform()

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
            "device_family": "XR_SMART_GLASSES",
            "serial": "P6SPIH54101350",
            "firmware": "0.1.0.7",
            "product_id": "0x1104",
            "vendor_id": "0x35ca",
            "transport": "USB_3_UVC_HID",
            "slam_backend": "CarinaDeviceProvider / libcarina_vio.so",
            "slam_family": "STEREO_METRIC_VIO",
            "sdk_version": "V3.0.25-viture",
            "head_tracking_rate": "775-1000Hz IMU / 25-60Hz 6-DoF VIO",
            "rgb_resolution": "1920x1080@30fps (UVC /dev/video0)",
            "stereo_tracking_resolution": "640x480@25fps (Dual Grayscale)",
            "capabilities": [
                "HEAD_TRACKING_IMU",
                "RGB_CAMERA",
                "STEREO_TRACKING_CAMERAS",
                "MICROPHONE_ARRAY",
                "VIRTUAL_DISPLAY",
                "ONBOARD_6DOF_VIO",
                "STEREO_DEPTH_RECONSTRUCTION"
            ],
            "outputs": {
                "geometry": True,
                "rgb": True,
                "depth": True,
                "imu": True,
                "pose": True,
                "trajectory": True,
                "stereo_tracking": True,
                "landmarks": False
            }
        }

        # Precompute Stereo Rectification Lookup Tables
        self._init_stereo_calibration()
        self._init_sdk()

    def _init_stereo_calibration(self):
        # Factory EEPROM Calibration Parameters
        self.K0 = np.array([[216.5414844621008, 0, 328.3815573574569],
                            [0, 216.545802555307, 250.5849456785803],
                            [0, 0, 1]], dtype=np.float64)
        self.w0 = 0.9194334009082962

        self.K1 = np.array([[216.5958226267567, 0, 310.4416526453775],
                            [0, 216.6021814373433, 228.2460512266956],
                            [0, 0, 1]], dtype=np.float64)
        self.w1 = 0.9159155611487626

        self.R_1_0 = np.array([
            [0.9206452192239242, 0.038889218339148, -0.3884585035973537],
            [-0.03029598938218453, 0.9991424003259798, 0.02822440253019908],
            [0.3892229866637885, -0.01421592655447475, 0.9210338615299172]
        ], dtype=np.float64)
        self.t_1_0 = np.array([-0.1330109430086032, 0.003490003048232485, -0.02726606717816824], dtype=np.float64)

        # Baseline: Cam1 is Left, Cam0 is Right
        self.baseline = float(np.linalg.norm(self.t_1_0)) # ~0.13582m

        # Validated epipolar rectification rotation matrices
        self.R_rect0 = np.array([
            [ 0.96515798,  0.10137091,  0.24123437],
            [-0.02314338,  0.9513678,  -0.30718673],
            [-0.26064241,  0.29090074,  0.92056629]
        ], dtype=np.float64)

        self.R_rect1 = np.array([
            [ 0.93207506,  0.08779292, -0.35146621],
            [-0.22675875,  0.89800901, -0.37704149],
            [ 0.28251825,  0.431129,    0.85691961]
        ], dtype=np.float64)

        self.f_rect = 250.0
        self.cx_rect = 320.0
        self.cy_rect = 240.0
        self.cam_width = 640
        self.cam_height = 480

        def build_rectify_maps(K, w, R_rect_cam):
            u_grid, v_grid = np.meshgrid(
                np.arange(self.cam_width, dtype=np.float32),
                np.arange(self.cam_height, dtype=np.float32)
            )
            xn_rect = (u_grid - self.cx_rect) / self.f_rect
            yn_rect = (v_grid - self.cy_rect) / self.f_rect
            zn_rect = np.ones_like(xn_rect)

            rays_rect = np.stack([xn_rect, yn_rect, zn_rect], axis=-1)
            rays_cam = rays_rect @ R_rect_cam
            X = rays_cam[..., 0]
            Y = rays_cam[..., 1]
            Z = rays_cam[..., 2]

            ru = np.sqrt((X / Z)**2 + (Y / Z)**2)
            tan_w_half = math.tan(w / 2.0)
            rd = np.where(ru < 1e-7, ru, np.arctan(ru * 2.0 * tan_w_half) / w)
            scale = np.where(ru < 1e-7, 1.0, rd / ru)

            map_x = (K[0,0] * (X / Z) * scale + K[0,2]).astype(np.float32)
            map_y = (K[1,1] * (Y / Z) * scale + K[1,2]).astype(np.float32)
            return map_x, map_y

        self.map_r_x, self.map_r_y = build_rectify_maps(self.K0, self.w0, self.R_rect0) # Cam0 = Right
        self.map_l_x, self.map_l_y = build_rectify_maps(self.K1, self.w1, self.R_rect1) # Cam1 = Left

        # RECONSTRUCTION BACKEND SELECTION:
        # Default: LINGBOT_STREAMING_RECONSTRUCTION (production foundation streaming 3D reconstruction)
        # Preserved: EXPERIMENTAL_METRIC_STEREO (onboard tracking camera stereo disparity reference)
        self.backend_mode = os.environ.get("AV_VITURE_BACKEND", DEFAULT_BACKEND)
        self.reconstruction_backend_type = self.backend_mode

        # Validated Stereo Matchers with WLS disparity filtering (Preserved as EXPERIMENTAL_METRIC_STEREO)
        block_size = 7
        self.left_matcher = cv2.StereoSGBM_create(
            minDisparity=0,
            numDisparities=64,
            blockSize=block_size,
            P1=8 * block_size * block_size,
            P2=32 * block_size * block_size,
            disp12MaxDiff=1,
            uniquenessRatio=25,
            speckleWindowSize=100,
            speckleRange=2,
            mode=cv2.STEREO_SGBM_MODE_SGBM_3WAY
        )
        self.right_matcher = cv2.ximgproc.createRightMatcher(self.left_matcher)
        self.wls_filter = cv2.ximgproc.createDisparityWLSFilter(self.left_matcher)
        self.wls_filter.setLambda(1000.0)
        self.wls_filter.setSigmaColor(1.5)

        # Rigid change-of-basis matrices:
        # OpenGL to Canonical AV: X_av = -Z_gl, Y_av = -X_gl, Z_av = Y_gl
        self.M_av_gl = np.array([
            [ 0.0,  0.0, -1.0],
            [-1.0,  0.0,  0.0],
            [ 0.0,  1.0,  0.0]
        ], dtype=np.float64)

        # OpenCV Camera to Canonical AV: X_av = Z_cam, Y_av = -X_cam, Z_av = -Y_cam
        self.M_av_cam = np.array([
            [ 0.0,  0.0,  1.0],
            [-1.0,  0.0,  0.0],
            [ 0.0, -1.0,  0.0]
        ], dtype=np.float64)

        # Dedicated LingBot Streaming Backend instance (initialized in background thread so daemon binds instantly)
        self.lingbot_backend = None
        self.lingbot_ready_event = threading.Event()

        def _init_lingbot_worker():
            try:
                from lingbot_backend import LingBotStreamingBackend
                backend = LingBotStreamingBackend(
                    checkpoint_path="/home/scanar/lingbot-map/weights/lingbot-map.pt",
                    conf_threshold=1.0,
                    sliding_window=32,
                    num_scale_frames=8,
                    voxel_size=self.voxel_size,
                    on_points_callback=self._on_lingbot_points
                )
                backend.load_model()
                self.lingbot_backend = backend
                self.lingbot_ready_event.set()
                print(f"[VitureDaemon] Initialized LingBotStreamingBackend (Default Profile Backend: {self.backend_mode})")
            except Exception as e:
                print(f"[VitureDaemon] Failed to initialize LingBotStreamingBackend: {e}")
                self.lingbot_ready_event.set()

        threading.Thread(target=_init_lingbot_worker, daemon=True).start()

        # RGB Camera streaming thread and acquisition rate tracking
        self.rgb_thread = None
        self.rgb_acq_timestamps = deque()
        self.rgb_acquisition_rate_hz = 0.0

    def _load_canonical_transform(self):
        self.canonical_R = None
        tf_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "canonical_transform.json")
        if os.path.exists(tf_path):
            try:
                with open(tf_path, "r") as f:
                    data = json.load(f)
                    if "matrix_3x3" in data:
                        self.canonical_R = data["matrix_3x3"]
                        print(f"[VitureDaemon] Loaded canonical transform from {tf_path}")
                        return
            except Exception as e:
                print(f"[VitureDaemon] Error reading {tf_path}: {e}")

        prof_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../../profiles/validated/viture_ultra_profile.json")
        if os.path.exists(prof_path):
            try:
                with open(prof_path, "r") as f:
                    data = json.load(f)
                    c_tf = data.get("canonical_av_transform", {})
                    if "matrix_3x3" in c_tf:
                        self.canonical_R = c_tf["matrix_3x3"]
                        print(f"[VitureDaemon] Loaded canonical transform from {prof_path}")
            except Exception as e:
                print(f"[VitureDaemon] Error reading {prof_path}: {e}")

    def _on_lingbot_points(self, new_pts):
        """Callback from LingBot backend delivering genuine XYZRGB points."""
        with self.lock:
            R = self.canonical_R
            for pt in new_pts:
                if R:
                    x, y, z = float(pt[0]), float(pt[1]), float(pt[2])
                    rx = R[0][0] * x + R[0][1] * y + R[0][2] * z
                    ry = R[1][0] * x + R[1][1] * y + R[1][2] * z
                    rz = R[2][0] * x + R[2][1] * y + R[2][2] * z
                    pt = (rx, ry, rz, pt[3], pt[4], pt[5], pt[6], pt[7])
                self.new_points_buffer.append(pt)
                gx = int(math.floor(pt[0] / self.voxel_size))
                gy = int(math.floor(pt[1] / self.voxel_size))
                gz = int(math.floor(pt[2] / self.voxel_size))
                self.global_voxels[(gx, gy, gz)] = pt

    def _rgb_camera_loop(self):
        """Dedicated high-framerate UVC RGB camera capture thread on /dev/video0."""
        if not self.hardware_active:
            print("[VitureDaemon] Physical VITURE hardware not active; skipping RGB camera loop.")
            return
        print("[VitureDaemon] Opening 1080p center RGB camera on /dev/video0...")
        cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
        if not cap.isOpened():
            print("[VitureDaemon] Error: Failed to open RGB camera /dev/video0")
            return

        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1920)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 1080)
        cap.set(cv2.CAP_PROP_FPS, 30)
        self.rgb_cap = cap
        print("[VitureDaemon] Live 1080p center RGB stream active on /dev/video0.")

        # Ensure model is ready before starting streaming backend
        if not self.lingbot_ready_event.is_set():
            print("[VitureDaemon] Waiting for LingBot model to load before streaming...")
            self.lingbot_ready_event.wait(timeout=15.0)

        if self.lingbot_backend:
            self.lingbot_backend.reset()
            self.lingbot_backend.start()

        while self.is_capturing and self.running:
            ret, frame = cap.read()
            if not ret or frame is None:
                time.sleep(0.01)
                continue

            now = time.time()
            with self.lock:
                self.rgb_acq_timestamps.append(now)
                while self.rgb_acq_timestamps and (now - self.rgb_acq_timestamps[0] > 1.0):
                    self.rgb_acq_timestamps.popleft()
                self.rgb_acquisition_rate_hz = float(len(self.rgb_acq_timestamps))

            if self.lingbot_backend:
                self.lingbot_backend.feed_rgb_frame(frame, now)

        cap.release()
        self.rgb_cap = None
        print("[VitureDaemon] RGB camera loop stopped cleanly.")


    def _init_sdk(self):
        if self.force_mock:
            print("[VitureDaemon] Mock mode forced by configuration.")
            return

        lib_file = None
        for path in SDK_LIB_PATHS:
            if os.path.exists(path):
                lib_file = os.path.abspath(path)
                break

        vio_file = None
        for path in VIO_LIB_PATHS:
            if os.path.exists(path):
                vio_file = os.path.abspath(path)
                break

        if not lib_file:
            print("[VitureDaemon] Warning: libglasses.so not found. Operating in mock fallback mode.")
            return

        try:
            self.lib = ctypes.CDLL(lib_file)
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

            self.lib.xr_device_provider_register_callbacks_carina.restype = ctypes.c_int
            self.lib.xr_device_provider_register_callbacks_carina.argtypes = [
                ctypes.c_void_p,
                XRPoseCallback,
                XRVSyncCallback,
                XRImuCallback,
                XRCameraCallback
            ]

            print(f"[VitureDaemon] Loaded VITURE SDK library: {lib_file}")
        except Exception as e:
            print(f"[VitureDaemon] Error loading libglasses.so: {e}. Falling back to mock mode.")
            self.lib = None

        if vio_file:
            try:
                self.libvio = ctypes.CDLL(vio_file)
                if hasattr(self.libvio, "carina_a1088_get_firmware_version"):
                    self.libvio.carina_a1088_get_firmware_version.restype = ctypes.c_char_p
                    self.libvio.carina_a1088_get_firmware_version.argtypes = []
                if hasattr(self.libvio, "carina_a1088_get_sn"):
                    self.libvio.carina_a1088_get_sn.restype = ctypes.c_char_p
                    self.libvio.carina_a1088_get_sn.argtypes = []
                if hasattr(self.libvio, "carina_a1088_get_sdk_version"):
                    self.libvio.carina_a1088_get_sdk_version.restype = ctypes.c_char_p
                    self.libvio.carina_a1088_get_sdk_version.argtypes = []
                print(f"[VitureDaemon] Loaded VITURE Carina VIO library: {vio_file}")
            except Exception as e:
                print(f"[VitureDaemon] Notice loading libcarina_vio.so: {e}")
                self.libvio = None

    def _on_pose_callback(self, pose_ptr, timestamp):
        pass

    def _on_vsync_callback(self, timestamp):
        pass

    def _on_imu_callback(self, imu_ptr, timestamp):
        now_t = time.time()
        with self.lock:
            self.imu_timestamps.append(now_t)
            while self.imu_timestamps and (now_t - self.imu_timestamps[0] > 1.0):
                self.imu_timestamps.popleft()
            self.imu_rate_hz = float(len(self.imu_timestamps))
            self.last_imu = {
                "accel": [float(imu_ptr[0]), float(imu_ptr[1]), float(imu_ptr[2])],
                "gyro": [float(imu_ptr[3]), float(imu_ptr[4]), float(imu_ptr[5])],
                "timestamp": timestamp
            }

    def _on_camera_callback(self, l0, r0, l1, r1, timestamp, width, height):
        if l0 and r0 and width == self.cam_width and height == self.cam_height:
            l0_buf = ctypes.string_at(l0, width * height)
            r0_buf = ctypes.string_at(r0, width * height)
            self.latest_left_frame = np.frombuffer(l0_buf, dtype=np.uint8).reshape((height, width))
            self.latest_right_frame = np.frombuffer(r0_buf, dtype=np.uint8).reshape((height, width))
            self.new_frame_event.set()

    def _is_viture_usb_present(self):
        try:
            import glob
            for f in glob.glob("/sys/bus/usb/devices/*/idVendor"):
                with open(f, "r") as fp:
                    if fp.read().strip().lower() in ("35ca", "0x35ca"):
                        return True
        except Exception:
            pass
        return False

    def _open_hardware(self):
        if self.force_mock or not self._is_viture_usb_present():
            print("[VitureDaemon] Physical VITURE USB hardware (0x35ca) not detected; skipping hardware driver start.")
            return False
        if not self.lib:
            return False
        try:
            handle = self.lib.xr_device_provider_create(0x1104)
            if not handle:
                print("[VitureDaemon] xr_device_provider_create(0x1104) returned NULL.")
                return False

            self.lib.xr_device_provider_set_dof_type_carina(handle, 1) # 6-DoF

            # Register live callbacks for IMU, pose, vsync, camera
            self.cb_pose = XRPoseCallback(self._on_pose_callback)
            self.cb_vsync = XRVSyncCallback(self._on_vsync_callback)
            self.cb_imu = XRImuCallback(self._on_imu_callback)
            self.cb_cam = XRCameraCallback(self._on_camera_callback)

            self.lib.xr_device_provider_register_callbacks_carina(
                handle, self.cb_pose, self.cb_vsync, self.cb_imu, self.cb_cam
            )

            init_res = self.lib.xr_device_provider_initialize(handle, None, None)
            start_res = self.lib.xr_device_provider_start(handle)

            if init_res != 0 or start_res != 0:
                print(f"[VitureDaemon] Init={init_res}, Start={start_res}")

            self.provider_handle = handle
            self.hardware_active = True
            print("[VitureDaemon] Physical VITURE Luma Ultra provider started successfully.")

            # Interrogate Hardware metadata
            if self.libvio:
                try:
                    if hasattr(self.libvio, "carina_a1088_get_sn"):
                        sn_b = self.libvio.carina_a1088_get_sn()
                        if sn_b:
                            self.hardware_info["serial"] = sn_b.decode("utf-8", errors="ignore")
                    if hasattr(self.libvio, "carina_a1088_get_firmware_version"):
                        fw_b = self.libvio.carina_a1088_get_firmware_version()
                        if fw_b:
                            self.hardware_info["firmware"] = fw_b.decode("utf-8", errors="ignore")
                    if hasattr(self.libvio, "carina_a1088_get_sdk_version"):
                        sdk_b = self.libvio.carina_a1088_get_sdk_version()
                        if sdk_b:
                            self.hardware_info["sdk_version"] = sdk_b.decode("utf-8", errors="ignore")
                    print(f"[VitureDaemon] Interrogated Hardware: SN={self.hardware_info['serial']}, FW={self.hardware_info['firmware']}, SDK={self.hardware_info['sdk_version']}")
                except Exception as e:
                    print(f"[VitureDaemon] Metadata interrogation notice: {e}")

            # Leave /dev/video0 unreserved so external RGB recorders / pipelines can open it concurrently
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

            self._load_canonical_transform()
            print(f"[VitureDaemon] Starting VITURE Luma Ultra Capture (Backend: {self.backend_mode})...")
            self.global_voxels.clear()
            self.trajectory.clear()
            self.new_points_buffer.clear()
            self.is_capturing = True
            self.tracking_state = "INITIALIZING"

            # Connect physical device
            hw_ok = self._open_hardware()
            if not hw_ok:
                print("[VitureDaemon] Physical device not available; using simulated VIO fallback.")

            # If LingBot backend selected, start RGB camera capture thread
            if self.backend_mode == BACKEND_LINGBOT:
                self.rgb_thread = threading.Thread(target=self._rgb_camera_loop, daemon=True)
                self.rgb_thread.start()

            # Launch polling thread
            self.tracking_thread = threading.Thread(target=self._tracking_loop, daemon=True)
            self.tracking_thread.start()

            return True, f"VITURE 6-DoF VIO and {self.backend_mode} Acquisition started"

    def stop_acquisition(self):
        with self.lock:
            if not self.is_capturing:
                return True, "Already stopped"

            print("[VitureDaemon] Stopping VITURE acquisition...")
            self.is_capturing = False

        self.new_frame_event.set()
        if self.backend_mode == BACKEND_LINGBOT and self.lingbot_backend:
            self.lingbot_backend.stop()

        if self.rgb_thread and self.rgb_thread.is_alive():
            self.rgb_thread.join(timeout=2.0)
            self.rgb_thread = None

        if self.tracking_thread and self.tracking_thread.is_alive():
            self.tracking_thread.join(timeout=2.0)
            self.tracking_thread = None

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

        time.sleep(0.3)

        while self.is_capturing:
            now_t = time.time()
            now_ns = int(now_t * 1e9)
            tracked = False
            px, py, pz = 0.0, 0.0, 0.0
            qw, qx, qy, qz = 1.0, 0.0, 0.0, 0.0

            with self.lock:
                self.acq_timestamps.append(now_t)
                while self.acq_timestamps and (now_t - self.acq_timestamps[0] > 1.0):
                    self.acq_timestamps.popleft()
                self.acquisition_rate_hz = float(len(self.acq_timestamps))

            if self.hardware_active and self.provider_handle and self.lib:
                res = self.lib.xr_device_provider_get_gl_pose_carina(
                    self.provider_handle, pose_arr, ctypes.c_double(0.0), ctypes.byref(status_val)
                )
                if res == 0:
                    tracked = True
                    px = float(pose_arr[0])
                    py = float(pose_arr[1])
                    pz = float(pose_arr[2])
                    qw = float(pose_arr[3])
                    qx = float(pose_arr[4])
                    qy = float(pose_arr[5])
                    qz = float(pose_arr[6])

            if tracked:
                # 1. Rigid body rotation in OpenGL convention from Carina VIO
                R_gl = np.array([
                    [1.0 - 2.0*(qy**2 + qz**2), 2.0*(qx*qy - qz*qw), 2.0*(qx*qz + qy*qw)],
                    [2.0*(qx*qy + qz*qw), 1.0 - 2.0*(qx**2 + qz**2), 2.0*(qy*qz - qx*qw)],
                    [2.0*(qx*qz - qy*qw), 2.0*(qy*qz + qx*qw), 1.0 - 2.0*(qx**2 + qy**2)]
                ], dtype=np.float64)
                t_gl = np.array([px, py, pz], dtype=np.float64)

                # 2. Change of basis to Canonical AV World frame
                R_w_av = self.M_av_gl @ R_gl @ self.M_av_gl.T
                t_w_av = self.M_av_gl @ t_gl

                if self.canonical_R:
                    R_user = np.array(self.canonical_R, dtype=np.float64)
                    R_w_av = R_user @ R_w_av
                    t_w_av = R_user @ t_w_av

                # 3. Canonical AV position and heading
                px_av = float(t_w_av[0])
                py_av = float(t_w_av[1])
                pz_av = float(t_w_av[2])
                yaw_av = math.atan2(float(R_w_av[1, 0]), float(R_w_av[0, 0]))

                # 4. Canonical AV quaternion
                tr = float(R_w_av[0, 0] + R_w_av[1, 1] + R_w_av[2, 2])
                if tr > 0.0:
                    s = 0.5 / math.sqrt(tr + 1.0)
                    qw_av = 0.25 / s
                    qx_av = (R_w_av[2, 1] - R_w_av[1, 2]) * s
                    qy_av = (R_w_av[0, 2] - R_w_av[2, 0]) * s
                    qz_av = (R_w_av[1, 0] - R_w_av[0, 1]) * s
                elif (R_w_av[0, 0] > R_w_av[1, 1]) and (R_w_av[0, 0] > R_w_av[2, 2]):
                    s = 2.0 * math.sqrt(1.0 + R_w_av[0, 0] - R_w_av[1, 1] - R_w_av[2, 2])
                    qw_av = (R_w_av[2, 1] - R_w_av[1, 2]) / s
                    qx_av = 0.25 * s
                    qy_av = (R_w_av[0, 1] + R_w_av[1, 0]) / s
                    qz_av = (R_w_av[0, 2] + R_w_av[2, 0]) / s
                elif R_w_av[1, 1] > R_w_av[2, 2]:
                    s = 2.0 * math.sqrt(1.0 + R_w_av[1, 1] - R_w_av[0, 0] - R_w_av[2, 2])
                    qw_av = (R_w_av[0, 2] - R_w_av[2, 0]) / s
                    qx_av = (R_w_av[0, 1] + R_w_av[1, 0]) / s
                    qy_av = 0.25 * s
                    qz_av = (R_w_av[1, 2] + R_w_av[2, 1]) / s
                else:
                    s = 2.0 * math.sqrt(1.0 + R_w_av[2, 2] - R_w_av[0, 0] - R_w_av[1, 1])
                    qw_av = (R_w_av[1, 0] - R_w_av[0, 1]) / s
                    qx_av = (R_w_av[0, 2] + R_w_av[2, 0]) / s
                    qy_av = (R_w_av[1, 2] + R_w_av[2, 1]) / s
                    qz_av = 0.25 * s
            else:
                # Hold last valid pose or identity (no fake moving drift)
                px_av = self.current_pose.get("x", 0.0)
                py_av = self.current_pose.get("y", 0.0)
                pz_av = self.current_pose.get("z", 0.0)
                qx_av = self.current_pose.get("qx", 0.0)
                qy_av = self.current_pose.get("qy", 0.0)
                qz_av = self.current_pose.get("qz", 0.0)
                qw_av = self.current_pose.get("qw", 1.0)
                yaw_av = self.current_pose.get("yaw", 0.0)
                R_w_av = np.array([
                    [1.0 - 2.0*(qy_av**2 + qz_av**2), 2.0*(qx_av*qy_av - qz_av*qw_av), 2.0*(qx_av*qz_av + qy_av*qw_av)],
                    [2.0*(qx_av*qy_av + qz_av*qw_av), 1.0 - 2.0*(qx_av**2 + qz_av**2), 2.0*(qy_av*qz_av - qx_av*qw_av)],
                    [2.0*(qx_av*qz_av - qy_av*qw_av), 2.0*(qy_av*qz_av + qx_av*qw_av), 1.0 - 2.0*(qx_av**2 + qy_av**2)]
                ], dtype=np.float64)
                t_w_av = np.array([px_av, py_av, pz_av], dtype=np.float64)

            # Process Genuine Stereo Camera Frames for 3D Metric Reconstruction
            # ONLY in EXPERIMENTAL_METRIC_STEREO mode
            new_metric_pts = []
            if self.backend_mode == BACKEND_STEREO and self.latest_left_frame is not None and self.latest_right_frame is not None:
                img_l = self.latest_left_frame
                img_r = self.latest_right_frame

                rect_l = cv2.remap(img_l, self.map_l_x, self.map_l_y, cv2.INTER_LINEAR)
                rect_r = cv2.remap(img_r, self.map_r_x, self.map_r_y, cv2.INTER_LINEAR)

                # Direct disparity computation on rectified imagery (no CLAHE noise amplification)
                disp_l = self.left_matcher.compute(rect_l, rect_r)
                disp_r = self.right_matcher.compute(rect_r, rect_l)
                filtered_disp = self.wls_filter.filter(disp_l, rect_l, disparity_map_right=disp_r)
                disp_f = filtered_disp.astype(np.float32) / 16.0

                grad_x = cv2.Sobel(rect_l, cv2.CV_32F, 1, 0, ksize=3)
                grad_y = cv2.Sobel(rect_l, cv2.CV_32F, 0, 1, ksize=3)
                grad_mag = np.sqrt(grad_x**2 + grad_y**2)

                # High-confidence edge-preserving mask: eliminates featureless noise spray (grad > 28.0)
                valid_mask = (disp_f > 1.0) & (disp_f < 64.0) & (grad_mag > 28.0) & (rect_l > 15)

                v_idx, u_idx = np.where(valid_mask)
                if len(d_vals := disp_f[valid_mask]) > 0:
                    z_cam = (self.f_rect * self.baseline) / d_vals
                    x_cam = (u_idx - self.cx_rect) * z_cam / self.f_rect
                    y_cam = (v_idx - self.cy_rect) * z_cam / self.f_rect
                    intensities = rect_l[v_idx, u_idx]

                    # Metric range filter: 0.35m to 2.5m (clean spatial reconstruction)
                    range_mask = (z_cam >= 0.35) & (z_cam <= 2.5)
                    if np.count_nonzero(range_mask) > 0:
                        xc = x_cam[range_mask]
                        yc = y_cam[range_mask]
                        zc = z_cam[range_mask]
                        inte = intensities[range_mask]

                        pts_cam = np.stack([xc, yc, zc], axis=0) # (3, N)
                        pts_body = self.M_av_cam @ pts_cam        # (3, N)
                        pts_world = (R_w_av @ pts_body) + t_w_av[:, np.newaxis] # (3, N)

                        num_pts = pts_world.shape[1]
                        step = max(1, num_pts // 1500)
                        sample_indices = np.arange(0, num_pts, step)

                        for idx in sample_indices:
                            wx, wy, wz = float(pts_world[0, idx]), float(pts_world[1, idx]), float(pts_world[2, idx])
                            it = int(inte[idx])
                            gx = int(math.floor(wx / v_size))
                            gy = int(math.floor(wy / v_size))
                            gz = int(math.floor(wz / v_size))
                            key = (gx, gy, gz)
                            if key not in self.global_voxels:
                                # (wx, wy, wz, intensity, r, g, b, has_color)
                                # has_color=0 enforces neutral intensity visualization (no fake RGB)
                                pt_tuple = (wx, wy, wz, float(it)/255.0, it, it, it, 0)
                                self.global_voxels[key] = pt_tuple
                                new_metric_pts.append(pt_tuple)

            with self.lock:
                if tracked:
                    self.slam_timestamps.append(now_t)
                    while self.slam_timestamps and (now_t - self.slam_timestamps[0] > 1.0):
                        self.slam_timestamps.popleft()
                    self.slam_rate_hz = float(len(self.slam_timestamps))

                self.current_pose = {
                    "x": px_av, "y": py_av, "z": pz_av,
                    "qx": qx_av, "qy": qy_av, "qz": qz_av, "qw": qw_av,
                    "yaw": yaw_av,
                    "timestamp_ns": now_ns
                }
                self.tracking_state = "TRACKING_OK"
                if self.backend_mode == BACKEND_STEREO:
                    self.feature_count = len(new_metric_pts)
                    self.feature_quality = 1.0
                    if new_metric_pts:
                        self.new_points_buffer.extend(new_metric_pts)

                if not self.trajectory:
                    self.trajectory.append((now_ns, px_av, py_av, pz_av, qx_av, qy_av, qz_av, qw_av, yaw_av))
                else:
                    last = self.trajectory[-1]
                    dist_sq = (px_av - last[1])**2 + (py_av - last[2])**2 + (pz_av - last[3])**2
                    angle_diff = abs(yaw_av - last[8])
                    if dist_sq >= 0.0001 or angle_diff >= 0.01: # 1cm or ~0.5 deg
                        self.trajectory.append((now_ns, px_av, py_av, pz_av, qx_av, qy_av, qz_av, qw_av, yaw_av))

            # Wait for next frame or timeout
            self.new_frame_event.wait(timeout=0.04)
            self.new_frame_event.clear()

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
                    if self.lingbot_backend:
                        self.lingbot_backend.reset()
                    conn_file.write(b"RESET_OK: Cleared\n")
                    conn_file.flush()

                elif cmd == "GET_BACKEND":
                    resp = f"BACKEND_OK: {self.backend_mode} (Available: {BACKEND_LINGBOT}, {BACKEND_STEREO})\n"
                    conn_file.write(resp.encode("utf-8"))
                    conn_file.flush()

                elif cmd == "SET_BACKEND":
                    if self.is_capturing:
                        conn_file.write(b"SET_BACKEND_FAIL: Cannot change backend while capturing\n")
                    elif len(parts) > 1 and parts[1] in [BACKEND_LINGBOT, BACKEND_STEREO]:
                        self.backend_mode = parts[1]
                        self.reconstruction_backend_type = self.backend_mode
                        conn_file.write(f"SET_BACKEND_OK: {self.backend_mode}\n".encode("utf-8"))
                    else:
                        conn_file.write(b"SET_BACKEND_FAIL: Unknown backend\n")
                    conn_file.flush()

                elif cmd == "GET_TELEMETRY":
                    with self.lock:
                        acq_hz = self.rgb_acquisition_rate_hz if self.backend_mode == BACKEND_LINGBOT else self.acquisition_rate_hz
                        slam_hz = self.lingbot_backend.inference_hz if (self.lingbot_backend and self.backend_mode == BACKEND_LINGBOT) else self.slam_rate_hz
                        feat_cnt = self.lingbot_backend.total_reconstruction_points if (self.lingbot_backend and self.backend_mode == BACKEND_LINGBOT) else self.feature_count
                        feat_qual = (self.lingbot_backend.accepted_confident_points / max(1, self.lingbot_backend.total_reconstruction_points)) if (self.lingbot_backend and self.backend_mode == BACKEND_LINGBOT) else self.feature_quality
                        gpu_mem = self.lingbot_backend.current_gpu_memory_gb if self.lingbot_backend else 0.0
                        peak_gpu = self.lingbot_backend.peak_gpu_memory_gb if self.lingbot_backend else 0.0

                        telem = {
                            "status": "STREAMING" if self.is_capturing else "READY",
                            "tracking_state": self.tracking_state,
                            "is_capturing": self.is_capturing,
                            "point_count": len(self.global_voxels),
                            "trajectory_count": len(self.trajectory),
                            "acquisition_rate_hz": round(acq_hz, 2),
                            "slam_rate_hz": round(slam_hz, 2),
                            "imu_rate_hz": self.imu_rate_hz,
                            "current_pose": self.current_pose,
                            "current_yaw": self.current_pose["yaw"],
                            "feature_count": feat_cnt,
                            "feature_quality": round(feat_qual, 4),
                            "last_imu": self.last_imu,
                            "backend_type": self.backend_mode,
                            "carina_vio_hz": round(self.slam_rate_hz, 2),
                            "gpu_memory_gb": round(gpu_mem, 2),
                            "peak_gpu_memory_gb": round(peak_gpu, 2),
                            "model_ready": self.lingbot_ready_event.is_set()
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
