import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    title: "AV Scan — Find Your Sense"
    color: "#121418"

    // Top Navigation / Header Bar
    Rectangle {
        id: header
        height: 64
        color: "#1a1d24"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            // Brand Mark
            RowLayout {
                spacing: 10
                Rectangle {
                    width: 32
                    height: 32
                    radius: 6
                    color: "#007acc"
                    Text {
                        anchors.centerIn: parent
                        text: "AV"
                        color: "white"
                        font.bold: true
                        font.pixelSize: 14
                    }
                }
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: "AV SCAN"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 16
                    }
                    Text {
                        text: "Find Your Sense."
                        color: "#8a94a6"
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                width: 1
                height: 36
                color: "#2d3340"
                Layout.leftMargin: 10
                Layout.rightMargin: 10
            }

            // Current Project Info
            ColumnLayout {
                spacing: 2
                Text {
                    text: "ACTIVE PROJECT"
                    color: "#6c7a89"
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.projectName
                    color: "#e2e8f0"
                    font.bold: true
                    font.pixelSize: 14
                }
            }

            Button {
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
                    color: "#2b3240"
                    radius: 4
                    border.color: "#3e485b"
                }
            }

            ComboBox {
                id: projectCombo
                model: bridge.availableProjects
                visible: bridge.availableProjects.length > 0
                onActivated: {
                    bridge.openProject(currentText)
                }
                background: Rectangle {
                    implicitWidth: 160
                    implicitHeight: 32
                    color: "#1e232d"
                    border.color: "#3e485b"
                    radius: 4
                }
                contentItem: Text {
                    leftPadding: 10
                    text: projectCombo.displayText
                    color: "#e2e8f0"
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillWidth: true }

            // Platform Pill
            Rectangle {
                height: 28
                implicitWidth: hostLabel.contentWidth + 20
                radius: 14
                color: "#1f2937"
                border.color: "#374151"

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#10b981"
                    }
                    Text {
                        id: hostLabel
                        text: bridge.hostPlatform
                        color: "#9ca3af"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }
    }

    // Main Content Workspace: Split View (3D Orbit View & 2D Floor Plan)
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

        // 3D Viewport Panel
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 65
            color: "#181b22"
            radius: 8
            border.color: "#252b36"
            clip: true

            // Viewport Header Overlay
            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                z: 10

                Rectangle {
                    color: "#222733"
                    radius: 4
                    height: 24
                    implicitWidth: view3dLabel.contentWidth + 16
                    Text {
                        id: view3dLabel
                        anchors.centerIn: parent
                        text: "3D SPATIAL MAP"
                        color: "#60a5fa"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: bridge.isCapturing ? (bridge.pointsCaptured + " Points") : "Viewport Ready"
                    color: "#94a3b8"
                    font.pixelSize: 12
                }
            }

            // Mock 3D Canvas / Grid Visualizer
            Canvas {
                id: canvas3d
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // Perspective Grid
                    ctx.strokeStyle = "#252c38";
                    ctx.lineWidth = 1;
                    var cy = height * 0.65;
                    var cx = width * 0.5;

                    for (var x = -cx; x <= cx; x += 40) {
                        ctx.beginPath();
                        ctx.moveTo(cx + x * 0.2, cy - 80);
                        ctx.lineTo(cx + x * 2.5, height);
                        ctx.stroke();
                    }

                    for (var y = cy - 80; y < height; y += (y - cy + 90) * 0.35 + 8) {
                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(width, y);
                        ctx.stroke();
                    }

                    // Simulated Realtime Trajectory & Cloud Preview
                    if (bridge.isCapturing) {
                        ctx.strokeStyle = "#00e5ff";
                        ctx.lineWidth = 2;
                        ctx.beginPath();
                        ctx.arc(cx, cy, 14, 0, 2 * Math.PI);
                        ctx.stroke();

                        // Point clusters
                        ctx.fillStyle = "#38bdf8";
                        for (var i = 0; i < 40; ++i) {
                            var px = cx + (Math.sin(i * 3.7) * (i * 5));
                            var py = cy - (Math.cos(i * 2.1) * (i * 2.5)) - 30;
                            ctx.fillRect(px, py, 2, 2);
                        }
                    }
                }
            }

            // Status message center if idle
            ColumnLayout {
                anchors.centerIn: parent
                visible: !bridge.isCapturing && bridge.captureState !== "STOPPED"
                spacing: 8
                Text {
                    text: "No active stream"
                    color: "#475569"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    text: "Select a scanner and click Start Capture"
                    color: "#334155"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        // 2D Floor Plan / Coverage Panel
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 35
            color: "#181b22"
            radius: 8
            border.color: "#252b36"
            clip: true

            // Floor Plan Header Overlay
            RowLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                z: 10

                Rectangle {
                    color: "#222733"
                    radius: 4
                    height: 24
                    implicitWidth: view2dLabel.contentWidth + 16
                    Text {
                        id: view2dLabel
                        anchors.centerIn: parent
                        text: "2D FLOOR PLAN"
                        color: "#34d399"
                        font.bold: true
                        font.pixelSize: 11
                    }
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "Top-Down Ortho"
                    color: "#94a3b8"
                    font.pixelSize: 12
                }
            }

            Canvas {
                id: canvas2d
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);

                    // Top-down Orthogonal Grid
                    ctx.strokeStyle = "#202530";
                    ctx.lineWidth = 1;
                    for (var x = 0; x < width; x += 30) {
                        ctx.beginPath();
                        ctx.moveTo(x, 0);
                        ctx.lineTo(x, height);
                        ctx.stroke();
                    }
                    for (var y = 0; y < height; y += 30) {
                        ctx.beginPath();
                        ctx.moveTo(0, y);
                        ctx.lineTo(width, y);
                        ctx.stroke();
                    }

                    if (bridge.isCapturing) {
                        ctx.fillStyle = "#10b981";
                        ctx.beginPath();
                        ctx.arc(width * 0.5, height * 0.5, 6, 0, 2 * Math.PI);
                        ctx.fill();
                    }
                }
            }
        }
    }

    // Bottom Control, Telemetry & Action Bar
    Rectangle {
        id: bottomBar
        height: 76
        color: "#1a1d24"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 24

            // Scanner Selector
            ColumnLayout {
                spacing: 2
                Text {
                    text: "SCANNER CONFIGURATION"
                    color: "#6c7a89"
                    font.pixelSize: 10
                    font.bold: true
                }
                ComboBox {
                    id: scannerCombo
                    model: bridge.availableScanners
                    onActivated: bridge.selectScannerConfig(currentText)
                    background: Rectangle {
                        implicitWidth: 260
                        implicitHeight: 36
                        color: "#1e232d"
                        border.color: "#3e485b"
                        radius: 4
                    }
                    contentItem: Text {
                        leftPadding: 10
                        text: scannerCombo.displayText
                        color: "#e2e8f0"
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 12
                    }
                }
            }

            // Minimal Sensor Status
            ColumnLayout {
                spacing: 4
                Text {
                    text: "SENSOR STATUS"
                    color: "#6c7a89"
                    font.pixelSize: 10
                    font.bold: true
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: bridge.sensorStatus === "STREAMING" ? "#10b981" : 
                               bridge.sensorStatus === "READY" ? "#3b82f6" : "#ef4444"
                    }
                    Text {
                        text: bridge.sensorStatus
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            // Minimal Tracking Status
            ColumnLayout {
                spacing: 4
                Text {
                    text: "TRACKING STATE"
                    color: "#6c7a89"
                    font.pixelSize: 10
                    font.bold: true
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: bridge.trackingStatus === "TRACKING_OK" ? "#10b981" : "#f59e0b"
                    }
                    Text {
                        text: bridge.trackingStatus
                        color: "#f1f5f9"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            // Elapsed Capture Timer
            ColumnLayout {
                spacing: 2
                Text {
                    text: "ELAPSED TIME"
                    color: "#6c7a89"
                    font.pixelSize: 10
                    font.bold: true
                }
                Text {
                    text: bridge.elapsedTimeString
                    color: "#38bdf8"
                    font.pixelSize: 20
                    font.bold: true
                    font.family: "Monospace"
                }
            }

            Item { Layout.fillWidth: true }

            // Action Buttons: START, STOP, SAVE
            RowLayout {
                spacing: 12

                Button {
                    id: startBtn
                    text: "START"
                    enabled: !bridge.isCapturing && bridge.projectId !== ""
                    implicitWidth: 110
                    implicitHeight: 42
                    onClicked: {
                        bridge.startCapture();
                        canvas3d.requestPaint();
                        canvas2d.requestPaint();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: startBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: startBtn.enabled ? "#16a34a" : "#1e293b"
                        radius: 6
                    }
                }

                Button {
                    id: stopBtn
                    text: "STOP"
                    enabled: bridge.isCapturing
                    implicitWidth: 110
                    implicitHeight: 42
                    onClicked: {
                        bridge.stopCapture();
                        canvas3d.requestPaint();
                        canvas2d.requestPaint();
                    }
                    contentItem: Text {
                        text: parent.text
                        color: stopBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: stopBtn.enabled ? "#dc2626" : "#1e293b"
                        radius: 6
                    }
                }

                Button {
                    id: saveBtn
                    text: "SAVE"
                    enabled: bridge.canSave
                    implicitWidth: 110
                    implicitHeight: 42
                    onClicked: bridge.saveCapture()
                    contentItem: Text {
                        text: parent.text
                        color: saveBtn.enabled ? "white" : "#64748b"
                        font.bold: true
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: saveBtn.enabled ? "#2563eb" : "#1e293b"
                        radius: 6
                    }
                }
            }
        }
    }

    // Modal: New Project Dialog
    Dialog {
        id: newProjectDialog
        title: "Create New Project"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel

        background: Rectangle {
            color: "#1e232d"
            radius: 8
            border.color: "#374151"
        }

        header: Rectangle {
            height: 48
            color: "#252c38"
            radius: 8
            Text {
                anchors.centerIn: parent
                text: "Create New Project"
                color: "white"
                font.bold: true
                font.pixelSize: 14
            }
        }

        ColumnLayout {
            spacing: 12
            width: parent.width

            Text {
                text: "Project Name:"
                color: "#94a3b8"
                font.pixelSize: 12
            }
            TextField {
                id: projNameInput
                Layout.fillWidth: true
                placeholderText: "e.g., Facility Survey 01"
                color: "white"
                background: Rectangle {
                    color: "#12151b"
                    radius: 4
                    border.color: "#3b4556"
                }
            }

            Text {
                text: "Description:"
                color: "#94a3b8"
                font.pixelSize: 12
            }
            TextField {
                id: projDescInput
                Layout.fillWidth: true
                placeholderText: "Optional project notes"
                color: "white"
                background: Rectangle {
                    color: "#12151b"
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

    // Notification toast popup
    Popup {
        id: toastPopup
        anchors.centerIn: parent
        width: 320
        height: 80
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#1e293b"
            radius: 8
            border.color: "#38bdf8"
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
            }
            Text {
                text: toastPopup.toastMessage
                color: "#e2e8f0"
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Timer {
            id: toastTimer
            interval: 3000
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
