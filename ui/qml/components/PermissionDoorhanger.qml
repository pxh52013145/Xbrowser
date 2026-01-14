import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int cornerRadius: 10
    property int spacing: 8

    property string origin: ""
    property int permissionKind: 0
    property bool userInitiated: false

    signal responded(int state, bool remember)

    function permissionLabel(kind) {
        switch (kind) {
        case 1: return "Microphone"
        case 2: return "Camera"
        case 3: return "Location"
        case 4: return "Notifications"
        case 5: return "Sensors"
        case 6: return "Clipboard"
        case 7: return "Multiple downloads"
        case 8: return "File access"
        case 9: return "Autoplay"
        case 10: return "Local fonts"
        case 11: return "MIDI SysEx"
        case 12: return "Window management"
        default: return "Permission"
        }
    }

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 320
    implicitHeight: column.implicitHeight + spacing * 2

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: root.spacing
        spacing: root.spacing

        Label {
            Layout.fillWidth: true
            text: permissionLabel(root.permissionKind) + " permission"
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: root.origin
            opacity: 0.75
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            visible: !root.userInitiated
            text: "Requested by the page"
            opacity: 0.55
            font.pixelSize: 11
            elide: Text.ElideRight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: root.spacing
            rowSpacing: root.spacing

            Button {
                Layout.fillWidth: true
                text: "Allow once"
                onClicked: root.responded(1, false)
            }

            Button {
                Layout.fillWidth: true
                text: "Always allow"
                onClicked: root.responded(1, true)
            }

            Button {
                Layout.fillWidth: true
                text: "Deny once"
                onClicked: root.responded(2, false)
            }

            Button {
                Layout.fillWidth: true
                text: "Always deny"
                onClicked: root.responded(2, true)
            }
        }
    }
}

