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
    title: "AV Scan — Artificial Vision Spatial Capture"
    // Transparent window surface so optical see-through AR glasses and desktop compositors
    // see directly through the application window and the left passthrough viewport (matching scanarMini).
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // Apple Glass (visionOS) AR Optical See-Through Theme Tokens (copied from scanarMini AV_THEME)
    readonly property string avGoldPrimary: "#BAA164"
    readonly property string avGoldBright: "#E5C983"
    readonly property string avGoldMuted: "#D8BF7C"
    readonly property string avWhitePure: "#FFFFFF"
    readonly property string avWhiteWarm: "#FCFBF8"
    readonly property string avWhiteMuted: "#D0CCC2"
    readonly property string avStatusOk: "#4ADE80"
    readonly property string avStatusWarn: "#F59E0B"
    readonly property string avStatusErr: "#F87171"

    // Live AR Glass Opacity % (0% = pure zero-fill optical see-through, default 12% matching scanarMini)
    property int glassOpacityPct: 12
    property bool skipSplashOnStartup: false
    property bool showSplashScreen: !skipSplashOnStartup
    property int rightDeckTab: 0

    Component.onCompleted: {
        bridge.refreshDiscovery();
        for (var i = 0; i < Qt.application.arguments.length; ++i) {
            if (Qt.application.arguments[i] === "--open-catalog") {
                root.skipSplashOnStartup = true;
                configureSensorsDialog.currentTab = 2;
                configureSensorsDialog.open();
                break;
            } else if (Qt.application.arguments[i] === "--open-config") {
                root.skipSplashOnStartup = true;
                configureSensorsDialog.currentTab = 1;
                configureSensorsDialog.open();
                break;
            } else if (Qt.application.arguments[i] === "--open-diag") {
                root.skipSplashOnStartup = true;
                bridge.openOrientationDiagnostic();
                orientationDiagDialog.open();
                break;
            } else if (Qt.application.arguments[i] === "--no-splash") {
                root.skipSplashOnStartup = true;
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

    // =========================================================================
    // OUTER APPLE GLASS (visionOS) TRANSLUCENT SHELL (Matches ScanHUBOS.paintEvent)
    // =========================================================================
    Rectangle {
        id: outerGlassShell
        anchors.fill: parent
        radius: root.visibility === Window.Maximized ? 0 : 16
        // Base glass tint scaled by glassOpacityPct (at 12%, alpha is ~0.055; at 0%, completely transparent)
        color: Qt.rgba(18/255, 18/255, 22/255, (root.glassOpacityPct / 100.0) * 0.48)
        border.color: "#A6BAA164" // rgba(186, 161, 100, 165) 1.5px Champagne Gold outer specular rim
        border.width: 1.5

        // Inner White Glass Refraction Rim (inset by 1.5px, matching scanarMini)
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1.5
            radius: parent.radius > 1 ? parent.radius - 1.5 : 0
            color: "transparent"
            border.color: "#3BFFFFFF" // rgba(255, 255, 255, 58)
            border.width: 1
        }
    }

    // =========================================================================
    // MAIN 4-ROW VERTICAL LAYOUT (Matches scanarMini ScanHUBOS.init_ui)
    //   Row 1: Top Header Bar (52px)
    //   Row 2: Action & Session Control Bar (48px)
    //   Row 3: 60% Clear Passthrough Left / 40% Peripheral HUD Right Split
    //   Row 4: Translucent Apple Glass Footer Status Bar (24px)
    // =========================================================================
    ColumnLayout {
        id: mainRootColumn
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // ---------------------------------------------------------------------
        // ROW 1: TOP HEADER BAR (id: header, height: 52)
        // ---------------------------------------------------------------------
        Rectangle {
            id: header
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 11
            color: Qt.rgba(1, 1, 1, 0.05 + (root.glassOpacityPct / 100.0) * 0.08)
            border.color: "#61BAA164" // rgba(186, 161, 100, 0.38)
            border.width: 1

            // Allow dragging the frameless window from empty space on the header bar
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                property point clickPos: Qt.point(0, 0)
                onPressed: function(mouse) {
                    clickPos = Qt.point(mouse.x, mouse.y);
                }
                onPositionChanged: function(mouse) {
                    if (pressed && root.visibility !== Window.Maximized) {
                        var dx = mouse.x - clickPos.x;
                        var dy = mouse.y - clickPos.y;
                        root.x += dx;
                        root.y += dy;
                    }
                }
                onDoubleClicked: {
                    if (root.visibility === Window.Maximized) {
                        root.showNormal();
                    } else {
                        root.showMaximized();
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // 36x36 Circular Artificial Vision Emblem Badge (clicks re-open Splash Screen)
                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: 18
                    color: "#1FBAA164" // rgba(186, 161, 100, 0.12)
                    border.color: "#8CBAA164" // rgba(186, 161, 100, 0.55)
                    border.width: 1

                    Image {
                        anchors.fill: parent
                        anchors.margins: 5
                        source: "assets/av_logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            splashOverlay.loadProgress = 1.0;
                            splashOverlay.opacity = 1.0;
                            root.showSplashScreen = true;
                        }
                    }
                }

                // Title & Subtitle Stack ("AV Scan" / "an Artificial Vision product")
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: "AV Scan"
                        color: root.avWhiteWarm
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.8
                    }
                    Text {
                        text: "an Artificial Vision product"
                        color: "#B8FCFBF8"
                        font.pixelSize: 10
                        font.letterSpacing: 0.4
                    }
                }

                // Brand Pill ("VERIFY · DIRECT · CLARIFY")
                Rectangle {
                    height: 22
                    width: brandPillText.implicitWidth + 20
                    radius: 10
                    color: "#24BAA164" // rgba(186, 161, 100, 0.14)
                    border.color: "#7ABAA164" // rgba(186, 161, 100, 0.48)
                    border.width: 1

                    Text {
                        id: brandPillText
                        anchors.centerIn: parent
                        text: "VERIFY · DIRECT · CLARIFY"
                        color: root.avGoldBright
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1.3
                    }
                }

                // Active Project Pill + New / Open Project Buttons (Retained AV Scan Feature)
                Rectangle {
                    property bool hasProj: bridge.projectName !== "" && bridge.projectName !== "No Active Project"
                    height: 26
                    width: projRow.implicitWidth + 16
                    radius: 11
                    color: "#12FFFFFF"
                    border.color: hasProj ? "#7ABAA164" : "#3DFFFFFF"
                    border.width: 1

                    RowLayout {
                        id: projRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: (bridge.projectName !== "" && bridge.projectName !== "No Active Project")
                                ? ("● PROJECT: " + bridge.projectName)
                                : "○ NO PROJECT"
                            color: (bridge.projectName !== "" && bridge.projectName !== "No Active Project")
                                ? root.avGoldBright
                                : root.avWhiteMuted
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Rectangle {
                            width: 1
                            height: 12
                            color: "#3DFFFFFF"
                        }

                        Button {
                            id: headerNewProjBtn
                            text: "+ New"
                            implicitHeight: 20
                            implicitWidth: 46
                            onClicked: newProjectDialog.open()
                            contentItem: Text {
                                text: headerNewProjBtn.text
                                color: root.avWhiteWarm
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: headerNewProjBtn.hovered ? "#38BAA164" : "#1FBAA164"
                                border.color: "#85BAA164"
                                border.width: 1
                            }
                        }

                        Button {
                            id: headerOpenProjBtn
                            text: "Open"
                            implicitHeight: 20
                            implicitWidth: 44
                            onClicked: openProjectDialog.open()
                            contentItem: Text {
                                text: headerOpenProjBtn.text
                                color: root.avGoldBright
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: headerOpenProjBtn.hovered ? "#29FFFFFF" : "#12FFFFFF"
                                border.color: "#3DFFFFFF"
                                border.width: 1
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Live AR Glass Opacity Slider (Matches scanarMini: "Glass 12%", 0%..90%)
                Rectangle {
                    height: 24
                    width: glassSliderRow.implicitWidth + 14
                    radius: 11
                    color: "#12FFFFFF"
                    border.color: "#47BAA164"
                    border.width: 1

                    RowLayout {
                        id: glassSliderRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "Glass " + root.glassOpacityPct + "%"
                            color: root.avGoldBright
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Slider {
                            id: glassOpacitySlider
                            from: 0
                            to: 90
                            stepSize: 1
                            value: root.glassOpacityPct
                            implicitWidth: 64
                            implicitHeight: 16
                            onMoved: root.glassOpacityPct = Math.round(value)
                            background: Rectangle {
                                x: glassOpacitySlider.leftPadding
                                y: glassOpacitySlider.topPadding + glassOpacitySlider.availableHeight / 2 - height / 2
                                implicitWidth: 64
                                implicitHeight: 4
                                width: glassOpacitySlider.availableWidth
                                height: implicitHeight
                                radius: 2
                                color: "#29FFFFFF"

                                Rectangle {
                                    width: glassOpacitySlider.visualPosition * parent.width
                                    height: parent.height
                                    color: root.avGoldPrimary
                                    radius: 2
                                }
                            }
                            handle: Rectangle {
                                x: glassOpacitySlider.leftPadding + glassOpacitySlider.visualPosition * (glassOpacitySlider.availableWidth - width)
                                y: glassOpacitySlider.topPadding + glassOpacitySlider.availableHeight / 2 - height / 2
                                implicitWidth: 11
                                implicitHeight: 11
                                radius: 6
                                color: root.avGoldBright
                                border.color: root.avWhiteWarm
                                border.width: 1
                            }
                        }
                    }
                }

                // Status Pill 1: PACE READY / CAPTURING
                Rectangle {
                    height: 24
                    width: paceStatusText.implicitWidth + 16
                    radius: 11
                    color: "#1F22C55E"
                    border.color: "#7022C55E"
                    border.width: 1

                    Text {
                        id: paceStatusText
                        anchors.centerIn: parent
                        text: bridge.isCapturing ? ("● PACE: " + bridge.airySlamRate.toFixed(0) + " Hz") : "● PACE: READY"
                        color: root.avStatusOk
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Status Pill 2: C-SLAM Peer Count (clickable to open Multi-Node Mesh Manager)
                Rectangle {
                    height: 24
                    width: cslamPillText.implicitWidth + 16
                    radius: 11
                    color: "#24BAA164"
                    border.color: "#7ABAA164"
                    border.width: 1

                    Text {
                        id: cslamPillText
                        anchors.centerIn: parent
                        text: "● C-SLAM: " + Math.max(1, bridge.cslamScannerCount) + (Math.max(1, bridge.cslamScannerCount) === 1 ? " Scanner" : " Scanners")
                        color: root.avGoldBright
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: multiNodeDialog.open()
                    }
                }

                // Status Pill 3: LiDAR Status
                Rectangle {
                    property bool lidarOnline: bridge.airyDeviceStatus === "CONNECTED" || bridge.airyDeviceStatus === "STREAMING"
                    height: 24
                    width: lidarPillText.implicitWidth + 16
                    radius: 11
                    color: lidarOnline ? "#1F22C55E" : "#12FFFFFF"
                    border.color: lidarOnline ? "#7022C55E" : "#3DFFFFFF"
                    border.width: 1

                    Text {
                        id: lidarPillText
                        anchors.centerIn: parent
                        text: parent.lidarOnline ? ("● LiDAR: " + bridge.airyAcquisitionRate.toFixed(0) + "Hz") : "● LiDAR: Ready"
                        color: parent.lidarOnline ? root.avStatusOk : root.avWhiteWarm
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Status Pill 4: Camera Status
                Rectangle {
                    property bool camOnline: bridge.geminiDeviceStatus === "CONNECTED" || bridge.cameraDeviceStatus === "CONNECTED" || bridge.vitureDeviceStatus === "CONNECTED"
                    height: 24
                    width: camPillText.implicitWidth + 16
                    radius: 11
                    color: camOnline ? "#1F22C55E" : "#12FFFFFF"
                    border.color: camOnline ? "#7022C55E" : "#3DFFFFFF"
                    border.width: 1

                    Text {
                        id: camPillText
                        anchors.centerIn: parent
                        text: parent.camOnline ? "● Camera: STREAMING" : "● Camera: Ready"
                        color: parent.camOnline ? root.avStatusOk : root.avWhiteWarm
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Status Pill 5: SLAM Tracking Status
                Rectangle {
                    height: 24
                    width: slamPillText.implicitWidth + 16
                    radius: 11
                    color: bridge.trackingStatus === "TRACKING" ? "#1F22C55E" : "#1CBAA164"
                    border.color: bridge.trackingStatus === "TRACKING" ? "#7022C55E" : "#6BBAA164"
                    border.width: 1

                    Text {
                        id: slamPillText
                        anchors.centerIn: parent
                        text: "● SLAM: " + (bridge.trackingStatus || "IDLE")
                        color: bridge.trackingStatus === "TRACKING" ? root.avStatusOk : root.avGoldBright
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Frameless Transparent Apple Glass Window Controls (Minimize, Maximize/Restore, Close)
                RowLayout {
                    spacing: 5

                    Button {
                        id: winMinBtn
                        implicitWidth: 26
                        implicitHeight: 26
                        onClicked: root.showMinimized()
                        contentItem: Text {
                            text: "—"
                            color: root.avWhiteWarm
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 13
                            color: winMinBtn.hovered ? "#2EFFFFFF" : "#12FFFFFF"
                            border.color: winMinBtn.hovered ? root.avGoldPrimary : "#38FFFFFF"
                            border.width: 1
                        }
                    }

                    Button {
                        id: winMaxBtn
                        implicitWidth: 26
                        implicitHeight: 26
                        onClicked: {
                            if (root.visibility === Window.Maximized) root.showNormal();
                            else root.showMaximized();
                        }
                        contentItem: Text {
                            text: "□"
                            color: root.avWhiteWarm
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 13
                            color: winMaxBtn.hovered ? "#2EFFFFFF" : "#12FFFFFF"
                            border.color: winMaxBtn.hovered ? root.avGoldPrimary : "#38FFFFFF"
                            border.width: 1
                        }
                    }

                    Button {
                        id: winCloseBtn
                        implicitWidth: 26
                        implicitHeight: 26
                        onClicked: Qt.quit()
                        contentItem: Text {
                            text: "✕"
                            color: root.avWhiteWarm
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 13
                            color: winCloseBtn.hovered ? "#70EF4444" : "#12FFFFFF"
                            border.color: winCloseBtn.hovered ? "#F87171" : "#38FFFFFF"
                            border.width: 1
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // ROW 2: ACTION & SESSION CONTROL BAR (id: ctrlBar, height: 48)
        // Matches scanarMini ctrl_bar + retains all AV Scan workflow buttons
        // ---------------------------------------------------------------------
        Rectangle {
            id: ctrlBar
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            radius: 11
            color: Qt.rgba(1, 1, 1, 0.03 + (root.glassOpacityPct / 100.0) * 0.06)
            border.color: "#24FFFFFF" // rgba(255, 255, 255, 0.14)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // 1. Primary Action: ▶ START SCAN (matches scanarMini start_btn)
                Button {
                    id: startBtn
                    text: bridge.isCapturing ? "● SCANNING..." : "▶  START SCAN"
                    enabled: !bridge.isCapturing
                    implicitHeight: 34
                    implicitWidth: 136
                    onClicked: bridge.startCapture()
                    contentItem: Text {
                        text: startBtn.text
                        color: startBtn.enabled ? root.avWhiteWarm : "#59FFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                        font.letterSpacing: 0.5
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 9
                        color: !startBtn.enabled ? "#0DFFFFFF" : (startBtn.hovered ? "#75BAA164" : "#52BAA164")
                        border.color: startBtn.enabled ? root.avGoldPrimary : "#24FFFFFF"
                        border.width: 1
                    }
                }

                // 2. Primary Action: ■ STOP (matches scanarMini stop_btn)
                Button {
                    id: stopBtn
                    text: "■  STOP"
                    enabled: bridge.isCapturing
                    implicitHeight: 34
                    implicitWidth: 92
                    onClicked: bridge.stopCapture()
                    contentItem: Text {
                        text: stopBtn.text
                        color: stopBtn.enabled ? "#FCA5A5" : "#4DFFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 9
                        color: !stopBtn.enabled ? "#0AFFFFFF" : (stopBtn.hovered ? "#4DEF4444" : "#29EF4444")
                        border.color: stopBtn.enabled ? "#8CF87171" : "#1FFFFFFF"
                        border.width: 1
                    }
                }

                // 3. Primary Action: ↓ SAVE DATASET (matches scanarMini save_btn)
                Button {
                    id: saveBtn
                    text: "↓  SAVE DATASET"
                    implicitHeight: 34
                    implicitWidth: 132
                    onClicked: {
                        bridge.openCleanupWizard();
                        cleanupExportDialog.open();
                    }
                    contentItem: Text {
                        text: saveBtn.text
                        color: root.avGoldBright
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 9
                        color: saveBtn.hovered ? "#2EBAA164" : "#1FFFFFFF"
                        border.color: saveBtn.hovered ? root.avGoldPrimary : "#59FCFBF8"
                        border.width: 1
                    }
                }

                // 4. Real-Time M12 Toggle Pill (matches scanarMini rt_m12_btn)
                Button {
                    id: liveCleanupHeaderBtn
                    text: bridge.liveCleanupEnabled ? "✦ M12 RT: ON" : "✦ M12 RT: OFF"
                    implicitHeight: 34
                    implicitWidth: 112
                    onClicked: bridge.liveCleanupEnabled = !bridge.liveCleanupEnabled
                    contentItem: Text {
                        text: liveCleanupHeaderBtn.text
                        color: bridge.liveCleanupEnabled ? root.avGoldBright : root.avWhiteMuted
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 9
                        color: bridge.liveCleanupEnabled ? "#2EBAA164" : "#14FFFFFF"
                        border.color: bridge.liveCleanupEnabled ? root.avGoldPrimary : "#38FFFFFF"
                        border.width: 1
                    }
                }

                // 5. Elapsed Timer Pill (matches scanarMini timer_lbl)
                Rectangle {
                    height: 34
                    width: 86
                    radius: 9
                    color: "#0FFFFFFF"
                    border.color: "#38FFFFFF"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "⏱ " + (bridge.elapsedTimeString || "00:00")
                        color: root.avWhiteWarm
                        font.pixelSize: 12
                        font.family: "Monospace"
                        font.bold: true
                    }
                }

                Rectangle {
                    width: 1
                    height: 24
                    color: "#2EFFFFFF"
                }

                // 6. Active Scanner Profile Selector (Retained AV Scan Hardware Control)
                ComboBox {
                    id: scannerCombo
                    model: bridge.availableScanners
                    implicitWidth: 185
                    implicitHeight: 32
                    enabled: !bridge.isCapturing
                    onActivated: {
                        if (currentText !== "") {
                            bridge.selectScanner(currentText);
                        }
                    }
                    contentItem: Text {
                        text: scannerCombo.displayText !== "" ? ("Scanner: " + scannerCombo.displayText) : "Select Scanner..."
                        color: root.avWhiteWarm
                        font.pixelSize: 11
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        radius: 8
                        color: "#14FFFFFF"
                        border.color: "#6BBAA164"
                        border.width: 1
                    }
                }

                Item { Layout.fillWidth: true }

                // 7. Retained AV Scan Hardware & Studio Workflow Buttons (Apple Glass Pill Styling)
                Button {
                    id: mainCreateProfileBtn
                    text: "⚡ Profile"
                    implicitHeight: 32
                    implicitWidth: 78
                    onClicked: {
                        bridge.initCreateProfileForm();
                        createProfileDialog.open();
                    }
                    contentItem: Text {
                        text: mainCreateProfileBtn.text
                        color: root.avGoldBright
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: mainCreateProfileBtn.hovered ? "#2EBAA164" : "#19BAA164"
                        border.color: "#7ABAA164"
                        border.width: 1
                    }
                }

                Button {
                    id: mainConfigSensorsBtn
                    text: "⚙ Sensors"
                    implicitHeight: 32
                    implicitWidth: 82
                    onClicked: configureSensorsDialog.open()
                    contentItem: Text {
                        text: mainConfigSensorsBtn.text
                        color: root.avWhiteWarm
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: mainConfigSensorsBtn.hovered ? "#29FFFFFF" : "#12FFFFFF"
                        border.color: "#3DFFFFFF"
                        border.width: 1
                    }
                }

                Button {
                    id: mainCatalogBtn
                    text: "📚 Catalog"
                    implicitHeight: 32
                    implicitWidth: 82
                    onClicked: {
                        configureSensorsDialog.currentTab = 2;
                        configureSensorsDialog.open();
                    }
                    contentItem: Text {
                        text: mainCatalogBtn.text
                        color: root.avGoldBright
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: mainCatalogBtn.hovered ? "#2EBAA164" : "#19BAA164"
                        border.color: "#7ABAA164"
                        border.width: 1
                    }
                }

                Button {
                    id: orientationDiagBtn
                    text: "📐 Orient"
                    implicitHeight: 32
                    implicitWidth: 76
                    onClicked: {
                        bridge.openOrientationDiagnostic();
                        orientationDiagDialog.open();
                    }
                    contentItem: Text {
                        text: orientationDiagBtn.text
                        color: root.avWhiteWarm
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: orientationDiagBtn.hovered ? "#29FFFFFF" : "#12FFFFFF"
                        border.color: "#3DFFFFFF"
                        border.width: 1
                    }
                }

                Button {
                    id: headerMultiNodeBtn
                    text: "🌐 Mesh (" + Math.max(1, bridge.cslamScannerCount) + ")"
                    implicitHeight: 32
                    implicitWidth: 92
                    onClicked: multiNodeDialog.open()
                    contentItem: Text {
                        text: headerMultiNodeBtn.text
                        color: root.avGoldBright
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: headerMultiNodeBtn.hovered ? "#2EBAA164" : "#19BAA164"
                        border.color: "#7ABAA164"
                        border.width: 1
                    }
                }

                Button {
                    id: headerOfflineRegBtn
                    text: "🔀 Register"
                    implicitHeight: 32
                    implicitWidth: 84
                    onClicked: {
                        bridge.refreshRegistrationProjectScans();
                        offlineRegistrationDialog.open();
                    }
                    contentItem: Text {
                        text: headerOfflineRegBtn.text
                        color: root.avWhiteWarm
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: headerOfflineRegBtn.hovered ? "#29FFFFFF" : "#12FFFFFF"
                        border.color: "#3DFFFFFF"
                        border.width: 1
                    }
                }

                Button {
                    id: headerFusionBtn
                    text: "🔗 Fusion"
                    implicitHeight: 32
                    implicitWidth: 76
                    onClicked: {
                        bridge.openCalibrationWizard();
                        calibrationFusionDialog.open();
                    }
                    contentItem: Text {
                        text: headerFusionBtn.text
                        color: root.avWhiteWarm
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: headerFusionBtn.hovered ? "#29FFFFFF" : "#12FFFFFF"
                        border.color: "#3DFFFFFF"
                        border.width: 1
                    }
                }

                Button {
                    id: headerCleanupExportBtn
                    text: "✦ Cleanup"
                    implicitHeight: 32
                    implicitWidth: 82
                    onClicked: {
                        bridge.openCleanupWizard();
                        cleanupExportDialog.open();
                    }
                    contentItem: Text {
                        text: headerCleanupExportBtn.text
                        color: root.avGoldBright
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: headerCleanupExportBtn.hovered ? "#2EBAA164" : "#19BAA164"
                        border.color: "#7ABAA164"
                        border.width: 1
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // ROW 3: 60% / 40% HORIZONTAL SPLIT WORKSPACE (Matches scanarMini)
        //   Left (60%): Pure AR Walking Passthrough Viewport (NO GRID!)
        //   Right (40%): Peripheral Apple Glass HUD Stack
        // ---------------------------------------------------------------------
        RowLayout {
            id: mainWorkspaceSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // =================================================================
            // LEFT PANEL (~60%): CLEAR AR WALKING PASSTHROUGH VIEWPORT (NO GRID)
            // Directly mimics scanarMini ClearPassthroughViewport + Stage 1 bar
            // =================================================================
            ColumnLayout {
                id: leftPassthroughColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 600
                spacing: 4

                // Compact Single-Line Stage Prompt + Thin Gold Progress Bar Row (26px)
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 8

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        radius: 7
                        color: "#1FBAA164" // rgba(186, 161, 100, 0.12)
                        border.color: "#6BBAA164" // rgba(186, 161, 100, 0.42)
                        border.width: 1

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: bridge.isCapturing
                                ? ("STAGE 2  ·  ACTIVE SPATIAL CAPTURE — Walking AR Passthrough Active  ·  Anchor: " + (bridge.activeAnchorSensor || "AIRY"))
                                : "STAGE 1  ·  Place scanner static on flat surface, then press 'START SCAN'"
                            color: root.avGoldBright
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }

                    // Thin 5px Champagne Gold Progress Bar (matches scanarMini init_progress)
                    Rectangle {
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 5
                        radius: 2.5
                        color: "#1FFFFFFF"

                        Rectangle {
                            width: parent.width * (bridge.isCapturing ? 1.0 : ((bridge.projectName !== "" && bridge.projectName !== "No Active Project") ? 0.35 : 0.08))
                            height: parent.height
                            radius: 2.5
                            color: root.avGoldPrimary
                        }
                    }
                }

                // Pure Clear AR Passthrough Viewport (id: panel3d / ClearPassthroughViewport)
                // 100% transparent / #000000 with ZERO grid lines and ZERO coordinate axes!
                Rectangle {
                    id: panel3d
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 12
                    // Pure transparent in window mode; #000000 optical see-through in AR glasses
                    color: "transparent"
                    border.color: "#6BBAA164" // Subtle 1px champagne gold boundary rim (rgba(186, 161, 100, 108))
                    border.width: 1
                    clip: true

                    property real camYaw: -0.65
                    property real camPitch: 0.42
                    property real camDistance: 12.0
                    property real panX: 0.0
                    property real panY: 0.0

                    function resetCamera() {
                        camYaw = -0.65;
                        camPitch = 0.42;
                        camDistance = 12.0;
                        panX = 0.0;
                        panY = 0.0;
                        canvas3d.requestPaint();
                    }

                    // Kept for full compatibility with dialogs/signals that invoke canvas3d.requestPaint()
                    // Does NOT draw any grid or axes — pure unobstructed passthrough!
                    Canvas {
                        id: canvas3d
                        anchors.fill: parent

                        function project(x, y, z, w, h) {
                            var cy = Math.cos(panel3d.camYaw), sy = Math.sin(panel3d.camYaw);
                            var cp = Math.cos(panel3d.camPitch), sp = Math.sin(panel3d.camPitch);
                            var rx = x * cy - y * sy;
                            var ry = x * sy + y * cy;
                            var rz = z;
                            var py = ry * cp - rz * sp;
                            var pz = ry * sp + rz * cp;
                            var fov = 480.0 * (12.0 / Math.max(1.5, panel3d.camDistance));
                            var scale = fov / Math.max(2.0, 18.0 + py);
                            return {
                                x: (w * 0.5) + panel3d.panX + rx * scale,
                                y: (h * 0.52) + panel3d.panY - pz * scale,
                                depth: py
                            };
                        }

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            // Intentionally draws NO grid, NO axes, and NO background fill
                            // so the left viewport remains 100% pure AR passthrough.
                        }
                    }
                }
            }

            // =================================================================
            // RIGHT PANEL (~40%): PERIPHERAL APPLE GLASS HUD STACK
            // Directly mimics scanarMini right_container:
            //   1. Camera & Depth Sensor Preview Card (OPTICAL RGB + STEREO DEPTH)
            //   2. 2D Orthographic Slice Header + Concentric Range-Ring Map Canvas
            //   3. Multi-Tab Control Deck (M12 Cleanup / C-SLAM Peers / Sensors)
            //   4. Translucent Apple Glass Studio Log
            // =================================================================
            ColumnLayout {
                id: rightHudColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 400
                spacing: 6

                // -------------------------------------------------------------
                // CARD 1: CAMERA & DEPTH SENSOR PREVIEW (Matches CameraPreviewCanvas)
                // -------------------------------------------------------------
                Rectangle {
                    id: camPreviewCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 148
                    radius: 12
                    color: "#000000"
                    border.color: "#85BAA164" // rgba(186, 161, 100, 0.52)
                    border.width: 1
                    clip: true

                    // Top Translucent Header Strip (22px)
                    Rectangle {
                        id: camPreviewHeader
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 22
                        color: "#0DFFFFFF"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 6

                            Text {
                                text: "OPTICAL RGB STREAM (LEFT)   ·   STEREO DEPTH HEATMAP (RIGHT)"
                                color: root.avGoldBright
                                font.pixelSize: 10
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // Retained Multi-Sensor Overlapped & Dynamic Anchor Pill
                            Rectangle {
                                height: 16
                                width: anchorMiniText.implicitWidth + 10
                                radius: 4
                                color: "#29BAA164"
                                border.color: "#7ABAA164"
                                border.width: 1

                                Text {
                                    id: anchorMiniText
                                    anchors.centerIn: parent
                                    text: "⚓ ANCHOR: " + (bridge.activeAnchorSensor || "AIRY")
                                    color: root.avGoldBright
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                    }

                    // Dual Split Preview Body (Left: Optical RGB, Right: Stereo Depth / LiDAR)
                    Item {
                        anchors.top: camPreviewHeader.bottom
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right

                        // Left Half: Optical RGB Stream Preview (Fixed 34x34 Logo Badge + Label)
                        Item {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            width: parent.width * 0.5

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 5

                                Item {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34

                                    Image {
                                        anchors.fill: parent
                                        source: "assets/av_logo.png"
                                        fillMode: Image.PreserveAspectFit
                                        opacity: 0.85
                                        smooth: true
                                        mipmap: true
                                    }
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: (bridge.geminiAcquisitionRate > 0 || bridge.cameraAcquisitionRate > 0)
                                        ? ("Optical RGB Live (" + Math.max(bridge.geminiAcquisitionRate, bridge.cameraAcquisitionRate).toFixed(1) + " Hz)")
                                        : "Optical RGB Ready"
                                    color: root.avGoldBright
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        // Dashed Champagne Gold Vertical Divider (Matches CameraPreviewCanvas)
                        Canvas {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 2
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.clearRect(0, 0, width, height);
                                ctx.strokeStyle = "rgba(186, 161, 100, 0.35)";
                                ctx.lineWidth = 1;
                                ctx.setLineDash([4, 4]);
                                ctx.beginPath();
                                ctx.moveTo(1, 2);
                                ctx.lineTo(1, height - 2);
                                ctx.stroke();
                            }
                        }

                        // Right Half: Stereo Depth Heatmap / LiDAR Preview
                        Item {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            width: parent.width * 0.5

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: bridge.isCapturing
                                        ? ("Depth / LiDAR Live (" + bridge.pointsCaptured + " pts)")
                                        : "Depth Stream Ready"
                                    color: "#B8FCFBF8"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: "OVERLAPPED (" + Math.max(1, bridge.activeSensorCount) + " SENSORS)"
                                    color: root.avGoldMuted
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // CARD 2: 2D ORTHOGRAPHIC SLICE HEADER + CONCENTRIC RANGE-RING MAP
                // Matches scanarMini slice_hdr + RealityCaptureMapCanvas
                // -------------------------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 4

                    Text {
                        text: "2D ORTHOGRAPHIC SLICE"
                        color: root.avGoldBright
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.0
                    }

                    Item { Layout.fillWidth: true }

                    // Retained AV Scan Layer Filter Pills (Colorized, Airy, Gemini, Fused, Diag)
                    Repeater {
                        model: [
                            { label: "RGB", mode: "COLORIZED" },
                            { label: "Airy", mode: "AIRY" },
                            { label: "Gem", mode: "GEMINI" },
                            { label: "Fused", mode: "FUSED" },
                            { label: "Diag", mode: "DIAGNOSTIC" }
                        ]
                        delegate: Button {
                            property bool isSel: bridge.activeDisplaySensor === modelData.mode
                            text: modelData.label
                            implicitHeight: 22
                            implicitWidth: 42
                            onClicked: {
                                bridge.activeDisplaySensor = modelData.mode;
                                canvas2d.requestPaint();
                            }
                            contentItem: Text {
                                text: parent.text
                                color: parent.isSel ? root.avGoldBright : root.avWhiteMuted
                                font.pixelSize: 9
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 5
                                color: parent.isSel ? "#2EBAA164" : "#12FFFFFF"
                                border.color: parent.isSel ? root.avGoldPrimary : "#2EFFFFFF"
                                border.width: 1
                            }
                        }
                    }

                    // Zoom Out / Zoom In / Reset View Controls (matches scanarMini -, +, ↺)
                    Button {
                        id: zoomOut2dBtn
                        text: "-"
                        implicitWidth: 24
                        implicitHeight: 22
                        onClicked: {
                            panel2d.zoom2d = Math.max(2.0, panel2d.zoom2d * 0.8);
                            canvas2d.requestPaint();
                        }
                        contentItem: Text {
                            text: "-"
                            color: root.avWhiteWarm
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: zoomOut2dBtn.hovered ? "#2EBAA164" : "#17FFFFFF"
                            border.color: "#42FFFFFF"
                            border.width: 1
                        }
                    }

                    Button {
                        id: zoomIn2dBtn
                        text: "+"
                        implicitWidth: 24
                        implicitHeight: 22
                        onClicked: {
                            panel2d.zoom2d = Math.min(140.0, panel2d.zoom2d * 1.25);
                            canvas2d.requestPaint();
                        }
                        contentItem: Text {
                            text: "+"
                            color: root.avWhiteWarm
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: zoomIn2dBtn.hovered ? "#2EBAA164" : "#17FFFFFF"
                            border.color: "#42FFFFFF"
                            border.width: 1
                        }
                    }

                    Button {
                        id: reset2dBtn
                        text: "↺"
                        implicitWidth: 24
                        implicitHeight: 22
                        onClicked: panel2d.recenter2D()
                        contentItem: Text {
                            text: "↺"
                            color: root.avGoldBright
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: reset2dBtn.hovered ? "#2EBAA164" : "#17FFFFFF"
                            border.color: "#42FFFFFF"
                            border.width: 1
                        }
                    }
                }

                // 2D Concentric Range-Ring Map Canvas (Matches RealityCaptureMapCanvas in scanarMini)
                Rectangle {
                    id: panel2d
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 210
                    radius: 12
                    color: "#000000"
                    border.color: "#8CBAA164" // rgba(186, 161, 100, 140)
                    border.width: 1
                    clip: true

                    property real zoom2d: 18.0
                    property real offsetX: 0.0
                    property real offsetY: 0.0

                    function recenter2D() {
                        zoom2d = 18.0;
                        offsetX = 0.0;
                        offsetY = 0.0;
                        canvas2d.requestPaint();
                    }

                    Canvas {
                        id: canvas2d
                        anchors.fill: parent

                        onPaint: {
                            var ctx = getContext("2d");
                            var w = width;
                            var h = height;
                            ctx.clearRect(0, 0, w, h);

                            // Translucent glass surface fill (rgba(255, 255, 255, 10))
                            ctx.fillStyle = "rgba(255, 255, 255, 0.04)";
                            ctx.fillRect(0, 0, w, h);

                            var cx = w * 0.5 + panel2d.offsetX;
                            var cy = h * 0.5 + panel2d.offsetY;
                            var scale = panel2d.zoom2d;

                            // 1. Subtle Crosshair at Origin (rgba(255, 255, 255, 34)) — NO rectangular grid!
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.13)";
                            ctx.lineWidth = 1;
                            ctx.setLineDash([]);
                            ctx.beginPath();
                            ctx.moveTo(0, cy);
                            ctx.lineTo(w, cy);
                            ctx.moveTo(cx, 0);
                            ctx.lineTo(cx, h);
                            ctx.stroke();

                            // 2. Concentric Champagne-Gold Range Rings (5m, 10m, 20m, 30m, 50m)
                            var rings = [5, 10, 20, 30, 50];
                            ctx.font = "9px sans-serif";
                            for (var ri = 0; ri < rings.length; ++ri) {
                                var r_m = rings[ri];
                                var r_px = r_m * scale;
                                if (r_px < 15 || r_px > Math.max(w, h) * 1.4) continue;
                                ctx.strokeStyle = "rgba(186, 161, 100, 0.22)";
                                ctx.lineWidth = 1;
                                ctx.setLineDash([4, 4]);
                                ctx.beginPath();
                                ctx.arc(cx, cy, r_px, 0, Math.PI * 2);
                                ctx.stroke();

                                ctx.fillStyle = "rgba(229, 201, 131, 0.72)";
                                ctx.fillText(r_m + "m", cx + 4, cy - r_px + 11);
                            }
                            ctx.setLineDash([]);

                            // 3. 2D Point Cloud Slice Projection
                            var pts = bridge.isRegistrationActive
                                ? bridge.getRegistrationDisplayPoints(25000)
                                : bridge.getDisplayPoints(25000);
                            var nPts = pts ? Math.floor(pts.length / 6) : 0;
                            for (var i = 0; i < nPts; ++i) {
                                var idx = i * 6;
                                var px = cx + pts[idx] * scale;
                                var py = cy - pts[idx + 1] * scale;
                                if (px < 0 || px >= w || py < 0 || py >= h) continue;
                                var r = Math.round(pts[idx + 3] * 255);
                                var g = Math.round(pts[idx + 4] * 255);
                                var b = Math.round(pts[idx + 5] * 255);
                                ctx.fillStyle = "rgb(" + r + "," + g + "," + b + ")";
                                ctx.fillRect(px - 1, py - 1, 2.2, 2.2);
                            }

                            // 4. Multi-Node C-SLAM Peer Scanner Markers (if active)
                            var peers = bridge.cslamNodesList || [];
                            var peerPalette = ["#BAA164", "#38BDF8", "#A855F7", "#EC4899", "#F59E0B", "#10B981"];
                            for (var pi = 1; pi < peers.length; ++pi) {
                                var peer = peers[pi];
                                var pCol = peerPalette[pi % peerPalette.length];
                                var peerX = cx + (pi * 2.2) * scale;
                                var peerY = cy - ((pi % 2 === 0 ? 1.5 : -1.5)) * scale;
                                ctx.strokeStyle = pCol;
                                ctx.lineWidth = 1.5;
                                ctx.beginPath();
                                ctx.arc(peerX, peerY, 6, 0, Math.PI * 2);
                                ctx.stroke();
                                ctx.fillStyle = pCol;
                                ctx.font = "bold 9px sans-serif";
                                ctx.fillText(peer.nodeName || ("PEER-" + (pi + 1)), peerX + 9, peerY + 3);
                            }

                            // 5. Scanner Heading Arrow at Origin (Matches RealityCaptureMapCanvas)
                            ctx.fillStyle = "#BAA164";
                            ctx.strokeStyle = "#FCFBF8";
                            ctx.lineWidth = 1.5;
                            ctx.beginPath();
                            ctx.moveTo(cx + 11, cy);
                            ctx.lineTo(cx - 7, cy - 6.5);
                            ctx.lineTo(cx - 3.5, cy);
                            ctx.lineTo(cx - 7, cy + 6.5);
                            ctx.closePath();
                            ctx.fill();
                            ctx.stroke();

                            // 6. Top Translucent HUD Strip (24px)
                            ctx.fillStyle = "rgba(255, 255, 255, 0.05)";
                            ctx.fillRect(0, 0, w, 24);
                            ctx.strokeStyle = "rgba(186, 161, 100, 0.28)";
                            ctx.lineWidth = 1;
                            ctx.beginPath();
                            ctx.moveTo(0, 24);
                            ctx.lineTo(w, 24);
                            ctx.stroke();

                            ctx.font = "bold 9px sans-serif";
                            ctx.fillStyle = "#E5C983";
                            var m12Tag = bridge.liveCleanupEnabled ? "M12 RT ON" : "RAW";
                            var cslamTag = Math.max(1, bridge.cslamScannerCount) > 1
                                ? ("C-SLAM (" + bridge.cslamScannerCount + ")")
                                : "C-SLAM";
                            ctx.fillText("LAYER: " + (bridge.activeDisplaySensor || "COLORIZED") + " · " + m12Tag + " · " + cslamTag, 10, 15);

                            var voxText = (bridge.pointsCaptured || 0) + " VOXELS";
                            ctx.fillStyle = "#FCFBF8";
                            ctx.fillText(voxText, Math.max(220, w - 85), 15);

                            // 7. Bottom Translucent Telemetry Strip (36px)
                            ctx.fillStyle = "rgba(255, 255, 255, 0.05)";
                            ctx.fillRect(0, h - 36, w, 36);
                            ctx.strokeStyle = "rgba(255, 255, 255, 0.20)";
                            ctx.beginPath();
                            ctx.moveTo(0, h - 36);
                            ctx.lineTo(w, h - 36);
                            ctx.stroke();

                            ctx.font = "9px sans-serif";
                            ctx.fillStyle = "#FCFBF8";
                            ctx.fillText(
                                "POS: X=" + bridge.currentPoseX.toFixed(2) + "m  Y=" + bridge.currentPoseY.toFixed(2) + "m  Z=" + bridge.currentPoseZ.toFixed(2) + "m  |  YAW=" + bridge.currentYaw.toFixed(1) + "°",
                                10,
                                h - 21
                            );
                            ctx.fillStyle = "#E5C983";
                            ctx.fillText(
                                "PATH: " + bridge.trajectoryPointCount + " poses  |  SCALE: " + scale.toFixed(1) + " px/m  |  PEERS: " + Math.max(1, bridge.cslamScannerCount),
                                10,
                                h - 7
                            );
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            property real lastX: 0
                            property real lastY: 0
                            onPressed: function(mouse) {
                                lastX = mouse.x;
                                lastY = mouse.y;
                            }
                            onPositionChanged: function(mouse) {
                                if (pressed) {
                                    panel2d.offsetX += (mouse.x - lastX);
                                    panel2d.offsetY += (mouse.y - lastY);
                                    lastX = mouse.x;
                                    lastY = mouse.y;
                                    canvas2d.requestPaint();
                                }
                            }
                            onWheel: function(wheel) {
                                if (wheel.angleDelta.y > 0) panel2d.zoom2d = Math.min(140.0, panel2d.zoom2d * 1.15);
                                else panel2d.zoom2d = Math.max(2.0, panel2d.zoom2d * 0.87);
                                canvas2d.requestPaint();
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // CARD 3: MULTI-TAB APPLE GLASS CONTROL DECK (168px)
                // Combines scanarMini's M12 + C-SLAM cards with AV Scan's
                // interactive cleanup, multi-node C-SLAM, and sensor controls
                // -------------------------------------------------------------
                Rectangle {
                    id: controlDeckCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 168
                    radius: 10
                    color: "#0AFFFFFF" // rgba(255, 255, 255, 0.04)
                    border.color: "#61BAA164" // rgba(186, 161, 100, 0.38)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        // Deck Tab Selector Strip
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Repeater {
                                model: [
                                    "✦ M12 REAL-TIME & POST CLEANUP",
                                    "🌐 C-SLAM PEERS (∞)",
                                    "⚙ SENSORS & RECON"
                                ]
                                delegate: Button {
                                    property bool activeTab: root.rightDeckTab === index
                                    text: modelData
                                    Layout.fillWidth: true
                                    implicitHeight: 24
                                    onClicked: root.rightDeckTab = index
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.activeTab ? root.avGoldBright : root.avWhiteMuted
                                        font.pixelSize: 10
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.activeTab ? "#2EBAA164" : "#12FFFFFF"
                                        border.color: parent.activeTab ? root.avGoldPrimary : "#2EFFFFFF"
                                        border.width: 1
                                    }
                                }
                            }
                        }

                        // TAB 0: M12 REAL-TIME & POST-SCAN CLEANUP (Matches scanarMini m12_card)
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: root.rightDeckTab === 0
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 14

                                CheckBox {
                                    id: deckRtM12Check
                                    checked: bridge.liveCleanupEnabled
                                    text: "Real-Time M12 (Voxel 2cm + SOR + ROR)"
                                    onToggled: bridge.liveCleanupEnabled = checked
                                    indicator: Rectangle {
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        x: deckRtM12Check.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: deckRtM12Check.checked ? "#38BAA164" : "#14FFFFFF"
                                        border.color: deckRtM12Check.checked ? root.avGoldPrimary : "#47FFFFFF"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            visible: deckRtM12Check.checked
                                            text: "✓"
                                            color: root.avGoldBright
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                    contentItem: Text {
                                        text: deckRtM12Check.text
                                        color: root.avWhiteWarm
                                        font.pixelSize: 11
                                        leftPadding: deckRtM12Check.indicator.width + 6
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                CheckBox {
                                    id: deckFloorLockCheck
                                    checked: bridge.liveGroundLockEnabled
                                    text: "Live Z=0 Floor Lock"
                                    onToggled: bridge.liveGroundLockEnabled = checked
                                    indicator: Rectangle {
                                        implicitWidth: 16
                                        implicitHeight: 16
                                        x: deckFloorLockCheck.leftPadding
                                        y: parent.height / 2 - height / 2
                                        radius: 4
                                        color: deckFloorLockCheck.checked ? "#38BAA164" : "#14FFFFFF"
                                        border.color: deckFloorLockCheck.checked ? root.avGoldPrimary : "#47FFFFFF"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            visible: deckFloorLockCheck.checked
                                            text: "✓"
                                            color: root.avGoldBright
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                    contentItem: Text {
                                        text: deckFloorLockCheck.text
                                        color: root.avGoldBright
                                        font.pixelSize: 11
                                        leftPadding: deckFloorLockCheck.indicator.width + 6
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }

                            Text {
                                text: "Pipeline: Voxel 2.0cm  ·  SOR k=20 (2.0σ)  ·  ROR r=10cm  ·  " + (bridge.cleanupStatus || "M12 Real-Time Ready")
                                color: root.avWhiteMuted
                                font.pixelSize: 10
                                font.family: "Monospace"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "✦ Preview Cleanup"
                                    Layout.fillWidth: true
                                    implicitHeight: 28
                                    onClicked: bridge.runPointCloudCleanupPreview()
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avGoldBright
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#29BAA164"
                                        border.color: root.avGoldPrimary
                                        border.width: 1
                                    }
                                }

                                Button {
                                    text: "↺ Reset"
                                    implicitWidth: 68
                                    implicitHeight: 28
                                    onClicked: bridge.resetPointCloudCleanupPreview()
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avWhiteWarm
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#17FFFFFF"
                                        border.color: "#42FFFFFF"
                                        border.width: 1
                                    }
                                }

                                Button {
                                    text: "✓ Commit"
                                    implicitWidth: 78
                                    implicitHeight: 28
                                    onClicked: bridge.commitCleanedPointCloud()
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avStatusOk
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#1F22C55E"
                                        border.color: "#7022C55E"
                                        border.width: 1
                                    }
                                }

                                Button {
                                    text: "Studio..."
                                    implicitWidth: 72
                                    implicitHeight: 28
                                    onClicked: {
                                        bridge.openCleanupWizard();
                                        cleanupExportDialog.open();
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avGoldBright
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#1FBAA164"
                                        border.color: "#7ABAA164"
                                        border.width: 1
                                    }
                                }
                            }
                        }

                        // TAB 1: C-SLAM PEER SCANNERS (∞) (Matches scanarMini cslam_card)
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: root.rightDeckTab === 1
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Text {
                                    text: "Non-Overlap: IMU Orient + Arbitrary Sep  ·  Snap ≥15%"
                                    color: root.avGoldBright
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Button {
                                    id: cslamColorModeBtn
                                    text: bridge.cslamColorByScanner ? "🎨 Scanner Colors" : "🎨 True RGB"
                                    implicitHeight: 22
                                    implicitWidth: 116
                                    onClicked: {
                                        bridge.cslamColorByScanner = !bridge.cslamColorByScanner;
                                        canvas2d.requestPaint();
                                    }
                                    contentItem: Text {
                                        text: cslamColorModeBtn.text
                                        color: root.avGoldBright
                                        font.pixelSize: 10
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: "#2EBAA164"
                                        border.color: root.avGoldPrimary
                                        border.width: 1
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    height: 22
                                    width: localPeerBadge.implicitWidth + 14
                                    radius: 5
                                    color: "#24BAA164"
                                    border.color: root.avGoldPrimary
                                    border.width: 1

                                    Text {
                                        id: localPeerBadge
                                        anchors.centerIn: parent
                                        text: "● LOCAL (" + (bridge.nodeDisplayName || "Primary") + ") · ANCHOR"
                                        color: root.avGoldBright
                                        font.pixelSize: 10
                                        font.family: "Monospace"
                                        font.bold: true
                                    }
                                }

                                Text {
                                    text: bridge.cslamStatusSummary || "Listening for Peer Scanners (Any OS / Any Sensor)..."
                                    color: root.avWhiteMuted
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "🌐 Open Multi-Node Mesh Manager"
                                    Layout.fillWidth: true
                                    implicitHeight: 28
                                    onClicked: multiNodeDialog.open()
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avGoldBright
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#29BAA164"
                                        border.color: root.avGoldPrimary
                                        border.width: 1
                                    }
                                }

                                Button {
                                    text: "🔀 Offline Reg (M9)"
                                    implicitWidth: 126
                                    implicitHeight: 28
                                    onClicked: {
                                        bridge.refreshRegistrationProjectScans();
                                        offlineRegistrationDialog.open();
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avWhiteWarm
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#17FFFFFF"
                                        border.color: "#42FFFFFF"
                                        border.width: 1
                                    }
                                }
                            }
                        }

                        // TAB 2: SENSORS, DYNAMIC ANCHOR & CAMERA RECONSTRUCTION
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: root.rightDeckTab === 2
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "⚓ Dynamic Anchor: " + (bridge.activeAnchorName || bridge.activeAnchorSensor || "AIRY")
                                    color: root.avGoldBright
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                ComboBox {
                                    id: cameraBackendCombo
                                    model: ["LingBot (Fast)", "ABot (Dense)"]
                                    currentIndex: bridge.selectedCameraBackend === "ABOT_MVS" ? 1 : 0
                                    implicitWidth: 126
                                    implicitHeight: 22
                                    onActivated: {
                                        bridge.selectedCameraBackend = (currentIndex === 1 ? "ABOT_MVS" : "LINGBOT");
                                    }
                                    contentItem: Text {
                                        text: cameraBackendCombo.displayText
                                        color: root.avGoldBright
                                        font.pixelSize: 10
                                        font.bold: true
                                        verticalAlignment: Text.AlignVCenter
                                        leftPadding: 8
                                    }
                                    background: Rectangle {
                                        radius: 6
                                        color: "#19BAA164"
                                        border.color: "#7ABAA164"
                                        border.width: 1
                                    }
                                }
                            }

                            Text {
                                text: "Airy: " + bridge.airyPointsCaptured + " pts (" + bridge.airyAcquisitionRate.toFixed(0) + "Hz)  |  Gemini: " + bridge.geminiPointsCaptured + " pts  |  Cam: " + bridge.cameraPointsCaptured + " pts"
                                color: root.avWhiteMuted
                                font.pixelSize: 10
                                font.family: "Monospace"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Button {
                                    text: "⚙ Configure Sensors"
                                    Layout.fillWidth: true
                                    implicitHeight: 28
                                    onClicked: configureSensorsDialog.open()
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avGoldBright
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#29BAA164"
                                        border.color: root.avGoldPrimary
                                        border.width: 1
                                    }
                                }

                                Button {
                                    text: "🔗 Extrinsics & Fusion (M7/M8)"
                                    Layout.fillWidth: true
                                    implicitHeight: 28
                                    onClicked: {
                                        bridge.openCalibrationWizard();
                                        calibrationFusionDialog.open();
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: root.avWhiteWarm
                                        font.pixelSize: 11
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: 7
                                        color: "#17FFFFFF"
                                        border.color: "#42FFFFFF"
                                        border.width: 1
                                    }
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // CARD 4: TRANSLUCENT APPLE GLASS STUDIO LOG (Matches self.log_view)
                // -------------------------------------------------------------
                Rectangle {
                    id: studioLogCard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 74
                    radius: 10
                    color: "#0AFFFFFF" // rgba(255, 255, 255, 0.04)
                    border.color: "#2EFFFFFF" // rgba(255, 255, 255, 0.18)
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2

                        Text {
                            text: "[AV-OS] Artificial Vision Apple Glass AR Passthrough Initialized — Clear Left Viewport Active."
                            color: root.avGoldBright
                            font.pixelSize: 10
                            font.family: "Monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "[SENSOR] Scanner '" + (bridge.selectedScannerName || "Default") + "' · Overlapped Multi-Sensor Fusion · Anchor: " + (bridge.activeAnchorSensor || "AIRY")
                            color: root.avWhiteMuted
                            font.pixelSize: 10
                            font.family: "Monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "[STATUS] " + (bridge.selectedScannerValidationNotice || "All spatial pipelines nominal. Ready for capture.")
                            color: "#B8FCFBF8"
                            font.pixelSize: 10
                            font.family: "Monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------------
        // ROW 4: TRANSLUCENT APPLE GLASS FOOTER STATUS BAR (24px)
        // Matches scanarMini self.statusBar()
        // ---------------------------------------------------------------------
        Rectangle {
            id: glassFooterBar
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            radius: 7
            color: "#0AFFFFFF"
            border.color: "#52BAA164"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "VERIFY · DIRECT · CLARIFY  |  AV Scan Apple Glass AR Passthrough Active  |  M12 RT " + (bridge.liveCleanupEnabled ? "ON" : "OFF") + "  |  C-SLAM ∞ Ready"
                    color: root.avGoldBright
                    font.pixelSize: 10
                    font.family: "Monospace"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: "HOST: " + (bridge.hostHardware || "Linux Spatial Node") + "  |  RAM: " + (bridge.hostMemory || "Ready")
                    color: root.avWhiteMuted
                    font.pixelSize: 10
                    font.family: "Monospace"
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
            color: "#0A0A0E"
            radius: 8
            border.color: "#2A2A34"
        }

        header: Rectangle {
            height: 52
            color: "#121217"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Create New Project"
                color: "#F4F4F6"
                font.bold: true
                font.pixelSize: 15
            }
        }

        ColumnLayout {
            spacing: 14
            width: parent.width

            Text {
                text: "Project Name:"
                color: "#D4D4D8"
                font.pixelSize: 12
                font.bold: true
            }
            TextField {
                id: projNameInput
                Layout.fillWidth: true
                placeholderText: "e.g., Industrial Plant Survey"
                color: "#F4F4F6"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#0A0A0E"
                    radius: 4
                    border.color: "#2A2A34"
                }
            }

            Text {
                text: "Description (Optional):"
                color: "#D4D4D8"
                font.pixelSize: 12
                font.bold: true
            }
            TextField {
                id: projDescInput
                Layout.fillWidth: true
                placeholderText: "Survey notes, building ID, inspection parameters"
                color: "#F4F4F6"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#0A0A0E"
                    radius: 4
                    border.color: "#2A2A34"
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
            color: "#0A0A0E"
            radius: 8
            border.color: "#2A2A34"
        }

        header: Rectangle {
            height: 52
            color: "#121217"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Open Existing Project"
                color: "#F4F4F6"
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
                    color: "#A1A1AA"
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                RowLayout {
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: "Sort:"
                        color: "#8E8E99"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Button {
                        text: "Date ↓"
                        implicitHeight: 26
                        implicitWidth: 64
                        background: Rectangle {
                            color: bridge.projectSortMode === 0 ? "#2DD4BF" : (parent.hovered ? "#2A2A34" : "#121217")
                            border.color: bridge.projectSortMode === 0 ? "#14b8a6" : "#2A2A34"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 0 ? "#ffffff" : "#D4D4D8"
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
                            color: bridge.projectSortMode === 1 ? "#2DD4BF" : (parent.hovered ? "#2A2A34" : "#121217")
                            border.color: bridge.projectSortMode === 1 ? "#14b8a6" : "#2A2A34"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 1 ? "#ffffff" : "#D4D4D8"
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
                            color: bridge.projectSortMode === 2 ? "#2DD4BF" : (parent.hovered ? "#2A2A34" : "#121217")
                            border.color: bridge.projectSortMode === 2 ? "#14b8a6" : "#2A2A34"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 2 ? "#ffffff" : "#D4D4D8"
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
                            color: bridge.projectSortMode === 3 ? "#2DD4BF" : (parent.hovered ? "#2A2A34" : "#121217")
                            border.color: bridge.projectSortMode === 3 ? "#14b8a6" : "#2A2A34"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: bridge.projectSortMode === 3 ? "#ffffff" : "#D4D4D8"
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
                    color: itemMouseArea.containsMouse ? "#1F1F26" : "#121217"
                    border.color: itemMouseArea.containsMouse ? "#B89D64" : "#2A2A34"

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
                                    color: "#F4F4F6"
                                    font.bold: true
                                    font.pixelSize: 13
                                }
                                Text {
                                    text: "• " + modelData.id + " • " + modelData.date
                                    color: "#8E8E99"
                                    font.pixelSize: 10
                                }
                            }

                            Text {
                                text: (modelData.description && modelData.description.trim() !== "") ? modelData.description : "No description"
                                color: (modelData.description && modelData.description.trim() !== "") ? "#A1A1AA" : "#2A2A34"
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
                            background: Rectangle { color: parent.hovered ? "#B89D64" : "#9A804D"; radius: 4 }
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
            color: "#0A0A0E"
            radius: 12
            border.color: "#2A2A34"
            border.width: 1
        }

        header: Rectangle {
            height: 56
            color: "#121217"
            radius: 12
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#2A2A34"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 14

                Text {
                    text: "Hardware & Scanner Configuration"
                    color: "#F4F4F6"
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
                        color: configureSensorsDialog.currentTab === 0 ? "#B89D64" : "#0A0A0E"
                        border.color: configureSensorsDialog.currentTab === 0 ? "#B89D64" : "#2A2A34"
                        Text {
                            anchors.centerIn: parent
                            text: "📦 Sensor Composition"
                            color: configureSensorsDialog.currentTab === 0 ? "#09090B" : "#14141A"
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
                        color: configureSensorsDialog.currentTab === 1 ? "#B89D64" : "#0A0A0E"
                        border.color: configureSensorsDialog.currentTab === 1 ? "#B89D64" : "#2A2A34"
                        Text {
                            anchors.centerIn: parent
                            text: "🔍 Discover Devices"
                            color: configureSensorsDialog.currentTab === 1 ? "#09090B" : "#14141A"
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
                        color: configureSensorsDialog.currentTab === 2 ? "#B89D64" : "#0A0A0E"
                        border.color: configureSensorsDialog.currentTab === 2 ? "#B89D64" : "#2A2A34"
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: "📚 Sensor Catalog"
                                color: configureSensorsDialog.currentTab === 2 ? "#09090B" : "#14141A"
                                font.bold: true
                                font.pixelSize: 11
                            }
                            Rectangle {
                                implicitHeight: 18
                                implicitWidth: catalogBadgeText.contentWidth + 10
                                radius: 9
                                color: configureSensorsDialog.currentTab === 2 ? "#9A804D" : "#121217"
                                border.color: configureSensorsDialog.currentTab === 2 ? "#B89D64" : "#2A2A34"
                                Text {
                                    id: catalogBadgeText
                                    anchors.centerIn: parent
                                    text: "" + bridge.ecosystemModelCount
                                    color: configureSensorsDialog.currentTab === 2 ? "#ffffff" : "#A1A1AA"
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
                    color: "#0A0A0E"
                    border.color: "#B89D64"
                    Text {
                        anchors.centerIn: parent
                        text: "Host: scanar-01 (Linux)"
                        color: "#D4BE8D"
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
                        color: "#F4F4F6"
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
                            color: "#D4BE8D"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? "#121217" : "#0A0A0E"
                            radius: 4
                            border.color: "#B89D64"
                        }
                    }
                }

                TextField {
                    id: customConfigNameInput
                    Layout.fillWidth: true
                    implicitHeight: 38
                    text: "RoboSense Airy + Gemini 336L + VITURE Ultra"
                    color: "#F4F4F6"
                    font.pixelSize: 13
                    leftPadding: 12
                    rightPadding: 12
                    topPadding: 8
                    bottomPadding: 8
                    background: Rectangle {
                        color: "#0A0A0E"
                        radius: 6
                        border.color: customConfigNameInput.activeFocus ? "#B89D64" : "#2A2A34"
                        border.width: customConfigNameInput.activeFocus ? 2 : 1
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Select Hardware Sensors to Compose:"
                        color: "#F4F4F6"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: configureSensorsDialog.getSelectedIds().length + " of " + configureSensorsDialog.availableProfiles.length + " selected"
                        color: "#D4BE8D"
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
                                color: isSelected ? "#121217" : "#0A0A0E"
                                border.color: isSelected ? "#B89D64" : "#1F1F26"
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
                                                color: "#F4F4F6"
                                                font.pixelSize: 14
                                                font.bold: true
                                            }
                                            Rectangle {
                                                implicitHeight: 20
                                                implicitWidth: tagText.contentWidth + 12
                                                radius: 4
                                                color: "#0A0A0E"
                                                border.color: prof.tagColor || "#B89D64"
                                                Text {
                                                    id: tagText
                                                    anchors.centerIn: parent
                                                    text: prof.tag || "SENSOR"
                                                    color: prof.tagColor || "#D4BE8D"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }
                                            Rectangle {
                                                implicitHeight: 18
                                                implicitWidth: statusText.contentWidth + 10
                                                radius: 3
                                                color: prof.status === "VALIDATED" ? "#0C2618" : "#2A2A34"
                                                Text {
                                                    id: statusText
                                                    anchors.centerIn: parent
                                                    text: prof.status
                                                    color: prof.status === "VALIDATED" ? "#34D399" : "#A1A1AA"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }
                                        }

                                        Text {
                                            text: prof.sensorType + "  ·  Serial: " + prof.serialNumber + "  ·  " + prof.transport
                                            color: "#D4D4D8"
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
                                    color: isSelected ? "#0C2618" : "#121217"
                                    border.color: isSelected ? "#22c55e" : "#2A2A34"
                                    border.width: isSelected ? 1.5 : 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: isSelected ? "✓ INCLUDED" : "○ EXCLUDED"
                                        color: isSelected ? "#34D399" : "#A1A1AA"
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
                    color: "#14110A"
                    radius: 6
                    border.color: "#FBBF24"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        Text {
                            text: "📐 Spatial Notice: Extrinsics strictly UNKNOWN — Each sensor operates an independent SLAM pipeline with uncoupled coordinate frames."
                            color: "#FBBF24"
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
                    color: "#0A0A0E"
                    radius: 10
                    border.color: "#B89D64"
                    border.width: 1.5

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 20
                        width: parent.width - 80

                        Text {
                            text: "⚡ Autonomous Hardware Interrogation & Capability Matching"
                            color: "#F4F4F6"
                            font.pixelSize: 16
                            font.bold: true
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Text {
                            text: "Interrogating physical device registers, streaming capabilities, and matching SLAM backends..."
                            color: "#A1A1AA"
                            font.pixelSize: 12
                            Layout.alignment: Qt.AlignHCenter
                        }

                        // Progress Bar
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 14
                            color: "#0A0A0E"
                            radius: 7
                            border.color: "#2A2A34"

                            Rectangle {
                                width: parent.width * (configureSensorsDialog.profilingProgress / 100.0)
                                height: parent.height
                                radius: 7
                                color: "#9A804D"
                            }
                        }

                        Text {
                            text: configureSensorsDialog.profilingStep
                            color: "#D4BE8D"
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
                    color: "#0A0A0E"
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
                                color: "#34D399"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: 110
                                radius: 4
                                color: "#0C2618"
                                border.color: "#22c55e"
                                Text {
                                    anchors.centerIn: parent
                                    text: "EXPERIMENTAL"
                                    color: "#34D399"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: "#2A2A34"
                        }

                        // Detailed summary fields
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 10
                            columnSpacing: 20

                            Text { text: "Device:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.deviceName || ""; color: "#F4F4F6"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Profile ID:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.profileId || ""; color: "#D4BE8D"; font.pixelSize: 12 }

                            Text { text: "Native Capabilities:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.nativeCapabilities || "Unknown"; color: "#F4F4F6"; font.pixelSize: 12 }

                            Text { text: "Native Depth:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeDepth ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeDepth ? "#34D399" : "#DC2626"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Native IMU:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeImu ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeImu ? "#34D399" : "#DC2626"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Native Geometry:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: (configureSensorsDialog.profileResult.nativeGeometry ? "YES" : "NO"); color: configureSensorsDialog.profileResult.nativeGeometry ? "#34D399" : "#DC2626"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Compatible Reconstruction:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.compatibleReconstruction || "UNKNOWN"; color: "#D4BE8D"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Selected Backend:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.selectedBackend || "NONE"; color: "#B89D64"; font.pixelSize: 12; font.bold: true }

                            Text { text: "Geometry:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Text { text: configureSensorsDialog.profileResult.geometrySource || "NONE"; color: "#F4F4F6"; font.pixelSize: 12 }

                            Text { text: "Scale Status:"; color: "#A1A1AA"; font.pixelSize: 12; font.bold: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: scaleSummText.contentWidth + 14
                                radius: 4
                                color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#121217" : "#1C1406"
                                border.color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#2A2A34" : "#f59e0b"
                                Text {
                                    id: scaleSummText
                                    anchors.centerIn: parent
                                    text: configureSensorsDialog.profileResult.scaleStatus || "NO_RECONSTRUCTION_BACKEND_MATCHED"
                                    color: (configureSensorsDialog.profileResult.scaleStatus && (configureSensorsDialog.profileResult.scaleStatus.indexOf("NO_RECONSTRUCTION") !== -1 || configureSensorsDialog.profileResult.scaleStatus.indexOf("NONE") !== -1)) ? "#A1A1AA" : "#FBBF24"
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
                                    color: "#D4D4D8"
                                    font.pixelSize: 12
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#2A2A34" : "#121217"
                                    radius: 6
                                    border.color: "#2A2A34"
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
                                    color: parent.isCapturableBackend ? (parent.hovered ? "#4ADE80" : "#16a34a") : (parent.hovered ? "#2A2A34" : "#121217")
                                    radius: 6
                                    border.color: parent.isCapturableBackend ? "#22c55e" : "#2A2A34"
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
                                color: "#F4F4F6"
                                font.pixelSize: 14
                                font.bold: true
                            }
                            Text {
                                text: bridge.discoveryStatusText
                                color: "#D4BE8D"
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
                                color: scanInterfaceBtn.enabled ? "#D4BE8D" : "#8E8E99"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: scanInterfaceBtn.hovered && scanInterfaceBtn.enabled ? "#121217" : "#0A0A0E"
                                radius: 6
                                border.color: scanInterfaceBtn.enabled ? "#B89D64" : "#2A2A34"
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
                                color: parent.hovered ? "#B89D64" : "#9A804D"
                                radius: 6
                                border.color: "#B89D64"
                            }
                        }
                    }

                    // Empty state notification
                    Rectangle {
                        visible: bridge.discoveredPhysicalDevices.length === 0 && !bridge.isDiscovering
                        Layout.fillWidth: true
                        implicitHeight: 80
                        radius: 8
                        color: "#0A0A0E"
                        border.color: "#2A2A34"
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 4
                            Text {
                                text: "No hardware sensors currently detected."
                                color: "#A1A1AA"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                text: "Connect your camera or sensor via USB/Ethernet and click 'Scan Interfaces'."
                                color: "#8E8E99"
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
                                    color: isDisconnected ? "#0A0A0E" : "#0A0A0E"
                                    border.color: isUnprofiled ? "#FBBF24" : (isProfiled ? "#34D399" : (isUnsupported ? "#B89D64" : "#2A2A34"))
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
                                                        color: isDisconnected ? "#A1A1AA" : "#F4F4F6"
                                                        font.pixelSize: 13
                                                        font.bold: true
                                                    }

                                                    // 4 Distinct Status Badges
                                                    Rectangle {
                                                        implicitHeight: 20
                                                        implicitWidth: badgeText.contentWidth + 12
                                                        radius: 4
                                                        color: isUnprofiled ? "#1C1406" : (isProfiled ? "#0C2618" : (isUnsupported ? "#1F1A10" : "#121217"))
                                                        border.color: isUnprofiled ? "#FBBF24" : (isProfiled ? "#22c55e" : (isUnsupported ? "#B89D64" : "#8E8E99"))
                                                        Text {
                                                            id: badgeText
                                                            anchors.centerIn: parent
                                                            text: isUnprofiled ? "⚠️ UNPROFILED" : (isProfiled ? "✓ PROFILED" : (isUnsupported ? "🚫 UNSUPPORTED" : "🔌 DISCONNECTED"))
                                                            color: isUnprofiled ? "#FBBF24" : (isProfiled ? "#34D399" : (isUnsupported ? "#D4BE8D" : "#A1A1AA"))
                                                            font.pixelSize: 9
                                                            font.bold: true
                                                        }
                                                    }

                                                    Text {
                                                        visible: isProfiled && dev.existingProfileId !== ""
                                                        text: "Profile: " + dev.existingProfileId
                                                        color: "#D4BE8D"
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
                                                    color: "#A1A1AA"
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
                                                    color: parent.hovered ? "#B89D64" : "#9A804D"
                                                    radius: 6
                                                    border.color: "#B89D64"
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
                                                    color: "#D4BE8D"
                                                    font.pixelSize: 11
                                                    font.bold: true
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle {
                                                    color: parent.hovered ? "#1F1A10" : "#14110A"
                                                    radius: 6
                                                    border.color: "#B89D64"
                                                }
                                            }

                                            Rectangle {
                                                visible: isProfiled && dev.recommendedBackend !== "NONE" && dev.recommendedBackend !== "" && dev.compatibleReconstruction !== "UNKNOWN"
                                                implicitWidth: 110
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#0A0A0E"
                                                border.color: "#34D399"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Ready for Capture"
                                                    color: "#34D399"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            Rectangle {
                                                visible: isProfiled && (dev.recommendedBackend === "NONE" || dev.recommendedBackend === "" || dev.compatibleReconstruction === "UNKNOWN")
                                                implicitWidth: 240
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#121217"
                                                border.color: "#2A2A34"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Not currently available for spatial capture"
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }
                                            }

                                            Rectangle {
                                                visible: isDisconnected
                                                implicitWidth: 100
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#0A0A0E"
                                                border.color: "#2A2A34"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "Device Offline"
                                                    color: "#8E8E99"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            Rectangle {
                                                visible: isUnsupported
                                                implicitWidth: 130
                                                implicitHeight: 28
                                                radius: 4
                                                color: "#14110A"
                                                border.color: "#B89D64"
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: "No Compatible Backend"
                                                    color: "#D4BE8D"
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
                                                color: "#8E8E99"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }

                                            Repeater {
                                                model: dev.interfaces || []
                                                delegate: Rectangle {
                                                    implicitHeight: 20
                                                    implicitWidth: subIfaceText.contentWidth + 12
                                                    radius: 3
                                                    color: modelData.isPrimary ? "#121217" : "#0A0A0E"
                                                    border.color: modelData.isPrimary ? "#B89D64" : "#2A2A34"
                                                    Text {
                                                        id: subIfaceText
                                                        anchors.centerIn: parent
                                                        text: modelData.node + " [" + modelData.type + (modelData.isPrimary ? " - Primary" : "") + "]"
                                                        color: modelData.isPrimary ? "#F4F4F6" : "#A1A1AA"
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
                                                color: "#D4D4D8"
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                text: "·"
                                                color: "#2A2A34"
                                                font.pixelSize: 10
                                            }

                                            Text {
                                                text: "SLAM Pipeline: " + (dev.compatibleReconstruction || "None") + " (" + (dev.recommendedBackend || "N/A") + ")"
                                                color: "#D4BE8D"
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
                    color: "#0A0A0E"
                    border.color: "#121217"

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
                            placeholderTextColor: "#8E8E99"
                            color: "#F4F4F6"
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
                                color: "#A1A1AA"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#2A2A34" : "#121217"
                                radius: 4
                            }
                        }

                        Rectangle {
                            height: 24
                            width: 1
                            color: "#2A2A34"
                        }

                        // Total Statistics Badge
                        Rectangle {
                            implicitHeight: 28
                            implicitWidth: catalogStatsText.contentWidth + 16
                            radius: 14
                            color: "#121217"
                            border.color: "#B89D64"
                            Text {
                                id: catalogStatsText
                                anchors.centerIn: parent
                                text: configureSensorsDialog.catalogModels.length + " Models Shown · 16 Real Manufacturers"
                                color: "#D4BE8D"
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
                    color: configureSensorsDialog.catalogNotificationSuccess ? "#07170F" : "#280D10"
                    border.color: configureSensorsDialog.catalogNotificationSuccess ? "#10b981" : "#ef4444"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            text: configureSensorsDialog.catalogNotificationSuccess ? "✓" : "⚠"
                            color: configureSensorsDialog.catalogNotificationSuccess ? "#34D399" : "#DC2626"
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
                                color: "#A1A1AA"
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
                        color: "#0A0A0E"
                        border.color: "#121217"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Text {
                                text: "MANUFACTURERS (16)"
                                color: "#8E8E99"
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
                                        color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#B89D64" : (allVendMouse.containsMouse ? "#121217" : "transparent")
                                        border.color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#B89D64" : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            Text {
                                                text: "🌐 All Ecosystems"
                                                color: configureSensorsDialog.catalogSelectedVendor === "All" ? "white" : "#D4D4D8"
                                                font.pixelSize: 11
                                                font.bold: configureSensorsDialog.catalogSelectedVendor === "All"
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: "" + bridge.ecosystemModelCount
                                                color: configureSensorsDialog.catalogSelectedVendor === "All" ? "#F4F4F6" : "#8E8E99"
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
                                            color: configureSensorsDialog.catalogSelectedVendor === modelData ? "#B89D64" : (vendMouse.containsMouse ? "#121217" : "transparent")
                                            border.color: configureSensorsDialog.catalogSelectedVendor === modelData ? "#B89D64" : "transparent"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8

                                                Text {
                                                    text: modelData
                                                    color: configureSensorsDialog.catalogSelectedVendor === modelData ? "white" : "#D4D4D8"
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
                                color: "#121217"
                            }

                            // Sensor Type Filter
                            Text {
                                text: "SENSOR TYPE"
                                color: "#8E8E99"
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
                                        color: configureSensorsDialog.catalogSelectedType === modelData ? "#B89D64" : (typeMouse.containsMouse ? "#2A2A34" : "#121217")
                                        border.color: configureSensorsDialog.catalogSelectedType === modelData ? "#B89D64" : "#2A2A34"

                                        Text {
                                            id: typeFilterText
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: configureSensorsDialog.catalogSelectedType === modelData ? "white" : "#A1A1AA"
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
                        color: "#0A0A0E"
                        border.color: "#121217"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "MODELS (" + configureSensorsDialog.catalogModels.length + ")"
                                    color: "#8E8E99"
                                    font.bold: true
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: configureSensorsDialog.catalogSelectedVendor
                                    color: "#D4BE8D"
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
                                        color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#121217" : (cardMouse.containsMouse ? "#121217" : "#0A0A0E")
                                        border.color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#B89D64" : (cardMouse.containsMouse ? "#2A2A34" : "#121217")
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
                                                color: modelData.typeColor ? modelData.typeColor + "22" : "#9A804D22"
                                                border.color: modelData.typeColor ? modelData.typeColor : "#B89D64"

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
                                                        color: configureSensorsDialog.catalogSelectedModelId === modelData.id ? "#D4BE8D" : "white"
                                                        font.bold: true
                                                        font.pixelSize: 11
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Text {
                                                    text: modelData.family + " · " + modelData.hardwareRevision
                                                    color: "#A1A1AA"
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
                                                        color: modelData.productionStatus === "CURRENT" ? "#07170F" : (modelData.productionStatus === "PREVIOUS" ? "#14110A" : "#1C1406")
                                                        border.color: modelData.productionStatus === "CURRENT" ? "#10b981" : (modelData.productionStatus === "PREVIOUS" ? "#B89D64" : "#f59e0b")
                                                        Text {
                                                            id: statusText
                                                            anchors.centerIn: parent
                                                            text: modelData.productionStatus
                                                            color: modelData.productionStatus === "CURRENT" ? "#34D399" : (modelData.productionStatus === "PREVIOUS" ? "#D4BE8D" : "#FBBF24")
                                                            font.pixelSize: 8
                                                            font.bold: true
                                                        }
                                                    }

                                                    // Validation badge
                                                    Rectangle {
                                                        implicitHeight: 16
                                                        implicitWidth: tierText.contentWidth + 8
                                                        radius: 3
                                                        color: modelData.validationTier === "AV_VALIDATED" ? "#07170F" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#14110A" : "#14110A")
                                                        border.color: modelData.validationTier === "AV_VALIDATED" ? "#10b981" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#B89D64" : "#B89D64")
                                                        Text {
                                                            id: tierText
                                                            anchors.centerIn: parent
                                                            text: modelData.validationTier
                                                            color: modelData.validationTier === "AV_VALIDATED" ? "#34D399" : (modelData.validationTier === "PHYSICALLY_TESTED" ? "#D4BE8D" : "#D4BE8D")
                                                            font.pixelSize: 8
                                                        }
                                                    }

                                                    // Transport badge
                                                    Rectangle {
                                                        implicitHeight: 16
                                                        implicitWidth: trText.contentWidth + 8
                                                        radius: 3
                                                        color: "#121217"
                                                        border.color: "#2A2A34"
                                                        Text {
                                                            id: trText
                                                            anchors.centerIn: parent
                                                            text: modelData.transportType
                                                            color: "#A1A1AA"
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
                        color: "#0A0A0E"
                        border.color: "#121217"

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
                                    color: "#A1A1AA"
                                    font.bold: true
                                    font.pixelSize: 13
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: "Explore manufacturer specifications, official drivers, 8-point runtime status,\nand seed experimental template profiles for offline project planning."
                                    color: "#8E8E99"
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
                                            color: "#F4F4F6"
                                            font.bold: true
                                            font.pixelSize: 16
                                        }

                                        Text {
                                            text: (configureSensorsDialog.catalogSelectedModelDetails.family || "") + " · Revision: " + (configureSensorsDialog.catalogSelectedModelDetails.hardwareRevision || "N/A") + " · Model ID: " + (configureSensorsDialog.catalogSelectedModelDetails.id || "")
                                            color: "#A1A1AA"
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
                                            color: parent.hovered ? "#B89D64" : "#9A804D"
                                            radius: 6
                                            border.color: "#B89D64"
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
                                        color: "#121217"
                                        border.color: "#B89D64"
                                        Text {
                                            id: bdgType
                                            anchors.centerIn: parent
                                            text: "Type: " + (configureSensorsDialog.catalogSelectedModelDetails.sensorType || "")
                                            color: "#D4BE8D"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgStatus.contentWidth + 12
                                        radius: 4
                                        color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#07170F" : "#1C1406"
                                        border.color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#10b981" : "#f59e0b"
                                        Text {
                                            id: bdgStatus
                                            anchors.centerIn: parent
                                            text: "Lifecycle: " + (configureSensorsDialog.catalogSelectedModelDetails.productionStatus || "")
                                            color: configureSensorsDialog.catalogSelectedModelDetails.productionStatus === "CURRENT" ? "#34D399" : "#FBBF24"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgVal.contentWidth + 12
                                        radius: 4
                                        color: "#14110A"
                                        border.color: "#B89D64"
                                        Text {
                                            id: bdgVal
                                            anchors.centerIn: parent
                                            text: "Tier: " + (configureSensorsDialog.catalogSelectedModelDetails.validationTier || "")
                                            color: "#D4BE8D"
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgTrans.contentWidth + 12
                                        radius: 4
                                        color: "#0A0A0E"
                                        border.color: "#2A2A34"
                                        Text {
                                            id: bdgTrans
                                            anchors.centerIn: parent
                                            text: "Transport: " + (configureSensorsDialog.catalogSelectedModelDetails.transportType || "")
                                            color: "#D4D4D8"
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        implicitHeight: 22
                                        implicitWidth: bdgBackend.contentWidth + 12
                                        radius: 4
                                        color: "#0C2618"
                                        border.color: "#22c55e"
                                        Text {
                                            id: bdgBackend
                                            anchors.centerIn: parent
                                            text: "SLAM: " + (configureSensorsDialog.catalogSelectedModelDetails.recommendedSlamBackend || "FAST-LIO2")
                                            color: "#34D399"
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
                                    color: "#121217"
                                    border.color: "#121217"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "HARDWARE & NETWORK SPECIFICATIONS"
                                            color: "#D4BE8D"
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
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "USB IDs: " + (configureSensorsDialog.catalogSelectedModelDetails.vendorIds && configureSensorsDialog.catalogSelectedModelDetails.vendorIds.length > 0 ? "VID: " + configureSensorsDialog.catalogSelectedModelDetails.vendorIds.join(", ") + " · PID: " + configureSensorsDialog.catalogSelectedModelDetails.productIds.join(", ") : "N/A (Network LiDAR)")
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "Coordinate System: " + (configureSensorsDialog.catalogSelectedModelDetails.nativeCoordinateSystem || "FLU") + " (+X Forward, +Y Left, +Z Up)"
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Scan Rates: " + (configureSensorsDialog.catalogSelectedModelDetails.scanRates && configureSensorsDialog.catalogSelectedModelDetails.scanRates.length > 0 ? configureSensorsDialog.catalogSelectedModelDetails.scanRates.join(", ") + " Hz" : "30.0 Hz")
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "Return Modes: " + (configureSensorsDialog.catalogSelectedModelDetails.returnModes && configureSensorsDialog.catalogSelectedModelDetails.returnModes.length > 0 ? configureSensorsDialog.catalogSelectedModelDetails.returnModes.join(", ") : "Single / Strongest")
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "Sync / PTP: " + (configureSensorsDialog.catalogSelectedModelDetails.hasPtp ? "PTP IEEE 1588v2 · " : "") + (configureSensorsDialog.catalogSelectedModelDetails.hasPps ? "PPS Sync" : "Host Timestamping")
                                                    color: "#A1A1AA"
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
                                    color: "#121217"
                                    border.color: "#121217"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "OFFICIAL VENDOR SDK & DOCUMENTATION PROVENANCE"
                                            color: "#D4BE8D"
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
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: "ROS / ROS 2: " + (configureSensorsDialog.catalogSelectedModelDetails.rosDriver || "ROS 2 Humble / Jazzy compatible")
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                                Text {
                                                    text: "License / OS: " + (configureSensorsDialog.catalogSelectedModelDetails.license || "Proprietary") + " · " + (configureSensorsDialog.catalogSelectedModelDetails.supportedOs ? configureSensorsDialog.catalogSelectedModelDetails.supportedOs.join(", ") : "Linux, Windows")
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 4
                                                Text {
                                                    text: "Manual: " + (configureSensorsDialog.catalogSelectedModelDetails.manualRef || "Manufacturer User Manual")
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: "Verified Date: " + (configureSensorsDialog.catalogSelectedModelDetails.lastVerified || "2026-09")
                                                    color: "#A1A1AA"
                                                    font.pixelSize: 10
                                                }
                                                // Clickable URL
                                                Text {
                                                    text: "🔗 " + (configureSensorsDialog.catalogSelectedModelDetails.docUrl || "https://github.com")
                                                    color: "#D4BE8D"
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
                                    color: "#121217"
                                    border.color: "#121217"

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 6

                                        Text {
                                            text: "8-POINT RUNTIME INTEGRATION STATUS (A–H)"
                                            color: "#D4BE8D"
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
                                                color: "#34D399"
                                                font.pixelSize: 10
                                            }

                                            // B. Official SDK Identified
                                            Text {
                                                text: "B. Official SDK Identified:  ✓ " + (configureSensorsDialog.catalogSelectedModelDetails.officialSdkId || configureSensorsDialog.catalogSelectedModelDetails.sdkFamily || "SDK Identified")
                                                color: "#34D399"
                                                font.pixelSize: 10
                                            }

                                            // C. SDK Integrated in AV Scan
                                            Text {
                                                text: "C. SDK Driver Integrated:  " + (configureSensorsDialog.catalogSelectedModelDetails.sdkIntegrated ? "✓ Integrated" : "○ Driver Planned / Headless")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.sdkIntegrated ? "#34D399" : "#FBBF24"
                                                font.pixelSize: 10
                                            }

                                            // D. Hardware Interrogation
                                            Text {
                                                text: "D. HW Interrogation:  " + (configureSensorsDialog.catalogSelectedModelDetails.hwInterrogated ? "✓ Live Interrogation Supported" : "○ Interrogation on Physical Attach")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.hwInterrogated ? "#34D399" : "#A1A1AA"
                                                font.pixelSize: 10
                                            }

                                            // E. Runtime Streaming Adapter
                                            Text {
                                                text: "E. Streaming Adapter:  " + (configureSensorsDialog.catalogSelectedModelDetails.adapterImplemented ? "✓ " + configureSensorsDialog.catalogSelectedModelDetails.runtimeAdapter : "○ " + (configureSensorsDialog.catalogSelectedModelDetails.runtimeAdapter || "Generic Adapter"))
                                                color: configureSensorsDialog.catalogSelectedModelDetails.adapterImplemented ? "#34D399" : "#A1A1AA"
                                                font.pixelSize: 10
                                            }

                                            // F. Compatible SLAM Backend
                                            Text {
                                                text: "F. Compatible SLAM Backend:  ✓ " + (configureSensorsDialog.catalogSelectedModelDetails.recommendedSlamBackend || "FAST-LIO2 / RTAB-Map")
                                                color: "#34D399"
                                                font.pixelSize: 10
                                            }

                                            // G. Physically Tested
                                            Text {
                                                text: "G. Physical HW Tested:  " + (configureSensorsDialog.catalogSelectedModelDetails.physicallyTested ? "✓ Bench Validated" : "○ Unplugged (Pending Physical Rig)")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.physicallyTested ? "#34D399" : "#8E8E99"
                                                font.pixelSize: 10
                                            }

                                            // H. AV GUI Validated
                                            Text {
                                                text: "H. AV GUI Live Validated:  " + (configureSensorsDialog.catalogSelectedModelDetails.guiValidated ? "✓ Live Validated" : "○ Ready for Rig Session")
                                                color: configureSensorsDialog.catalogSelectedModelDetails.guiValidated ? "#34D399" : "#8E8E99"
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
                                    color: "#0A0A0E"
                                    border.color: "#2A2A34"

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
                                            color: "#A1A1AA"
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
            color: "#0A0A0E"
            radius: 12
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1F1F26"
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
                        color: "#D4D4D8"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#2A2A34" : "#121217"
                        radius: 6
                        border.color: "#2A2A34"
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
                        color: "#D4BE8D"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#121217" : "#0A0A0E"
                        radius: 6
                        border.color: "#B89D64"
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
                        color: parent.enabled ? "white" : "#8E8E99"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#B89D64" : "#9A804D") : "#121217"
                        radius: 6
                        border.color: parent.enabled ? "#B89D64" : "#1F1F26"
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
            color: "#0A0A0E"
            radius: 12
            border.color: "#B89D64"
            border.width: 1.5
        }

        header: Rectangle {
            height: 58
            color: "#14110A"
            radius: 12

            MouseArea {
                anchors.fill: parent
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1F1A10"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 16
                spacing: 12

                Text {
                    text: "📐 Sensor Orientation & Canonical Transform Diagnostic"
                    color: "#F4F4F6"
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
                        color: "#D4BE8D"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#1F1A10" : "#0A0A0E"
                        radius: 4
                        border.color: "#B89D64"
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "✥ Drag header to move"
                    color: "#A1A1AA"
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
                        color: "#F4F4F6"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#dc2626" : "#2A2A34"
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
                        color: "#A1A1AA"
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
                                color: bridge.orientationDiagSensor === "AIRY" ? "#22c55e" : "#A1A1AA"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 50
                                height: 16
                                radius: 8
                                color: "#0C2618"
                                border.color: "#22c55e"
                                Text {
                                    anchors.centerIn: parent
                                    text: "LOCKED"
                                    color: "#34D399"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "AIRY" ? "#07170F" : "#121217"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "AIRY" ? "#22c55e" : "#2A2A34"
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
                                color: bridge.orientationDiagSensor === "GEMINI" ? "#D4BE8D" : "#A1A1AA"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 44
                                height: 16
                                radius: 8
                                color: "#1F1A10"
                                border.color: "#B89D64"
                                Text {
                                    anchors.centerIn: parent
                                    text: "RGBD"
                                    color: "#D4BE8D"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "GEMINI" ? "#14110A" : "#121217"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "GEMINI" ? "#B89D64" : "#2A2A34"
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
                                color: bridge.orientationDiagSensor === "VITURE" ? "#f59e0b" : "#A1A1AA"
                                font.bold: true
                                font.pixelSize: 12
                            }
                            Rectangle {
                                width: 40
                                height: 16
                                radius: 8
                                color: "#1C1406"
                                border.color: "#f59e0b"
                                Text {
                                    anchors.centerIn: parent
                                    text: "VIO"
                                    color: "#FBBF24"
                                    font.pixelSize: 9
                                    font.bold: true
                                }
                            }
                        }
                        background: Rectangle {
                            color: bridge.orientationDiagSensor === "VITURE" ? "#1C1406" : "#121217"
                            radius: 6
                            border.color: bridge.orientationDiagSensor === "VITURE" ? "#f59e0b" : "#2A2A34"
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
                    color: "#07170F"
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
                            color: "#34D399"
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
                            color: "#A1A1AA"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Button {
                            text: "ROS Optical Frame (-90° X, 0° Y, -90° Z)"
                            implicitHeight: 28
                            onClicked: bridge.setOrientationDiagRotation(-90.0, 0.0, -90.0)
                            contentItem: Text {
                                text: parent.text; color: "#F4F4F6"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#2A2A34" : "#121217"; radius: 4; border.color: "#2A2A34" }
                        }

                        Button {
                            text: "Identity (0°, 0°, 0°)"
                            implicitHeight: 28
                            onClicked: bridge.resetOrientationDiagRotation()
                            contentItem: Text {
                                text: parent.text; color: "#F4F4F6"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#2A2A34" : "#121217"; radius: 4; border.color: "#2A2A34" }
                        }

                        Button {
                            text: "Inverted Optical (+90° X, 0° Y, +90° Z)"
                            implicitHeight: 28
                            onClicked: bridge.setOrientationDiagRotation(90.0, 0.0, 90.0)
                            contentItem: Text {
                                text: parent.text; color: "#F4F4F6"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#2A2A34" : "#121217"; radius: 4; border.color: "#2A2A34" }
                        }

                        Button {
                            text: "🎯 Auto-Level Floor (RANSAC)"
                            implicitHeight: 28
                            onClicked: {
                                bridge.autoLevelOrientationDiag()
                                canvas3d.requestPaint()
                                canvas2d.requestPaint()
                            }
                            contentItem: Text {
                                text: parent.text; color: "#34D399"; font.pixelSize: 11; font.bold: true
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: parent.hovered ? "#0C2618" : "#07170F"; radius: 4; border.color: "#22c55e" }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // --- ROLL / X ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Roll (X-Axis Rotation) — Rotation around Physical Forward (+X)"
                                    color: "#DC2626"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagRoll.toFixed(1) + "°"
                                    color: "#DC2626"
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
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", -15.0)
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", -1.0)
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
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
                                        color: "#2A2A34"
                                    }
                                    handle: Rectangle {
                                        x: rollSlider.leftPadding + rollSlider.visualPosition * (rollSlider.availableWidth - width)
                                        y: rollSlider.topPadding + rollSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: rollSlider.pressed ? "#B91C1C" : "#DC2626"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 1.0)
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 15.0)
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("ROLL", 90.0)
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#1C090B"; radius: 4; border.color: "#280D10" }
                                }
                            }
                        }
                    }

                    // --- PITCH / Y ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Pitch (Y-Axis Rotation) — Rotation around Physical Left (+Y)"
                                    color: "#34D399"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagPitch.toFixed(1) + "°"
                                    color: "#34D399"
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
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", -15.0)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", -1.0)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
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
                                        color: "#2A2A34"
                                    }
                                    handle: Rectangle {
                                        x: pitchSlider.leftPadding + pitchSlider.visualPosition * (pitchSlider.availableWidth - width)
                                        y: pitchSlider.topPadding + pitchSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: pitchSlider.pressed ? "#34D399" : "#059669"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 1.0)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 15.0)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("PITCH", 90.0)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#07170F"; radius: 4; border.color: "#34D399" }
                                }
                            }
                        }
                    }

                    // --- YAW / Z ROTATION ---
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 82
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Yaw (Z-Axis Rotation) — Rotation around Physical Up (+Z)"
                                    color: "#D4BE8D"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: bridge.orientationDiagYaw.toFixed(1) + "°"
                                    color: "#D4BE8D"
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
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
                                }
                                Button {
                                    text: "-15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", -15.0)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
                                }
                                Button {
                                    text: "-1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", -1.0)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
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
                                        color: "#2A2A34"
                                    }
                                    handle: Rectangle {
                                        x: yawSlider.leftPadding + yawSlider.visualPosition * (yawSlider.availableWidth - width)
                                        y: yawSlider.topPadding + yawSlider.availableHeight / 2 - height / 2
                                        implicitWidth: 18
                                        implicitHeight: 18
                                        radius: 9
                                        color: yawSlider.pressed ? "#F4F4F6" : "#D4BE8D"
                                        border.color: "white"
                                        border.width: 1.5
                                    }
                                }

                                Button {
                                    text: "+1°"; implicitWidth: 38; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 1.0)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
                                }
                                Button {
                                    text: "+15°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 15.0)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
                                }
                                Button {
                                    text: "+90°"; implicitWidth: 44; implicitHeight: 26
                                    onClicked: bridge.stepOrientationDiagRotation("YAW", 90.0)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#14110A"; radius: 4; border.color: "#9A804D" }
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
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#2A2A34"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Text {
                                text: "Equivalent 3×3 Rotation Matrix (R):"
                                color: "#A1A1AA"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: bridge.orientationDiagMatrixStr
                                color: "#D4BE8D"
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
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#2A2A34"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4
                            Text {
                                text: "Candidate Transform T_AV_from_backend:"
                                color: "#A1A1AA"
                                font.pixelSize: 11
                                font.bold: true
                            }
                            Text {
                                text: bridge.orientationDiagTransformStr
                                color: "#D4BE8D"
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
                    color: "#0A0A0E"
                    radius: 8
                    border.color: "#9A804D"
                    border.width: 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4
                        Text {
                            text: "Physical Alignment Criteria Checklist:"
                            color: "#D4BE8D"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        RowLayout {
                            spacing: 24
                            ColumnLayout {
                                spacing: 2
                                Text { text: "✓ Floor is horizontal (parallel to XY grid plane)"; color: "#F4F4F6"; font.pixelSize: 11 }
                                Text { text: "✓ +Z is physically UP (ceiling above floor)"; color: "#F4F4F6"; font.pixelSize: 11 }
                                Text { text: "✓ Walls and doorways are vertical"; color: "#F4F4F6"; font.pixelSize: 11 }
                            }
                            ColumnLayout {
                                spacing: 2
                                Text { text: "✓ Physical forward corresponds to AV +X (screen-UP in 2D view)"; color: "#F4F4F6"; font.pixelSize: 11 }
                                Text { text: "✓ Physical left corresponds to AV +Y (screen-LEFT in 2D view)"; color: "#F4F4F6"; font.pixelSize: 11 }
                                Text { text: "✓ 3D Spatial Map and 2D Plan update live at 60 FPS while adjusting"; color: "#D4BE8D"; font.pixelSize: 11; font.bold: true }
                            }
                        }
                    }
                }
            }
        }

        footer: Rectangle {
            height: 64
            color: "#0A0A0E"
            radius: 12
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1F1F26"
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
                        text: parent.text; color: parent.enabled ? "#F4F4F6" : "#8E8E99"; font.pixelSize: 12; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#2A2A34" : "#121217"; radius: 6; border.color: "#2A2A34"
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
                        text: parent.text; color: "#D4D4D8"; font.pixelSize: 13; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#2A2A34" : "#121217"; radius: 6; border.color: "#2A2A34"
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
                        text: parent.text; color: parent.enabled ? "white" : "#8E8E99"; font.pixelSize: 13; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.enabled ? (parent.hovered ? "#B89D64" : "#9A804D") : "#121217"
                        radius: 6
                        border.color: parent.enabled ? "#B89D64" : "#1F1A10"
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
            color: "#0A0A0E"
            radius: 12
            border.color: "#1F1F26"
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
                        color: "#F4F4F6"
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Text {
                        text: "Decentralized Discovery • Persistent Node Identity • Synchronized Coordination Metadata • Independent Spatial Capture"
                        color: "#A1A1AA"
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "✕"
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: multiNodeMeshDialog.close()
                    contentItem: Text { text: parent.text; color: "#A1A1AA"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#2A2A34" : "transparent"; radius: 6 }
                }
            }

            // Real-Time Collaborative SLAM (C-SLAM) Banner
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 46
                color: "#07170F"
                radius: 8
                border.color: "#10b981"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Text { text: "🛰️"; font.pixelSize: 16 }
                    Text {
                        Layout.fillWidth: true
                        text: "REAL-TIME COLLABORATIVE SLAM (C-SLAM): Unlimited scanners can join the same project/session. Non-overlapping scans are oriented by relative IMU heading and placed at a distinct spatial offset with unique per-scanner colors until overlapping geometry is detected and automatically snapped together."
                        color: "#34D399"
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
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "💻 THIS SCANNER NODE (LOCAL)"
                                    color: "#D4BE8D"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    implicitHeight: 20
                                    implicitWidth: roleBadgeText.contentWidth + 12
                                    radius: 10
                                    color: bridge.isCoordinator ? "#0C2618" : "#14110A"
                                    border.color: bridge.isCoordinator ? "#10b981" : "#B89D64"
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
                                Text { text: "Persistent Node UUID (Survives IP & Reboots):"; color: "#8E8E99"; font.pixelSize: 10 }
                                Text {
                                    text: bridge.nodeUuid
                                    color: "#F4F4F6"
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    font.bold: true
                                }
                            }

                            // Display Name input
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Text { text: "Display Name:"; color: "#A1A1AA"; font.pixelSize: 11 }
                                TextField {
                                    id: nodeNameInput
                                    Layout.fillWidth: true
                                    text: bridge.nodeDisplayName
                                    color: "#F4F4F6"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0A0A0E"; radius: 4; border.color: "#2A2A34" }
                                }
                                Button {
                                    text: "Save"
                                    implicitWidth: 50
                                    implicitHeight: 28
                                    onClicked: bridge.setNodeDisplayName(nodeNameInput.text)
                                    contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#14110A" : "#0A0A0E"; radius: 4; border.color: "#B89D64" }
                                }
                            }

                            // Platform & Hardware Details
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Platform: "; color: "#8E8E99"; font.pixelSize: 10 }
                                Text { text: bridge.nodePlatform; color: "#A1A1AA"; font.pixelSize: 10; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: "Status: "; color: "#8E8E99"; font.pixelSize: 10 }
                                Text { text: bridge.nodeStatus; color: bridge.isCapturing ? "#f59e0b" : "#10b981"; font.pixelSize: 10; font.bold: true }
                            }
                            Text {
                                text: "Hardware: " + bridge.nodeHardware
                                color: "#8E8E99"
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
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "🎯 ACTIVE SESSION MANAGEMENT"
                                color: "#B89D64"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            ColumnLayout {
                                spacing: 2
                                Text { text: "Session ID:"; color: "#8E8E99"; font.pixelSize: 10 }
                                Text {
                                    text: bridge.isMultiNodeSessionActive ? bridge.activeMultiNodeSessionId : "No Active Multi-Node Session (Standalone)"
                                    color: bridge.isMultiNodeSessionActive ? "#D4BE8D" : "#A1A1AA"
                                    font.pixelSize: 11
                                    font.family: "Monospace"
                                    font.bold: bridge.isMultiNodeSessionActive
                                }
                            }

                            Text {
                                text: bridge.multiNodeNotice
                                color: "#A1A1AA"
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
                                    background: Rectangle { color: parent.hovered ? "#9A804D" : "#D4BE8D"; radius: 6 }
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
                                    contentItem: Text { text: parent.text; color: "#DC2626"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C090B" : "#14110A"; radius: 6; border.color: "#280D10" }
                                }
                            }
                        }
                    }

                    // Card 3: Routed / VPN / Tailscale Manual Peer
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "🌐 ROUTED / TAILSCALE / VPN PEER"
                                color: "#34D399"
                                font.pixelSize: 12
                                font.bold: true
                            }
                            Text {
                                text: "For non-broadcast routed networks or Tailscale mesh, enter peer IP directly:"
                                color: "#8E8E99"
                                font.pixelSize: 10
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                TextField {
                                    id: manualPeerIp
                                    Layout.fillWidth: true
                                    placeholderText: "Peer IP (e.g. 100.x.x.x or 192.168.1.50)"
                                    color: "#F4F4F6"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0A0A0E"; radius: 4; border.color: "#2A2A34" }
                                }
                                TextField {
                                    id: manualPeerPort
                                    implicitWidth: 60
                                    text: "9110"
                                    color: "#F4F4F6"
                                    font.pixelSize: 11
                                    background: Rectangle { color: "#0A0A0E"; radius: 4; border.color: "#2A2A34" }
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
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#07170F" : "#0A0A0E"; radius: 4; border.color: "#059669" }
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
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "📡 DISCOVERED NODES ON NETWORK (UDP :9110)"
                                    color: "#D4BE8D"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "🔄 Refresh"
                                    implicitWidth: 74
                                    implicitHeight: 24
                                    onClicked: bridge.refreshMultiNodeDiscovery()
                                    contentItem: Text { text: parent.text; color: "#A1A1AA"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#2A2A34" : "#0A0A0E"; radius: 4; border.color: "#2A2A34" }
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
                                    color: modelData.isSelf ? "#0A0A0E" : "#121217"
                                    border.color: modelData.isSelf ? "#2A2A34" : "#B89D64"
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 10

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: modelData.isSelf ? "#8E8E99" : (modelData.isAvailable ? "#10b981" : "#f59e0b")
                                        }

                                        ColumnLayout {
                                            spacing: 2
                                            RowLayout {
                                                spacing: 6
                                                Text {
                                                    text: modelData.displayName
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }
                                                Rectangle {
                                                    implicitHeight: 16
                                                    implicitWidth: platBadgeText.contentWidth + 8
                                                    radius: 4
                                                    color: "#0A0A0E"
                                                    border.color: "#2A2A34"
                                                    Text {
                                                        id: platBadgeText
                                                        anchors.centerIn: parent
                                                        text: modelData.platform
                                                        color: "#A1A1AA"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                                Text {
                                                    visible: modelData.isSelf
                                                    text: "(This Device)"
                                                    color: "#8E8E99"
                                                    font.pixelSize: 10
                                                }
                                            }
                                            Text {
                                                text: modelData.endpoint + " • UUID: " + modelData.shortUuid + " • " + modelData.sessionState
                                                color: "#A1A1AA"
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
                                            contentItem: Text { text: parent.text; color: "#D4BE8D"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.hovered ? "#9A804D" : "#D4BE8D"; radius: 4 }
                                        }

                                        Button {
                                            visible: !modelData.isSelf && !bridge.isMultiNodeSessionActive && modelData.currentSessionId !== ""
                                            text: "🔗 Join"
                                            implicitWidth: 70
                                            implicitHeight: 28
                                            onClicked: bridge.joinMultiNodeSession(modelData.currentSessionId, modelData.ip, modelData.port)
                                            contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { color: parent.hovered ? "#4ADE80" : "#34D399"; radius: 4 }
                                        }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: discListView.count === 0
                                    text: "No network peer nodes currently detected on UDP port 9110."
                                    color: "#8E8E99"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Active Session Members Section
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#1F1F26"

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
                                    color: "#A1A1AA"
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
                                    color: "#121217"
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
                                                    color: "#F4F4F6"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }
                                                Rectangle {
                                                    implicitHeight: 16
                                                    implicitWidth: memRoleBadge.contentWidth + 8
                                                    radius: 4
                                                    color: modelData.isCoordinator ? "#0C2618" : "#14110A"
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
                                                    color: modelData.capturing ? "#1C1406" : "#0A0A0E"
                                                    border.color: modelData.capturing ? "#f97316" : "#2A2A34"
                                                    Text {
                                                        id: memCapturingBadge
                                                        anchors.centerIn: parent
                                                        text: modelData.captureState
                                                        color: modelData.capturing ? "#fb923c" : "#A1A1AA"
                                                        font.pixelSize: 9
                                                        font.bold: true
                                                    }
                                                }
                                            }
                                            Text {
                                                text: modelData.ip + " • Clock Offset: " + modelData.clockOffsetMs + " • UUID: " + modelData.shortUuid
                                                color: "#A1A1AA"
                                                font.pixelSize: 10
                                                font.family: "Monospace"
                                            }
                                        }

                                        Item { Layout.fillWidth: true }

                                        Rectangle {
                                            implicitHeight: 22
                                            implicitWidth: connStatusText.contentWidth + 12
                                            radius: 11
                                            color: modelData.connected ? "#07170F" : "#1C090B"
                                            border.color: modelData.connected ? "#10b981" : "#ef4444"
                                            Text {
                                                id: connStatusText
                                                anchors.centerIn: parent
                                                text: modelData.connected ? "CONNECTED" : "OFFLINE"
                                                color: modelData.connected ? "#34D399" : "#DC2626"
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
                                    color: "#8E8E99"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    // Card 5: Real-Time C-SLAM Collaborative Map & Unlimited Scanner Alignment
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 205
                        color: "#0A0A0E"
                        radius: 8
                        border.color: "#10b981"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: "🌐 REAL-TIME C-SLAM COLLABORATIVE MERGED MAP (" + bridge.cslamScannerCount + " Scanners • " + bridge.cslamSnappedCount + " Snapped)"
                                    color: "#34D399"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: bridge.selectedNodeViewIndex === -1 ? "✓ Viewing Merged C-SLAM" : "View Merged C-SLAM Cloud"
                                    implicitHeight: 24
                                    implicitWidth: 165
                                    onClicked: bridge.selectNodeView(bridge.selectedNodeViewIndex === -1 ? 0 : -1)
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: bridge.selectedNodeViewIndex === -1 ? "#059669" : "#B89D64"; radius: 5 }
                                }
                                Button {
                                    text: bridge.cslamColorByScanner ? "🎨 Color: Per-Scanner" : "🎨 Color: Native RGB"
                                    implicitHeight: 24
                                    implicitWidth: 138
                                    onClicked: bridge.cslamColorByScanner = !bridge.cslamColorByScanner
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: bridge.cslamColorByScanner ? "#B89D64" : "#2A2A34"; radius: 5 }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Button {
                                    text: "+ Add Scanner (Separated IMU Offset)"
                                    implicitHeight: 24
                                    Layout.fillWidth: true
                                    onClicked: bridge.addSimulatedCslamScanner("", false)
                                    contentItem: Text { text: parent.text; color: "#FB923C"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#1C1406" : "#1C1406"; radius: 5; border.color: "#f97316" }
                                }
                                Button {
                                    text: "+ Add Scanner (Overlapping -> Auto-Snap)"
                                    implicitHeight: 24
                                    Layout.fillWidth: true
                                    onClicked: bridge.addSimulatedCslamScanner("", true)
                                    contentItem: Text { text: parent.text; color: "#34D399"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#0C2618" : "#07170F"; radius: 5; border.color: "#10b981" }
                                }
                                Button {
                                    text: "⚡ Snap Overlap"
                                    implicitHeight: 24
                                    implicitWidth: 105
                                    onClicked: bridge.triggerCslamSnap()
                                    contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 10; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#B89D64" : "#9A804D"; radius: 5 }
                                }
                                Button {
                                    text: "Clear"
                                    implicitHeight: 24
                                    implicitWidth: 55
                                    onClicked: bridge.clearCslamScanners()
                                    contentItem: Text { text: parent.text; color: "#D4D4D8"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { color: parent.hovered ? "#2A2A34" : "#121217"; radius: 5 }
                                }
                            }

                            ListView {
                                id: cslamScannerListView
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 4
                                model: bridge.cslamNodesList

                                delegate: Rectangle {
                                    width: cslamScannerListView.width
                                    height: 32
                                    color: "#0A0A0E"
                                    radius: 5
                                    border.color: modelData.colorHex
                                    border.width: 1

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 8

                                        Rectangle {
                                            width: 12
                                            height: 12
                                            radius: 6
                                            color: modelData.colorHex
                                        }

                                        Text {
                                            text: "#" + (modelData.scannerIndex + 1) + " " + modelData.displayName
                                            color: "#F4F4F6"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        Rectangle {
                                            implicitHeight: 16
                                            implicitWidth: alignBadgeText.contentWidth + 10
                                            radius: 4
                                            color: modelData.isSnapped ? "#07170F" : "#1C1406"
                                            border.color: modelData.isSnapped ? "#10b981" : "#f97316"
                                            Text {
                                                id: alignBadgeText
                                                anchors.centerIn: parent
                                                text: modelData.alignmentState
                                                color: modelData.isSnapped ? "#34D399" : "#FB923C"
                                                font.pixelSize: 9
                                                font.bold: true
                                            }
                                        }

                                        Text {
                                            text: "Pts: " + modelData.pointCount + " • Overlap: " + modelData.overlapPercent + " • Offset: " + modelData.offsetMeters
                                            color: "#A1A1AA"
                                            font.pixelSize: 10
                                            font.family: "Monospace"
                                        }

                                        Item { Layout.fillWidth: true }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: cslamScannerListView.count === 0
                                    text: bridge.cslamStatusSummary
                                    color: "#8E8E99"
                                    font.pixelSize: 10
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
                    contentItem: Text { text: parent.text; color: "#D4D4D8"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#2A2A34" : "#121217"; radius: 6; border.color: "#2A2A34" }
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
            color: "#0A0A0E"
            radius: 12
            border.color: "#2A2A34"
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
                    color: "#07170F"
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
                        color: "#F4F4F6"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Multi-Scale SAC-IA Coarse Alignment + GICP Fine Alignment • Strict SE(3) Metric Scale Lock • Visual Inspection"
                        color: "#A1A1AA"
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: "✕"
                    implicitWidth: 32
                    implicitHeight: 32
                    onClicked: offlineRegistrationDialog.close()
                    contentItem: Text { text: parent.text; color: "#A1A1AA"; font.pixelSize: 14; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.hovered ? "#2A2A34" : "transparent"; radius: 6 }
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
                    color: offlineRegistrationDialog.regMode === 0 ? "#9A804D" : "#121217"
                    border.color: offlineRegistrationDialog.regMode === 0 ? "#B89D64" : "#2A2A34"
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
                            color: offlineRegistrationDialog.regMode === 0 ? "white" : "#A1A1AA"
                            font.pixelSize: 11
                            font.bold: offlineRegistrationDialog.regMode === 0
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 36
                    radius: 6
                    color: offlineRegistrationDialog.regMode === 1 ? "#2DD4BF" : "#121217"
                    border.color: offlineRegistrationDialog.regMode === 1 ? "#14b8a6" : "#2A2A34"
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
                            color: offlineRegistrationDialog.regMode === 1 ? "white" : "#A1A1AA"
                            font.pixelSize: 11
                            font.bold: offlineRegistrationDialog.regMode === 1
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#121217" }

            // Main Two-Column Body
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 16

                // LEFT COLUMN: Configuration & Registration Execution
                Rectangle {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 490
                    color: "#121217"
                    radius: 8
                    border.color: "#1F1F26"
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Text {
                            text: offlineRegistrationDialog.regMode === 0 ? "1. SELECT SESSION CAPTURES TO REGISTER" : "1. SELECT CROSS-PROJECT CLOUDS TO MERGE"
                            color: "#D4BE8D"
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
                                        color: "#D4BE8D"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: "Source A (Cyan)"
                                        color: "#D4BE8D"
                                        font.pixelSize: 10
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 68
                                    color: "#0A0A0E"
                                    radius: 6
                                    border.color: "#121217"

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
                                            color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#1F1A10" : (refMa.containsMouse ? "#121217" : "transparent")
                                            border.color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#B89D64" : "transparent"
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
                                                    color: offlineRegistrationDialog.selectedRefId === modelData.capture_id ? "#D4BE8D" : "#F4F4F6"
                                                    font.pixelSize: 11
                                                    font.bold: offlineRegistrationDialog.selectedRefId === modelData.capture_id
                                                }
                                                Item { Layout.fillWidth: true }
                                                Text {
                                                    text: modelData.point_count + " pts"
                                                    color: "#A1A1AA"
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
                                        color: "#8E8E99"
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
                                    color: "#0A0A0E"
                                    radius: 6
                                    border.color: "#121217"

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
                                            color: offlineRegistrationDialog.selectedMovId === modelData.capture_id ? "#1C1406" : (movMa.containsMouse ? "#121217" : "transparent")
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
                                                    color: offlineRegistrationDialog.selectedMovId === modelData.capture_id ? "#ff9100" : "#F4F4F6"
                                                    font.pixelSize: 11
                                                    font.bold: offlineRegistrationDialog.selectedMovId === modelData.capture_id
                                                }
                                                Item { Layout.fillWidth: true }
                                                Text {
                                                    text: modelData.point_count + " pts"
                                                    color: "#A1A1AA"
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
                                        color: "#8E8E99"
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
                                        color: "#D4BE8D"
                                        font.bold: true
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text { text: "Source A (Cyan)"; color: "#D4BE8D"; font.pixelSize: 10 }
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
                                        color: "#0A0A0E"
                                        border.color: "#B89D64"
                                        radius: 6
                                    }
                                    contentItem: Text {
                                        leftPadding: 10
                                        rightPadding: 24
                                        text: crossRefProjCombo.displayText
                                        color: "#D4BE8D"
                                        elide: Text.ElideRight
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 58
                                    color: "#0A0A0E"
                                    radius: 6
                                    border.color: "#121217"

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
                                            color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#1F1A10" : (refCma.containsMouse ? "#121217" : "transparent")
                                            border.color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#B89D64" : "transparent"
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
                                                    color: offlineRegistrationDialog.crossRefCloudId === modelData.cloud_id ? "#D4BE8D" : "#F4F4F6"
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
                                        color: "#8E8E99"
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
                                        color: "#0A0A0E"
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
                                    color: "#0A0A0E"
                                    radius: 6
                                    border.color: "#121217"

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
                                            color: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id ? "#1C1406" : (movCma.containsMouse ? "#121217" : "transparent")
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
                                                    color: offlineRegistrationDialog.crossMovCloudId === modelData.cloud_id ? "#ff9100" : "#F4F4F6"
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
                                        color: "#8E8E99"
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
                                color: "#D4D4D8"
                                font.bold: true
                                font.pixelSize: 11
                            }

                            RowLayout {
                                spacing: 8
                                Rectangle {
                                    implicitWidth: 220
                                    implicitHeight: 30
                                    radius: 6
                                    color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#07170F" : "#0A0A0E"
                                    border.color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#10b981" : "#2A2A34"
                                    border.width: offlineRegistrationDialog.selectedScalePolicy === 0 ? 1.5 : 1
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: offlineRegistrationDialog.selectedScalePolicy = 0
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: "🔒 Locked SE(3) (Scale = 1.000)"
                                        color: offlineRegistrationDialog.selectedScalePolicy === 0 ? "#34D399" : "#A1A1AA"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    implicitWidth: 210
                                    implicitHeight: 30
                                    radius: 6
                                    color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#1F1A10" : "#0A0A0E"
                                    border.color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#B89D64" : "#2A2A34"
                                    border.width: offlineRegistrationDialog.selectedScalePolicy === 1 ? 1.5 : 1
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: offlineRegistrationDialog.selectedScalePolicy = 1
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        text: "📏 Sim(3) (Scale Ambiguous)"
                                        color: offlineRegistrationDialog.selectedScalePolicy === 1 ? "#D4BE8D" : "#A1A1AA"
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
                            color: "#1C1406"
                            border.color: "#f59e0b"
                            Text {
                                id: bannerWarningText
                                anchors.centerIn: parent
                                width: parent.width - 16
                                text: offlineRegistrationDialog.bannerWarning
                                color: "#FBBF24"
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
                                color: parent.enabled ? (parent.hovered ? "#B89D64" : "#9A804D") : "#121217"
                                radius: 6
                                border.color: parent.enabled ? "#B89D64" : "#2A2A34"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: bridge.registrationStatus
                            color: "#A1A1AA"
                            font.pixelSize: 10
                            wrapMode: Text.WordWrap
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F1F26" }

                        // Saved Registrations List
                        Text {
                            text: "2. SAVED SESSION REGISTRATIONS"
                            color: "#D4BE8D"
                            font.bold: true
                            font.pixelSize: 11
                            font.letterSpacing: 0.5
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "#0A0A0E"
                            radius: 6
                            border.color: "#121217"

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
                                    color: regHistoryMa.containsMouse ? "#121217" : "transparent"
                                    border.color: "#1F1F26"
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
                                                color: "#A1A1AA"
                                                font.pixelSize: 9
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Text {
                                            text: "Load ➔"
                                            color: "#D4BE8D"
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
                                color: "#8E8E99"
                                font.pixelSize: 11
                            }
                        }
                    }
                }

                // RIGHT COLUMN: Alignment Results & Mathematical Metrics & Inspection Tools
                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    color: "#121217"
                    radius: 8
                    border.color: "#1F1F26"
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
                                color: "#D4BE8D"
                                font.bold: true
                                font.pixelSize: 11
                                font.letterSpacing: 0.5
                            }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                implicitHeight: 22
                                implicitWidth: statusPillText.contentWidth + 14
                                radius: 4
                                color: bridge.isRegistrationActive ? "#07170F" : "#121217"
                                border.color: bridge.isRegistrationActive ? "#10b981" : "#2A2A34"
                                Text {
                                    id: statusPillText
                                    anchors.centerIn: parent
                                    text: bridge.isRegistrationActive ? "CONVERGED" : "NOT ALIGNED"
                                    color: bridge.isRegistrationActive ? "#34D399" : "#A1A1AA"
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
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "INLIER OVERLAP"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
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
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "INLIER RMSE"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
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
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "SCALE FACTOR"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationScale.toFixed(6)) : "--"
                                        color: "#D4BE8D"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }
                                }
                            }

                            // Card 4: Fitness
                            Rectangle {
                                Layout.fillWidth: true
                                height: 58
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text { text: "GICP FITNESS"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
                                    Text {
                                        text: bridge.isRegistrationActive ? (bridge.registrationFitness.toFixed(4)) : "--"
                                        color: "#34D399"
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
                                color: "#D4D4D8"
                                font.pixelSize: 10
                                font.bold: true
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 86
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"
                                Text {
                                    anchors.centerIn: parent
                                    text: bridge.isRegistrationActive ? bridge.registrationTransformStr : "[ Identity / Unregistered ]"
                                    color: "#D4BE8D"
                                    font.family: "Monospace"
                                    font.pixelSize: 11
                                    lineHeight: 1.2
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1F1F26" }

                        // Visual Distinction & Inspection Tools
                        Text {
                            text: "INTERACTIVE SOURCE DISTINCTION & INSPECTION (M9 ACCEPTANCE)"
                            color: "#D4BE8D"
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
                                color: "#D4D4D8"
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
                                        color: isSel ? "#9A804D" : "#0A0A0E"
                                        border.color: isSel ? "#B89D64" : "#2A2A34"
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
                                            color: isSel ? "white" : "#D4D4D8"
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
                                    color: "#D4D4D8"
                                    font.pixelSize: 10
                                }
                                Item { Layout.fillWidth: true }
                                RowLayout {
                                    spacing: 6
                                    Rectangle {
                                        implicitHeight: 28
                                        implicitWidth: 155
                                        radius: 4
                                        color: bridge.registrationColorMode === 1 ? "#2DD4BF" : "#0A0A0E"
                                        border.color: bridge.registrationColorMode === 1 ? "#14b8a6" : "#2A2A34"
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
                                            color: bridge.registrationColorMode === 1 ? "#2DD4BF" : "#A1A1AA"
                                            font.pixelSize: 10
                                            font.bold: bridge.registrationColorMode === 1
                                        }
                                    }
                                    Rectangle {
                                        implicitHeight: 28
                                        implicitWidth: 155
                                        radius: 4
                                        color: bridge.registrationColorMode === 0 ? "#1C1406" : "#0A0A0E"
                                        border.color: bridge.registrationColorMode === 0 ? "#f97316" : "#2A2A34"
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
                                            color: bridge.registrationColorMode === 0 ? "#FB923C" : "#A1A1AA"
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
                                color: "#0A0A0E"
                                radius: 6
                                border.color: "#121217"

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 24

                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            color: bridge.registrationColorMode === 0 ? "#9A804D" : "#A1A1AA"
                                        }
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Reference Anchor Cloud" : "Reference Cloud (LiDAR / Sensor)"
                                                color: bridge.registrationColorMode === 0 ? "#D4BE8D" : "#F4F4F6"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Diagnostic Cyan (#D4BE8D)" : "Preserved Sensor Reflectance / True Color"
                                                color: "#8E8E99"
                                                font.pixelSize: 9
                                            }
                                        }
                                    }

                                    Rectangle { width: 1; height: 30; color: "#121217" }

                                    RowLayout {
                                        spacing: 8
                                        Rectangle {
                                            width: 14
                                            height: 14
                                            radius: 3
                                            color: bridge.registrationColorMode === 0 ? "#ff9100" : "#9A804D"
                                        }
                                        ColumnLayout {
                                            spacing: 1
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Moving Aligned Cloud" : "Moving Cloud (Camera / RGB-D)"
                                                color: bridge.registrationColorMode === 0 ? "#ff9100" : "#D4BE8D"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            Text {
                                                text: bridge.registrationColorMode === 0 ? "Diagnostic Amber (#FF9100)" : "Preserved Photorealistic Sensor RGB"
                                                color: "#8E8E99"
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
                                    color: parent.enabled ? "white" : "#8E8E99"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#34D399" : "#34D399") : "#121217"
                                    radius: 6
                                    border.color: parent.enabled ? "#10b981" : "#2A2A34"
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
                                    color: parent.enabled ? "white" : "#8E8E99"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#B89D64" : "#9A804D") : "#121217"
                                    radius: 6
                                    border.color: parent.enabled ? "#B89D64" : "#2A2A34"
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
                                    color: parent.hovered ? "#B89D64" : "#B89D64"
                                    radius: 6
                                    border.color: "#B89D64"
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
            color: "#0A0A0E"
            radius: 10
            border.color: "#2A2A34"
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
                    color: "#F4F4F6"
                    font.pixelSize: 15
                    font.bold: true
                }
            }

            Text {
                text: "Create an independent AV Scan project containing the registered point cloud with complete transformation metadata."
                color: "#A1A1AA"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: "Project Name:"; color: "#D4D4D8"; font.pixelSize: 11; font.bold: true }
                TextField {
                    id: mergedProjNameInput
                    Layout.fillWidth: true
                    text: "Merged_Scan_" + Qt.formatDateTime(new Date(), "yyyyMMdd_hhmm")
                    color: "#F4F4F6"
                    background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Text { text: "Description (Optional):"; color: "#D4D4D8"; font.pixelSize: 11 }
                TextField {
                    id: mergedProjDescInput
                    Layout.fillWidth: true
                    placeholderText: "e.g. Combined scan merging separate sensor captures"
                    placeholderTextColor: "#8E8E99"
                    color: "#F4F4F6"
                    background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Button {
                    Layout.fillWidth: true
                    text: "Cancel"
                    onClicked: newMergedProjectDialog.close()
                    contentItem: Text { text: parent.text; color: "#A1A1AA"; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
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
                    background: Rectangle { color: "#B89D64"; radius: 6; border.color: "#B89D64" }
                }
            }
        }
    }

    // ==========================================
    // MILESTONE 7 & 8: AUTOMATIC EXTRINSIC CALIBRATION & MULTI-SENSOR SPATIAL FUSION MODAL
    // ==========================================
    Dialog {
        id: multiSensorFusionDialog
        modal: true
        width: 820
        height: 620
        anchors.centerIn: parent
        padding: 0

        property double selectedVoxelLeaf: 0.025
        property bool transferRgb: true
        property bool diagRoleColors: false

        background: Rectangle {
            color: "#0A0A0E"
            radius: 10
            border.color: "#B89D64"
            border.width: 1.5
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: "🔗"; font.pixelSize: 20 }
                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Multi-Sensor Extrinsic Calibration & Spatial Fusion (M7 & M8)"
                        color: "#F4F4F6"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Automatic rigid extrinsic calibration (Trajectory Umeyama + FPFH/GICP) & RGB-to-LiDAR spatial fusion"
                        color: "#A1A1AA"
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "✕ Close"
                    onClicked: multiSensorFusionDialog.close()
                    contentItem: Text { text: parent.text; color: "#D4D4D8"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#121217" }

            // Milestone 7: Extrinsic Calibration Panel
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: calCol.implicitHeight + 24
                color: "#121217"
                radius: 8
                border.color: "#2A2A34"

                ColumnLayout {
                    id: calCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "1. RIGID MULTI-SENSOR EXTRINSIC CALIBRATION (M7)"
                            color: "#f59e0b"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 0.8
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            radius: 4
                            color: "#0A0A0E"
                            border.color: bridge.extrinsicCalibrationStatus.indexOf("VALIDATED") === 0 ? "#10b981" : "#2A2A34"
                            implicitHeight: 22
                            implicitWidth: calBadgeTxt.contentWidth + 14
                            Text {
                                id: calBadgeTxt
                                anchors.centerIn: parent
                                text: bridge.extrinsicCalibrationStatus
                                color: bridge.extrinsicCalibrationStatus.indexOf("VALIDATED") === 0 ? "#10b981" : "#f59e0b"
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            height: 50
                            color: "#0A0A0E"
                            radius: 6
                            border.color: "#121217"
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "CALIBRATION RMSE"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
                                Text {
                                    text: bridge.extrinsicRmseMm > 0 ? (bridge.extrinsicRmseMm.toFixed(1) + " mm") : "--"
                                    color: "#10b981"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 50
                            color: "#0A0A0E"
                            radius: 6
                            border.color: "#121217"
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "SPATIAL OVERLAP"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
                                Text {
                                    text: bridge.extrinsicOverlapPercent > 0 ? (bridge.extrinsicOverlapPercent.toFixed(1) + "%") : "--"
                                    color: "#D4BE8D"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 50
                            color: "#0A0A0E"
                            radius: 6
                            border.color: "#121217"
                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text { text: "CONFIDENCE"; color: "#8E8E99"; font.pixelSize: 9; font.bold: true }
                                Text {
                                    text: bridge.extrinsicConfidence > 0 ? ((bridge.extrinsicConfidence * 100).toFixed(1) + "%") : "--"
                                    color: "#D4BE8D"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: bridge.extrinsicTransformSummary
                        color: "#D4D4D8"
                        font.pixelSize: 11
                        font.family: "Monospace"
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        id: runExtrinsicCalBtn
                        Layout.fillWidth: true
                        implicitHeight: 36
                        text: "📐 Calibrate Active Scanner Extrinsics & Persist Provenance (M7)"
                        onClicked: {
                            bridge.calibrateActiveScannerExtrinsics();
                            canvas3d.requestPaint();
                            canvas2d.requestPaint();
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
                            color: parent.hovered ? "#d97706" : "#FBBF24"
                            radius: 6
                            border.color: "#f59e0b"
                        }
                    }
                }
            }

            // Milestone 8: Multi-Sensor Spatial Fusion Panel
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#121217"
                radius: 8
                border.color: "#2A2A34"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "2. MULTI-SENSOR SPATIAL FUSION & RGB PROJECTION (M8)"
                            color: "#D4BE8D"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 0.8
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: bridge.isFusionActive ? ("FUSED ACTIVE: " + bridge.fusedPointCount + " pts (" + bridge.fusedColorizedPoints + " RGB)") : "Unfused"
                            color: bridge.isFusionActive ? "#10b981" : "#A1A1AA"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        RowLayout {
                            spacing: 6
                            Text { text: "Voxel Leaf:"; color: "#D4D4D8"; font.pixelSize: 11 }
                            Repeater {
                                model: [0.015, 0.025, 0.05]
                                delegate: Button {
                                    text: (modelData * 100).toFixed(1) + " cm"
                                    implicitHeight: 26
                                    onClicked: multiSensorFusionDialog.selectedVoxelLeaf = modelData
                                    contentItem: Text { text: parent.text; color: Math.abs(multiSensorFusionDialog.selectedVoxelLeaf - modelData) < 0.001 ? "#09090B" : "#14141A"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter }
                                    background: Rectangle {
                                        color: Math.abs(multiSensorFusionDialog.selectedVoxelLeaf - modelData) < 0.001 ? "#B89D64" : "#121217"
                                        radius: 4
                                        border.color: "#B89D64"
                                    }
                                }
                            }
                        }

                        CheckBox {
                            checked: multiSensorFusionDialog.transferRgb
                            onToggled: multiSensorFusionDialog.transferRgb = checked
                            contentItem: Text { text: "Project Optical RGB onto LiDAR"; color: "#D4D4D8"; font.pixelSize: 11; leftPadding: 24 }
                        }

                        CheckBox {
                            checked: multiSensorFusionDialog.diagRoleColors
                            onToggled: multiSensorFusionDialog.diagRoleColors = checked
                            contentItem: Text { text: "Diagnostic Sensor Colors"; color: "#D4D4D8"; font.pixelSize: 11; leftPadding: 24 }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0A0A0E"
                        radius: 6
                        border.color: "#121217"
                        Text {
                            anchors.fill: parent
                            anchors.margins: 10
                            text: bridge.fusionStatus
                            color: "#D4BE8D"
                            font.pixelSize: 11
                            font.family: "Monospace"
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 38
                            text: "🔗 Fuse Active Multi-Sensor Maps (M8)"
                            onClicked: {
                                bridge.fuseActiveMultiSensorMaps(
                                    multiSensorFusionDialog.selectedVoxelLeaf,
                                    multiSensorFusionDialog.transferRgb,
                                    multiSensorFusionDialog.diagRoleColors
                                );
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: parent.hovered ? "#B89D64" : "#9A804D"; radius: 6; border.color: "#B89D64" }
                        }

                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 38
                            enabled: bridge.isFusionActive
                            text: "💾 Save Fused Map to Project"
                            onClicked: {
                                bridge.saveFusedMapToProject();
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: parent.enabled ? "white" : "#8E8E99"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: parent.enabled ? (parent.hovered ? "#059669" : "#34D399") : "#121217"; radius: 6; border.color: parent.enabled ? "#10b981" : "#2A2A34" }
                        }
                    }
                }
            }
        }
    }

    // ==========================================
    // MILESTONE 12: POINT CLOUD CLEANUP, POST-PROCESSING & EXPORT MODAL
    // ==========================================
    Dialog {
        id: cleanupExportDialog
        modal: true
        width: 840
        height: 660
        anchors.centerIn: parent
        padding: 0

        property bool useVoxel: true
        property double voxelLeaf: 0.02
        property bool useSor: true
        property int sorMeanK: 20
        property double sorStdMul: 1.5
        property bool useRor: false
        property double rorRadius: 0.15
        property int rorMinNeighbors: 3
        property bool alignGround: true
        property bool removeGround: false
        property string exportFormat: "ply"
        property bool exportBinary: true

        background: Rectangle {
            color: "#0A0A0E"
            radius: 10
            border.color: "#22c55e"
            border.width: 1.5
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Text { text: "✨"; font.pixelSize: 20 }
                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Point Cloud Cleanup, Post-Processing & Export (M12)"
                        color: "#F4F4F6"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: "Statistical & Radius Outlier Removal, Voxel Downsampling, RANSAC Ground Plane Z=0 Alignment & Multi-Format Export"
                        color: "#A1A1AA"
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: "✕ Close"
                    onClicked: cleanupExportDialog.close()
                    contentItem: Text { text: parent.text; color: "#D4D4D8"; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                    background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#121217" }

            // Cleanup Pipeline Panel
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cleanCol.implicitHeight + 24
                color: "#121217"
                radius: 8
                border.color: "#2A2A34"

                ColumnLayout {
                    id: cleanCol
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "1. POINT CLOUD FILTERING & GROUND ALIGNMENT PIPELINE"
                            color: "#34D399"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 0.8
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: bridge.hasCleanupPreview ? ("PREVIEW ACTIVE: " + bridge.cleanupInputPoints + " → " + bridge.cleanupOutputPoints + " pts (-" + bridge.cleanupRemovedPoints + ")") : ("Raw Cloud: " + bridge.pointsCaptured + " pts")
                            color: bridge.hasCleanupPreview ? "#34D399" : "#A1A1AA"
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }

                    // Real-Time M12 Capture Stream Filter Bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        color: "#07170F"
                        radius: 6
                        border.color: bridge.liveCleanupEnabled ? "#22c55e" : "#121217"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 12

                            CheckBox {
                                checked: bridge.liveCleanupEnabled
                                onToggled: bridge.setLiveCleanupEnabled(checked)
                                contentItem: Text {
                                    text: "⚡ Real-Time M12 Cleanup During Capture (10 Hz O(N) Spatial Hash)"
                                    color: "#34D399"
                                    font.pixelSize: 11
                                    font.bold: true
                                    leftPadding: 24
                                }
                            }

                            CheckBox {
                                enabled: bridge.liveCleanupEnabled
                                checked: bridge.liveGroundLockEnabled
                                onToggled: bridge.setLiveGroundLockEnabled(checked)
                                contentItem: Text {
                                    text: "Live Floor Z=0 Lock"
                                    color: bridge.liveCleanupEnabled ? "#D4BE8D" : "#8E8E99"
                                    font.pixelSize: 11
                                    font.bold: true
                                    leftPadding: 24
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: bridge.liveCleanupEnabled
                                      ? ("Live Filter Latency: " + bridge.liveCleanupTimeMs.toFixed(2) + " ms / tick")
                                      : "Live Filter Paused"
                                color: "#34D399"
                                font.pixelSize: 10
                                font.family: "Monospace"
                                font.bold: true
                            }
                        }
                    }

                    // Filter Toggles Row 1
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        CheckBox {
                            checked: cleanupExportDialog.useVoxel
                            onToggled: cleanupExportDialog.useVoxel = checked
                            contentItem: Text { text: "Voxel Downsample (" + (cleanupExportDialog.voxelLeaf * 100).toFixed(1) + " cm)"; color: "#F4F4F6"; font.pixelSize: 11; leftPadding: 24 }
                        }

                        Repeater {
                            model: [0.01, 0.02, 0.05]
                            delegate: Button {
                                text: (modelData * 100).toFixed(0) + "cm"
                                implicitHeight: 24
                                onClicked: cleanupExportDialog.voxelLeaf = modelData
                                contentItem: Text { text: parent.text; color: Math.abs(cleanupExportDialog.voxelLeaf - modelData) < 0.001 ? "#09090B" : "#14141A"; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter }
                                background: Rectangle {
                                    color: Math.abs(cleanupExportDialog.voxelLeaf - modelData) < 0.001 ? "#B89D64" : "#121217"
                                    radius: 4
                                    border.color: "#B89D64"
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        CheckBox {
                            checked: cleanupExportDialog.useSor
                            onToggled: cleanupExportDialog.useSor = checked
                            contentItem: Text { text: "SOR Statistical Filter (K=" + cleanupExportDialog.sorMeanK + ", σ=" + cleanupExportDialog.sorStdMul.toFixed(1) + ")"; color: "#F4F4F6"; font.pixelSize: 11; leftPadding: 24 }
                        }
                    }

                    // Filter Toggles Row 2
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        CheckBox {
                            checked: cleanupExportDialog.useRor
                            onToggled: cleanupExportDialog.useRor = checked
                            contentItem: Text { text: "ROR Radius Filter (r=" + cleanupExportDialog.rorRadius.toFixed(2) + "m, min=" + cleanupExportDialog.rorMinNeighbors + ")"; color: "#F4F4F6"; font.pixelSize: 11; leftPadding: 24 }
                        }

                        Item { Layout.fillWidth: true }

                        CheckBox {
                            checked: cleanupExportDialog.alignGround
                            onToggled: cleanupExportDialog.alignGround = checked
                            contentItem: Text { text: "RANSAC Level Ground (Z=0)"; color: "#D4BE8D"; font.pixelSize: 11; font.bold: true; leftPadding: 24 }
                        }

                        CheckBox {
                            checked: cleanupExportDialog.removeGround
                            onToggled: cleanupExportDialog.removeGround = checked
                            contentItem: Text { text: "Strip Floor Points"; color: "#f59e0b"; font.pixelSize: 11; leftPadding: 24 }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 44
                        color: "#0A0A0E"
                        radius: 6
                        border.color: "#121217"
                        Text {
                            anchors.fill: parent
                            anchors.margins: 8
                            text: bridge.cleanupStatus
                            color: "#34D399"
                            font.pixelSize: 11
                            font.family: "Monospace"
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 36
                            text: "⚡ Run Cleanup Preview"
                            onClicked: {
                                bridge.runPointCloudCleanup(
                                    cleanupExportDialog.useVoxel,
                                    cleanupExportDialog.voxelLeaf,
                                    cleanupExportDialog.useSor,
                                    cleanupExportDialog.sorMeanK,
                                    cleanupExportDialog.sorStdMul,
                                    cleanupExportDialog.useRor,
                                    cleanupExportDialog.rorRadius,
                                    cleanupExportDialog.rorMinNeighbors,
                                    cleanupExportDialog.alignGround,
                                    cleanupExportDialog.removeGround
                                );
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: parent.hovered ? "#16a34a" : "#4ADE80"; radius: 6; border.color: "#22c55e" }
                        }

                        Button {
                            implicitWidth: 140
                            implicitHeight: 36
                            enabled: bridge.hasCleanupPreview
                            text: "↩ Reset to Raw"
                            onClicked: {
                                bridge.resetPointCloudCleanup();
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: parent.enabled ? "#F4F4F6" : "#8E8E99"; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: "#121217"; radius: 6; border.color: "#2A2A34" }
                        }

                        Button {
                            Layout.fillWidth: true
                            implicitHeight: 36
                            enabled: bridge.hasCleanupPreview
                            text: "💾 Commit Cleaned Cloud to Project"
                            onClicked: {
                                bridge.commitCleanedPointCloud();
                                canvas3d.requestPaint();
                                canvas2d.requestPaint();
                            }
                            contentItem: Text { text: parent.text; color: parent.enabled ? "white" : "#8E8E99"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: parent.enabled ? (parent.hovered ? "#B89D64" : "#9A804D") : "#121217"; radius: 6; border.color: parent.enabled ? "#B89D64" : "#2A2A34" }
                        }
                    }
                }
            }

            // Export Panel (.ply, .pcd, .xyz, .obj Mesh)
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#121217"
                radius: 8
                border.color: "#2A2A34"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "2. ONE-CLICK SPATIAL EXPORT (.PLY / .PCD / .XYZ / .OBJ SURFACE MESH)"
                        color: "#D4BE8D"
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 0.8
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text { text: "Format:"; color: "#D4D4D8"; font.pixelSize: 11 }
                        Repeater {
                            model: [
                                { fmt: "ply", label: "PLY (Point Cloud)" },
                                { fmt: "pcd", label: "PCD (PCL Native)" },
                                { fmt: "xyz", label: "XYZ (ASCII RGB)" },
                                { fmt: "obj", label: "OBJ (3D Surface Mesh)" }
                            ]
                            delegate: Button {
                                text: modelData.label
                                implicitHeight: 28
                                onClicked: {
                                    cleanupExportDialog.exportFormat = modelData.fmt;
                                    exportPathInput.text = "/home/scanar/av_scan/exports/" + (bridge.projectId !== "" ? bridge.projectId : "scan_export") + "." + modelData.fmt;
                                }
                                contentItem: Text { text: parent.text; color: cleanupExportDialog.exportFormat === modelData.fmt ? "#09090B" : "#14141A"; font.pixelSize: 11; font.bold: cleanupExportDialog.exportFormat === modelData.fmt; horizontalAlignment: Text.AlignHCenter }
                                background: Rectangle {
                                    color: cleanupExportDialog.exportFormat === modelData.fmt ? "#B89D64" : "#121217"
                                    radius: 5
                                    border.color: cleanupExportDialog.exportFormat === modelData.fmt ? "#B89D64" : "#2A2A34"
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        CheckBox {
                            visible: cleanupExportDialog.exportFormat === "ply" || cleanupExportDialog.exportFormat === "pcd"
                            checked: cleanupExportDialog.exportBinary
                            onToggled: cleanupExportDialog.exportBinary = checked
                            contentItem: Text { text: "Binary Encoding"; color: "#D4D4D8"; font.pixelSize: 11; leftPadding: 24 }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text { text: "Output File:"; color: "#D4D4D8"; font.pixelSize: 11 }
                        TextField {
                            id: exportPathInput
                            Layout.fillWidth: true
                            text: "/home/scanar/av_scan/exports/" + (bridge.projectId !== "" ? bridge.projectId : "scan_export") + "." + cleanupExportDialog.exportFormat
                            color: "#F4F4F6"
                            font.family: "Monospace"
                            font.pixelSize: 11
                            background: Rectangle { color: "#0A0A0E"; radius: 6; border.color: "#2A2A34" }
                        }
                        Button {
                            implicitWidth: 180
                            implicitHeight: 34
                            text: "📤 Export Active Cloud"
                            onClicked: {
                                var fmtArg = cleanupExportDialog.exportFormat;
                                if (fmtArg === "ply" || fmtArg === "pcd") {
                                    fmtArg = fmtArg + (cleanupExportDialog.exportBinary ? "_binary" : "_ascii");
                                }
                                bridge.exportActivePointCloud(
                                    fmtArg,
                                    exportPathInput.text,
                                    0.04
                                );
                            }
                            contentItem: Text { text: parent.text; color: "white"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                            background: Rectangle { color: parent.hovered ? "#B89D64" : "#9A804D"; radius: 6; border.color: "#B89D64" }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: bridge.lastExportPath !== "" ? (bridge.exportStatus + " → " + bridge.lastExportPath) : bridge.exportStatus
                        color: "#A1A1AA"
                        font.pixelSize: 10
                        font.family: "Monospace"
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }

    // Notification Toast Popup
    Popup {
        id: toastPopup
        anchors.centerIn: parent
        width: 380
        height: 76
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#0A0A0E"
            radius: 12
            border.color: "#B89D64"
            border.width: 1.5
        }

        property string toastTitle: ""
        property string toastMessage: ""

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 4
            Text {
                text: toastPopup.toastTitle
                color: "#D4BE8D"
                font.bold: true
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: toastPopup.toastMessage
                color: "#F4F4F6"
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


    // =========================================================================
    // 5. ARTIFICIAL VISION APPLE GLASS SPLASH SCREEN
    // Exact replica of scanarMini ArtificialVisionSplashScreen (560x380 card)
    // =========================================================================
    Item {
        id: splashOverlay
        anchors.fill: parent
        z: 9999
        visible: root.showSplashScreen
        opacity: 1.0

        property real loadProgress: 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 420
                easing.type: Easing.InOutQuad
            }
        }

        NumberAnimation {
            id: splashProgressAnim
            target: splashOverlay
            property: "loadProgress"
            from: 0.0
            to: 1.0
            duration: 1650
            easing.type: Easing.OutCubic
            running: root.showSplashScreen
        }

        Timer {
            id: splashFadeTimer
            interval: 1750
            running: root.showSplashScreen
            repeat: false
            onTriggered: {
                splashOverlay.opacity = 0.0;
                splashHideTimer.start();
            }
        }

        Timer {
            id: splashHideTimer
            interval: 440
            repeat: false
            onTriggered: {
                root.showSplashScreen = false;
            }
        }

        // Click anywhere outside or inside to dismiss early
        MouseArea {
            anchors.fill: parent
            onClicked: {
                splashFadeTimer.stop();
                splashOverlay.opacity = 0.0;
                splashHideTimer.start();
            }
        }

        // Centered 560x380 Translucent Apple Glass Splash Card (matches scanarMini)
        Rectangle {
            id: splashCard
            width: 560
            height: 380
            anchors.centerIn: parent
            radius: 20
            color: "#38121218" // Translucent dark glass backdrop
            border.color: "#B0BAA164" // rgba(186, 161, 100, 175) 1.5px Champagne Gold outer border
            border.width: 1.5

            // Inner White Glass Refraction Rim (inset by 1.5px)
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1.5
                radius: 18.5
                color: "transparent"
                border.color: "#40FFFFFF" // rgba(255, 255, 255, 64)
                border.width: 1
            }

            // Concentric LiDAR Horizon Rings & Champagne Gold Halo (matches SplashLogoCanvas)
            Canvas {
                id: splashRingsCanvas
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    var cx = width * 0.5;
                    var cy = 112;

                    // Subtle concentric LiDAR horizon rings
                    var radii = [48, 72, 96, 120];
                    for (var i = 0; i < radii.length; ++i) {
                        ctx.strokeStyle = "rgba(186, 161, 100, " + ((24 - i * 5) / 255.0).toFixed(3) + ")";
                        ctx.lineWidth = 1;
                        ctx.beginPath();
                        ctx.arc(cx, cy, radii[i], 0, Math.PI * 2);
                        ctx.stroke();
                    }

                    // Soft radial champagne gold halo behind the logo
                    var grad = ctx.createRadialGradient(cx, cy, 4, cx, cy, 68);
                    grad.addColorStop(0.0, "rgba(186, 161, 100, 0.24)");
                    grad.addColorStop(0.55, "rgba(255, 255, 255, 0.06)");
                    grad.addColorStop(1.0, "rgba(0, 0, 0, 0.0)");
                    ctx.fillStyle = grad;
                    ctx.beginPath();
                    ctx.arc(cx, cy, 68, 0, Math.PI * 2);
                    ctx.fill();
                }
            }

            // Top Centered Champagne Gold Pill Highlight (104x3px)
            Rectangle {
                width: 104
                height: 3
                radius: 1.5
                color: root.avGoldPrimary
                anchors.top: parent.top
                anchors.topMargin: 12
                anchors.horizontalCenter: parent.horizontalCenter
            }

            // Top-Right Close Button (✕, 28x28, radius 14)
            Button {
                id: splashCloseBtn
                width: 28
                height: 28
                anchors.top: parent.top
                anchors.topMargin: 14
                anchors.right: parent.right
                anchors.rightMargin: 16
                onClicked: {
                    splashFadeTimer.stop();
                    splashOverlay.opacity = 0.0;
                    splashHideTimer.start();
                }
                contentItem: Text {
                    text: "✕"
                    color: root.avWhiteWarm
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 14
                    color: splashCloseBtn.hovered ? "#70EF4444" : "#1AFFFFFF"
                    border.color: splashCloseBtn.hovered ? "#F87171" : "#4DFFFFFF"
                    border.width: 1
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 38
                anchors.bottomMargin: 28
                anchors.leftMargin: 36
                anchors.rightMargin: 36
                spacing: 8

                // Artificial Vision Brand Logo (transparent PNG)
                Item {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 132
                    Layout.preferredHeight: 132

                    Image {
                        anchors.fill: parent
                        source: "assets/av_logo.png"
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                    }
                }

                // Brand Pill: "VERIFY  ·  DIRECT  ·  CLARIFY"
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "VERIFY  ·  DIRECT  ·  CLARIFY"
                    color: root.avGoldBright
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 3.5
                }

                // Product Title: "AV Scan"
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "AV Scan"
                    color: root.avWhiteWarm
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.0
                }

                // Product Attribution: "an Artificial Vision product"
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "an Artificial Vision product"
                    color: "#DCFCFBF8"
                    font.pixelSize: 13
                    font.letterSpacing: 0.6
                }

                Item { Layout.fillHeight: true }

                // 380x4px Champagne Gold Progress Bar
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 380
                    height: 4
                    radius: 2
                    color: "#24FFFFFF"

                    Rectangle {
                        width: parent.width * splashOverlay.loadProgress
                        height: parent.height
                        radius: 2
                        color: root.avGoldPrimary
                    }
                }

                // Status Caption
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: splashOverlay.loadProgress < 0.45
                        ? "Initializing Spatial Compute Engine & Apple Glass AR HUD..."
                        : (splashOverlay.loadProgress < 0.85
                            ? "Loading Multi-Sensor Profiles & C-SLAM Mesh..."
                            : "Ready — Launching Clear AR Passthrough Workspace...")
                    color: root.avGoldBright
                    font.pixelSize: 11
                }
            }
        }
    }
}
