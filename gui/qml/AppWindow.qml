import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root
    visible: true
    visibility: Window.Maximized
    width: 1600
    height: 960
    minimumWidth: 1024
    minimumHeight: 680
    title: "AV Scan — Find Your Sense (Commercial Spatial Capture)"
    color: "#0f1115"

    Component.onCompleted: {
        bridge.refreshDiscovery();
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i] === "--open-catalog") {
                configureSensorsDialog.currentTab = 2;
                configureSensorsDialog.open();
                break;
            } else if (Qt.application.arguments[i] === "--open-config") {
                configureSensorsDialog.currentTab = 1;
                configureSensorsDialog.open();
                break;
            } else if (Qt.application.arguments[i] === "--open-diag") {
                bridge.openOrientationDiagnostic();
                orientationDiagDialog.open();
                break;
            }
        }
    }

    function getSensorPoints(id, defaultCount) {
        var c = 0;
        if (id === "AIRY") c = bridge.airyPointsCaptured;
        else if (id === "GEMINI") c = bridge.geminiPointsCaptured;
        else if (id === "VITURE") c = bridge.viturePointsCaptured;
        else if (id === "CAMERA") c = bridge.cameraPointsCaptured;
        return (c > 0) ? c : (defaultCount || 0);
    }

    function getSensorAcqRate(id) {
        if (id === "AIRY") return bridge.airyAcquisitionRate;
        if (id === "GEMINI") return bridge.geminiAcquisitionRate;
        if (id === "VITURE") return bridge.vitureAcquisitionRate;
        if (id === "CAMERA") return bridge.cameraAcquisitionRate;
        return 0.0;
    }

    function getSensorSlamRate(id) {
        if (id === "AIRY") return bridge.airySlamRate;
        if (id === "GEMINI") return bridge.geminiSlamRate;
        if (id === "VITURE") return bridge.vitureSlamRate;
        if (id === "CAMERA") return bridge.cameraSlamRate;
        return 0.0;
    }

    function getSensorTracking(id, sData) {
        if (sData && sData.trackingStatus && sData.trackingStatus !== "IDLE" && sData.trackingStatus !== "") return sData.trackingStatus;
        if (id === "AIRY") return bridge.airyTrackingStatus;
        if (id === "GEMINI") return bridge.geminiTrackingStatus;
        if (id === "VITURE") return bridge.vitureTrackingStatus;
        if (id === "CAMERA") return bridge.cameraTrackingStatus;
        if (sData && sData.trackingStatus) return sData.trackingStatus;
        return "IDLE";
    }

    function getSensorAcquisition(id, sData) {
        if (sData && sData.acquisitionStatus && sData.acquisitionStatus !== "STOPPED" && sData.acquisitionStatus !== "") return sData.acquisitionStatus;
        if (id === "AIRY") return bridge.airyAcquisitionStatus;
        if (id === "GEMINI") return bridge.geminiAcquisitionStatus;
        if (id === "VITURE") return bridge.vitureAcquisitionStatus;
        if (id === "CAMERA") return bridge.cameraAcquisitionStatus;
        if (sData && sData.acquisitionStatus) return sData.acquisitionStatus;
        return "STOPPED";
    }

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

                Button {
                    id: headerOrientationDiagBtn
                    text: "📐 Orientation Diagnostic"
                    onClicked: {
                        bridge.openOrientationDiagnostic();
                        orientationDiagDialog.open();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#c084fc"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: headerOrientationDiagBtn.hovered ? "#3b0764" : "#1e1b4b"
                        radius: 6
                        border.color: "#a855f7"
                    }
                }

                Button {
                    id: headerMultiNodeBtn
                    text: "🌐 Multi-Node Mesh"
                    onClicked: {
                        bridge.refreshMultiNodeDiscovery();
                        multiNodeMeshDialog.open();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: bridge.isMultiNodeSessionActive ? "#38bdf8" : "#94a3b8"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: headerMultiNodeBtn.hovered ? "#0c4a6e" : (bridge.isMultiNodeSessionActive ? "#082f49" : "#1e293b")
                        radius: 6
                        border.color: bridge.isMultiNodeSessionActive ? "#38bdf8" : "#334155"
                        border.width: bridge.isMultiNodeSessionActive ? 2 : 1
                    }
                }

                Button {
                    id: headerOfflineRegBtn
                    text: "🔀 Register Scans (M9)"
                    onClicked: {
                        bridge.refreshSessionCaptures();
                        bridge.refreshRegistrations();
                        offlineRegistrationDialog.open();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: bridge.isRegistrationActive ? "#00e5ff" : "#34d399"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: headerOfflineRegBtn.hovered ? "#064e3b" : (bridge.isRegistrationActive ? "#083344" : "#062a22")
                        radius: 6
                        border.color: bridge.isRegistrationActive ? "#00e5ff" : "#059669"
                        border.width: bridge.isRegistrationActive ? 2 : 1
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
            bottom: sensorDataBar.top
            left: parent.left
            right: parent.right
            margins: 12
            bottomMargin: 8
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

                // Dual-Sensor View Switcher (M6 Multi-Sensor Qualification)
                // Multi-Sensor View Switcher (M6 Multi-Sensor Qualification)
                RowLayout {
                    visible: bridge.activeSensorCount > 1
                    spacing: 6

                    Rectangle {
                        color: "#0f172a"
                        radius: 4
                        border.color: "#334155"
                        implicitHeight: 24
                        implicitWidth: extUnkText.contentWidth + 12
                        Text {
                            id: extUnkText
                            anchors.centerIn: parent
                            text: "Extrinsics: UNKNOWN (Independent SLAM, Unfused)"
                            color: "#f59e0b"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Repeater {
                        model: bridge.activeSensors
                        delegate: Rectangle {
                            id: tabSensorBtn
                            property var sData: modelData
                            property bool isCurrent: bridge.activeDisplaySensor === sData.id

                            implicitHeight: 28
                            implicitWidth: tabBtnRow.implicitWidth + 20
                            radius: 5
                            color: isCurrent ? "#1e3a5f" : (tabMa.containsMouse ? "#1e293b" : "#0f172a")
                            border.color: isCurrent ? "#38bdf8" : (tabMa.containsMouse ? "#64748b" : "#334155")
                            border.width: isCurrent ? 2 : 1

                            MouseArea {
                                id: tabMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onPressed: {
                                    bridge.setActiveDisplaySensor(sData.id)
                                    canvas3d.requestPaint()
                                    canvas2d.requestPaint()
                                }
                            }

                            RowLayout {
                                id: tabBtnRow
                                anchors.centerIn: parent
                                spacing: 5

                                Text {
                                    text: sData.icon || (sData.sensorType === "LIDAR" ? "📡" : (sData.id === "VITURE" ? "👓" : "📷"))
                                    font.pixelSize: 11
                                }

                                Text {
                                    text: sData.shortName
                                    color: isCurrent ? "#ffffff" : (tabMa.containsMouse ? "#e2e8f0" : "#cbd5e1")
                                    font.pixelSize: 10
                                    font.bold: isCurrent
                                }

                                Text {
                                    text: "(" + getSensorPoints(sData.id, sData.pointCount) + ")"
                                    color: isCurrent ? "#7dd3fc" : "#94a3b8"
                                    font.pixelSize: 10
                                    font.family: "Monospace"
                                }
                            }
                        }
                    }
                }

                // Milestone 10: Multi-Node View Switcher (Independent coordinate frames, unaligned)
                RowLayout {
                    visible: bridge.isMultiNodeSessionActive
                    spacing: 6

                    Rectangle {
                        color: "#082f49"
                        radius: 4
                        border.color: "#0284c7"
                        implicitHeight: 24
                        implicitWidth: mnRoleText.contentWidth + 12
                        Text {
                            id: mnRoleText
                            anchors.centerIn: parent
                            text: "MESH: " + bridge.nodeRole + " | NON-FUSED"
                            color: "#38bdf8"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Repeater {
                        model: bridge.sessionMembers
                        delegate: Rectangle {
                            id: nodeTabBtn
                            property var mData: modelData
                            property bool isCurrent: bridge.selectedNodeViewIndex === index

                            implicitHeight: 28
                            implicitWidth: nodeTabBtnRow.implicitWidth + 16
                            radius: 5
                            color: isCurrent ? "#0c4a6e" : (nodeTabMa.containsMouse ? "#1e293b" : "#0f172a")
                            border.color: isCurrent ? "#38bdf8" : (nodeTabMa.containsMouse ? "#64748b" : "#334155")
                            border.width: isCurrent ? 2 : 1

                            MouseArea {
                                id: nodeTabMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    bridge.selectNodeView(index)
                                }
                            }

                            RowLayout {
                                id: nodeTabBtnRow
                                anchors.centerIn: parent
                                spacing: 5

                                Text {
                                    text: mData.isCoordinator ? "👑" : "💻"
                                    font.pixelSize: 11
                                }

                                Text {
                                    text: mData.displayName
                                    color: isCurrent ? "#ffffff" : (nodeTabMa.containsMouse ? "#e2e8f0" : "#cbd5e1")
                                    font.pixelSize: 10
                                    font.bold: isCurrent
                                }

                                Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: mData.connected ? "#10b981" : "#ef4444"
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    color: "#1e2532"
                    radius: 4
                    height: 24
                    implicitWidth: points3dLabel.contentWidth + 14
                    Text {
                        id: points3dLabel
                        anchors.centerIn: parent
                        text: bridge.isDualSensorMode ?
                              ("Total Points: " + bridge.pointsCaptured) :
                              (bridge.isCapturing ? (bridge.pointsCaptured + " Points (Real)") : "Common AV Map")
                        color: "#94a3b8"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            // Milestone 9: Registration Active In-Viewport HUD Badge
            Rectangle {
                id: regViewportHud
                visible: bridge.isRegistrationActive
                z: 20
                anchors {
                    top: parent.top
                    topMargin: 46
                    left: parent.left
                    leftMargin: 12
                }
                color: "#0a192f"
                border.color: "#00e5ff"
                border.width: 1.5
                radius: 6
                implicitHeight: regHudRow.implicitHeight + 12
                implicitWidth: regHudRow.implicitWidth + 20

                RowLayout {
                    id: regHudRow
                    anchors.centerIn: parent
                    spacing: 12

                    RowLayout {
                        spacing: 6
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: "#00e5ff"
                        }
                        Text {
                            text: "REGISTRATION INSPECTION ACTIVE"
                            color: "#00e5ff"
                            font.bold: true
                            font.pixelSize: 10
                            font.letterSpacing: 0.8
                        }
                    }

                    Rectangle { width: 1; height: 16; color: "#1e3a5f" }

                    Text {
                        text: "Overlap: " + bridge.registrationOverlapPercent.toFixed(1) + "%"
                        color: "#38bdf8"
                        font.bold: true
                        font.pixelSize: 10
                    }

                    Text {
                        text: "RMSE: " + bridge.registrationRmseMm.toFixed(1) + " mm"
                        color: "#a7f3d0"
                        font.bold: true
                        font.pixelSize: 10
                    }

                    Rectangle { width: 1; height: 16; color: "#1e3a5f" }

                    // View Mode Toggle Pills
                    RowLayout {
                        spacing: 4
                        Repeater {
                            model: [
                                { label: "Combined", mode: 0 },
                                { label: "Ref (Airy)", mode: 1 },
                                { label: "Mov (Gemini)", mode: 2 }
                            ]
                            delegate: Rectangle {
                                property bool isSel: bridge.registrationViewMode === modelData.mode
                                implicitWidth: vText.contentWidth + 12
                                implicitHeight: 22
                                radius: 4
                                color: isSel ? "#0284c7" : "#0f172a"
                                border.color: isSel ? "#38bdf8" : "#334155"
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        bridge.setRegistrationViewMode(modelData.mode);
                                        canvas3d.requestPaint();
                                        canvas2d.requestPaint();
                                    }
                                }
                                Text {
                                    id: vText
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: isSel ? "white" : "#94a3b8"
                                    font.pixelSize: 9
                                    font.bold: isSel
                                }
                            }
                        }
                    }

                    Rectangle { width: 1; height: 16; color: "#1e3a5f" }

                    // Color Differentiation Schemes: True RGB vs Cyan/Amber Diagnostic
                    RowLayout {
                        spacing: 4
                        Rectangle {
                            implicitWidth: rgbBtnText.contentWidth + 14
                            implicitHeight: 22
                            radius: 4
                            color: bridge.registrationColorMode === 1 ? "#0f766e" : "#0f172a"
                            border.color: bridge.registrationColorMode === 1 ? "#14b8a6" : "#334155"
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    bridge.setRegistrationColorMode(1);
                                    canvas3d.requestPaint();
                                    canvas2d.requestPaint();
                                }
                            }
                            Text {
                                id: rgbBtnText
                                anchors.centerIn: parent
                                text: "📷 True Merged RGB"
                                color: bridge.registrationColorMode === 1 ? "#5eead4" : "#94a3b8"
                                font.pixelSize: 9
                                font.bold: bridge.registrationColorMode === 1
                            }
                        }

                        Rectangle {
                            implicitWidth: diffBtnText.contentWidth + 14
                            implicitHeight: 22
                            radius: 4
                            color: bridge.registrationColorMode === 0 ? "#7c2d12" : "#0f172a"
                            border.color: bridge.registrationColorMode === 0 ? "#f97316" : "#334155"
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    bridge.setRegistrationColorMode(0);
                                    canvas3d.requestPaint();
                                    canvas2d.requestPaint();
                                }
                            }
                            Text {
                                id: diffBtnText
                                anchors.centerIn: parent
                                text: "🎨 Cyan / Amber Diagnostic"
                                color: bridge.registrationColorMode === 0 ? "#fed7aa" : "#94a3b8"
                                font.pixelSize: 9
                                font.bold: bridge.registrationColorMode === 0
                            }
                        }
                    }

                    // Exit Inspection Button
                    Rectangle {
                        implicitWidth: 22
                        implicitHeight: 22
                        radius: 4
                        color: "#7f1d1d"
                        border.color: "#ef4444"
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                bridge.clearActiveRegistration();
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "white"
                            font.pixelSize: 10
                            font.bold: true
                        }
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
                    function onMapDataUpdated() { canvas3d.requestPaint(); }
                    function onPoseChanged() { canvas3d.requestPaint(); }
                    function onActiveDisplaySensorChanged() { canvas3d.requestPaint(); }
                    function onActiveRegistrationChanged() { canvas3d.requestPaint(); }
                    function onRegistrationViewModeChanged() { canvas3d.requestPaint(); }
                    function onRegistrationColorModeChanged() { canvas3d.requestPaint(); }
                }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    var cx = width * 0.5;
                    var cy = height * 0.55;

                    // Camera math (Spherical coordinates to cartesian eye position in Canonical AV frame: +X Fwd, +Y Left, +Z Up)
                    var yawRad = bridge.camYaw3D * Math.PI / 180.0;
                    var pitchRad = bridge.camPitch3D * Math.PI / 180.0;
                    var dist = bridge.camDistance3D;

                    var eyeX = bridge.camTargetX3D - dist * Math.cos(pitchRad) * Math.cos(yawRad);
                    var eyeY = bridge.camTargetY3D - dist * Math.cos(pitchRad) * Math.sin(yawRad);
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

                    // 4. Draw Real 3D Trajectory Path
                    var traj = bridge.getTrajectoryPoints();
                    if (traj.length > 1) {
                        ctx.strokeStyle = "#f59e0b";
                        ctx.lineWidth = 2.5;
                        ctx.beginPath();
                        var first = true;
                        for (var ti = 0; ti < traj.length; ++ti) {
                            var ptT = project(traj[ti][0], traj[ti][1], traj[ti][2]);
                            if (ptT) {
                                if (first) {
                                    ctx.moveTo(ptT.x, ptT.y);
                                    first = false;
                                } else {
                                    ctx.lineTo(ptT.x, ptT.y);
                                }
                            }
                        }
                        ctx.stroke();
                    }

                    // 5. Draw Real Point Cloud Points (Real RGB or Neutral Grayscale Intensity)
                    var pts = bridge.getDisplayPoints(30000);
                    if (pts.length > 0) {
                        for (var pi = 0; pi < pts.length; ++pi) {
                            var pt = pts[pi];
                            var p3 = project(pt[0], pt[1], pt[2]);
                            if (p3) {
                                if (pt.length >= 8 && pt[7] === true) {
                                    ctx.fillStyle = "rgb(" + pt[4] + "," + pt[5] + "," + pt[6] + ")";
                                } else {
                                    // True neutral grayscale intensity visualization for geometry (Milestone 5):
                                    // Pure monochrome surface reflectance (R=G=B) with zero artificial tinting.
                                    var rawI = (pt.length >= 4 && pt[3] !== undefined) ? pt[3] : 0.8;
                                    var intVal = Math.min(255, Math.max(50, (rawI <= 1.0 ? Math.round(rawI * 255) : Math.round(rawI))));
                                    ctx.fillStyle = "rgb(" + intVal + "," + intVal + "," + intVal + ")";
                                }
                                ctx.fillRect(p3.x - 1, p3.y - 1, 2.5, 2.5);
                            }
                        }
                    }

                    // 6. Draw Scanner Pose & Heading Vector in 3D
                    var scX = bridge.currentPoseX;
                    var scY = bridge.currentPoseY;
                    var scZ = bridge.currentPoseZ;
                    var scYaw = bridge.currentYaw;
                    var ptScanner = project(scX, scY, scZ);
                    if (ptScanner) {
                        var fwdX = scX + 0.6 * Math.cos(scYaw);
                        var fwdY = scY + 0.6 * Math.sin(scYaw);
                        var ptFwd = project(fwdX, fwdY, scZ);

                        // Scanner position marker
                        ctx.fillStyle = "#38bdf8";
                        ctx.beginPath();
                        ctx.arc(ptScanner.x, ptScanner.y, 5, 0, 2 * Math.PI);
                        ctx.fill();
                        ctx.strokeStyle = "#ffffff";
                        ctx.lineWidth = 1.5;
                        ctx.stroke();

                        // Heading arrow
                        if (ptFwd) {
                            ctx.strokeStyle = "#ef4444";
                            ctx.lineWidth = 2.5;
                            ctx.beginPath();
                            ctx.moveTo(ptScanner.x, ptScanner.y);
                            ctx.lineTo(ptFwd.x, ptFwd.y);
                            ctx.stroke();
                        }
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
                visible: bridge.pointsCaptured === 0 && !bridge.hasMapData

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
                    text: bridge.isCapturing ? ("Spatial Session Active (Ingesting " + bridge.selectedScannerName + ")") : "AV Common Map Viewport"
                    color: "#e2e8f0"
                    font.bold: true
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    Layout.alignment: Qt.AlignHCenter
                }
                Text {
                    text: "Hardware Pipeline: " + bridge.selectedScannerName + " (" + bridge.sensorStatus + ")"
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
                    function onPointsCapturedChanged() { canvas2d.requestPaint(); }
                    function onMapDataUpdated() { canvas2d.requestPaint(); }
                    function onPoseChanged() { canvas2d.requestPaint(); }
                    function onActiveDisplaySensorChanged() { canvas2d.requestPaint(); }
                    function onActiveRegistrationChanged() { canvas2d.requestPaint(); }
                    function onRegistrationViewModeChanged() { canvas2d.requestPaint(); }
                    function onRegistrationColorModeChanged() { canvas2d.requestPaint(); }
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
                            x: cx - (wy - panY) * zoom, // Canonical AV: +Y (Left) moves screen Left
                            y: cy - (wx - panX) * zoom  // Canonical AV: +X (Forward) moves screen Up
                        };
                    }

                    // 1. Draw Orthogonal Metric Grid
                    // Determine grid line step based on zoom
                    var step = (zoom > 50) ? 1.0 : (zoom > 15) ? 2.0 : 5.0;

                    var minWorldX = panX - (cy / zoom) - step;
                    var maxWorldX = panX + (cy / zoom) + step;
                    var minWorldY = panY - (cx / zoom) - step;
                    var maxWorldY = panY + (cx / zoom) + step;

                    var startX = Math.floor(minWorldX / step) * step;
                    var startY = Math.floor(minWorldY / step) * step;

                    ctx.lineWidth = 1;
                    ctx.font = "9px monospace";

                    // Constant X lines (horizontal across screen)
                    for (var x = startX; x <= maxWorldX; x += step) {
                        var pLeft = worldToScreen(x, maxWorldY);
                        var pRight = worldToScreen(x, minWorldY);
                        var isZeroX = Math.abs(x) < 0.001;

                        ctx.strokeStyle = isZeroX ? "#ef4444" : ((Math.round(x) % 5 === 0) ? "#263244" : "#1a212d");
                        ctx.beginPath();
                        ctx.moveTo(pLeft.x, pLeft.y);
                        ctx.lineTo(pRight.x, pRight.y);
                        ctx.stroke();

                        // Label
                        if (Math.round(x) % 5 === 0 || isZeroX) {
                            ctx.fillStyle = isZeroX ? "#ef4444" : "#475569";
                            ctx.fillText(x.toFixed(0) + "m", cx + 6, pLeft.y - 3);
                        }
                    }

                    // Constant Y lines (vertical down screen)
                    for (var y = startY; y <= maxWorldY; y += step) {
                        var pTop = worldToScreen(maxWorldX, y);
                        var pBottom = worldToScreen(minWorldX, y);
                        var isZeroY = Math.abs(y) < 0.001;

                        ctx.strokeStyle = isZeroY ? "#10b981" : ((Math.round(y) % 5 === 0) ? "#263244" : "#1a212d");
                        ctx.beginPath();
                        ctx.moveTo(pTop.x, pTop.y);
                        ctx.lineTo(pBottom.x, pBottom.y);
                        ctx.stroke();

                        // Label
                        if (Math.round(y) % 5 === 0 || isZeroY) {
                            ctx.fillStyle = isZeroY ? "#10b981" : "#475569";
                            ctx.fillText(y.toFixed(0) + "m", pBottom.x + 3, cy + 12);
                        }
                    }

                    // 2. Origin Crosshair
                    var orig = worldToScreen(0, 0);
                    ctx.strokeStyle = "#f59e0b";
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.arc(orig.x, orig.y, 4, 0, 2 * Math.PI);
                    ctx.stroke();

                    // 3. Direction Compass (Top-Right: +X Forward is UP)
                    var compX = width - 42;
                    var compY = 56;
                    ctx.fillStyle = "#1e293b";
                    ctx.beginPath();
                    ctx.arc(compX, compY, 18, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.strokeStyle = "#38bdf8";
                    ctx.lineWidth = 1.5;
                    ctx.stroke();

                    // Forward Arrow pointing UP (+X)
                    ctx.fillStyle = "#ef4444";
                    ctx.beginPath();
                    ctx.moveTo(compX, compY - 14);
                    ctx.lineTo(compX - 5, compY + 4);
                    ctx.lineTo(compX + 5, compY + 4);
                    ctx.closePath();
                    ctx.fill();

                    ctx.fillStyle = "white";
                    ctx.font = "bold 9px sans-serif";
                    ctx.fillText("+X", compX - 6.5, compY - 16);

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

                    // 5. Draw Real 2D Trajectory Path
                    var traj2d = bridge.getTrajectoryPoints();
                    if (traj2d.length > 1) {
                        ctx.strokeStyle = "#f59e0b";
                        ctx.lineWidth = 2.5;
                        ctx.beginPath();
                        for (var tj = 0; tj < traj2d.length; ++tj) {
                            var sP = worldToScreen(traj2d[tj][0], traj2d[tj][1]);
                            if (tj === 0) ctx.moveTo(sP.x, sP.y);
                            else ctx.lineTo(sP.x, sP.y);
                        }
                        ctx.stroke();
                    }

                    // 6. Draw Real 2D Point Cloud (Floor Plan Projection with Matching Neutral Grayscale)
                    var pts2d = bridge.getDisplayPoints(30000);
                    if (pts2d.length > 0) {
                        for (var pk = 0; pk < pts2d.length; ++pk) {
                            var p2 = pts2d[pk];
                            var sPt = worldToScreen(p2[0], p2[1]);
                            if (p2.length >= 8 && p2[7] === true) {
                                ctx.fillStyle = "rgb(" + p2[4] + "," + p2[5] + "," + p2[6] + ")";
                            } else {
                                // Match 3D viewport: pure monochrome grayscale surface reflectance
                                var rawI2 = (p2.length >= 4 && p2[3] !== undefined) ? p2[3] : 0.8;
                                var intVal2 = Math.min(255, Math.max(50, (rawI2 <= 1.0 ? Math.round(rawI2 * 255) : Math.round(rawI2))));
                                ctx.fillStyle = "rgb(" + intVal2 + "," + intVal2 + "," + intVal2 + ")";
                            }
                            ctx.fillRect(sPt.x - 1, sPt.y - 1, 2, 2);
                        }
                    }

                    // 7. Draw Scanner 2D Pose & Directional Heading Cone
                    var sc2X = bridge.currentPoseX;
                    var sc2Y = bridge.currentPoseY;
                    var sc2Yaw = bridge.currentYaw;
                    var sScanner = worldToScreen(sc2X, sc2Y);

                    // Heading Cone (Forward field of view: heading 0 points UP (+X))
                    var coneLen = 22; // pixels
                    var coneAngle = 0.45; // ~26 deg
                    var tipX = sScanner.x - coneLen * Math.sin(sc2Yaw);
                    var tipY = sScanner.y - coneLen * Math.cos(sc2Yaw);
                    var leftX = sScanner.x - (coneLen * 0.7) * Math.sin(sc2Yaw + coneAngle);
                    var leftY = sScanner.y - (coneLen * 0.7) * Math.cos(sc2Yaw + coneAngle);
                    var rightX = sScanner.x - (coneLen * 0.7) * Math.sin(sc2Yaw - coneAngle);
                    var rightY = sScanner.y - (coneLen * 0.7) * Math.cos(sc2Yaw - coneAngle);

                    ctx.fillStyle = "rgba(56, 189, 248, 0.35)";
                    ctx.beginPath();
                    ctx.moveTo(sScanner.x, sScanner.y);
                    ctx.lineTo(leftX, leftY);
                    ctx.lineTo(tipX, tipY);
                    ctx.lineTo(rightX, rightY);
                    ctx.closePath();
                    ctx.fill();

                    // Scanner Center Dot
                    ctx.fillStyle = "#38bdf8";
                    ctx.beginPath();
                    ctx.arc(sScanner.x, sScanner.y, 4.5, 0, 2 * Math.PI);
                    ctx.fill();
                    ctx.strokeStyle = "#ffffff";
                    ctx.lineWidth = 1.5;
                    ctx.stroke();
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
    // 3. SENSOR TELEMETRY & SYSTEM METRICS BAR (ABOVE CONFIG & CONTROLS)
    // ==========================================
    Rectangle {
        id: sensorDataBar
        height: 64
        color: "#12151c"
        anchors {
            bottom: bottomBar.top
            left: parent.left
            right: parent.right
        }

        // Top border line
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: "#1e2430"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 16

            // Section Label Badge
            Rectangle {
                implicitWidth: 104
                implicitHeight: 28
                radius: 4
                color: "#1a202c"
                border.color: "#2d3748"
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "LIVE SENSORS"
                        color: "#38bdf8"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                }
            }

            Rectangle {
                width: 1
                height: 38
                color: "#232834"
            }

            // Multi-Sensor Mode: All active sensor cards side-by-side
            RowLayout {
                visible: bridge.activeSensorCount > 1
                spacing: 16
                Layout.fillWidth: true

                Repeater {
                    model: bridge.activeSensors
                    delegate: RowLayout {
                        property var sData: modelData
                        property bool isCurrent: bridge.activeDisplaySensor === sData.id
                        spacing: 16

                        Rectangle {
                            radius: 6
                            color: isCurrent ? "#1e293b" : (cardMa.containsMouse ? "#141c28" : "transparent")
                            border.color: isCurrent ? "#38bdf8" : (cardMa.containsMouse ? "#334155" : "transparent")
                            border.width: isCurrent ? 2 : 1
                            implicitWidth: cardCol.implicitWidth + 16
                            implicitHeight: 46

                            MouseArea {
                                id: cardMa
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onPressed: {
                                    bridge.setActiveDisplaySensor(sData.id)
                                    canvas3d.requestPaint()
                                    canvas2d.requestPaint()
                                }
                            }

                            ColumnLayout {
                                id: cardCol
                                anchors.centerIn: parent
                                spacing: 3
                                RowLayout {
                                    spacing: 6
                                    Text {
                                        text: (sData.icon ? (sData.icon + " ") : "") + sData.name.toUpperCase()
                                        color: sData.sensorType === "LIDAR" ? "#38bdf8" : (sData.id === "GEMINI" ? "#c084fc" : (sData.id === "CAMERA" ? "#f59e0b" : "#2dd4bf"))
                                        font.pixelSize: 11
                                        font.bold: true
                                        font.letterSpacing: 0.8
                                    }
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: {
                                            var trk = getSensorTracking(sData.id, sData);
                                            return (trk === "TRACKING_OK" || trk === "RUNNING") ? "#10b981" :
                                                   (trk === "INITIALIZING") ? "#f59e0b" : "#64748b";
                                        }
                                    }
                                    Text {
                                        text: getSensorAcquisition(sData.id, sData) + " | " + getSensorTracking(sData.id, sData)
                                        color: "#f1f5f9"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                                Text {
                                    text: sData.isCapturable ? (getSensorAcqRate(sData.id).toFixed(1) + " Hz acq  ·  " +
                                                                getSensorSlamRate(sData.id).toFixed(1) + " Hz SLAM  ·  " +
                                                                getSensorPoints(sData.id, sData.pointCount) + " pts") :
                                                               (sData.transport + "  ·  Backend: " + (sData.backendName || "None"))
                                    color: "#cbd5e1"
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                }
                                Text {
                                    visible: sData.scaleStatus && sData.scaleStatus !== "METRIC_VERIFIED"
                                    text: "Scale: " + sData.scaleStatus
                                    color: (sData.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || sData.scaleStatus.indexOf("NONE") !== -1) ? "#94a3b8" : "#f59e0b"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                                RowLayout {
                                    visible: sData.id === "CAMERA"
                                    spacing: 4
                                    Text {
                                        text: "Recon Backend:"
                                        color: "#94a3b8"
                                        font.pixelSize: 10
                                    }
                                    Rectangle {
                                        implicitWidth: 65
                                        implicitHeight: 18
                                        radius: 3
                                        color: bridge.selectedCameraBackend === "LingBotStreamingBackend" ? "#047857" : "#1e293b"
                                        border.color: bridge.selectedCameraBackend === "LingBotStreamingBackend" ? "#10b981" : "#334155"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "LingBot"
                                            color: bridge.selectedCameraBackend === "LingBotStreamingBackend" ? "#ffffff" : "#94a3b8"
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: bridge.selectCameraBackend("LingBotStreamingBackend")
                                        }
                                    }
                                    Rectangle {
                                        implicitWidth: 80
                                        implicitHeight: 18
                                        radius: 3
                                        color: bridge.selectedCameraBackend === "ABotStreamingBackend" ? "#b45309" : "#1e293b"
                                        border.color: bridge.selectedCameraBackend === "ABotStreamingBackend" ? "#f59e0b" : "#334155"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "ABot (Exp)"
                                            color: bridge.selectedCameraBackend === "ABotStreamingBackend" ? "#ffffff" : "#94a3b8"
                                            font.pixelSize: 9
                                            font.bold: true
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: bridge.selectCameraBackend("ABotStreamingBackend")
                                        }
                                    }
                                }
                                Text {
                                    visible: sData.id === "CAMERA" && bridge.selectedCameraBackend === "ABotStreamingBackend"
                                    text: "⚠️ RESEARCH / EXPERIMENTAL (NON-COMMERCIAL)"
                                    color: "#f59e0b"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            width: 1
                            height: 38
                            color: "#232834"
                        }
                    }
                }

                // Extrinsics Status Pill Badge
                Rectangle {
                    implicitHeight: 28
                    implicitWidth: extrinsicsPillText.contentWidth + 16
                    radius: 4
                    color: "#0f172a"
                    border.color: "#334155"
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            id: extrinsicsPillText
                            text: "📐 Extrinsics: UNKNOWN (Independent SLAM)"
                            color: "#f59e0b"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // System Platform Metrics (CPU / GPU / RAM)
                ColumnLayout {
                    spacing: 3
                    Text {
                        text: "⚡ JETSON ORIN TELEMETRY"
                        color: "#22c55e"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Text {
                        text: "CPU: " + bridge.hostCpuUtil.toFixed(1) + "%  |  GPU: " + bridge.hostGpuUtil.toFixed(1) + "%  |  RAM: " + bridge.hostMemory
                        color: "#f1f5f9"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }

            // Single Sensor Mode: Single sensor status + tracking state + Jetson
            RowLayout {
                visible: bridge.activeSensorCount <= 1
                spacing: 16
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 3
                    Text {
                        text: "SENSOR STATUS"
                        color: "#64748b"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    RowLayout {
                        spacing: 6
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.isCapturing ? "#10b981" : 
                                   bridge.sensorStatus === "READY" ? "#3b82f6" : "#64748b"
                        }
                        Text {
                            text: bridge.sensorStatus
                            color: "#f1f5f9"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: "· " + bridge.selectedScannerDetails
                            color: "#94a3b8"
                            font.pixelSize: 10
                        }
                    }
                }

                Rectangle {
                    width: 1
                    height: 38
                    color: "#232834"
                }

                ColumnLayout {
                    spacing: 3
                    Text {
                        text: "TRACKING STATE"
                        color: "#64748b"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    RowLayout {
                        spacing: 6
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: bridge.trackingStatus === "TRACKING_OK" ? "#10b981" : 
                                   bridge.trackingStatus === "READY" ? "#3b82f6" : "#64748b"
                        }
                        Text {
                            text: bridge.trackingStatus
                            color: "#f1f5f9"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                ColumnLayout {
                    spacing: 3
                    Text {
                        text: "⚡ JETSON ORIN TELEMETRY"
                        color: "#22c55e"
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 0.8
                    }
                    Text {
                        text: "CPU: " + bridge.hostCpuUtil.toFixed(1) + "%  |  GPU: " + bridge.hostGpuUtil.toFixed(1) + "%  |  RAM: " + bridge.hostMemory
                        color: "#f1f5f9"
                        font.pixelSize: 11
                        font.bold: true
                        font.family: "Monospace"
                    }
                }
            }
        }
    }

    // ==========================================
    // 4. BOTTOM CONFIGURATION & CONTROL BAR (START / STOP / SAVE)
    // ==========================================
    Rectangle {
        id: bottomBar
        height: 64
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

        // Right-Pinned Primary Action Controls & Capture Duration (Always 100% visible, cannot be pushed off)
        RowLayout {
            id: bottomBarActions
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            // Elapsed Capture Timer
            ColumnLayout {
                spacing: 2
                Text {
                    text: "CAPTURE TIME"
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

            Rectangle {
                width: 1
                height: 40
                color: "#28303e"
            }

            // Primary Action Buttons: START, STOP, SAVE
            RowLayout {
                spacing: 8

                Button {
                    id: startBtn
                    text: "START"
                    enabled: !bridge.isCapturing && (bridge.canStart || (bridge.activeSensors.length > 0 && bridge.activeSensors[0].isCapturable))
                    implicitWidth: 105
                    implicitHeight: 44
                    ToolTip.visible: hovered && !enabled
                    ToolTip.delay: 300
                    ToolTip.text: bridge.isCapturing ? "Capture is actively recording" :
                                  (bridge.activeSensors.length === 0 ? "No active sensor selected" :
                                  "Selected sensor not available for spatial capture")
                    onClicked: {
                        if (bridge.projectId === "") {
                            var dateStr = Qt.formatDateTime(new Date(), "yyyyMMdd_hhmmss");
                            bridge.createProject("Scan_" + dateStr, "Autonomous capture session");
                        }
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
                    implicitWidth: 105
                    implicitHeight: 44
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
                    implicitWidth: 105
                    implicitHeight: 44
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

        // Left / Center Configuration & Scanner Selection
        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: bottomBarActions.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Text {
                text: "CONFIGURATION:"
                color: "#94a3b8"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 0.8
            }

            ComboBox {
                id: scannerCombo
                model: bridge.availableScanners
                currentIndex: bridge.selectedScannerIndex
                onActivated: bridge.selectScannerByIndex(currentIndex)
                background: Rectangle {
                    implicitWidth: 260
                    implicitHeight: 38
                    color: "#1c222c"
                    border.color: "#334155"
                    radius: 6
                }
                contentItem: Text {
                    leftPadding: 12
                    rightPadding: 24
                    text: scannerCombo.displayText
                    color: "#f8fafc"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Button {
                id: mainCreateProfileBtn
                text: "⚡ Create Profile"
                implicitHeight: 38
                implicitWidth: 140
                onClicked: {
                    configureSensorsDialog.currentTab = 1;
                    configureSensorsDialog.open();
                    configureSensorsDialog.refreshDiscovery();
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? "#0284c7" : "#0369a1"
                    radius: 6
                    border.color: "#38bdf8"
                    border.width: 1.5
                }
            }

            Button {
                text: "⚙️ Configure Sensors"
                implicitHeight: 38
                implicitWidth: 150
                onClicked: {
                    configureSensorsDialog.currentTab = 1;
                    configureSensorsDialog.open();
                }
                contentItem: Text {
                    text: parent.text
                    color: "#38bdf8"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? "#1e293b" : "#0f172a"
                    radius: 6
                    border.color: "#38bdf8"
                }
            }

            Button {
                id: mainCatalogBtn
                text: "📚 Sensor Catalog"
                implicitHeight: 38
                implicitWidth: 145
                onClicked: {
                    configureSensorsDialog.currentTab = 2;
                    configureSensorsDialog.open();
                    configureSensorsDialog.refreshCatalog();
                }
                contentItem: Text {
                    text: parent.text
                    color: "#38bdf8"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? "#1e293b" : "#0f172a"
                    radius: 6
                    border.color: "#38bdf8"
                }
            }

            Button {
                id: orientationDiagBtn
                text: "📐 Orientation Diagnostic"
                implicitHeight: 38
                implicitWidth: 175
                onClicked: {
                    bridge.openOrientationDiagnostic();
                    orientationDiagDialog.open();
                }
                contentItem: Text {
                    text: parent.text
                    color: "#c084fc"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: orientationDiagBtn.hovered ? "#3b0764" : "#1e1b4b"
                    radius: 6
                    border.color: "#a855f7"
                }
            }

            // Neural Reconstruction Backend Selector (Milestone 14.6)
            RowLayout {
                visible: bridge.selectedScannerName.indexOf("Camera") !== -1 || bridge.selectedScannerName.indexOf("48MP") !== -1
                spacing: 6
                Text {
                    text: "Recon Engine:"
                    color: "#94a3b8"
                    font.pixelSize: 11
                    font.bold: true
                }
                ComboBox {
                    id: cameraBackendCombo
                    implicitHeight: 38
                    implicitWidth: 200
                    model: [
                        "LingBot-Map (Commercial)",
                        "ABot-Recon (Experimental)"
                    ]
                    currentIndex: bridge.selectedCameraBackend === "ABotStreamingBackend" ? 1 : 0
                    onActivated: (index) => {
                        if (index === 1) {
                            bridge.selectCameraBackend("ABotStreamingBackend");
                        } else {
                            bridge.selectCameraBackend("LingBotStreamingBackend");
                        }
                    }
                    contentItem: Text {
                        text: cameraBackendCombo.displayText
                        color: cameraBackendCombo.currentIndex === 1 ? "#f59e0b" : "#10b981"
                        font.pixelSize: 11
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    background: Rectangle {
                        color: "#0f172a"
                        radius: 6
                        border.color: cameraBackendCombo.currentIndex === 1 ? "#f59e0b" : "#10b981"
                    }
                }
            }

            Text {
                text: bridge.selectedScannerValidationNotice
                color: bridge.selectedScannerStatus === "CONFIG" ? "#38bdf8" : "#f59e0b"
                font.pixelSize: 11
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
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
        width: 620
        height: 440
        standardButtons: Dialog.Cancel

        onOpened: {
            bridge.refreshData();
        }

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
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Select a project from the repository:"
                    color: "#94a3b8"
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: "Sort:"
                        color: "#64748b"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Button {
                        text: "Date ↓"
                        implicitHeight: 26
                        implicitWidth: 64
                        background: Rectangle {
                            color: bridge.projectSortMode === 0 ? "#0f766e" : (parent.hovered ? "#334155" : "#1e293b")
                            border.color: bridge.projectSortMode === 0 ? "#14b8a6" : "#475569"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 0 ? "#ffffff" : "#cbd5e1"
                            font.bold: bridge.projectSortMode === 0
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: bridge.setProjectSortMode(0)
                    }

                    Button {
                        text: "Date ↑"
                        implicitHeight: 26
                        implicitWidth: 64
                        background: Rectangle {
                            color: bridge.projectSortMode === 1 ? "#0f766e" : (parent.hovered ? "#334155" : "#1e293b")
                            border.color: bridge.projectSortMode === 1 ? "#14b8a6" : "#475569"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 1 ? "#ffffff" : "#cbd5e1"
                            font.bold: bridge.projectSortMode === 1
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: bridge.setProjectSortMode(1)
                    }

                    Button {
                        text: "Name A-Z"
                        implicitHeight: 26
                        implicitWidth: 72
                        background: Rectangle {
                            color: bridge.projectSortMode === 2 ? "#0f766e" : (parent.hovered ? "#334155" : "#1e293b")
                            border.color: bridge.projectSortMode === 2 ? "#14b8a6" : "#475569"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 2 ? "#ffffff" : "#cbd5e1"
                            font.bold: bridge.projectSortMode === 2
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: bridge.setProjectSortMode(2)
                    }

                    Button {
                        text: "Name Z-A"
                        implicitHeight: 26
                        implicitWidth: 72
                        background: Rectangle {
                            color: bridge.projectSortMode === 3 ? "#0f766e" : (parent.hovered ? "#334155" : "#1e293b")
                            border.color: bridge.projectSortMode === 3 ? "#14b8a6" : "#475569"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 3 ? "#ffffff" : "#cbd5e1"
                            font.bold: bridge.projectSortMode === 3
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: bridge.setProjectSortMode(3)
                    }
                }
            }

            ListView {
                id: projListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: bridge.projectList

                delegate: Rectangle {
                    width: projListView.width
                    height: 64
                    radius: 6
                    color: itemMouseArea.containsMouse ? "#273244" : "#1e2634"
                    border.color: itemMouseArea.containsMouse ? "#3b82f6" : "#334155"

                    MouseArea {
                        id: itemMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onDoubleClicked: {
                            bridge.openProject(modelData.id);
                            openProjectDialog.close();
                            canvas3d.requestPaint();
                            canvas2d.requestPaint();
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 3

                            RowLayout {
                                spacing: 8
                                Text {
                                    text: modelData.name
                                    color: "#f8fafc"
                                    font.bold: true
                                    font.pixelSize: 13
                                }
                                Text {
                                    text: "• " + modelData.id + " • " + modelData.date
                                    color: "#64748b"
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                text: (modelData.description && modelData.description.trim() !== "") ? modelData.description : "No description"
                                color: (modelData.description && modelData.description.trim() !== "") ? "#94a3b8" : "#475569"
                                font.pixelSize: 11
                                font.italic: !(modelData.description && modelData.description.trim() !== "")
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        Button {
                            text: "Load"
                            z: 2
                            implicitHeight: 32
                            implicitWidth: 68
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                bridge.openProject(modelData.id);
                                openProjectDialog.close();
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: "white"; font.bold: true; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: parent.hovered ? "#0284c7" : "#0369a1"; radius: 4 }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                }
            }
        }
    }

    // Dialog: Configure Sensors & Device Discovery (Milestone 14 Autonomous Onboarding)
    Dialog {
        id: configureSensorsDialog
        title: "Hardware & Scanner Configuration"
        modal: true
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: parent
        width: 1160
        height: 760
        padding: 0
        topPadding: 12
        bottomPadding: 12
        leftPadding: 20
        rightPadding: 20

        property int currentTab: 1 // Default to 1: Hardware Discovery
        property var availableProfiles: []
        property var selectedProfileMap: ({})
        property var discoveredDevices: bridge.discoveredPhysicalDevices
        property bool isProfiling: false
        property string profilingStep: ""
        property int profilingProgress: 0
        property bool profileResultVisible: false
        property var profileResult: ({})
        property string profilingDeviceId: ""
        property string catalogSelectedVendor: "All"
        property string catalogSelectedType: "All"
        property string catalogSearchQuery: ""
        property var catalogModels: []
        property string catalogSelectedModelId: ""
        property var catalogSelectedModelDetails: ({})
        property string catalogNotification: ""
        property bool catalogNotificationSuccess: true

        function refreshCatalog() {
            catalogModels = bridge.getEcosystemModels(catalogSelectedVendor, catalogSelectedType, catalogSearchQuery);
            if (catalogModels && catalogModels.length > 0) {
                var found = false;
                if (catalogSelectedModelId !== "") {
                    for (var i = 0; i < catalogModels.length; ++i) {
                        if (catalogModels[i].id === catalogSelectedModelId) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found) {
                    catalogSelectedModelId = catalogModels[0].id;
                }
                catalogSelectedModelDetails = bridge.getModelDetails(catalogSelectedModelId);
            } else {
                catalogSelectedModelId = "";
                catalogSelectedModelDetails = ({});
            }
        }

        background: Rectangle {
            color: "#161b24"
            radius: 12
            border.color: "#334155"
            border.width: 1
        }

        header: Rectangle {
            height: 56
            color: "#1e2838"
            radius: 12
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#334155"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 14

                Text {
                    text: "Hardware & Scanner Configuration"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 15
                }

                // Tab Switcher
                RowLayout {
                    spacing: 8
                    Rectangle {
                        implicitHeight: 32
                        implicitWidth: 160
                        radius: 6
                        color: configureSensorsDialog.currentTab === 0 ? "#0284c7" : "#0f172a"
                        border.color: configureSensorsDialog.currentTab === 0 ? "#38bdf8" : "#334155"
                        Text {
                            anchors.centerIn: parent
                            text: "📦 Sensor Composition"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: configureSensorsDialog.currentTab = 0
                        }
                    }

                    Rectangle {
                        implicitHeight: 32
                        implicitWidth: 160
                        radius: 6
                        color: configureSensorsDialog.currentTab === 1 ? "#0284c7" : "#0f172a"
                        border.color: configureSensorsDialog.currentTab === 1 ? "#38bdf8" : "#334155"
                        Text {
                            anchors.centerIn: parent
                            text: "🔍 Discover Devices"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 11
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                configureSensorsDialog.currentTab = 1;
                                configureSensorsDialog.refreshDiscovery();
                            }
                        }
                    }
                    Rectangle {
                        implicitHeight: 32
                        implicitWidth: 170
                        radius: 6
                        color: configureSensorsDialog.currentTab === 2 ? "#0284c7" : "#0f172a"
                        border.color: configureSensorsDialog.currentTab === 2 ? "#38bdf8" : "#334155"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: "📚 Sensor Catalog"
                                color: "white"
                                font.bold: true
                                font.pixelSize: 11
                            }
                            Rectangle {
                                implicitHeight: 18
                                implicitWidth: catalogBadgeText.contentWidth + 10
                                radius: 9
                                color: configureSensorsDialog.currentTab === 2 ? "#0369a1" : "#1e293b"
                                border.color: configureSensorsDialog.currentTab === 2 ? "#7dd3fc" : "#475569"
                                Text {
                                    id: catalogBadgeText
                                    anchors.centerIn: parent
                                    text: "" + bridge.ecosystemModelCount
                                    color: configureSensorsDialog.currentTab === 2 ? "#ffffff" : "#94a3b8"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                configureSensorsDialog.currentTab = 2;
                                configureSensorsDialog.refreshCatalog();
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    implicitHeight: 24
                    implicitWidth: 150
                    radius: 4
                    color: "#0f172a"
                    border.color: "#38bdf8"
                    Text {
                        anchors.centerIn: parent
                        text: "Host: scanar-01 (Linux)"
                        color: "#38bdf8"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        contentItem: Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // TAB 0: SENSOR COMPOSITION
            ColumnLayout {
                anchors.fill: parent
                visible: configureSensorsDialog.currentTab === 0
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Scanner Configuration Name:"
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "🔍 Discover Unprofiled Hardware"
                        implicitHeight: 28
                        onClicked: {
                            configureSensorsDialog.currentTab = 1;
                            configureSensorsDialog.refreshDiscovery();
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#38bdf8"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#1e293b" : "#0f172a"
                            radius: 4
                            border.color: "#38bdf8"
                        }
                    }
                }

                TextField {
                    id: customConfigNameInput
                    Layout.fillWidth: true
                    implicitHeight: 38
                    text: "RoboSense Airy + Gemini 336L + VITURE Ultra"
                    color: "white"
                    font.pixelSize: 13
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    background: Rectangle {
                        color: "#0f172a"
                        radius: 6
                        border.color: customConfigNameInput.activeFocus ? "#38bdf8" : "#334155"
                        border.width: customConfigNameInput.activeFocus ? 2 : 1
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Select Hardware Sensors to Compose:"
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: configureSensorsDialog.getSelectedIds().length + " of " + configureSensorsDialog.availableProfiles.length + " selected"
                        color: "#38bdf8"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // Sensor Cards List
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: configureSensorsDialog.availableProfiles
                            delegate: Rectangle {
                                property var prof: modelData
                                property bool isSelected: !!configureSensorsDialog.selectedProfileMap[prof.profileId]
                                Layout.fillWidth: true
                                implicitHeight: 66
                                radius: 8
                                color: isSelected ? "#1e293b" : "#111827"
                                border.color: isSelected ? "#38bdf8" : "#28303e"
                                border.width: isSelected ? 2 : 1

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: configureSensorsDialog.toggleSelection(prof.profileId)
                                }

                                RowLayout {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 16
                                    anchors.right: incExcBtn.left
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 14

                                    CheckBox {
                                        checked: isSelected
                                        onToggled: configureSensorsDialog.toggleSelection(prof.profileId)
                                    }

                                    Text {
                                        text: prof.icon || "📷"
                                        font.pixelSize: 24
                                    }

                                    ColumnLayout {
                                        spacing: 3
                                        Layout.fillWidth: true

                                        RowLayout {
                                            spacing: 8
                                            Text {
                                                text: prof.name
                                                color: "#f8fafc"
                                                font.pixelSize: 14
                                                font.bold: true
                                            }
                                            Rectangle {
                                                implicitHeight: 20
                                                implicitWidth: tagText.contentWidth + 12
                                                radius: 4
                                                color: "#0f172a"
                                                border.color: prof.tagColor || "#38bdf8"
                                                Text {
                                                    id: tagText
                                                    anchors.centerIn: parent
                                                    text: prof.tag || "SENSOR"
                                                    color: prof.tagColor || "#38bdf8"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                            Rectangle {
                                                implicitHeight: 18
                                                implicitWidth: statusText.contentWidth + 10
                                                radius: 3
                                                color: prof.status === "VALIDATED" ? "#14532d" : "#334155"
                                                Text {
                                                    id: statusText
                                                    anchors.centerIn: parent
                                                    text: prof.status
                                                    color: prof.status === "VALIDATED" ? "#4ade80" : "#94a3b8"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }
                                        }

                                        Text {
                                            text: prof.sensorType + "  ·  Serial: " + prof.serialNumber + "  ·  " + prof.transport
                                            color: "#cbd5e1"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }

                                Rectangle {
                                    id: incExcBtn
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 104
                                    height: 28
                                    radius: 4
                                    color: isSelected ? "#14532d" : "#1e293b"
                                    border.color: isSelected ? "#22c55e" : "#475569"
                                    border.width: isSelected ? 1.5 : 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: isSelected ? "✓ INCLUDED" : "○ EXCLUDED"
                                        color: isSelected ? "#4ade80" : "#94a3b8"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 38
                    color: "#1c1917"
                    radius: 6
                    border.color: "#b45309"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        Text {
                            text: "📐 Spatial Notice: Extrinsics strictly UNKNOWN — Each sensor operates an independent SLAM pipeline with uncoupled coordinate frames."
                            color: "#fbbf24"
                            font.pixelSize: 11
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            // TAB 1: DEVICE DISCOVERY & ONBOARDING
            ColumnLayout {
                anchors.fill: parent
                visible: configureSensorsDialog.currentTab === 1
                spacing: 12

                // State 1: Profiling in Progress
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: configureSensorsDialog.isProfiling
                    color: "#111827"
                    radius: 10
                    border.color: "#38bdf8"
                    border.width: 1.5

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20
                        width: parent.width - 80

                        Text {
                            text: "⚡ Autonomous Hardware Interrogation & Capability Matching"
                            color: "white"
                            font.pixelSize: 16
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Text {
                            text: "Interrogating physical device registers, streaming capabilities, and matching SLAM backends..."
                            color: "#94a3b8"
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }

                        // Progress Bar
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 14
                            color: "#0f172a"
                            radius: 7
                            border.color: "#334155"

                            Rectangle {
                                width: parent.width * (configureSensorsDialog.profilingProgress / 100.0)
                                height: parent.height
                                radius: 7
                                color: "#38bdf8"
                            }
                        }

                        Text {
                            text: configureSensorsDialog.profilingStep
                            color: "#38bdf8"
                            font.pixelSize: 14
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                // State 2: Profile Result Summary Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !configureSensorsDialog.isProfiling && configureSensorsDialog.profileResultVisible
                    color: "#111827"
                    radius: 10
                    border.color: "#22c55e"
                    border.width: 1.5

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text {
                                text: "✓ EXPERIMENTAL PROFILE GENERATED & REGISTERED"
                                color: "#4ade80"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: 110
                                radius: 4
                                color: "#14532d"
                                border.color: "#22c55e"
                                Text {
                                    anchors.centerIn: parent
                                    text: "EXPERIMENTAL"
                                    color: "#4ade80"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: "#334155"
                        }

                        // Detailed summary fields
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 20

                            Text { text: "Device:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.deviceName || ""; color: "white"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Profile ID:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.profileId || ""; color: "#38bdf8"; font.pixelSize: 12 }

                            Text { text: "Native Capabilities:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.nativeCapabilities || "Unknown"; color: "white"; font.pixelSize: 12 }

                            Text { text: "Native Depth:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeDepth ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeDepth ? "#4ade80" : "#f87171"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Native IMU:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeImu ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeImu ? "#4ade80" : "#f87171"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Native Geometry:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeGeometry ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeGeometry ? "#4ade80" : "#f87171"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Compatible Reconstruction:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.compatibleReconstruction || "UNKNOWN"; color: "#38bdf8"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Selected Backend:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.selectedBackend || "NONE"; color: "#a855f7"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Geometry:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.geometrySource || "NONE"; color: "white"; font.pixelSize: 12 }

                            Text { text: "Scale Status:"; color: "#94a3b8"; font.pixelSize: 12; font.bold: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: scaleSummText.contentWidth + 14
                                radius: 4
                                color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#1e293b" : "#451a03"
                                border.color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#475569" : "#f59e0b"
                                Text {
                                    id: scaleSummText
                                    anchors.centerIn: parent
                                    text: configureSensorsDialog.profileResult.scaleStatus || "NO_RECONSTRUCTION_BACKEND_MATCHED"
                                    color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#94a3b8" : "#fbbf24"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Button {
                                text: "← Back to Discovered Devices"
                                implicitWidth: 200
                                implicitHeight: 38
                                onClicked: configureSensorsDialog.profileResultVisible = false
                                contentItem: Text {
                                    text: parent.text
                                    color: "#cbd5e1"
                                    font.pixelSize: 12
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#334155" : "#1e293b"
                                    radius: 6
                                    border.color: "#475569"
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Button {
                                property bool isCapturableBackend: Boolean(configureSensorsDialog.profileResult && configureSensorsDialog.profileResult.selectedBackend && configureSensorsDialog.profileResult.selectedBackend !== "NONE" && configureSensorsDialog.profileResult.selectedBackend !== "")
                                text: isCapturableBackend ? "✓ Add & Activate Sensor for Capture" : "✓ Select Profile (Capture Disabled)"
                                implicitWidth: 280
                                implicitHeight: 38
                                onClicked: {
                                    var pid = configureSensorsDialog.profileResult.profileId;
                                    bridge.selectProfileForCapture(pid);
                                    configureSensorsDialog.close();
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: "white"
                                    font.pixelSize: 12
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.isCapturableBackend ? (parent.hovered ? "#15803d" : "#16a34a") : (parent.hovered ? "#334155" : "#1e293b")
                                    radius: 6
                                    border.color: parent.isCapturableBackend ? "#22c55e" : "#475569"
                                }
                            }
                        }
                    }
                }

                // State 3: Discovered Devices List
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: !configureSensorsDialog.isProfiling && !configureSensorsDialog.profileResultVisible
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            spacing: 3
                            Text {
                                text: "Discovered Physical Sensors"
                                color: "#f8fafc"
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Text {
                                text: bridge.discoveryStatusText
                                color: "#38bdf8"
                                font.pixelSize: 11
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: scanInterfaceBtn
                            text: bridge.isDiscovering ? "⏳ Scanning Interfaces..." : "🔄 Scan Interfaces"
                            enabled: !bridge.isDiscovering
                            implicitHeight: 34
                            implicitWidth: 160
                            onClicked: bridge.refreshDiscovery()
                            contentItem: Text {
                                text: scanInterfaceBtn.text
                                color: scanInterfaceBtn.enabled ? "#38bdf8" : "#64748b"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: scanInterfaceBtn.hovered && scanInterfaceBtn.enabled ? "#1e293b" : "#0f172a"
                                radius: 6
                                border.color: scanInterfaceBtn.enabled ? "#38bdf8" : "#334155"
                            }
                        }

                        Button {
                            id: quickCreateProfileBtn
                            text: "⚡ Create Profile"
                            visible: bridge.discoveredPhysicalDevices.length > 0
                            enabled: !bridge.isDiscovering
                            implicitHeight: 34
                            implicitWidth: 140
                            onClicked: {
                                for (var i = 0; i < bridge.discoveredPhysicalDevices.length; ++i) {
                                    var d = bridge.discoveredPhysicalDevices[i];
                                    if (d.profileStatus === "UNPROFILED") {
                                        configureSensorsDialog.startProfiling(d.id);
                                        return;
                                    }
                                }
                                if (bridge.discoveredPhysicalDevices.length > 0) {
                                    configureSensorsDialog.startProfiling(bridge.discoveredPhysicalDevices[0].id);
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#0284c7" : "#0369a1"
                                radius: 6
                                border.color: "#38bdf8"
                            }
                        }
                    }

                    // Empty state notification
                    Rectangle {
                        visible: bridge.discoveredPhysicalDevices.length === 0 && !bridge.isDiscovering
                        Layout.fillWidth: true
                        implicitHeight: 80
                        radius: 8
                        color: "#111827"
                        border.color: "#334155"
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "No hardware sensors currently detected."
                                color: "#94a3b8"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                text: "Connect your camera or sensor via USB/Ethernet and click 'Scan Interfaces'."
                                color: "#64748b"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        visible: bridge.discoveredPhysicalDevices.length > 0

                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                            Repeater {
                                model: bridge.discoveredPhysicalDevices
                                delegate: Rectangle {
                                    property var dev: modelData
                                    property string status: dev.profileStatus || "UNPROFILED"
                                    property bool isUnprofiled: status === "UNPROFILED"
                                    property bool isProfiled: status === "PROFILED"
                                    property bool isUnsupported: status === "UNSUPPORTED"
                                    property bool isDisconnected: status === "DISCONNECTED"

                                    Layout.fillWidth: true
                                    implicitHeight: cardInnerCol.implicitHeight + 24
                                    radius: 8
                                    color: isDisconnected ? "#0b0f17" : "#111827"
                                    border.color: isUnprofiled ? "#b45309" : (isProfiled ? "#166534" : (isUnsupported ? "#6b21a8" : "#334155"))
                                    border.width: isUnprofiled ? 1.5 : 1

                                    Component.onCompleted: {
                                        console.log("[STAGE 5 — QML RENDERING] Card rendered for device: id=" + dev.id + ", name=" + dev.name + ", status=" + status + ", primaryEndpoint=" + dev.primaryEndpoint + ", isUnprofiled=" + isUnprofiled);
                                    }

                                    ColumnLayout {
                                        id: cardInnerCol
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 14
                                        spacing: 8

                                        // Top Row: Device icon, Physical identity, Status Badge, Action Button
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 12

                                            Text {
                                                text: dev.icon || "⚙️"
                                                font.pixelSize: 26
                                            }

                                            ColumnLayout {
                                                spacing: 2
                                                Layout.fillWidth: true

                                                RowLayout {
                                                    spacing: 8
                                                    Text {
                                                        text: dev.name
                                                        color: isDisconnected ? "#94a3b8" : "#f8fafc"
                                                        font.pixelSize: 13
                                                        font.bold: true
                                                    }

                                                    // 4 Distinct Status Badges
                                                    Rectangle {
                                                        implicitHeight: 20
                                                        implicitWidth: badgeText.contentWidth + 12
                                                        radius: 4
                                                        color: isUnprofiled ? "#78350f" : (isProfiled ? "#14532d" : (isUnsupported ? "#581c87" : "#1e293b"))
                                                        border.color: isUnprofiled ? "#fbbf24" : (isProfiled ? "#22c55e" : (isUnsupported ? "#a855f7" : "#64748b"))
                                                        Text {
                                                            id: badgeText
                                                            anchors.centerIn: parent
                                                            text: isUnprofiled ? "⚠️ UNPROFILED" : (isProfiled ? "✓ PROFILED" : (isUnsupported ? "🚫 UNSUPPORTED" : "🔌 DISCONNECTED"))
                                                            color: isUnprofiled ? "#fbbf24" : (isProfiled ? "#4ade80" : (isUnsupported ? "#d8b4fe" : "#94a3b8"))
                                                            font.pixelSize: 9
                                                            font.bold: true
                                                        }
                                                    }

                                                    Text {
                                                        visible: isProfiled && dev.existingProfileId !== ""
                                                        text: "Profile: " + dev.existingProfileId
                                                        color: "#38bdf8"
                                                        font.pixelSize: 11
                                                    }
                                                }

                                                // Physical bus and identifier summary
                                                Text {
                                                    text: (dev.manufacturer !== "Generic" ? (dev.manufacturer + "  ·  ") : "") +
                                                          "Bus: " + dev.busInfo +
                                                          (dev.vendorId ? ("  ·  USB ID: " + dev.vendorId + ":" + dev.productId) : "") +
                                                          (dev.serial && dev.serial !== "N/A" ? ("  ·  SN: " + dev.serial) : "") +
                                                          "  ·  Driver: " + dev.driver
                                                    color: "#94a3b8"
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }

                                            // Action Buttons / Indicators
                                            Button {
                                                visible: isUnprofiled || isUnsupported
                                                text: "⚡ Create Profile"
                                                implicitWidth: 130
                                                implicitHeight: 34
                                                onClicked: configureSensorsDialog.startProfiling(dev.id)
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: "white"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle {
                                                    color: parent.hovered ? "#0284c7" : "#0369a1"
                                                    radius: 6
                                                    border.color: "#38bdf8"
                                                }
                                            }

                                            Button {
                                                visible: isProfiled
                                                text: "⚡ Re-create Profile"
                                                implicitWidth: 130
                                                implicitHeight: 34
                                                onClicked: configureSensorsDialog.startProfiling(dev.id)
                                                contentItem: Text {
                                                    text: parent.text
                                                    color: "#38bdf8"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle {
                                                    color: parent.hovered ? "#0c4a6e" : "#082f49"
                                                    radius: 6
                                                    border.color: "#0284c7"
                                                }
                                            }

                                            Rectangle {
                                                visible: isProfiled && dev.recommendedBackend !== "NONE" && dev.recommendedBackend !== "" && dev.compatibleReconstruction !== "UNKNOWN"
                                                implicitWidth: 110
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#0f172a"
                                                border.color: "#166534"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Ready for Capture"
                                                    color: "#4ade80"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            Rectangle {
                                                visible: isProfiled && (dev.recommendedBackend === "NONE" || dev.recommendedBackend === "" || dev.compatibleReconstruction === "UNKNOWN")
                                                implicitWidth: 240
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#1e293b"
                                                border.color: "#475569"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Not currently available for spatial capture"
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            Rectangle {
                                                visible: isDisconnected
                                                implicitWidth: 100
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#0f172a"
                                                border.color: "#334155"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Device Offline"
                                                    color: "#64748b"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            Rectangle {
                                                visible: isUnsupported
                                                implicitWidth: 130
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#1e102e"
                                                border.color: "#6b21a8"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "No Compatible Backend"
                                                    color: "#d8b4fe"
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }

                                        // Physical Device Grouping: List all sub-interfaces belonging to this physical sensor
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 6
                                            visible: dev.interfaces && dev.interfaces.length > 0

                                            Text {
                                                text: "Physical Interfaces (" + (dev.interfaces ? dev.interfaces.length : 0) + "):"
                                                color: "#64748b"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }

                                            Repeater {
                                                model: dev.interfaces || []
                                                delegate: Rectangle {
                                                    implicitHeight: 20
                                                    implicitWidth: subIfaceText.contentWidth + 12
                                                    radius: 3
                                                    color: modelData.isPrimary ? "#1e293b" : "#0f172a"
                                                    border.color: modelData.isPrimary ? "#38bdf8" : "#334155"
                                                    Text {
                                                        id: subIfaceText
                                                        anchors.centerIn: parent
                                                        text: modelData.node + " [" + modelData.type + (modelData.isPrimary ? " - Primary" : "") + "]"
                                                        color: modelData.isPrimary ? "#e2e8f0" : "#94a3b8"
                                                        font.pixelSize: 9
                                                        font.bold: modelData.isPrimary
                                                    }
                                                }
                                            }
                                        }

                                        // SLAM / Reconstruction Capability Row
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8
                                            visible: !isDisconnected

                                            Text {
                                                text: "Native Caps: " + (dev.nativeCapsDisplay || "Standard Video")
                                                color: "#cbd5e1"
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                text: "·"
                                                color: "#475569"
                                                font.pixelSize: 10
                                            }

                                            Text {
                                                text: "SLAM Pipeline: " + (dev.compatibleReconstruction || "None") + " (" + (dev.recommendedBackend || "N/A") + ")"
                                                color: "#38bdf8"
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

            // =========================================================================
            // TAB 2: UNIVERSAL SENSOR ECOSYSTEM CATALOG (Milestone 14.5)
            // =========================================================================
            ColumnLayout {
                anchors.fill: parent
                visible: configureSensorsDialog.currentTab === 2
                spacing: 10

                // Top Search and Statistics Bar
                Rectangle {
                    Layout.fillWidth: true
                    height: 52
                    radius: 8
                    color: "#0f172a"
                    border.color: "#1e293b"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 12

                        Text {
                            text: "🔍"
                            font.pixelSize: 16
                        }

                        TextField {
                            id: catalogSearchInput
                            Layout.fillWidth: true
                            placeholderText: "Search 106 models, 16 manufacturers, SDKs (e.g. SICK multiScan, Livox Mid-360L, Ouster Rev8, Pandar)..."
                            placeholderTextColor: "#64748b"
                            color: "white"
                            font.pixelSize: 12
                            background: Rectangle {
                                color: "transparent"
                            }
                            onTextChanged: {
                                configureSensorsDialog.catalogSearchQuery = text;
                                configureSensorsDialog.refreshCatalog();
                            }
                        }

                        Button {
                            visible: catalogSearchInput.text.length > 0
                            text: "✕ Clear"
                            implicitHeight: 28
                            implicitWidth: 64
                            onClicked: {
                                catalogSearchInput.text = "";
                                configureSensorsDialog.catalogSearchQuery = "";
                                configureSensorsDialog.refreshCatalog();
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#94a3b8"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1e293b"
                                radius: 4
                            }
                        }

                        Rectangle {
                            height: 24
                            width: 1
                            color: "#334155"
                        }

                        // Total Statistics Badge
                        Rectangle {
                            implicitHeight: 28
                            implicitWidth: catalogStatsText.contentWidth + 16
                            radius: 14
                            color: "#1e293b"
                            border.color: "#38bdf8"
                            Text {
                                id: catalogStatsText
                                anchors.centerIn: parent
                                text: configureSensorsDialog.catalogModels.length + " Models Shown · 16 Real Manufacturers"
                                color: "#38bdf8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }
                }

                // Notification Toast Banner (e.g. when template profile is created)
                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    visible: configureSensorsDialog.catalogNotification !== ""
                    color: configureSensorsDialog.catalogNotificationSuccess ? "#064e3b" : "#7f1d1d"
                    border.color: configureSensorsDialog.catalogNotificationSuccess ? "#10b981" : "#ef4444"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: configureSensorsDialog.catalogNotificationSuccess ? "✓" : "⚠"
                            color: configureSensorsDialog.catalogNotificationSuccess ? "#34d399" : "#f87171"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        Text {
                            Layout.fillWidth: true
                            text: configureSensorsDialog.catalogNotification
                            color: "#ffffff"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            implicitWidth: 20
                            implicitHeight: 20
                            cursorShape: Qt.PointingHandCursor
                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: "#94a3b8"
                                font.pixelSize: 12
                            }
                            onClicked: configureSensorsDialog.catalogNotification = ""
                        }
                    }
                }

                // 3-Pane Main View
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    // ---------------------------------------------------------
                    // PANE 1: MANUFACTURERS & SENSOR TYPE FILTERS (Left, 220px)
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.preferredWidth: 220
                        Layout.fillHeight: true
                        radius: 8
                        color: "#0f172a"
                        border.color: "#1e293b"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Text {
                                text: "MANUFACTURERS (16)"
                                color: "#64748b"
                                font.bold: true
                                font.pixelSize: 10
                            }

                            // Manufacturers List
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                ColumnLayout {
                                    width: parent.width
                                    spacing: 3

                                    // "All Manufacturers" item
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 32
                                        radius: 4
                                        color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#0284c7" : (allVendMouse.containsMouse ? "#1e293b" : "transparent")
                                        border.color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#38bdf8" : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            Text {
                                                text: "🌐 All Ecosystems"
                                                color: configureSensorsDialog.catalogSelectedVendor === "All" ? "white" : "#cbd5e1"
                                                font.pixelSize: 11
                                                font.bold: configureSensorsDialog.catalogSelectedVendor === "All"
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: "" + bridge.ecosystemModelCount
                                                color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#e0f2fe" : "#64748b"
                                                font.pixelSize: 10
                                            }
                                        }

                                        MouseArea {
                                            id: allVendMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                configureSensorsDialog.catalogSelectedVendor = "All";
                                                configureSensorsDialog.refreshCatalog();
                                            }
                                        }
                                    }

                                    // List of 16 Manufacturers
                                    Repeater {
                                        model: bridge.getEcosystemManufacturers()
                                        delegate: Rectangle {
                                            Layout.fillWidth: true
                                            height: 32
                                            radius: 4
                                            color: configureSensorsDialog.catalogSelectedVendor === modelData ? "#0284c7" : (vendMouse.containsMouse ? "#1e293b" : "transparent")
                                            border.color: configureSensorsDialog.catalogSelectedVendor === modelData ? "#38bdf8" : "transparent"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8

                                                Text {
                                                    text: modelData
                                                    color: configureSensorsDialog.catalogSelectedVendor === modelData ? "white" : "#cbd5e1"
                                                    font.pixelSize: 11
                                                    font.bold: configureSensorsDialog.catalogSelectedVendor === modelData
                                                    Layout.fillWidth: true
                                                    elide: Text.ElideRight
                                                }

                                                // Icon badge for vendor category
                                                Text {
                                                    text: (modelData === "SICK" ? "🇩🇪" :
                                                           modelData === "Ouster" || modelData === "Intel" ? "🇺🇸" :
                                                           modelData === "Stereolabs" || modelData === "Valeo" ? "🇫🇷" : "📡")
                                                    font.pixelSize: 10
                                                }
                                            }

                                            MouseArea {
                                                id: vendMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    configureSensorsDialog.catalogSelectedVendor = modelData;
                                                    configureSensorsDialog.refreshCatalog();
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: "#1e293b"
                            }

                            // Sensor Type Filter
                            Text {
                                text: "SENSOR TYPE"
                                color: "#64748b"
                                font.bold: true
                                font.pixelSize: 10
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: 4

                                Repeater {
                                    model: ["All", "LiDAR", "RGB-D", "Stereo", "ToF", "RGB Camera"]
                                    delegate: Rectangle {
                                        implicitHeight: 24
                                        implicitWidth: typeFilterText.contentWidth + 14
                                        radius: 12
                                        color: configureSensorsDialog.catalogSelectedType === modelData ? "#0284c7" : (typeMouse.containsMouse ? "#334155" : "#1e293b")
                                        border.color: configureSensorsDialog.catalogSelectedType === modelData ? "#38bdf8" : "#334155"

                                        Text {
                                            id: typeFilterText
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: configureSensorsDialog.catalogSelectedType === modelData ? "white" : "#94a3b8"
                                            font.pixelSize: 9
                                            font.bold: configureSensorsDialog.catalogSelectedType === modelData
                                        }

                                        MouseArea {
                                            id: typeMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                configureSensorsDialog.catalogSelectedType = modelData;
                                                configureSensorsDialog.refreshCatalog();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // PANE 2: MODEL LIST CARDS (Middle, 340px)
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.preferredWidth: 340
                        Layout.fillHeight: true
                        radius: 8
                        color: "#0f172a"
                        border.color: "#1e293b"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "MODELS (" + configureSensorsDialog.catalogModels.length + ")"
                                    color: "#64748b"
                                    font.bold: true
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: configureSensorsDialog.catalogSelectedVendor
                                    color: "#38bdf8"
                                    font.bold: true
                                    font.pixelSize: 10
                                }
                            }

                            // Model Cards List
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                ListView {
                                    width: parent.width
                                    model: configureSensorsDialog.catalogModels
                                    spacing: 6

                                    delegate: Rectangle {
                                        width: ListView.view.width
                                        height: 74
                                        radius: 6
                                        color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#1e293b" : (cardMouse.containsMouse ? "#141c2b" : "#0b1120")
                                        border.color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#38bdf8" : (cardMouse.containsMouse ? "#334155" : "#1e293b")
                                        border.width: configureSensorsDialog.catalogSelectedModelId === modelData.id ? 1.5 : 1

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 10

                                            // Type icon circle
                                            Rectangle {
                                                implicitWidth: 38
                                                implicitHeight: 38
                                                radius: 19
                                                color: modelData.typeColor ? modelData.typeColor + "22" : "#38bdf822"
                                                border.color: modelData.typeColor ? modelData.typeColor : "#38bdf8"

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.icon || "📡"
                                                    font.pixelSize: 16
                                                }
                                            }

                                            // Model Info
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Text {
                                                        text: modelData.vendor + " " + modelData.model
                                                        color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#38bdf8" : "white"
                                                        font.bold: true
                                                        font.pixelSize: 11
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Text {
                                                    text: modelData.family + " · " + modelData.hardwareRevision
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                    elide: Text.ElideRight
                                                }

                                                // Badges row
                                                RowLayout {
                                                    spacing: 4

                                                    // Status badge
                                                    Rectangle {
                                                        implicitHeight: 16
                                                        implicitWidth: statusText.contentWidth + 8
                                                        radius: 3
                                                        color: modelData.productionStatus === "CURRENT" ? "#064e3b" : (modelData.productionStatus === "PREVIOUS" ? "#1e3a8a" : "#451a03")
                                                        border.color: modelData.productionStatus === "CURRENT" ? "#10b981" : (modelData.productionStatus === "PREVIOUS" ? "#3b82f6" : "#f59e0b")
                                                        Text {
                                                            id: statusText
                                                            anchors.centerIn: parent
                                                            text: modelData.productionStatus
                                                            color: modelData.productionStatus === "CURRENT" ? "#34d399" : (modelData.productionStatus === "PREVIOUS" ? "#93c5fd" : "#fbbf24")
                                                            font.pixelSize: 8
                                                            font.bold: true
                                                        }
                                                    }

                                                    // Validation badge
                                                    Rectangle {
                                                        implicitHeight: 16
                                                        implicitWidth: tierText.contentWidth + 8
                                                        radius: 3
                                                        color: modelData.validationTier === "AV_VALIDATED" ? "#064e3b" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#083344" : "#1e1b4b")
                                                        border.color: modelData.validationTier === "AV_VALIDATED" ? "#10b981" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#06b6d4" : "#6366f1")
                                                        Text {
                                                            id: tierText
                                                            anchors.centerIn: parent
                                                            text: modelData.validationTier
                                                            color: modelData.validationTier === "AV_VALIDATED" ? "#34d399" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#67e8f9" : "#a5b4fc")
                                                            font.pixelSize: 8
                                                        }
                                                    }

                                                    // Transport badge
                                                    Rectangle {
                                                        implicitHeight: 16
                                                        implicitWidth: trText.contentWidth + 8
                                                        radius: 3
                                                        color: "#1e293b"
                                                        border.color: "#475569"
                                                        Text {
                                                            id: trText
                                                            anchors.centerIn: parent
                                                            text: modelData.transportType
                                                            color: "#94a3b8"
                                                            font.pixelSize: 8
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: cardMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                configureSensorsDialog.catalogSelectedModelId = modelData.id;
                                                configureSensorsDialog.catalogSelectedModelDetails = bridge.getModelDetails(modelData.id);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // PANE 3: DETAILED SPECIFICATIONS & ACTIONS (Right, Fill)
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 8
                        color: "#0f172a"
                        border.color: "#1e293b"

                        // Empty State if no model selected
                        Item {
                            anchors.fill: parent
                            visible: configureSensorsDialog.catalogSelectedModelId === "" || !configureSensorsDialog.catalogSelectedModelDetails.id

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 12
                                Text {
                                    text: "📚"
                                    font.pixelSize: 36
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "Select a Sensor Model to View Specifications"
                                    color: "#94a3b8"
                                    font.bold: true
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "Explore manufacturer specifications, official drivers, 8-point runtime status,\nand seed experimental template profiles for offline project planning."
                                    color: "#64748b"
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        // Selected Model Specification View
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 14
                            clip: true
                            visible: configureSensorsDialog.catalogSelectedModelId !== "" && configureSensorsDialog.catalogSelectedModelDetails.id

                            ColumnLayout {
                                width: parent.width - 12
                                spacing: 12

                                // Title & Header
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: (configureSensorsDialog.catalogSelectedModelDetails.vendor || "") + " " + (configureSensorsDialog.catalogSelectedModelDetails.model || "")
                                            color: "white"
                                            font.bold: true
                                            font.pixelSize: 16
                                        }

                                        Text {
                                            text: (configureSensorsDialog.catalogSelectedModelDetails.family || "") + " · Revision: " + (configureSensorsDialog.catalogSelectedModelDetails.hardwareRevision || "N/A") + " · Model ID: " + (configureSensorsDialog.catalogSelectedModelDetails.id || "")
                                            color: "#94a3b8"
                                            font.pixelSize: 11
                                        }
                                    }

                                    // Action Button: Add as Template Profile
                                    Button {
                                        text: "➕ Add as Template Profile"
                                        implicitHeight: 36
                                        implicitWidth: 190
                                        contentItem: Text {
                                            text: parent.text
                                            color: "white"
                                            font.bold: true
                                            font.pixelSize: 11
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: parent.hovered ? "#0284c7" : "#0369a1"
                                            radius: 6
                                            border.color: "#38bdf8"
                                        }
                                        onClicked: {
                                            var res = bridge.createTemplateProfileFromModel(configureSensorsDialog.catalogSelectedModelId);
                                            if (res && res.success) {
                                                configureSensorsDialog.catalogNotification = "Template Profile '" + res.profileId + "' created! Available in Sensor Composition.";
                                                configureSensorsDialog.catalogNotificationSuccess = true;
                                                configureSensorsDialog.availableProfiles = bridge.getAvailableDeviceProfiles();
                                            } else {
                                                configureSensorsDialog.catalogNotification = "Failed to create template profile: " + (res.message || "Unknown error");
                                                configureSensorsDialog.catalogNotificationSuccess = false;
                                            }
                                        }
                                    }
                                }

                                // Badges Row
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgType.contentWidth + 12
                                        radius: 4
                                        color: "#1e293b"
                                        border.color: "#38bdf8"
                                        Text {
                                            id: bdgType
                                            anchors.centerIn: parent
                                            text: "Type: " + (configureSensorsDialog.catalogSelectedModelDetails.sensorType || "")
                                            color: "#38bdf8"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgStatus.contentWidth + 12
                                        radius: 4
                                        color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#064e3b" : "#451a03"
                                        border.color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#10b981" : "#f59e0b"
                                        Text {
                                            id: bdgStatus
                                            anchors.centerIn: parent
                                            text: "Lifecycle: " + (configureSensorsDialog.catalogSelectedModelDetails.productionStatus || "")
                                            color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#34d399" : "#fbbf24"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgVal.contentWidth + 12
                                        radius: 4
                                        color: "#1e1b4b"
                                        border.color: "#6366f1"
                                        Text {
                                            id: bdgVal
                                            anchors.centerIn: parent
                                            text: "Tier: " + (configureSensorsDialog.catalogSelectedModelDetails.validationTier || "")
                                            color: "#a5b4fc"
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgTrans.contentWidth + 12
                                        radius: 4
                                        color: "#0f172a"
                                        border.color: "#475569"
                                        Text {
                                            id: bdgTrans
                                            anchors.centerIn: parent
                                            text: "Transport: " + (configureSensorsDialog.catalogSelectedModelDetails.transportType || "")
                                            color: "#cbd5e1"
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgBackend.contentWidth + 12
                                        radius: 4
                                        color: "#14532d"
                                        border.color: "#22c55e"
                                        Text {
                                            id: bdgBackend
                                            anchors.centerIn: parent
                                            text: "SLAM: " + (configureSensorsDialog.catalogSelectedModelDetails.recommendedSlamBackend || "FAST-LIO2")
                                            color: "#86efac"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }

                                // -----------------------------------------------------
                                // Section 1: Physical & Network Interface Specs
                                // -----------------------------------------------------
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 120
                                    radius: 6
                                    color: "#161f30"
                                    border.color: "#1e293b"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "HARDWARE & NETWORK SPECIFICATIONS"
                                            color: "#38bdf8"
                                            font.bold: true
                                            font.pixelSize: 10
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Default Endpoint: " + (configureSensorsDialog.catalogSelectedModelDetails.defaultIp || "Dynamic / USB") + 
                                                          (configureSensorsDialog.catalogSelectedModelDetails.defaultPorts && configureSensorsDialog.catalogSelectedModelDetails.defaultPorts.length > 0 ? " : [" + configureSensorsDialog.catalogSelectedModelDetails.defaultPorts.join(", ") + "]" : "")
                                                    color: "#e2e8f0"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "USB IDs: " + (configureSensorsDialog.catalogSelectedModelDetails.vendorIds && configureSensorsDialog.catalogSelectedModelDetails.vendorIds.length > 0 ? "VID: " + configureSensorsDialog.catalogSelectedModelDetails.vendorIds.join(", ") + " · PID: " + configureSensorsDialog.catalogSelectedModelDetails.productIds.join(", ") : "N/A (Network LiDAR)")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "Coordinate System: " + (configureSensorsDialog.catalogSelectedModelDetails.nativeCoordinateSystem || "FLU") + " (+X Forward, +Y Left, +Z Up)"
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Scan Rates: " + (configureSensorsDialog.catalogSelectedModelDetails.scanRates && configureSensorsDialog.catalogSelectedModelDetails.scanRates.length > 0 ? configureSensorsDialog.catalogSelectedModelDetails.scanRates.join(", ") + " Hz" : "30.0 Hz")
                                                    color: "#e2e8f0"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "Return Modes: " + (configureSensorsDialog.catalogSelectedModelDetails.returnModes && configureSensorsDialog.catalogSelectedModelDetails.returnModes.length > 0 ? configureSensorsDialog.catalogSelectedModelDetails.returnModes.join(", ") : "Single / Strongest")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "Sync / PTP: " + (configureSensorsDialog.catalogSelectedModelDetails.hasPtp ? "PTP IEEE 1588v2 · " : "") + (configureSensorsDialog.catalogSelectedModelDetails.hasPps ? "PPS Sync" : "Host Timestamping")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }
                                    }
                                }

                                // -----------------------------------------------------
                                // Section 2: Official Vendor SDK & Provenance
                                // -----------------------------------------------------
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 120
                                    radius: 6
                                    color: "#161f30"
                                    border.color: "#1e293b"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "OFFICIAL VENDOR SDK & DOCUMENTATION PROVENANCE"
                                            color: "#38bdf8"
                                            font.bold: true
                                            font.pixelSize: 10
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Official Driver: " + (configureSensorsDialog.catalogSelectedModelDetails.currentDriver || "Generic Driver") + " (SDK " + (configureSensorsDialog.catalogSelectedModelDetails.currentSdk || "1.0") + ")"
                                                    color: "#e2e8f0"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "ROS / ROS 2: " + (configureSensorsDialog.catalogSelectedModelDetails.rosDriver || "ROS 2 Humble / Jazzy compatible")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "License / OS: " + (configureSensorsDialog.catalogSelectedModelDetails.license || "Proprietary") + " · " + (configureSensorsDialog.catalogSelectedModelDetails.supportedOs ? configureSensorsDialog.catalogSelectedModelDetails.supportedOs.join(", ") : "Linux, Windows")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Manual: " + (configureSensorsDialog.catalogSelectedModelDetails.manualRef || "Manufacturer User Manual")
                                                    color: "#e2e8f0"
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: "Verified Date: " + (configureSensorsDialog.catalogSelectedModelDetails.lastVerified || "2026-09")
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                }
                                                // Clickable URL
                                                Text {
                                                    text: "🔗 " + (configureSensorsDialog.catalogSelectedModelDetails.docUrl || "https://github.com")
                                                    color: "#38bdf8"
                                                    font.pixelSize: 10
                                                    font.underline: true
                                                    elide: Text.ElideRight
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (configureSensorsDialog.catalogSelectedModelDetails.docUrl) {
                                                                Qt.openUrlExternally(configureSensorsDialog.catalogSelectedModelDetails.docUrl);
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // -----------------------------------------------------
                                // Section 3: 8-Point Runtime Integration Status Ladder
                                // -----------------------------------------------------
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 150
                                    radius: 6
                                    color: "#161f30"
                                    border.color: "#1e293b"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "8-POINT RUNTIME INTEGRATION STATUS (A–H)"
                                            color: "#38bdf8"
                                            font.bold: true
                                            font.pixelSize: 10
                                        }

                                        GridLayout {
                                            Layout.fillWidth: true
                                            columns: 2
                                            rowSpacing: 4
                                            columnSpacing: 14

                                            // A. Catalog Knowledge
                                            Text {
                                                text: "A. Catalog Knowledge:  ✓ Verified (" + (configureSensorsDialog.catalogSelectedModelDetails.sourceProvenance || "Official Specs") + ")"
                                                color: "#34d399"
                                                font.pixelSize: 10
                                            }

                                            // B. Official SDK Identified
                                            Text {
                                                text: "B. Official SDK Identified:  ✓ " + (configureSensorsDialog.catalogSelectedModelDetails.officialSdkId || configureSensorsDialog.catalogSelectedModelDetails.sdkFamily || "SDK Identified")
                                                color: "#34d399"
                                                font.pixelSize: 10
                                            }

                                            // C. SDK Integrated in AV Scan
                                            Text {
                                                text: "C. SDK Driver Integrated:  " + (configureSensorsDialog.catalogSelectedModelDetails.sdkIntegrated ? "✓ Integrated" : "○ Driver Planned / Headless")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.sdkIntegrated ? "#34d399" : "#fbbf24"
                                                font.pixelSize: 10
                                            }

                                            // D. Hardware Interrogation
                                            Text {
                                                text: "D. HW Interrogation:  " + (configureSensorsDialog.catalogSelectedModelDetails.hwInterrogated ? "✓ Live Interrogation Supported" : "○ Interrogation on Physical Attach")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.hwInterrogated ? "#34d399" : "#94a3b8"
                                                font.pixelSize: 10
                                            }

                                            // E. Runtime Streaming Adapter
                                            Text {
                                                text: "E. Streaming Adapter:  " + (configureSensorsDialog.catalogSelectedModelDetails.adapterImplemented ? "✓ " + configureSensorsDialog.catalogSelectedModelDetails.runtimeAdapter : "○ " + (configureSensorsDialog.catalogSelectedModelDetails.runtimeAdapter || "Generic Adapter"))
                                                color: configureSensorsDialog.catalogSelectedModelDetails.adapterImplemented ? "#34d399" : "#94a3b8"
                                                font.pixelSize: 10
                                            }

                                            // F. Compatible SLAM Backend
                                            Text {
                                                text: "F. Compatible SLAM Backend:  ✓ " + (configureSensorsDialog.catalogSelectedModelDetails.recommendedSlamBackend || "FAST-LIO2 / RTAB-Map")
                                                color: "#34d399"
                                                font.pixelSize: 10
                                            }

                                            // G. Physically Tested
                                            Text {
                                                text: "G. Physical HW Tested:  " + (configureSensorsDialog.catalogSelectedModelDetails.physicallyTested ? "✓ Bench Validated" : "○ Unplugged (Pending Physical Rig)")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.physicallyTested ? "#34d399" : "#64748b"
                                                font.pixelSize: 10
                                            }

                                            // H. AV GUI Validated
                                            Text {
                                                text: "H. AV GUI Live Validated:  " + (configureSensorsDialog.catalogSelectedModelDetails.guiValidated ? "✓ Live Validated" : "○ Ready for Rig Session")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.guiValidated ? "#34d399" : "#64748b"
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }

                                // Architecture Notice Callout
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 54
                                    radius: 6
                                    color: "#0f172a"
                                    border.color: "#334155"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 8

                                        Text {
                                            text: "ℹ️"
                                            font.pixelSize: 16
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: "Model Definitions are authoritative catalog specifications. Seeding a Template Profile creates an experimental configuration for offline project planning. Physical sensor calibration is verified automatically upon hardware connection."
                                            color: "#94a3b8"
                                            font.pixelSize: 10
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

        footer: Rectangle {
            height: 64
            color: "#121720"
            radius: 12
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#28303e"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                Button {
                    text: configureSensorsDialog.currentTab === 0 ? "Cancel" : "Close"
                    implicitWidth: 110
                    implicitHeight: 42
                    onClicked: configureSensorsDialog.close()
                    contentItem: Text {
                        text: parent.text
                        color: "#cbd5e1"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "#1e293b"
                        radius: 6
                        border.color: "#475569"
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    visible: configureSensorsDialog.currentTab === 1 || configureSensorsDialog.currentTab === 2
                    text: "← Return to Sensor Composition"
                    implicitWidth: 240
                    implicitHeight: 42
                    onClicked: configureSensorsDialog.currentTab = 0
                    contentItem: Text {
                        text: parent.text
                        color: "#38bdf8"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#1e293b" : "#0f172a"
                        radius: 6
                        border.color: "#38bdf8"
                    }
                }

                Button {
                    id: saveCustomConfigBtn
                    visible: configureSensorsDialog.currentTab === 0
                    text: "💾 Save & Activate Configuration"
                    implicitWidth: 320
                    implicitHeight: 42
                    enabled: configureSensorsDialog.getSelectedIds().length > 0 && customConfigNameInput.text.trim() !== ""
                    onClicked: {
                        var ids = configureSensorsDialog.getSelectedIds();
                        var name = customConfigNameInput.text.trim();
                        var ok = bridge.saveCustomScannerConfig(name, ids);
                        if (ok) {
                            var selId = bridge.selectedScannerId;
                            for (var i = 0; i < bridge.availableScanners.length; ++i) {
                                if (bridge.availableScanners[i].indexOf(name) !== -1 ||
                                    (selId !== "" && bridge.availableScanners[i].indexOf(selId) !== -1)) {
                                    scannerCombo.currentIndex = i;
                                    break;
                                }
                            }
                        }
                        configureSensorsDialog.close();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#0284c7" : "#0369a1") : "#1e293b"
                        radius: 6
                        border.color: parent.enabled ? "#38bdf8" : "#2d3748"
                    }
                }
            }
        }

        Timer {
            id: profilingAnimTimer
            interval: 220
            repeat: true
            property int step: 1
            onTriggered: {
                if (step === 1) {
                    configureSensorsDialog.profilingStep = "Reading device capabilities & descriptors...";
                    configureSensorsDialog.profilingProgress = 35;
                    step = 2;
                } else if (step === 2) {
                    configureSensorsDialog.profilingStep = "Inspecting stream formats and frame rates...";
                    configureSensorsDialog.profilingProgress = 55;
                    step = 3;
                } else if (step === 3) {
                    configureSensorsDialog.profilingStep = "Checking calibration provenance...";
                    configureSensorsDialog.profilingProgress = 75;
                    step = 4;
                } else if (step === 4) {
                    configureSensorsDialog.profilingStep = "Matching compatible SLAM/Reconstruction backend...";
                    configureSensorsDialog.profilingProgress = 90;
                    step = 5;
                } else if (step === 5) {
                    configureSensorsDialog.profilingStep = "Generating EXPERIMENTAL Device Profile...";
                    configureSensorsDialog.profilingProgress = 100;
                    step = 6;
                } else if (step === 6) {
                    profilingAnimTimer.stop();
                    var res = bridge.createProfileForDevice(configureSensorsDialog.profilingDeviceId);
                    configureSensorsDialog.isProfiling = false;
                    if (res && res.success) {
                        configureSensorsDialog.profileResult = res;
                        configureSensorsDialog.profileResultVisible = true;
                    }
                    configureSensorsDialog.availableProfiles = bridge.getAvailableDeviceProfiles();
                    configureSensorsDialog.refreshDiscovery();
                }
            }
        }

        onOpened: {
            availableProfiles = bridge.getAvailableDeviceProfiles();
            var m = {};
            for (var i = 0; i < availableProfiles.length; ++i) {
                m[availableProfiles[i].profileId] = true;
            }
            selectedProfileMap = m;
            customConfigNameInput.text = "RoboSense Airy + Gemini 336L + VITURE Ultra";
            refreshDiscovery();
            refreshCatalog();
            profileResultVisible = false;
            isProfiling = false;
        }

        function refreshDiscovery() {
            bridge.refreshDiscovery();
        }

        function startProfiling(devId) {
            profilingDeviceId = devId;
            isProfiling = true;
            profilingStep = "Detecting physical hardware & bus...";
            profilingProgress = 15;
            profilingAnimTimer.step = 1;
            profilingAnimTimer.start();
        }

        function toggleSelection(pid) {
            var m = Object.assign({}, selectedProfileMap);
            if (m[pid]) {
                delete m[pid];
            } else {
                m[pid] = true;
            }
            selectedProfileMap = m;
        }

        function getSelectedIds() {
            var keys = [];
            for (var k in selectedProfileMap) {
                if (selectedProfileMap[k]) keys.push(k);
            }
            return keys;
        }
    }

    // Dialog: Orientation Diagnostic Tool (Milestone 5.5 Canonical Orientation Calibration)
    Dialog {
        id: orientationDiagDialog
        title: "Orientation Diagnostic Tool"
        modal: false
        closePolicy: Popup.CloseOnEscape
        anchors.centerIn: parent
        width: 860
        height: 720
        padding: 0
        topPadding: 12
        bottomPadding: 12
        leftPadding: 20
        rightPadding: 20

        background: Rectangle {
            color: "#151922"
            radius: 12
            border.color: "#a855f7"
            border.width: 1.5
        }

        header: Rectangle {
            height: 58
            color: "#1e1b4b"
            radius: 12

            MouseArea {
                anchors.fill: parent
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#3b0764"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "📐 Sensor Orientation & Canonical Transform Diagnostic"
                    color: "#f3e8ff"
                    font.pixelSize: 15
                    font.bold: true
                }

                Button {
                    text: "⚡ Create Profile"
                    implicitHeight: 28
                    implicitWidth: 120
                    onClicked: {
                        configureSensorsDialog.currentTab = 1;
                        configureSensorsDialog.open();
                        configureSensorsDialog.refreshDiscovery();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#38bdf8"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#0c4a6e" : "#0f172a"
                        radius: 4
                        border.color: "#38bdf8"
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "✥ Drag header to move"
                    color: "#94a3b8"
                    font.pixelSize: 11
                }

                Button {
                    text: "✕"
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: {
                        bridge.closeOrientationDiagnostic();
                        orientationDiagDialog.close();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#e2e8f0"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#dc2626" : "#334155"
                        radius: 6
                    }
                }
            }
        }

        contentItem: ScrollView {
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded

            ColumnLayout {
                width: orientationDiagDialog.availableWidth
                spacing: 14

                // 1. Sensor Selector Tabs
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "Active Sensor Backend:"
                        color: "#94a3b8"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    // RoboSense Airy Tab (Frozen Reference)
                    Button {
                        implicitHeight: 34
                        implicitWidth: 190
                        onClicked: bridge.setOrientationDiagSensor("AIRY")
                        contentItem: RowLayout {
                            spacing: 6
                            anchors.centerIn: parent
                            Text {
                                text: "RoboSense Airy"
                                color: bridge.orientationDiagSensor === "AIRY" ? "#22c55e" : "#94a3b8"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 50
                                height: 16
                                radius: 8
                                color: "#14532d"
                                border.color: "#22c55e"
                                Text {
                                    anchors.centerIn: parent
                                    text: "LOCKED"
                                    color: "#86efac"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "AIRY" ? "#052e16" : "#1e293b"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "AIRY" ? "#22c55e" : "#334155"
                            border.width: bridge.orientationDiagSensor === "AIRY" ? 1.5 : 1
                        }
                    }

                    // Orbbec Gemini 336L Tab
                    Button {
                        implicitHeight: 34
                        implicitWidth: 170
                        onClicked: bridge.setOrientationDiagSensor("GEMINI")
                        contentItem: RowLayout {
                            spacing: 6
                            anchors.centerIn: parent
                            Text {
                                text: "Orbbec Gemini 336L"
                                color: bridge.orientationDiagSensor === "GEMINI" ? "#38bdf8" : "#94a3b8"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 44
                                height: 16
                                radius: 8
                                color: "#0c4a6e"
                                border.color: "#38bdf8"
                                Text {
                                    anchors.centerIn: parent
                                    text: "RGBD"
                                    color: "#7dd3fc"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "GEMINI" ? "#082f49" : "#1e293b"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "GEMINI" ? "#38bdf8" : "#334155"
                            border.width: bridge.orientationDiagSensor === "GEMINI" ? 1.5 : 1
                        }
                    }

                    // VITURE Luma Ultra Tab
                    Button {
                        implicitHeight: 34
                        implicitWidth: 160
                        onClicked: bridge.setOrientationDiagSensor("VITURE")
                        contentItem: RowLayout {
                            spacing: 6
                            anchors.centerIn: parent
                            Text {
                                text: "VITURE Ultra"
                                color: bridge.orientationDiagSensor === "VITURE" ? "#f59e0b" : "#94a3b8"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 40
                                height: 16
                                radius: 8
                                color: "#451a03"
                                border.color: "#f59e0b"
                                Text {
                                    anchors.centerIn: parent
                                    text: "VIO"
                                    color: "#fcd34d"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "VITURE" ? "#451a03" : "#1e293b"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "VITURE" ? "#f59e0b" : "#334155"
                            border.width: bridge.orientationDiagSensor === "VITURE" ? 1.5 : 1
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // Banner for Airy when selected
                Rectangle {
                    visible: bridge.orientationDiagIsFrozen
                    Layout.fillWidth: true
                    implicitHeight: 48
                    color: "#052e16"
                    radius: 8
                    border.color: "#22c55e"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Text {
                            text: "🔒"
                            font.pixelSize: 16
                        }
                        Text {
                            text: "Airy LiDAR (FAST-LIVO2 ROS FLU) is physically validated and frozen as the Ground Truth Reference (+X Forward, +Y Left, +Z Up, Ground XY). Orientation controls are disabled."
                            color: "#86efac"
                            font.pixelSize: 12
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // Controls Section (Enabled when not frozen)
                ColumnLayout {
                    visible: !bridge.orientationDiagIsFrozen
                    Layout.fillWidth: true
                    spacing: 12

                    // Quick Orientation Presets
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Presets:"
                            color: "#94a3b8"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Button {
                            text: "ROS Optical Frame (-90° X, 0° Y, -90° Z)"
                            implicitHeight: 28
                            onClicked: bridge.setOrientationDiagRotation(-90.0, 0.0, -90.0)
                            contentItem: Text {
                                text: parent.text; color: "#e2e8f0"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#334155" : "#1e293b"; radius: 4; border.color: "#475569" }
                        }

                        Button {
                            text: "Identity (0°, 0°, 0°)"
                            implicitHeight: 28
                            onClicked: bridge.resetOrientationDiagRotation()
                            contentItem: Text {
                                text: parent.text; color: "#e2e8f0"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#334155" : "#1e293b"; radius: 4; border.color: "#475569" }
                        }

                        Button {
                            text: "Inverted Optical (+90° X, 0° Y, +90° Z)"
                            implicitHeight: 28
                            onClicked: bridge.setOrientationDiagRotation(90.0, 0.0, 90.0)
                            contentItem: Text {
                                text: parent.text; color: "#e2e8f0"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#334155" : "#1e293b"; radius: 4; border.color: "#475569" }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // --- ROLL / X ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Roll (X-Axis Rotation) — Rotation around Physical Forward (+X)"
                                    color: "#f87171"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagRoll.toFixed(1) + "°"
                                    color: "#f87171"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "-90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", -90.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", -15.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", -1.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }

                                Slider {
                                    id: rollSlider
                                    Layout.fillWidth: true
                                    from: -180.0
                                    to: 180.0
                                    stepSize: 1.0
                                    value: bridge.orientationDiagRoll
                                    onMoved: bridge.setOrientationDiagRotation(rollSlider.value, bridge.orientationDiagPitch, bridge.orientationDiagYaw)
                                    background: Rectangle {
                                        x: rollSlider.leftPadding
                                        y: rollSlider.topPadding + rollSlider.availableHeight / 2 - height / 2
                                        width: rollSlider.availableWidth
                                        height: 6
                                        radius: 3
                                        color: "#334155"
                                    }
                                    handle: Rectangle {
                                        x: rollSlider.leftPadding + rollSlider.visualPosition * (rollSlider.availableWidth - width)
                                        y: rollSlider.topPadding + rollSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: rollSlider.pressed ? "#fca5a5" : "#f87171"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 1.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 15.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 90.0)
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#261315"; radius: 4; border.color: "#7f1d1d" }
                                }
                            }
                        }
                    }

                    // --- PITCH / Y ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Pitch (Y-Axis Rotation) — Rotation around Physical Left (+Y)"
                                    color: "#4ade80"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagPitch.toFixed(1) + "°"
                                    color: "#4ade80"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "-90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", -90.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", -15.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", -1.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }

                                Slider {
                                    id: pitchSlider
                                    Layout.fillWidth: true
                                    from: -180.0
                                    to: 180.0
                                    stepSize: 1.0
                                    value: bridge.orientationDiagPitch
                                    onMoved: bridge.setOrientationDiagRotation(bridge.orientationDiagRoll, pitchSlider.value, bridge.orientationDiagYaw)
                                    background: Rectangle {
                                        x: pitchSlider.leftPadding
                                        y: pitchSlider.topPadding + pitchSlider.availableHeight / 2 - height / 2
                                        width: pitchSlider.availableWidth
                                        height: 6
                                        radius: 3
                                        color: "#334155"
                                    }
                                    handle: Rectangle {
                                        x: pitchSlider.leftPadding + pitchSlider.visualPosition * (pitchSlider.availableWidth - width)
                                        y: pitchSlider.topPadding + pitchSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: pitchSlider.pressed ? "#bbf7d0" : "#4ade80"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 1.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 15.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 90.0)
                                    contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#052e16" : "#0d2818"; radius: 4; border.color: "#166534" }
                                }
                            }
                        }
                    }

                    // --- YAW / Z ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Yaw (Z-Axis Rotation) — Rotation around Physical Up (+Z)"
                                    color: "#60a5fa"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagYaw.toFixed(1) + "°"
                                    color: "#60a5fa"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "-90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", -90.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", -15.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", -1.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }

                                Slider {
                                    id: yawSlider
                                    Layout.fillWidth: true
                                    from: -180.0
                                    to: 180.0
                                    stepSize: 1.0
                                    value: bridge.orientationDiagYaw
                                    onMoved: bridge.setOrientationDiagRotation(bridge.orientationDiagRoll, bridge.orientationDiagPitch, yawSlider.value)
                                    background: Rectangle {
                                        x: yawSlider.leftPadding
                                        y: yawSlider.topPadding + yawSlider.availableHeight / 2 - height / 2
                                        width: yawSlider.availableWidth
                                        height: 6
                                        radius: 3
                                        color: "#334155"
                                    }
                                    handle: Rectangle {
                                        x: yawSlider.leftPadding + yawSlider.visualPosition * (yawSlider.availableWidth - width)
                                        y: yawSlider.topPadding + yawSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: yawSlider.pressed ? "#bfdbfe" : "#60a5fa"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 1.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 15.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 90.0)
                                    contentItem: Text { text: parent.text; color: "#60a5fa"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#082f49" : "#0c2135"; radius: 4; border.color: "#0369a1" }
                                }
                            }
                        }
                    }
                }

                // 3. Matrix & Transform Readout Cards
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    // 3x3 Rotation Matrix
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 90
                        color: "#0f172a"
                        radius: 8
                        border.color: "#334155"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Text {
                                text: "Equivalent 3×3 Rotation Matrix (R):"
                                color: "#94a3b8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: bridge.orientationDiagMatrixStr
                                color: "#38bdf8"
                                font.family: "Monospace"
                                font.pixelSize: 11
                                font.bold: true
                                lineHeight: 1.2
                            }
                        }
                    }

                    // Coordinate Mapping Formula
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 90
                        color: "#0f172a"
                        radius: 8
                        border.color: "#334155"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Text {
                                text: "Candidate Transform T_AV_from_backend:"
                                color: "#94a3b8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: bridge.orientationDiagTransformStr
                                color: "#c084fc"
                                font.family: "Monospace"
                                font.pixelSize: 11
                                font.bold: true
                                lineHeight: 1.2
                            }
                        }
                    }
                }

                // 4. Physical Alignment Acceptance Checklist
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 88
                    color: "#1e1e2d"
                    radius: 8
                    border.color: "#4f46e5"
                    border.width: 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4
                        Text {
                            text: "Physical Alignment Criteria Checklist:"
                            color: "#a5b4fc"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        RowLayout {
                            spacing: 24
                            ColumnLayout {
                                spacing: 2
                                Text { text: "✓ Floor is horizontal (parallel to XY grid plane)"; color: "#e2e8f0"; font.pixelSize: 11 }
                                Text { text: "✓ +Z is physically UP (ceiling above floor)"; color: "#e2e8f0"; font.pixelSize: 11 }
                                Text { text: "✓ Walls and doorways are vertical"; color: "#e2e8f0"; font.pixelSize: 11 }
                            }
                            ColumnLayout {
                                spacing: 2
                                Text { text: "✓ Physical forward corresponds to AV +X (screen-UP in 2D view)"; color: "#e2e8f0"; font.pixelSize: 11 }
                                Text { text: "✓ Physical left corresponds to AV +Y (screen-LEFT in 2D view)"; color: "#e2e8f0"; font.pixelSize: 11 }
                                Text { text: "✓ 3D Spatial Map and 2D Plan update live at 60 FPS while adjusting"; color: "#38bdf8"; font.pixelSize: 11; font.bold: true }
                            }
                        }
                    }
                }
            }
        }

        footer: Rectangle {
            height: 64
            color: "#11141c"
            radius: 12
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#28303e"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12

                Button {
                    text: "↺ Reset to Zero"
                    implicitWidth: 130
                    implicitHeight: 42
                    enabled: !bridge.orientationDiagIsFrozen
                    onClicked: bridge.resetOrientationDiagRotation()
                    contentItem: Text {
                        text: parent.text; color: parent.enabled ? "#e2e8f0" : "#64748b"; font.pixelSize: 12; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "#1e293b"; radius: 6; border.color: "#475569"
                    }
                }

                Text {
                    text: bridge.orientationDiagStatusMsg
                    color: "#f59e0b"
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Button {
                    text: "Cancel"
                    implicitWidth: 100
                    implicitHeight: 42
                    onClicked: {
                        bridge.closeOrientationDiagnostic();
                        orientationDiagDialog.close();
                    }
                    contentItem: Text {
                        text: parent.text; color: "#cbd5e1"; font.pixelSize: 13; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "#1e293b"; radius: 6; border.color: "#475569"
                    }
                }

                Button {
                    id: acceptOrientationBtn
                    text: "💾 ACCEPT ORIENTATION"
                    implicitWidth: 230
                    implicitHeight: 42
                    enabled: !bridge.orientationDiagIsFrozen
                    onClicked: {
                        var ok = bridge.acceptOrientationDiagRotation();
                        if (ok) {
                            orientationDiagDialog.close();
                        }
                    }
                    contentItem: Text {
                        text: parent.text; color: parent.enabled ? "white" : "#64748b"; font.pixelSize: 13; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#9333ea" : "#7e22ce") : "#1e293b"
                        radius: 6
                        border.color: parent.enabled ? "#c084fc" : "#3b0764"
                    }
                }
            }
        }
    }

    // Milestone 10: Multi-Node Mesh Session Infrastructure Dialog
    Dialog {
        id: multiNodeMeshDialog
        modal: true
        width: Math.min(1140, root.width - 40)
        height: Math.min(780, root.height - 40)
        anchors.centerIn: parent
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#11141c"
            radius: 12
            border.color: "#232834"
            border.width: 1.5
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            // Top Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "🌐"
                    font.pixelSize: 26
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Multi-Node Session Infrastructure (Milestone 10)"
                        color: "#f8fafc"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Text {
                        text: "Decentralized Discovery • Persistent Node Identity • Synchronized Coordination Metadata • Independent Spatial Capture"
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "✕"
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: multiNodeMeshDialog.close()
                    contentItem: Text { text: parent.text; color: "#94a3b8"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#334155" : "transparent"; radius: 6 }
                }
            }

            // Architectural Boundary Alert Banner (Requirement 1 & 12)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 46
                color: "#1c1917"
                radius: 8
                border.color: "#d97706"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Text { text: "⚠️"; font.pixelSize: 16 }
                    Text {
                        Layout.fillWidth: true
                        text: "M10 ARCHITECTURAL INVARIANT: Multi-node coordination, session membership, and capture synchronization only. Cross-node map fusion, SLAM state sharing, and collaborative spatial merging belong to M11. Each node maintains its independent coordinate system."
                        color: "#fbbf24"
                        font.pixelSize: 11
                        font.bold: true
                        wrapMode: Text.Wrap
                    }
                }
            }

            // Main Content Area: 2 Columns
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                // --- LEFT COLUMN: Node Identity & Session Control ---
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 420
                    spacing: 12

                    // Card 1: This Scanner Node Identity (Persistent)
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 220
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "💻 THIS SCANNER NODE (LOCAL)"
                                    color: "#38bdf8"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    implicitHeight: 20
                                    implicitWidth: roleBadgeText.contentWidth + 12
                                    radius: 10
                                    color: bridge.isCoordinator ? "#065f46" : "#1e3a5f"
                                    border.color: bridge.isCoordinator ? "#10b981" : "#38bdf8"
                                    Text {
                                        id: roleBadgeText
                                        anchors.centerIn: parent
                                        text: bridge.nodeRole
                                        color: "white"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }

                            // Persistent UUID
                            ColumnLayout {
                                spacing: 2
                                Text { text: "Persistent Node UUID (Survives IP & Reboots):"; color: "#64748b"; font.pixelSize: 10 }
                                Text {
                                    text: bridge.nodeUuid
                                    color: "#e2e8f0"
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    font.bold: true
                                }
                            }

                            // Display Name input
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text { text: "Display Name:"; color: "#94a3b8"; font.pixelSize: 11 }
                                TextField {
                                    id: nodeNameInput
                                    Layout.fillWidth: true
                                    text: bridge.nodeDisplayName
                                    color: "#f8fafc"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0f172a"; radius: 4; border.color: "#334155" }
                                }
                                Button {
                                    text: "Save"
                                    implicitWidth: 50
                                    implicitHeight: 28
                                    onClicked: bridge.setNodeDisplayName(nodeNameInput.text)
                                    contentItem: Text { text: parent.text; color: "#38bdf8"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1e3a5f" : "#0f172a"; radius: 4; border.color: "#38bdf8" }
                                }
                            }

                            // Platform & Hardware Details
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Platform: "; color: "#64748b"; font.pixelSize: 10 }
                                Text { text: bridge.nodePlatform; color: "#94a3b8"; font.pixelSize: 10; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: "Status: "; color: "#64748b"; font.pixelSize: 10 }
                                Text { text: bridge.nodeStatus; color: bridge.isCapturing ? "#f59e0b" : "#10b981"; font.pixelSize: 10; font.bold: true }
                            }
                            Text {
                                text: "Hardware: " + bridge.nodeHardware
                                color: "#64748b"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Card 2: Active Session Control
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 190
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "🎯 ACTIVE SESSION MANAGEMENT"
                                color: "#a855f7"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            ColumnLayout {
                                spacing: 2
                                Text { text: "Session ID:"; color: "#64748b"; font.pixelSize: 10 }
                                Text {
                                    text: bridge.isMultiNodeSessionActive ? bridge.activeMultiNodeSessionId : "No Active Multi-Node Session (Standalone)"
                                    color: bridge.isMultiNodeSessionActive ? "#c084fc" : "#94a3b8"
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    font.bold: bridge.isMultiNodeSessionActive
                                }
                            }

                            Text {
                                text: bridge.multiNodeNotice
                                color: "#94a3b8"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }

                            Item { Layout.fillHeight: true }

                            // Session Action Buttons
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Button {
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    visible: !bridge.isMultiNodeSessionActive
                                    text: "👑 Create Multi-Node Session"
                                    onClicked: bridge.createMultiNodeSession("")
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#7e22ce" : "#6b21a8"; radius: 6 }
                                }

                                Button {
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    visible: bridge.isMultiNodeSessionActive && bridge.isCoordinator && !bridge.isCapturing
                                    text: "⚡ Start Coordinated Capture"
                                    onClicked: bridge.startCoordinatedCapture()
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#059669" : "#10b981"; radius: 6 }
                                }

                                Button {
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    visible: bridge.isCapturing
                                    text: "🛑 Stop Coordinated Capture"
                                    onClicked: bridge.stopCoordinatedCapture()
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#dc2626" : "#ef4444"; radius: 6 }
                                }

                                Button {
                                    implicitWidth: 100
                                    implicitHeight: 34
                                    visible: bridge.isMultiNodeSessionActive
                                    text: "🚪 Leave"
                                    onClicked: bridge.leaveMultiNodeSession()
                                    contentItem: Text { text: parent.text; color: "#f87171"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#450a0a" : "#1e1b24"; radius: 6; border.color: "#7f1d1d" }
                                }
                            }
                        }
                    }

                    // Card 3: Routed / VPN / Tailscale Manual Peer
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "🌐 ROUTED / TAILSCALE / VPN PEER"
                                color: "#34d399"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                text: "For non-broadcast routed networks or Tailscale mesh, enter peer IP directly:"
                                color: "#64748b"
                                font.pixelSize: 10
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                TextField {
                                    id: manualPeerIp
                                    Layout.fillWidth: true
                                    placeholderText: "Peer IP (e.g. 100.x.x.x or 192.168.1.50)"
                                    color: "#f8fafc"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0f172a"; radius: 4; border.color: "#334155" }
                                }
                                TextField {
                                    id: manualPeerPort
                                    implicitWidth: 60
                                    text: "9110"
                                    color: "#f8fafc"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0f172a"; radius: 4; border.color: "#334155" }
                                }
                                Button {
                                    text: "Add Peer"
                                    implicitWidth: 80
                                    implicitHeight: 32
                                    onClicked: {
                                        if (manualPeerIp.text.trim() !== "") {
                                            bridge.addManualPeer(manualPeerIp.text.trim(), parseInt(manualPeerPort.text) || 9110);
                                            manualPeerIp.text = "";
                                        }
                                    }
                                    contentItem: Text { text: parent.text; color: "#34d399"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#064e3b" : "#0f172a"; radius: 4; border.color: "#059669" }
                                }
                            }
                        }
                    }
                }

                // --- RIGHT COLUMN: Discovered Nodes & Session Mesh ---
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 12

                    // Discovered Nodes Section
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 280
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "📡 DISCOVERED NODES ON NETWORK (UDP :9110)"
                                    color: "#38bdf8"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "🔄 Refresh"
                                    implicitWidth: 74
                                    implicitHeight: 24
                                    onClicked: bridge.refreshMultiNodeDiscovery()
                                    contentItem: Text { text: parent.text; color: "#94a3b8"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#334155" : "#0f172a"; radius: 4; border.color: "#334155" }
                                }
                            }

                            // Discovered Nodes List
                            ListView {
                                id: discListView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 6
                                model: bridge.discoveredNodes

                                delegate: Rectangle {
                                    width: discListView.width
                                    implicitHeight: 56
                                    radius: 6
                                    color: modelData.isSelf ? "#0f172a" : "#1e293b"
                                    border.color: modelData.isSelf ? "#334155" : "#3b82f6"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 10

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: modelData.isSelf ? "#64748b" : (modelData.isAvailable ? "#10b981" : "#f59e0b")
                                        }

                                        ColumnLayout {
                                            spacing: 2
                                            RowLayout {
                                                spacing: 6
                                                Text {
                                                    text: modelData.displayName
                                                    color: "#f8fafc"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }
                                                Rectangle {
                                                    implicitHeight: 16
                                                    implicitWidth: platBadgeText.contentWidth + 8
                                                    radius: 4
                                                    color: "#0f172a"
                                                    border.color: "#475569"
                                                    Text {
                                                        id: platBadgeText
                                                        anchors.centerIn: parent
                                                        text: modelData.platform
                                                        color: "#94a3b8"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                                Text {
                                                    visible: modelData.isSelf
                                                    text: "(This Device)"
                                                    color: "#64748b"
                                                    font.pixelSize: 10
                                                }
                                            }
                                            Text {
                                                text: modelData.endpoint + " • UUID: " + modelData.shortUuid + " • " + modelData.sessionState
                                                color: "#94a3b8"
                                                font.pixelSize: 10
                                                font.family: "Monospace"
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        // Actions
                                        Button {
                                            visible: !modelData.isSelf && bridge.isCoordinator && modelData.isAvailable
                                            text: "✉️ Invite"
                                            implicitWidth: 70
                                            implicitHeight: 28
                                            onClicked: bridge.inviteNodeToSession(modelData.uuid)
                                            contentItem: Text { text: parent.text; color: "#38bdf8"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.hovered ? "#0369a1" : "#075985"; radius: 4 }
                                        }

                                        Button {
                                            visible: !modelData.isSelf && !bridge.isMultiNodeSessionActive && modelData.currentSessionId !== ""
                                            text: "🔗 Join"
                                            implicitWidth: 70
                                            implicitHeight: 28
                                            onClicked: bridge.joinMultiNodeSession(modelData.currentSessionId, modelData.ip, modelData.port)
                                            contentItem: Text { text: parent.text; color: "#4ade80"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.hovered ? "#15803d" : "#166534"; radius: 4 }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: discListView.count === 0
                                    text: "No network peer nodes currently detected on UDP port 9110."
                                    color: "#64748b"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Active Session Members Section
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#181e28"
                        radius: 8
                        border.color: "#28303e"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "👥 ACTIVE SESSION MEMBERS"
                                    color: "#f472b6"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.isMultiNodeSessionActive ? (bridge.sessionMembers.length + " Node(s) in Mesh") : "Inactive"
                                    color: "#94a3b8"
                                    font.pixelSize: 11
                                }
                            }

                            ListView {
                                id: memberListView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 6
                                model: bridge.sessionMembers

                                delegate: Rectangle {
                                    width: memberListView.width
                                    implicitHeight: 52
                                    radius: 6
                                    color: "#1e293b"
                                    border.color: modelData.connected ? "#10b981" : "#ef4444"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 10

                                        Text {
                                            text: modelData.isCoordinator ? "👑" : "💻"
                                            font.pixelSize: 16
                                        }

                                        ColumnLayout {
                                            spacing: 2
                                            RowLayout {
                                                spacing: 6
                                                Text {
                                                    text: modelData.displayName
                                                    color: "#f8fafc"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }
                                                Rectangle {
                                                    implicitHeight: 16
                                                    implicitWidth: memRoleBadge.contentWidth + 8
                                                    radius: 4
                                                    color: modelData.isCoordinator ? "#065f46" : "#1e3a5f"
                                                    Text {
                                                        id: memRoleBadge
                                                        anchors.centerIn: parent
                                                        text: modelData.role
                                                        color: "white"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                                Rectangle {
                                                    implicitHeight: 16
                                                    implicitWidth: memCapturingBadge.contentWidth + 8
                                                    radius: 4
                                                    color: modelData.capturing ? "#7c2d12" : "#0f172a"
                                                    border.color: modelData.capturing ? "#f97316" : "#334155"
                                                    Text {
                                                        id: memCapturingBadge
                                                        anchors.centerIn: parent
                                                        text: modelData.captureState
                                                        color: modelData.capturing ? "#fb923c" : "#94a3b8"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                            }
                                            Text {
                                                text: modelData.ip + " • Clock Offset: " + modelData.clockOffsetMs + " • UUID: " + modelData.shortUuid
                                                color: "#94a3b8"
                                                font.pixelSize: 10
                                                font.family: "Monospace"
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Rectangle {
                                            implicitHeight: 22
                                            implicitWidth: connStatusText.contentWidth + 12
                                            radius: 11
                                            color: modelData.connected ? "#064e3b" : "#450a0a"
                                            border.color: modelData.connected ? "#10b981" : "#ef4444"
                                            Text {
                                                id: connStatusText
                                                anchors.centerIn: parent
                                                text: modelData.connected ? "CONNECTED" : "OFFLINE"
                                                color: modelData.connected ? "#34d399" : "#f87171"
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: !bridge.isMultiNodeSessionActive || memberListView.count === 0
                                    text: "No active multi-node session mesh members."
                                    color: "#64748b"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }

            // Bottom Footer
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: "Close"
                    implicitWidth: 100
                    implicitHeight: 36
                    onClicked: multiNodeMeshDialog.close()
                    contentItem: Text { text: parent.text; color: "#cbd5e1"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#334155" : "#1e293b"; radius: 6; border.color: "#475569" }
                }
            }
        }
    }

    // =========================================================================
    // Milestone 9: Cross-Sensor / Cross-Capture Offline Registration Dialog
    // =========================================================================
    Dialog {
        id: offlineRegistrationDialog
        modal: true
        width: Math.min(1180, root.width - 40)
        height: Math.min(840, root.height - 40)
        anchors.centerIn: parent
        padding: 0

        property int regMode: 0 // 0: Intra-Session Captures, 1: Cross-Project Clouds
        property string selectedRefId: ""
        property string selectedMovId: ""
        property int selectedScalePolicy: 0 // 0: Locked Metric SE(3), 1: Estimated Sim(3)

        // Cross-Project properties
        property string crossRefProjId: ""
        property string crossRefCloudId: ""
        property string crossMovProjId: ""
        property string crossMovCloudId: ""
        property var crossRefClouds: []
        property var crossMovClouds: []
        property string bannerWarning: ""

        function updateCrossRefClouds() {
            if (crossRefProjId !== "") {
                crossRefClouds = bridge.getProjectClouds(crossRefProjId);
                if (crossRefClouds.length > 0) {
                    crossRefCloudId = crossRefClouds[0].cloud_id;
                } else {
                    crossRefCloudId = "";
                }
            } else {
                crossRefClouds = [];
                crossRefCloudId = "";
            }
        }

        function updateCrossMovClouds() {
            if (crossMovProjId !== "") {
                crossMovClouds = bridge.getProjectClouds(crossMovProjId);
                if (crossMovClouds.length > 0) {
                    crossMovCloudId = crossMovClouds[0].cloud_id;
                } else {
                    crossMovCloudId = "";
                }
            } else {
                crossMovClouds = [];
                crossMovCloudId = "";
            }
        }

        onOpened: {
            bannerWarning = "";
            bridge.refreshData();
            bridge.refreshSessionCaptures();
            bridge.refreshRegistrations();

            var caps = bridge.availableSessionCaptures;
            if (caps.length > 0) {
                if (selectedRefId === "") {
                    for (var i = 0; i < caps.length; ++i) {
                        if (caps[i].capture_id.indexOf("airy") !== -1) {
                            selectedRefId = caps[i].capture_id;
                            break;
                        }
                    }
                    if (selectedRefId === "" && caps.length > 0) selectedRefId = caps[0].capture_id;
                }
                if (selectedMovId === "" || selectedMovId === selectedRefId) {
                    for (var j = 0; j < caps.length; ++j) {
                        if (caps[j].capture_id.indexOf("gemini") !== -1) {
                            selectedMovId = caps[j].capture_id;
                            break;
                        }
                    }
                    if ((selectedMovId === "" || selectedMovId === selectedRefId) && caps.length > 1) {
                        selectedMovId = (caps[0].capture_id === selectedRefId) ? caps[1].capture_id : caps[0].capture_id;
                    }
                }
            }

            var projs = bridge.projectList;
            if (projs.length > 0) {
                if (crossRefProjId === "") {
                    crossRefProjId = (bridge.projectId !== "") ? bridge.projectId : projs[0].id;
                }
                updateCrossRefClouds();

                if (crossMovProjId === "") {
                    if (projs.length > 1) {
                        crossMovProjId = (projs[0].id === crossRefProjId) ? projs[1].id : projs[0].id;
                    } else {
                        crossMovProjId = crossRefProjId;
                    }
                }
                updateCrossMovClouds();
            }

            // Auto-switch to Cross-Project if active project has < 2 captures
            if (caps.length < 2 && projs.length > 0) {
                regMode = 1;
            }
        }

        background: Rectangle {
            color: "#0f172a"
            radius: 12
            border.color: "#334155"
            border.width: 1.5
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            // Dialog Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 38
                    height: 38
                    radius: 8
                    color: "#042f2e"
                    border.color: "#14b8a6"
                    Text {
                        anchors.centerIn: parent
                        text: "🔀"
                        font.pixelSize: 18
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Cross-Capture & Cross-Sensor Offline Registration (Milestone 9)"
                        color: "#f8fafc"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Multi-Scale SAC-IA Coarse Alignment + GICP Fine Alignment • Strict SE(3) Metric Scale Lock • Visual Inspection"
                        color: "#94a3b8"
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "✕"
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: offlineRegistrationDialog.close()
                    contentItem: Text { text: parent.text; color: "#94a3b8"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#334155" : "transparent"; radius: 6 }
                }
            }

            // Mode Switcher Tabs (Intra-Session Captures vs Cross-Project Clouds)
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: offlineRegistrationDialog.regMode === 0 ? "#0369a1" : "#1e293b"
                    border.color: offlineRegistrationDialog.regMode === 0 ? "#38bdf8" : "#334155"
                    border.width: offlineRegistrationDialog.regMode === 0 ? 1.5 : 1
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            offlineRegistrationDialog.regMode = 0;
                            offlineRegistrationDialog.bannerWarning = "";
                        }
                    }
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Text { text: "⚡"; font.pixelSize: 13 }
                        Text {
                            text: "Same Session Captures (Multi-Sensor)"
                            color: offlineRegistrationDialog.regMode === 0 ? "white" : "#94a3b8"
                            font.pixelSize: 11
                            font.bold: offlineRegistrationDialog.regMode === 0
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: offlineRegistrationDialog.regMode === 1 ? "#0f766e" : "#1e293b"
                    border.color: offlineRegistrationDialog.regMode === 1 ? "#14b8a6" : "#334155"
                    border.width: offlineRegistrationDialog.regMode === 1 ? 1.5 : 1
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            offlineRegistrationDialog.regMode = 1;
                            offlineRegistrationDialog.bannerWarning = "";
                            if (offlineRegistrationDialog.crossRefClouds.length === 0) offlineRegistrationDialog.updateCrossRefClouds();
                            if (offlineRegistrationDialog.crossMovClouds.length === 0) offlineRegistrationDialog.updateCrossMovClouds();
                            if (crossRefProjCombo && crossMovProjCombo) {
                                for (var i = 0; i < bridge.projectList.length; ++i) {
                                    if (bridge.projectList[i].id === offlineRegistrationDialog.crossRefProjId) crossRefProjCombo.currentIndex = i;
                                    if (bridge.projectList[i].id === offlineRegistrationDialog.crossMovProjId) crossMovProjCombo.currentIndex = i;
                                }
                            }
                        }
                    }
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Text { text: "🌐"; font.pixelSize: 13 }
                        Text {
                            text: "Cross-Project Alignment (Merge Across Projects)"
                            color: offlineRegistrationDialog.regMode === 1 ? "white" : "#94a3b8"
                            font.pixelSize: 11
                            font.bold: offlineRegistrationDialog.regMode === 1
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#1e293b" }

            // Main Two-Column Body
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                // LEFT COLUMN: Configuration & Registration Execution
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 490
                    color: "#161d2b"
                    radius: 8
                    border.color: "#232e42"
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text {
                            text: offlineRegistrationDialog.regMode === 0 ? "1. SELECT SESSION CAPTURES TO REGISTER" : "1. SELECT CROSS-PROJECT CLOUDS TO MERGE"
                            color: "#38bdf8"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 0.5
                        }

                        // MODE 0: Intra-Session Captures Picker
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: offlineRegistrationDialog.regMode === 0

                            // Reference Capture Picker
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: "Reference Capture (Fixed Anchor Frame):"
                                        color: "#00e5ff"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: "Source A (Cyan)"
                                        color: "#00e5ff"
                                        font.pixelSize: 10
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 68
                                    color: "#0f172a"
                                    radius: 6
                                    border.color: "#1e293b"

                                    ListView {
                                        id: refListView
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: bridge.availableSessionCaptures
                                        delegate: Rectangle {
                                            width: refListView.width
                                            height: 30
                                            radius: 4
                                            color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#0e3a53" : (refMa.containsMouse ? "#1e293b" : "transparent")
                                            border.color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#00e5ff" : "transparent"
                                            MouseArea {
                                                id: refMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: offlineRegistrationDialog.selectedRefId = modelData.capture_id
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                Text {
                                                    text: (modelData.sensor_id.indexOf("airy") !== -1 ? "📡 " : "📷 ") + modelData.sensor_name
                                                    color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#00e5ff" : "#e2e8f0"
                                                    font.pixelSize: 11
                                                    font.bold: offlineRegistrationDialog.selectedRefId === modelData.capture_id
                                                }
                                                Item { Layout.fillWidth: true }
                                                Text {
                                                    text: modelData.point_count + " pts"
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                    font.family: "Monospace"
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: bridge.availableSessionCaptures.length === 0
                                        text: "No session captures found in active project."
                                        color: "#64748b"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            // Moving Capture Picker
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: "Moving Capture (To Be Transformed):"
                                        color: "#ff9100"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: "Source B (Amber)"
                                        color: "#ff9100"
                                        font.pixelSize: 10
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 68
                                    color: "#0f172a"
                                    radius: 6
                                    border.color: "#1e293b"

                                    ListView {
                                        id: movListView
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: bridge.availableSessionCaptures
                                        delegate: Rectangle {
                                            width: movListView.width
                                            height: 30
                                            radius: 4
                                            color: offlineRegistrationDialog.selectedMovId === modelData.capture_id ? "#451a03" : (movMa.containsMouse ? "#1e293b" : "transparent")
                                            border.color: offlineRegistrationDialog.selectedMovId === modelData.capture_id ? "#ff9100" : "transparent"
                                            MouseArea {
                                                id: movMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: offlineRegistrationDialog.selectedMovId = modelData.capture_id
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                Text {
                                                    text: (modelData.sensor_id.indexOf("gemini") !== -1 ? "📷 " : "📡 ") + modelData.sensor_name
                                                    color: offlineRegistrationDialog.selectedMovId === modelData.capture_id ? "#ff9100" : "#e2e8f0"
                                                    font.pixelSize: 11
                                                    font.bold: offlineRegistrationDialog.selectedMovId === modelData.capture_id
                                                }
                                                Item { Layout.fillWidth: true }
                                                Text {
                                                    text: modelData.point_count + " pts"
                                                    color: "#94a3b8"
                                                    font.pixelSize: 10
                                                    font.family: "Monospace"
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: bridge.availableSessionCaptures.length === 0
                                        text: "No session captures found in active project."
                                        color: "#64748b"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        // MODE 1: Cross-Project Selection Pickers
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: offlineRegistrationDialog.regMode === 1

                            // Project A & Cloud A Picker (Reference - Cyan)
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: "Project A (Reference Anchor):"
                                        color: "#00e5ff"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "Source A (Cyan)"; color: "#00e5ff"; font.pixelSize: 10 }
                                }

                                ComboBox {
                                    id: crossRefProjCombo
                                    Layout.fillWidth: true
                                    implicitHeight: 32
                                    model: bridge.projectList
                                    textRole: "name"
                                    onActivated: {
                                        var item = bridge.projectList[currentIndex];
                                        if (item) {
                                            offlineRegistrationDialog.crossRefProjId = item.id;
                                            offlineRegistrationDialog.updateCrossRefClouds();
                                        }
                                    }
                                    background: Rectangle {
                                        color: "#0f172a"
                                        border.color: "#00e5ff"
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        leftPadding: 10
                                        rightPadding: 24
                                        text: crossRefProjCombo.displayText
                                        color: "#00e5ff"
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 58
                                    color: "#0a0e17"
                                    radius: 6
                                    border.color: "#1e293b"

                                    ListView {
                                        id: crossRefCloudsListView
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: offlineRegistrationDialog.crossRefClouds
                                        delegate: Rectangle {
                                            width: crossRefCloudsListView.width
                                            height: 26
                                            radius: 4
                                            color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#0e3a53" : (refCma.containsMouse ? "#1e293b" : "transparent")
                                            border.color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#00e5ff" : "transparent"
                                            MouseArea {
                                                id: refCma
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: offlineRegistrationDialog.crossRefCloudId = modelData.cloud_id
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 6
                                                anchors.rightMargin: 6
                                                Text {
                                                    text: modelData.display_text
                                                    color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#00e5ff" : "#e2e8f0"
                                                    font.pixelSize: 10
                                                    font.bold: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        visible: offlineRegistrationDialog.crossRefClouds.length === 0
                                        text: "No point clouds found in Project A."
                                        color: "#64748b"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            // Project B & Cloud B Picker (Moving - Amber)
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                RowLayout {
                                    Text {
                                        text: "Project B (Moving to Align):"
                                        color: "#ff9100"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "Source B (Amber)"; color: "#ff9100"; font.pixelSize: 10 }
                                }

                                ComboBox {
                                    id: crossMovProjCombo
                                    Layout.fillWidth: true
                                    implicitHeight: 32
                                    model: bridge.projectList
                                    textRole: "name"
                                    onActivated: {
                                        var item = bridge.projectList[currentIndex];
                                        if (item) {
                                            offlineRegistrationDialog.crossMovProjId = item.id;
                                            offlineRegistrationDialog.updateCrossMovClouds();
                                        }
                                    }
                                    background: Rectangle {
                                        color: "#0f172a"
                                        border.color: "#ff9100"
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        leftPadding: 10
                                        rightPadding: 24
                                        text: crossMovProjCombo.displayText
                                        color: "#ff9100"
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 58
                                    color: "#0a0e17"
                                    radius: 6
                                    border.color: "#1e293b"

                                    ListView {
                                        id: crossMovCloudsListView
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: offlineRegistrationDialog.crossMovClouds
                                        delegate: Rectangle {
                                            width: crossMovCloudsListView.width
                                            height: 26
                                            radius: 4
                                            color: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id ? "#451a03" : (movCma.containsMouse ? "#1e293b" : "transparent")
                                            border.color: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id ? "#ff9100" : "transparent"
                                            MouseArea {
                                                id: movCma
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: offlineRegistrationDialog.crossMovCloudId = modelData.cloud_id
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 6
                                                anchors.rightMargin: 6
                                                Text {
                                                    text: modelData.display_text
                                                    color: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id ? "#ff9100" : "#e2e8f0"
                                                    font.pixelSize: 10
                                                    font.bold: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        visible: offlineRegistrationDialog.crossMovClouds.length === 0
                                        text: "No point clouds found in Project B."
                                        color: "#64748b"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        // Scale Policy Selector
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Scale Policy Invariant:"
                                color: "#cbd5e1"
                                font.bold: true
                                font.pixelSize: 11
                            }

                            RowLayout {
                                spacing: 8
                                Rectangle {
                                    implicitWidth: 220
                                    implicitHeight: 30
                                    radius: 6
                                    color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#064e3b" : "#0f172a"
                                    border.color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#10b981" : "#334155"
                                    border.width: offlineRegistrationDialog.selectedScalePolicy === 0 ? 1.5 : 1
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: offlineRegistrationDialog.selectedScalePolicy = 0
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: "🔒 Locked SE(3) (Scale = 1.000)"
                                        color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#6ee7b7" : "#94a3b8"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    implicitWidth: 210
                                    implicitHeight: 30
                                    radius: 6
                                    color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#3b0764" : "#0f172a"
                                    border.color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#a855f7" : "#334155"
                                    border.width: offlineRegistrationDialog.selectedScalePolicy === 1 ? 1.5 : 1
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: offlineRegistrationDialog.selectedScalePolicy = 1
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: "📏 Sim(3) (Scale Ambiguous)"
                                        color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#d8b4fe" : "#94a3b8"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        // Actionable Warning Banner (if user clicks without valid selection)
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: bannerWarningText.contentHeight + 12
                            visible: offlineRegistrationDialog.bannerWarning !== ""
                            radius: 6
                            color: "#451a03"
                            border.color: "#f59e0b"
                            Text {
                                id: bannerWarningText
                                anchors.centerIn: parent
                                width: parent.width - 16
                                text: offlineRegistrationDialog.bannerWarning
                                color: "#fef3c7"
                                font.pixelSize: 10
                                font.bold: true
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }

                        // Run Alignment Button (Always Interactive / Never Blocked)
                        Button {
                            id: runRegBtn
                            Layout.fillWidth: true
                            implicitHeight: 42
                            enabled: !bridge.isRegistering
                            onClicked: {
                                offlineRegistrationDialog.bannerWarning = "";
                                if (offlineRegistrationDialog.regMode === 0) {
                                    var rId = offlineRegistrationDialog.selectedRefId;
                                    var mId = offlineRegistrationDialog.selectedMovId;
                                    if (rId === "" || mId === "") {
                                        var caps = bridge.availableSessionCaptures;
                                        if (caps.length >= 2) {
                                            rId = caps[0].capture_id;
                                            mId = caps[1].capture_id;
                                            offlineRegistrationDialog.selectedRefId = rId;
                                            offlineRegistrationDialog.selectedMovId = mId;
                                        } else {
                                            offlineRegistrationDialog.bannerWarning = "⚠️ Current project has fewer than 2 captures. Click 'Cross-Project Alignment' above to merge point clouds across projects!";
                                            return;
                                        }
                                    }
                                    if (rId === mId) {
                                        offlineRegistrationDialog.bannerWarning = "⚠️ Reference and Moving captures must be distinct.";
                                        return;
                                    }
                                    bridge.runCaptureRegistration(rId, mId, offlineRegistrationDialog.selectedScalePolicy);
                                } else {
                                    var rPid = offlineRegistrationDialog.crossRefProjId;
                                    var rCid = offlineRegistrationDialog.crossRefCloudId;
                                    var mPid = offlineRegistrationDialog.crossMovProjId;
                                    var mCid = offlineRegistrationDialog.crossMovCloudId;
                                    if (rCid === "" || mCid === "") {
                                        offlineRegistrationDialog.bannerWarning = "⚠️ Please select a point cloud for both Project A and Project B.";
                                        return;
                                    }
                                    if (rPid === mPid && rCid === mCid) {
                                        offlineRegistrationDialog.bannerWarning = "⚠️ Reference and Moving clouds cannot be the exact same point cloud.";
                                        return;
                                    }
                                    bridge.runRegistration(rPid, rCid, mPid, mCid, offlineRegistrationDialog.selectedScalePolicy);
                                }
                            }
                            contentItem: RowLayout {
                                anchors.centerIn: parent
                                spacing: 8
                                Text {
                                    text: bridge.isRegistering ? "⏳" : "▶"
                                    font.pixelSize: 14
                                    color: "white"
                                }
                                Text {
                                    text: bridge.isRegistering ? "Computing SAC-IA & Fine GICP..." : "Run Automatic SAC-IA + GICP Alignment"
                                    color: "white"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }
                            background: Rectangle {
                                color: parent.enabled ? (parent.hovered ? "#0284c7" : "#0369a1") : "#1e293b"
                                radius: 6
                                border.color: parent.enabled ? "#38bdf8" : "#334155"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: bridge.registrationStatus
                            color: "#94a3b8"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#232e42" }

                        // Saved Registrations List
                        Text {
                            text: "2. SAVED SESSION REGISTRATIONS"
                            color: "#38bdf8"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 0.5
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#0f172a"
                            radius: 6
                            border.color: "#1e293b"

                            ListView {
                                id: regHistoryListView
                                anchors.fill: parent
                                anchors.margins: 4
                                clip: true
                                model: bridge.registrationList
                                delegate: Rectangle {
                                    width: regHistoryListView.width
                                    height: 38
                                    radius: 4
                                    color: regHistoryMa.containsMouse ? "#1e293b" : "transparent"
                                    border.color: "#232e42"
                                    MouseArea {
                                        id: regHistoryMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: bridge.loadRegistration(modelData.registration_id)
                                    }
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: modelData.registration_id + " (" + modelData.status + ")"
                                                color: modelData.status === "CONVERGED" ? "#10b981" : "#f59e0b"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            Text {
                                                text: modelData.overlap_percent.toFixed(1) + "% overlap • " + modelData.inlier_rmse_mm.toFixed(1) + " mm RMSE"
                                                color: "#94a3b8"
                                                font.pixelSize: 9
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: "Load ➔"
                                            color: "#38bdf8"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: bridge.registrationList.length === 0
                                text: "No registrations computed yet for this session."
                                color: "#64748b"
                                font.pixelSize: 11
                            }
                        }
                    }
                }

                // RIGHT COLUMN: Alignment Results & Mathematical Metrics & Inspection Tools
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    color: "#161d2b"
                    radius: 8
                    border.color: "#232e42"
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 12

                        // Header Status
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "ALIGNMENT RESULTS & QUANTITATIVE METRICS"
                                color: "#38bdf8"
                                font.bold: true
                                font.pixelSize: 11
                                font.letterSpacing: 0.5
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: statusPillText.contentWidth + 14
                                radius: 4
                                color: bridge.isRegistrationActive ? "#064e3b" : "#1e293b"
                                border.color: bridge.isRegistrationActive ? "#10b981" : "#475569"
                                Text {
                                    id: statusPillText
                                    anchors.centerIn: parent
                                    text: bridge.isRegistrationActive ? "CONVERGED" : "NOT ALIGNED"
                                    color: bridge.isRegistrationActive ? "#6ee7b7" : "#94a3b8"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        // Metric Cards Grid
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 4
                            rowSpacing: 8
                            columnSpacing: 8

                            // Card 1: Overlap
                            Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                color: "#0f172a"
                                radius: 6
                                border.color: "#1e293b"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "INLIER OVERLAP"; color: "#64748b"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationOverlapPercent.toFixed(1) + " %") : "--"
                                        color: bridge.registrationOverlapPercent >= 85.0 ? "#10b981" : "#f59e0b"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }
                            }

                            // Card 2: RMSE
                            Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                color: "#0f172a"
                                radius: 6
                                border.color: "#1e293b"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "INLIER RMSE"; color: "#64748b"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationRmseMm.toFixed(1) + " mm") : "--"
                                        color: bridge.registrationRmseMm <= 60.0 ? "#10b981" : "#ef4444"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }
                            }

                            // Card 3: Scale
                            Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                color: "#0f172a"
                                radius: 6
                                border.color: "#1e293b"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "SCALE FACTOR"; color: "#64748b"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationScale.toFixed(6)) : "--"
                                        color: "#38bdf8"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }
                            }

                            // Card 4: Fitness
                            Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                color: "#0f172a"
                                radius: 6
                                border.color: "#1e293b"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "GICP FITNESS"; color: "#64748b"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationFitness.toFixed(4)) : "--"
                                        color: "#a7f3d0"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        // Transform Matrix SE(3)
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Text {
                                text: "Solved SE(3) Rigid Transformation Matrix T_Ref←Mov:"
                                color: "#cbd5e1"
                                font.pixelSize: 10
                                font.bold: true
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 86
                                color: "#0a0e17"
                                radius: 6
                                border.color: "#1e293b"
                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.isRegistrationActive ? bridge.registrationTransformStr : "[ Identity / Unregistered ]"
                                    color: "#38bdf8"
                                    font.family: "Monospace"
                                    font.pixelSize: 11
                                    lineHeight: 1.2
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#232e42" }

                        // Visual Distinction & Inspection Tools
                        Text {
                            text: "INTERACTIVE SOURCE DISTINCTION & INSPECTION (M9 ACCEPTANCE)"
                            color: "#38bdf8"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 0.5
                        }

                        // View Mode Controls
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "Select Display Cloud View:"
                                color: "#cbd5e1"
                                font.pixelSize: 10
                            }

                            RowLayout {
                                spacing: 8
                                Repeater {
                                    model: [
                                        { label: "Combined Cloud", mode: 0, desc: "Reference + Transformed Moving" },
                                        { label: "Source A: Airy (Ref)", mode: 1, desc: "LiDAR Anchor Only" },
                                        { label: "Source B: Gemini (Mov)", mode: 2, desc: "Aligned RGB-D Only" }
                                    ]
                                    delegate: Rectangle {
                                        property bool isSel: bridge.registrationViewMode === modelData.mode
                                        implicitHeight: 34
                                        Layout.fillWidth: true
                                        radius: 6
                                        color: isSel ? "#0369a1" : "#0f172a"
                                        border.color: isSel ? "#38bdf8" : "#334155"
                                        border.width: isSel ? 1.5 : 1
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                bridge.setRegistrationViewMode(modelData.mode);
                                                canvas3d.requestPaint();
                                                canvas2d.requestPaint();
                                            }
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: isSel ? "white" : "#cbd5e1"
                                            font.pixelSize: 11
                                            font.bold: isSel
                                        }
                                    }
                                }
                            }
                        }

                        // Color Mode Toggle & Legend
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            RowLayout {
                                Text {
                                    text: "Color Differentiation Scheme:"
                                    color: "#cbd5e1"
                                    font.pixelSize: 10
                                }
                                Item { Layout.fillWidth: true }
                                RowLayout {
                                    spacing: 6
                                    Rectangle {
                                        implicitHeight: 28
                                        implicitWidth: 155
                                        radius: 4
                                        color: bridge.registrationColorMode === 1 ? "#0f766e" : "#0f172a"
                                        border.color: bridge.registrationColorMode === 1 ? "#14b8a6" : "#334155"
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                bridge.setRegistrationColorMode(1);
                                                canvas3d.requestPaint();
                                                canvas2d.requestPaint();
                                            }
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: "📷 True Merged RGB"
                                            color: bridge.registrationColorMode === 1 ? "#5eead4" : "#94a3b8"
                                            font.pixelSize: 10
                                            font.bold: bridge.registrationColorMode === 1
                                        }
                                    }
                                    Rectangle {
                                        implicitHeight: 28
                                        implicitWidth: 155
                                        radius: 4
                                        color: bridge.registrationColorMode === 0 ? "#7c2d12" : "#0f172a"
                                        border.color: bridge.registrationColorMode === 0 ? "#f97316" : "#334155"
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                bridge.setRegistrationColorMode(0);
                                                canvas3d.requestPaint();
                                                canvas2d.requestPaint();
                                            }
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            text: "🎨 Cyan / Amber Diagnostic"
                                            color: bridge.registrationColorMode === 0 ? "#fed7aa" : "#94a3b8"
                                            font.pixelSize: 10
                                            font.bold: bridge.registrationColorMode === 0
                                        }
                                    }
                                }
                            }

                            // Visual Legend Box
                            Rectangle {
                                Layout.fillWidth: true
                                height: 50
                                color: "#0a0e17"
                                radius: 6
                                border.color: "#1e293b"

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 24

                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            color: bridge.registrationColorMode === 0 ? "#00e5ff" : "#94a3b8"
                                        }
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Reference Anchor Cloud" : "Reference Cloud (LiDAR / Sensor)"
                                                color: bridge.registrationColorMode === 0 ? "#00e5ff" : "#e2e8f0"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Diagnostic Cyan (#00E5FF)" : "Preserved Sensor Reflectance / True Color"
                                                color: "#64748b"
                                                font.pixelSize: 9
                                            }
                                        }
                                    }

                                    Rectangle { width: 1; height: 30; color: "#1e293b" }

                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            color: bridge.registrationColorMode === 0 ? "#ff9100" : "#38bdf8"
                                        }
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Moving Aligned Cloud" : "Moving Cloud (Camera / RGB-D)"
                                                color: bridge.registrationColorMode === 0 ? "#ff9100" : "#38bdf8"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Diagnostic Amber (#FF9100)" : "Preserved Photorealistic Sensor RGB"
                                                color: "#64748b"
                                                font.pixelSize: 9
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        // Session Actions
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                id: saveSessionMapBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                enabled: bridge.isRegistrationActive
                                onClicked: bridge.saveRegistrationAsSessionMap("")
                                contentItem: Text {
                                    text: "💾 Set as Active Session Map"
                                    color: parent.enabled ? "white" : "#64748b"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#059669" : "#047857") : "#1e293b"
                                    radius: 6
                                    border.color: parent.enabled ? "#10b981" : "#334155"
                                }
                            }

                            Button {
                                id: saveMergedProjBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                enabled: bridge.isRegistrationActive
                                onClicked: newMergedProjectDialog.open()
                                contentItem: Text {
                                    text: "📁 Save as New Merged Project..."
                                    color: parent.enabled ? "white" : "#64748b"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#7c3aed" : "#6d28d9") : "#1e293b"
                                    radius: 6
                                    border.color: parent.enabled ? "#a78bfa" : "#334155"
                                }
                            }

                            Button {
                                id: inspect3dBtn
                                Layout.fillWidth: true
                                implicitHeight: 38
                                enabled: bridge.isRegistrationActive
                                onClicked: {
                                    offlineRegistrationDialog.close();
                                    canvas3d.requestPaint();
                                    canvas2d.requestPaint();
                                }
                                contentItem: Text {
                                    text: "🔍 Inspect in Main 3D View"
                                    color: "white"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#0284c7" : "#0284c7"
                                    radius: 6
                                    border.color: "#38bdf8"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Modal: Save Merged Registration As New Project
    Dialog {
        id: newMergedProjectDialog
        modal: true
        width: 480
        anchors.centerIn: parent
        padding: 0
        background: Rectangle {
            color: "#0f172a"
            radius: 10
            border.color: "#334155"
            border.width: 1.5
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "📁"; font.pixelSize: 18 }
                Text {
                    text: "Save as New Merged Project"
                    color: "white"
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Text {
                text: "Create an independent AV Scan project containing the registered point cloud with complete transformation metadata."
                color: "#94a3b8"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: "Project Name:"; color: "#cbd5e1"; font.pixelSize: 11; font.bold: true }
                TextField {
                    id: mergedProjNameInput
                    Layout.fillWidth: true
                    text: "Merged_Scan_" + Qt.formatDateTime(new Date(), "yyyyMMdd_hhmm")
                    color: "white"
                    background: Rectangle { color: "#1e293b"; radius: 6; border.color: "#475569" }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: "Description (Optional):"; color: "#cbd5e1"; font.pixelSize: 11 }
                TextField {
                    id: mergedProjDescInput
                    Layout.fillWidth: true
                    placeholderText: "e.g. Combined scan merging separate sensor captures"
                    placeholderTextColor: "#64748b"
                    color: "white"
                    background: Rectangle { color: "#1e293b"; radius: 6; border.color: "#475569" }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    Layout.fillWidth: true
                    text: "Cancel"
                    onClicked: newMergedProjectDialog.close()
                    contentItem: Text { text: parent.text; color: "#94a3b8"; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "#1e293b"; radius: 6; border.color: "#334155" }
                }
                Button {
                    Layout.fillWidth: true
                    text: "Create & Open Project"
                    onClicked: {
                        var ok = bridge.saveRegistrationAsNewProject(mergedProjNameInput.text, mergedProjDescInput.text);
                        if (ok) {
                            newMergedProjectDialog.close();
                            offlineRegistrationDialog.close();
                            canvas3d.requestPaint();
                            canvas2d.requestPaint();
                        }
                    }
                    contentItem: Text { text: parent.text; color: "white"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "#7c3aed"; radius: 6; border.color: "#a78bfa" }
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
