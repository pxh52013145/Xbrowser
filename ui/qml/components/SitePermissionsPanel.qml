import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int cornerRadius: 10
    property int spacing: 8

    required property var store
    required property string origin

    signal closeRequested()

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

    function syncCombo(combo, kind) {
        if (!combo) {
            return
        }
        if (!root.store || !root.origin || root.origin.trim().length === 0) {
            combo.currentIndex = 0
            return
        }
        combo.currentIndex = root.store.decision(root.origin, kind)
    }

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 380
    implicitHeight: column.implicitHeight + spacing * 2

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: root.spacing
        spacing: root.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Label {
                Layout.fillWidth: true
                text: "Site permissions"
                font.bold: true
            }

            ToolButton {
                text: "×"
                onClicked: root.closeRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.origin
            wrapMode: Text.Wrap
            opacity: 0.75
            visible: root.origin && root.origin.trim().length > 0
        }

        Label {
            Layout.fillWidth: true
            text: "No site origin available."
            opacity: 0.65
            visible: !root.origin || root.origin.trim().length === 0
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(0, 0, 0, 0.08)
            visible: root.origin && root.origin.trim().length > 0
        }

        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, contentHeight)
            clip: true
            contentWidth: width
            contentHeight: listColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            interactive: contentHeight > height

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: listColumn
                width: parent.width
                spacing: root.spacing

                Repeater {
                    model: [
                        { kind: 1 }, { kind: 2 }, { kind: 3 }, { kind: 4 }, { kind: 5 }, { kind: 6 },
                        { kind: 7 }, { kind: 8 }, { kind: 9 }, { kind: 10 }, { kind: 11 }, { kind: 12 }
                    ]

                    delegate: RowLayout {
                        required property var modelData

                        Layout.fillWidth: true
                        spacing: root.spacing

                        Label {
                            Layout.fillWidth: true
                            text: root.permissionLabel(modelData.kind)
                            elide: Text.ElideRight
                        }

                        ComboBox {
                            id: stateCombo
                            model: ["Ask", "Allow", "Deny"]
                            enabled: root.store && root.origin && root.origin.trim().length > 0

                            Component.onCompleted: root.syncCombo(stateCombo, modelData.kind)

                            Connections {
                                target: root.store
                                function onRevisionChanged() {
                                    root.syncCombo(stateCombo, modelData.kind)
                                }
                            }

                            Connections {
                                target: root
                                function onOriginChanged() {
                                    root.syncCombo(stateCombo, modelData.kind)
                                }
                            }

                            onActivated: {
                                if (!root.store || !root.origin || root.origin.trim().length === 0) {
                                    return
                                }
                                root.store.setDecision(root.origin, modelData.kind, stateCombo.currentIndex)
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing
            visible: root.origin && root.origin.trim().length > 0

            Button {
                Layout.fillWidth: true
                text: "Reset this site"
                enabled: root.store
                onClicked: {
                    if (root.store) {
                        root.store.clearOrigin(root.origin)
                    }
                }
            }

            Button {
                text: "Reload"
                enabled: root.store && root.store.reload
                onClicked: root.store.reload()
            }
        }
    }
}

