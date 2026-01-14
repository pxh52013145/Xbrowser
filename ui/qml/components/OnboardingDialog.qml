import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var settings

    signal accepted()
    signal rejected()
    signal closeRequested()

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 80)
        height: Math.min(420, parent.height - 80)
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.98)
        border.color: Qt.rgba(0, 0, 0, 0.12)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: "Welcome to XBrowser"
                    font.bold: true
                    font.pixelSize: 14
                }

                ToolButton {
                    text: "×"
                    onClicked: {
                        root.settings.onboardingSeen = true
                        root.closeRequested()
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Vertical tabs, workspaces, split view, and a Zen-inspired UI."
                wrapMode: Text.Wrap
            }

            GroupBox {
                Layout.fillWidth: true
                title: "Shortcuts"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    Label { text: "Ctrl+L  Focus address bar" }
                    Label { text: "Ctrl+T  New tab" }
                    Label { text: "Ctrl+W  Close tab" }
                    Label { text: "Ctrl+B  Toggle sidebar" }
                    Label { text: "Ctrl+Shift+L  Toggle address bar" }
                    Label { text: "Ctrl+E  Toggle split view" }
                    Label { text: "Ctrl+Shift+C  Toggle compact mode" }
                    Label { text: "F12  DevTools" }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Tip: type \">\" in the address bar for actions."
                opacity: 0.8
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Item { Layout.fillWidth: true }

                Button {
                    text: "OK"
                    onClicked: {
                        root.settings.onboardingSeen = true
                        root.accepted()
                    }
                }

                Button {
                    text: "Cancel"
                    onClicked: {
                        root.settings.onboardingSeen = true
                        root.rejected()
                    }
                }
            }
        }
    }
}
