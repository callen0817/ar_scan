import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root
    visible: true
    width: 1600
    height: 960
    minimumWidth: 1024
    minimumHeight: 680
    title: "AV Scan — Find Your Sense (Commercial Spatial Capture)"
    color: "#0f1115"

    // Top Header / Navigation Bar
    Rectangle {
        id: header
        height: 68
        color: "#161920"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#232834"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            // Brand Block
            RowLayout {
                spacing: 12
                Rectangle {
                    width: 36
                    height: 36
                    radius: 8
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0ea5e9" }
                        GradientStop { position: 1.0; color: "#0284c7" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "AV"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 15
                    }
                }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: "ARTIFICIAL VISION"
                        color: "#94a3b8"
                        font.bold: true
                        font.pixelSize: 10
                        font.letterSpacing: 1.5
                    }
                    Text {
                        text: "AV SCAN"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 17
                    }
                    Text {
                        text: "Find Your Sense."
                        color: "#38bdf8"
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                width: 1
                height: 38
                color: "#28303e"
                Layout.leftMargin: 8
                Layout.rightMargin: 8
            }

            // Project Identification
            ColumnLayout {
                spacing: 2
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 7
                        height: 7
                        radius: 3.5
                        color: bridge.projectId !== "" ? "#10b981" : "#f59e0b"
                    }
                    Text {
                        text: bridge.projectId !== "" ? "ACTIVE PROJECT" : "NO PROJECT LOADED"
                        color: "#64748b"
                        font.pixelSize: 9
                        font.bold: true
                        font.letterSpacing: 1.0
                    }
                }
                Text {
                    text: bridge.projectName
                    color: "#f8fafc"
                    font.bold: true
                    font.pixelSize: 15
                }
                Text {
                    text: bridge.projectId !== "" ? (bridge.projectId + " • " + bridge.projectCreatedAt) : "Create or Open a Project to enable capture"
                    color: "#64748b"
                    font.pixelSize: 11
                }
            }

            // Project Action Buttons
            RowLayout {
                spacing: 8
                Layout.leftMargin: 8

                Button {
                    id: newProjectBtn
                    text: "+ New Project"
                    onClicked: newProjectDialog.open()
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: newProjectBtn.hovered ? "#334155" : "#1e293b"
                        radius: 6
                        border.color: "#475569"
                    }
                }

                Button {
                    id: openProjectBtn
                    text: "Open Project..."
                    onClicked: openProjectDialog.open()
                    contentItem: Text {
                        text: parent.text
                        color: "#e2e8f0"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: openProjectBtn.hovered ? "#334155" : "#1e293b"
                        radius: 6
                        border.color: "#334155"
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Platform & Host Telemetry Pill
            Rectangle {
                height: 32
                implicitWidth: hostLayout.implicitWidth + 24
                radius: 16
                color: "#181e28"
                border.color: "#2a3445"

                RowLayout {
                    id: hostLayout
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#10b981"
                    }
                    Text {
                        text: bridge.hostPlatform
                        color: "#e2e8f0"
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Rectangle {
                        width: 1
                        height: 14
                        color: "#334155"
                    }
                    Text {
                        text: "RAM: " + bridge.hostMemory
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                    Rectangle {
                        width: 1
                        height: 14
                        color: "#334155"
                    }
                    Text {
                        text: "Storage: " + bridge.hostDisk
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    // Main Workspace: Split Dual Viewports (3D Perspective Spatial Map + 2D Ortho Floor Plan)
    RowLayout {
        id: viewportsArea
        spacing: 12
        anchors {
            top: header.bottom
            bottom: bottomBar.top
            left: parent.left
            right: parent.right
            margins: 12
        }

        // ==========================================
        // 1. 3D SPATIAL MAP VIEWPORT
        // ==========================================
        Rectangle {
            id: panel3d
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 60
            color: "#13161c"
            radius: 8
            border.color: "#232834"
            clip: true

            // Top Header Overlay
            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                z: 10

                Rectangle {
                    color: "#1e293b"
                    radius: 4
                    height: 24
                    implicitWidth: view3dLabel.contentWidth + 14
                    Text {
                        id: view3dLabel
                        anchors.centerIn: parent
                        text: "3D SPATIAL MAP"
                        color: "#38bdf8"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                // Preset Buttons
                RowLayout {
                    spacing: 4
                    Button {
                        text: "Recenter"
                        implicitHeight: 24
                        implicitWidth: 68
                        onClicked: {
                            bridge.recenter3D()
                            canvas3d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                    Button {
                        text: "Isometric"
                        implicitHeight: 24
                        implicitWidth: 64
                        onClicked: {
                            bridge.setPreset3D("iso")
                            canvas3d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                    Button {
                        text: "Top (Z)"
                        implicitHeight: 24
                        implicitWidth: 54
                        onClicked: {
                            bridge.setPreset3D("top")
                            canvas3d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                    Button {
                        text: "Side"
                        implicitHeight: 24
                        implicitWidth: 46
                        onClicked: {
                            bridge.setPreset3D("side")
                            canvas3d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    color: "#1e2532"
                    radius: 4
                    height: 24
                    implicitWidth: points3dLabel.contentWidth + 14
                    Text {
                        id: points3dLabel
                        anchors.centerIn: parent
                        text: bridge.isCapturing ? (bridge.pointsCaptured + " Points (Real)") : "Common AV Map"
                        color: "#94a3b8"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            // Interactive 3D Canvas
            Canvas {
                id: canvas3d
                anchors.fill: parent

                Connections {
                    target: bridge
                    function onCam3DChanged() { canvas3d.requestPaint(); }
                    function onPointsCapturedChanged() { canvas3d.requestPaint(); }
                    function onCaptureStateChanged() { canvas3d.requestPaint(); }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width * 0.5;
                    var cy = height * 0.55;

                    // Camera math (Spherical coordinates to cartesian eye position)
                    var yawRad = bridge.camYaw3D * Math.PI / 180.0;
                    var pitchRad = bridge.camPitch3D * Math.PI / 180.0;
                    var dist = bridge.camDistance3D;

                    var eyeX = bridge.camTargetX3D + dist * Math.cos(pitchRad) * Math.sin(yawRad);
                    var eyeY = bridge.camTargetY3D - dist * Math.cos(pitchRad) * Math.cos(yawRad);
                    var eyeZ = bridge.camTargetZ3D + dist * Math.sin(pitchRad);

                    var targetX = bridge.camTargetX3D;
                    var targetY = bridge.camTargetY3D;
                    var targetZ = bridge.camTargetZ3D;

                    // Forward vector
                    var fx = targetX - eyeX;
                    var fy = targetY - eyeY;
                    var fz = targetZ - eyeZ;
                    var fLen = Math.sqrt(fx * fx + fy * fy + fz * fz);
                    if (fLen > 0.0001) { fx /= fLen; fy /= fLen; fz /= fLen; }

                    // Up vector (world Z is [0, 0, 1])
                    var ux_w = 0, uy_w = 0, uz_w = 1;
                    // Right = forward x up_w
                    var rx = fy * uz_w - fz * uy_w;
                    var ry = fz * ux_w - fx * uz_w;
                    var rz = fx * uy_w - fy * ux_w;
                    var rLen = Math.sqrt(rx * rx + ry * ry + rz * rz);
                    if (rLen > 0.0001) { rx /= rLen; ry /= rLen; rz /= rLen; } else { rx = 1; ry = 0; rz = 0; }

                    // Up = right x forward
                    var ux = ry * fz - rz * fy;
                    var uy = rz * fx - rx * fz;
                    var uz = rx * fy - ry * fx;

                    var focal = (height * 0.5) / Math.tan(30.0 * Math.PI / 180.0);

                    function project(x, y, z) {
                        var dx = x - eyeX;
                        var dy = y - eyeY;
                        var dz = z - eyeZ;

                        var camX = dx * rx + dy * ry + dz * rz;
                        var camY = dx * ux + dy * uy + dz * uz;
                        var camZ = dx * fx + dy * fy + dz * fz;

                        if (camZ <= 0.1) return null;
                        return {
                            x: cx + (camX / camZ) * focal,
                            y: cy - (camY / camZ) * focal,
                            depth: camZ
                        };
                    }

                    // 1. Draw 3D Ground Reference Grid at Z=0 (-5m to +5m)
                    ctx.lineWidth = 1;
                    for (var gx = -5; gx <= 5; ++gx) {
                        var p1 = project(gx, -5, 0);
                        var p2 = project(gx, 5, 0);
                        if (p1 && p2) {
                            ctx.strokeStyle = (gx === 0) ? "#2e3b52" : "#1a212d";
                            ctx.beginPath();
                            ctx.moveTo(p1.x, p1.y);
                            ctx.lineTo(p2.x, p2.y);
                            ctx.stroke();
                        }
                    }
                    for (var gy = -5; gy <= 5; ++gy) {
                        var p3 = project(-5, gy, 0);
                        var p4 = project(5, gy, 0);
                        if (p3 && p4) {
                            ctx.strokeStyle = (gy === 0) ? "#2e3b52" : "#1a212d";
                            ctx.beginPath();
                            ctx.moveTo(p3.x, p3.y);
                            ctx.lineTo(p4.x, p4.y);
                            ctx.stroke();
                        }
                    }

                    // 2. Draw 3D Coordinate Axes at Origin (X: Red, Y: Green, Z: Blue)
                    var o = project(0, 0, 0);
                    var axX = project(2.0, 0, 0);
                    var axY = project(0, 2.0, 0);
                    var axZ = project(0, 0, 2.0);

                    if (o && axX) {
                        ctx.strokeStyle = "#ef4444";
                        ctx.lineWidth = 2.5;
                        ctx.beginPath();
                        ctx.moveTo(o.x, o.y);
                        ctx.lineTo(axX.x, axX.y);
                        ctx.stroke();
                        ctx.fillStyle = "#ef4444";
                        ctx.font = "bold 11px sans-serif";
                        ctx.fillText("+X (2m)", axX.x + 4, axX.y);
                    }
                    if (o && axY) {
                        ctx.strokeStyle = "#10b981";
                        ctx.lineWidth = 2.5;
                        ctx.beginPath();
                        ctx.moveTo(o.x, o.y);
                        ctx.lineTo(axY.x, axY.y);
                        ctx.stroke();
                        ctx.fillStyle = "#10b981";
                        ctx.font = "bold 11px sans-serif";
                        ctx.fillText("+Y (2m)", axY.x + 4, axY.y);
                    }
                    if (o && axZ) {
                        ctx.strokeStyle = "#3b82f6";
                        ctx.lineWidth = 2.5;
                        ctx.beginPath();
                        ctx.moveTo(o.x, o.y);
                        ctx.lineTo(axZ.x, axZ.y);
                        ctx.stroke();
                        ctx.fillStyle = "#3b82f6";
                        ctx.font = "bold 11px sans-serif";
                        ctx.fillText("+Z (2m)", axZ.x + 4, axZ.y);
                    }

                    // 3. Draw Camera Look-At Target Reticle
                    var ptTarget = project(targetX, targetY, targetZ);
                    if (ptTarget) {
                        ctx.strokeStyle = "#f59e0b";
                        ctx.lineWidth = 1.5;
                        ctx.beginPath();
                        ctx.arc(ptTarget.x, ptTarget.y, 5, 0, 2 * Math.PI);
                        ctx.stroke();
                    }
                }
            }

            // Mouse Area for 3D Interaction: Orbit, Pan, Zoom
            MouseArea {
                id: mouseArea3d
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                property real lastX: 0
                property real lastY: 0

                onPressed: {
                    lastX = mouse.x
                    lastY = mouse.y
                }

                onPositionChanged: {
                    var dx = mouse.x - lastX
                    var dy = mouse.y - lastY
                    lastX = mouse.x
                    lastY = mouse.y

                    if (mouse.buttons & Qt.RightButton || (mouse.buttons & Qt.LeftButton && (mouse.modifiers & Qt.ShiftModifier))) {
                        // Pan 3D
                        bridge.pan3D(dx, dy)
                    } else if (mouse.buttons & Qt.LeftButton) {
                        // Orbit 3D
                        bridge.orbit3D(dx, dy)
                    }
                }

                onWheel: {
                    bridge.zoom3D(wheel.angleDelta.y)
                }
            }

            // Status Notice Overlay when Map is Empty
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8
                visible: bridge.pointsCaptured === 0

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 44
                    height: 44
                    radius: 22
                    color: "#18202c"
                    border.color: "#283548"
                    Text {
                        anchors.centerIn: parent
                        text: "3D"
                        color: "#38bdf8"
                        font.bold: true
                        font.pixelSize: 15
                    }
                }
                Text {
                    text: bridge.isCapturing ? "Spatial Session Active (Awaiting M3 Hardware Streams)" : "AV Common Map Viewport"
                    color: "#e2e8f0"
                    font.bold: true
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: "Physical sensor stream qualification scheduled for M3 (RoboSense Airy)"
                    color: "#64748b"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            // Bottom HUD: 3D Camera Telemetry & Gesture Guide
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 10
                height: 28
                radius: 4
                color: "#181d26"
                border.color: "#232b38"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Text {
                        text: "Yaw: " + bridge.camYaw3D.toFixed(1) + "°  |  Pitch: " + bridge.camPitch3D.toFixed(1) + "°  |  Distance: " + bridge.camDistance3D.toFixed(1) + "m  |  Target: (" + bridge.camTargetX3D.toFixed(2) + ", " + bridge.camTargetY3D.toFixed(2) + ")"
                        color: "#94a3b8"
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "Orbit: Left Drag  •  Pan: Right Drag / Shift+Drag  •  Zoom: Wheel"
                        color: "#64748b"
                        font.pixelSize: 10
                    }
                }
            }
        }

        // ==========================================
        // 2. 2D FLOOR PLAN / TOP-DOWN VIEWPORT
        // ==========================================
        Rectangle {
            id: panel2d
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 40
            color: "#13161c"
            radius: 8
            border.color: "#232834"
            clip: true

            // Top Header Overlay
            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 12
                z: 10

                Rectangle {
                    color: "#1e293b"
                    radius: 4
                    height: 24
                    implicitWidth: view2dLabel.contentWidth + 14
                    Text {
                        id: view2dLabel
                        anchors.centerIn: parent
                        text: "2D FLOOR PLAN"
                        color: "#34d399"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                // Controls
                RowLayout {
                    spacing: 4
                    Button {
                        text: "Recenter"
                        implicitHeight: 24
                        implicitWidth: 68
                        onClicked: {
                            bridge.recenter2D()
                            canvas2d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                    Button {
                        text: "+"
                        implicitHeight: 24
                        implicitWidth: 32
                        onClicked: {
                            bridge.zoom2DByFactor(1.25)
                            canvas2d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                    Button {
                        text: "-"
                        implicitHeight: 24
                        implicitWidth: 32
                        onClicked: {
                            bridge.zoom2DByFactor(0.8)
                            canvas2d.requestPaint()
                        }
                        contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: parent.hovered ? "#334155" : "#1e2532"; radius: 4; border.color: "#334155" }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    color: "#1e2532"
                    radius: 4
                    height: 24
                    implicitWidth: mode2dLabel.contentWidth + 14
                    Text {
                        id: mode2dLabel
                        anchors.centerIn: parent
                        text: "Top-Down Ortho"
                        color: "#94a3b8"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            // Interactive 2D Canvas
            Canvas {
                id: canvas2d
                anchors.fill: parent

                Connections {
                    target: bridge
                    function onCam2DChanged() { canvas2d.requestPaint(); }
                    function onCaptureStateChanged() { canvas2d.requestPaint(); }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width * 0.5;
                    var cy = height * 0.5;
                    var zoom = bridge.zoom2D; // pixels per meter
                    var panX = bridge.pan2DX;
                    var panY = bridge.pan2DY;

                    function worldToScreen(wx, wy) {
                        return {
                            x: cx + (wx - panX) * zoom,
                            y: cy - (wy - panY) * zoom // Y-up
                        };
                    }

                    // 1. Draw Orthogonal Metric Grid
                    // Determine grid line step based on zoom
                    var step = (zoom > 50) ? 1.0 : (zoom > 15) ? 2.0 : 5.0;

                    var minWorldX = panX - (cx / zoom) - step;
                    var maxWorldX = panX + (cx / zoom) + step;
                    var minWorldY = panY - (cy / zoom) - step;
                    var maxWorldY = panY + (cy / zoom) + step;

                    var startX = Math.floor(minWorldX / step) * step;
                    var startY = Math.floor(minWorldY / step) * step;

                    ctx.lineWidth = 1;
                    ctx.font = "9px monospace";

                    for (var x = startX; x <= maxWorldX; x += step) {
                        var pTop = worldToScreen(x, maxWorldY);
                        var pBottom = worldToScreen(x, minWorldY);
                        var isZeroX = Math.abs(x) < 0.001;

                        ctx.strokeStyle = isZeroX ? "#ef4444" : ((Math.round(x) % 5 === 0) ? "#263244" : "#1a212d");
                        ctx.beginPath();
                        ctx.moveTo(pTop.x, pTop.y);
                        ctx.lineTo(pBottom.x, pBottom.y);
                        ctx.stroke();

                        // Label
                        if (Math.round(x) % 5 === 0 || isZeroX) {
                            ctx.fillStyle = isZeroX ? "#ef4444" : "#475569";
                            ctx.fillText(x.toFixed(0) + "m", pBottom.x + 3, cy + 12);
                        }
                    }

                    for (var y = startY; y <= maxWorldY; y += step) {
                        var pLeft = worldToScreen(minWorldX, y);
                        var pRight = worldToScreen(maxWorldX, y);
                        var isZeroY = Math.abs(y) < 0.001;

                        ctx.strokeStyle = isZeroY ? "#10b981" : ((Math.round(y) % 5 === 0) ? "#263244" : "#1a212d");
                        ctx.beginPath();
                        ctx.moveTo(pLeft.x, pLeft.y);
                        ctx.lineTo(pRight.x, pRight.y);
                        ctx.stroke();

                        // Label
                        if (Math.round(y) % 5 === 0 || isZeroY) {
                            ctx.fillStyle = isZeroY ? "#10b981" : "#475569";
                            ctx.fillText(y.toFixed(0) + "m", cx + 6, pLeft.y - 3);
                        }
                    }

                    // 2. Origin Crosshair
                    var orig = worldToScreen(0, 0);
                    ctx.strokeStyle = "#f59e0b";
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.arc(orig.x, orig.y, 4, 0, 2 * Math.PI);
                    ctx.stroke();

                    // 3. North Orientation Compass (Top-Right)
                    var compX = width - 42;
                    var compY = 56;
                    ctx.fillStyle = "#1e293b";
                    ctx.beginPath();
                    ctx.arc(compX, compY, 18, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.strokeStyle = "#38bdf8";
                    ctx.lineWidth = 1.5;
                    ctx.stroke();

                    // North Arrow pointing UP (+Y)
                    ctx.fillStyle = "#ef4444";
                    ctx.beginPath();
                    ctx.moveTo(compX, compY - 14);
                    ctx.lineTo(compX - 5, compY + 4);
                    ctx.lineTo(compX + 5, compY + 4);
                    ctx.closePath();
                    ctx.fill();

                    ctx.fillStyle = "white";
                    ctx.font = "bold 9px sans-serif";
                    ctx.fillText("N", compX - 3.5, compY - 16);

                    // 4. Metric Scale Bar (Bottom-Left)
                    var barPixels = zoom * 1.0; // 1 meter bar
                    var barX = 20;
                    var barY = height - 44;
                    ctx.strokeStyle = "#cbd5e1";
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(barX, barY);
                    ctx.lineTo(barX + barPixels, barY);
                    ctx.moveTo(barX, barY - 4);
                    ctx.lineTo(barX, barY + 4);
                    ctx.moveTo(barX + barPixels, barY - 4);
                    ctx.lineTo(barX + barPixels, barY + 4);
                    ctx.stroke();

                    ctx.fillStyle = "#cbd5e1";
                    ctx.font = "bold 10px monospace";
                    ctx.fillText("1.0 m", barX + (barPixels * 0.5) - 14, barY - 6);
                }
            }

            // Mouse Area for 2D Interaction: Pan and Zoom
            MouseArea {
                id: mouseArea2d
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                property real lastX: 0
                property real lastY: 0

                onPressed: {
                    lastX = mouse.x
                    lastY = mouse.y
                }

                onPositionChanged: {
                    var dx = mouse.x - lastX
                    var dy = mouse.y - lastY
                    lastX = mouse.x
                    lastY = mouse.y

                    if (mouse.buttons & (Qt.LeftButton | Qt.RightButton | Qt.MiddleButton)) {
                        bridge.pan2D(dx, dy)
                    }
                }

                onWheel: {
                    bridge.zoom2DByFactor(wheel.angleDelta.y > 0 ? 1.15 : 0.85)
                }
            }

            // Bottom HUD: 2D Scale and Center Coordinates
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 10
                height: 28
                radius: 4
                color: "#181d26"
                border.color: "#232b38"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    Text {
                        text: "Scale: " + bridge.zoom2D.toFixed(1) + " px/m  |  Center: (" + bridge.pan2DX.toFixed(2) + "m, " + bridge.pan2DY.toFixed(2) + "m)"
                        color: "#94a3b8"
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "Pan: Drag  •  Zoom: Scroll"
                        color: "#64748b"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }

    // ==========================================
    // 3. BOTTOM CONTROL, TELEMETRY & CAPTURE BAR
    // ==========================================
    Rectangle {
        id: bottomBar
        height: 84
        color: "#161920"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#232834"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 24

            // Scanner Selector
            ColumnLayout {
                spacing: 3
                Text {
                    text: "SENSOR / SCANNER CONFIGURATION"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }

                ComboBox {
                    id: scannerCombo
                    model: bridge.availableScanners
                    currentIndex: 0
                    onActivated: bridge.selectScannerByIndex(currentIndex)
                    background: Rectangle {
                        implicitWidth: 320
                        implicitHeight: 36
                        color: "#1c222c"
                        border.color: "#334155"
                        radius: 6
                    }
                    contentItem: Text {
                        leftPadding: 12
                        text: scannerCombo.displayText
                        color: "#f8fafc"
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                Text {
                    text: bridge.selectedScannerValidationNotice
                    color: bridge.selectedScannerStatus === "CONFIG" ? "#38bdf8" : "#f59e0b"
                    font.pixelSize: 10
                }
            }

            Rectangle {
                width: 1
                height: 48
                color: "#28303e"
            }

            // Minimal Sensor Status
            ColumnLayout {
                spacing: 4
                Text {
                    text: "SENSOR STATUS"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: bridge.isCapturing ? "#10b981" : 
                               bridge.sensorStatus === "READY" ? "#3b82f6" : "#64748b"
                    }
                    Text {
                        text: bridge.sensorStatus
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
                Text {
                    text: bridge.selectedScannerDetails
                    color: "#64748b"
                    font.pixelSize: 10
                }
            }

            Rectangle {
                width: 1
                height: 48
                color: "#28303e"
            }

            // Minimal Tracking Status
            ColumnLayout {
                spacing: 4
                Text {
                    text: "TRACKING STATE"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: bridge.trackingStatus === "TRACKING_OK" ? "#10b981" : 
                               bridge.trackingStatus === "READY" ? "#3b82f6" : "#64748b"
                    }
                    Text {
                        text: bridge.trackingStatus
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
                Text {
                    text: "Engine: FAST-LIO2 (M3)"
                    color: "#64748b"
                    font.pixelSize: 10
                }
            }

            Rectangle {
                width: 1
                height: 48
                color: "#28303e"
            }

            // Elapsed Capture Timer
            ColumnLayout {
                spacing: 2
                Text {
                    text: "CAPTURE DURATION"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                Text {
                    text: bridge.elapsedTimeString
                    color: bridge.isCapturing ? "#38bdf8" : "#94a3b8"
                    font.pixelSize: 22
                    font.bold: true
                    font.family: "Monospace"
                }
            }

            Item { Layout.fillWidth: true }

            // Primary Action Buttons: START, STOP, SAVE
            RowLayout {
                spacing: 12

                Button {
                    id: startBtn
                    text: "START"
                    enabled: bridge.canStart
                    implicitWidth: 120
                    implicitHeight: 46
                    onClicked: {
                        bridge.startCapture();
                        canvas3d.requestPaint();
                        canvas2d.requestPaint();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: startBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: startBtn.enabled ? (startBtn.hovered ? "#15803d" : "#16a34a") : "#1c2331"
                        radius: 6
                        border.color: startBtn.enabled ? "#22c55e" : "#2d3748"
                    }
                }

                Button {
                    id: stopBtn
                    text: "STOP"
                    enabled: bridge.canStop
                    implicitWidth: 120
                    implicitHeight: 46
                    onClicked: {
                        bridge.stopCapture();
                        canvas3d.requestPaint();
                        canvas2d.requestPaint();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: stopBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: stopBtn.enabled ? (stopBtn.hovered ? "#b91c1c" : "#dc2626") : "#1c2331"
                        radius: 6
                        border.color: stopBtn.enabled ? "#ef4444" : "#2d3748"
                    }
                }

                Button {
                    id: saveBtn
                    text: "SAVE"
                    enabled: bridge.canSave
                    implicitWidth: 120
                    implicitHeight: 46
                    onClicked: bridge.saveCapture()
                    contentItem: Text {
                        text: parent.text
                        color: saveBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 14
                        font.letterSpacing: 1.0
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: saveBtn.enabled ? (saveBtn.hovered ? "#1d4ed8" : "#2563eb") : "#1c2331"
                        radius: 6
                        border.color: saveBtn.enabled ? "#3b82f6" : "#2d3748"
                    }
                }
            }
        }
    }

    // ==========================================
    // 4. MODALS & DIALOGS
    // ==========================================

    // Dialog: Create New Project
    Dialog {
        id: newProjectDialog
        title: "Create New AV Scan Project"
        modal: true
        anchors.centerIn: parent
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#181e28"
            radius: 8
            border.color: "#334155"
        }

        header: Rectangle {
            height: 52
            color: "#202836"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Create New Project"
                color: "white"
                font.bold: true
                font.pixelSize: 15
            }
        }

        ColumnLayout {
            spacing: 14
            width: parent.width

            Text {
                text: "Project Name:"
                color: "#cbd5e1"
                font.pixelSize: 12
                font.bold: true
            }
            TextField {
                id: projNameInput
                Layout.fillWidth: true
                placeholderText: "e.g., Industrial Plant Survey"
                color: "white"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#0f1218"
                    radius: 4
                    border.color: "#3b4556"
                }
            }

            Text {
                text: "Description (Optional):"
                color: "#cbd5e1"
                font.pixelSize: 12
                font.bold: true
            }
            TextField {
                id: projDescInput
                Layout.fillWidth: true
                placeholderText: "Survey notes, building ID, inspection parameters"
                color: "white"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#0f1218"
                    radius: 4
                    border.color: "#3b4556"
                }
            }
        }

        onAccepted: {
            if (projNameInput.text.trim() !== "") {
                bridge.createProject(projNameInput.text.trim(), projDescInput.text.trim());
                projNameInput.text = "";
                projDescInput.text = "";
            }
        }
    }

    // Dialog: Open Existing Project
    Dialog {
        id: openProjectDialog
        title: "Open AV Scan Project"
        modal: true
        anchors.centerIn: parent
        width: 560
        height: 420
        standardButtons: Dialog.Cancel

        background: Rectangle {
            color: "#181e28"
            radius: 8
            border.color: "#334155"
        }

        header: Rectangle {
            height: 52
            color: "#202836"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Open Existing Project"
                color: "white"
                font.bold: true
                font.pixelSize: 15
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 12

            Text {
                text: "Select a project from the repository:"
                color: "#94a3b8"
                font.pixelSize: 12
            }

            ListView {
                id: projListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: bridge.projectList

                delegate: Rectangle {
                    width: projListView.width
                    height: 56
                    radius: 6
                    color: itemMouseArea.containsMouse ? "#273244" : "#1e2634"
                    border.color: "#334155"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Text {
                                text: modelData.name
                                color: "#f8fafc"
                                font.bold: true
                                font.pixelSize: 13
                            }
                            Text {
                                text: modelData.id + " • " + modelData.date + (modelData.description ? (" • " + modelData.description) : "")
                                color: "#94a3b8"
                                font.pixelSize: 11
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Load"
                            implicitHeight: 30
                            implicitWidth: 64
                            onClicked: {
                                bridge.openProject(modelData.id);
                                openProjectDialog.close();
                            }
                            contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: "#0284c7"; radius: 4 }
                        }
                    }

                    MouseArea {
                        id: itemMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: {
                            bridge.openProject(modelData.id);
                            openProjectDialog.close();
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                }
            }
        }
    }

    // Notification Toast Popup
    Popup {
        id: toastPopup
        anchors.centerIn: parent
        width: 360
        height: 76
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e293b"
            radius: 8
            border.color: "#38bdf8"
            border.width: 1.5
        }

        property string toastTitle: ""
        property string toastMessage: ""

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 4
            Text {
                text: toastPopup.toastTitle
                color: "#38bdf8"
                font.bold: true
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: toastPopup.toastMessage
                color: "#e2e8f0"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Timer {
            id: toastTimer
            interval: 3500
            onTriggered: toastPopup.close()
        }
    }

    Connections {
        target: bridge
        function onNotification(title, message) {
            toastPopup.toastTitle = title;
            toastPopup.toastMessage = message;
            toastPopup.open();
            toastTimer.restart();
        }
        function onErrorOccurred(title, message) {
            toastPopup.toastTitle = title;
            toastPopup.toastMessage = message;
            toastPopup.open();
            toastTimer.restart();
        }
    }
}
