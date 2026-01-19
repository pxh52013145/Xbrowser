import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Rectangle {
    id: root

    required property var workspaces
    required property var themes
    required property var settings
    required property int workspaceIndex

    signal closeRequested()

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: theme.cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.12)
    border.width: 1

    implicitWidth: 460
    implicitHeight: layout.implicitHeight + theme.spacing * 2

    readonly property bool hasWorkspace: !!root.workspaces && root.workspaceIndex >= 0
    property string themeOverrideId: ""

    function refreshState() {
        if (!root.hasWorkspace || !root.workspaces.themeOverrideAt) {
            root.themeOverrideId = ""
            return
        }
        root.themeOverrideId = String(root.workspaces.themeOverrideAt(root.workspaceIndex) || "")
    }

    Component.onCompleted: refreshState()

    Platform.ColorDialog {
        id: accentDialog
        title: "Workspace accent"
        currentColor: root.hasWorkspace && root.workspaces.accentColorAt ? root.workspaces.accentColorAt(root.workspaceIndex) : theme.accentColor
        onAccepted: {
            if (!root.hasWorkspace || !root.workspaces.setAccentColorAt) {
                return
            }
            root.workspaces.setAccentColorAt(root.workspaceIndex, color)
        }
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: "Workspace Theme"
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: root.hasWorkspace && root.workspaces.nameAt ? ("Workspace: " + root.workspaces.nameAt(root.workspaceIndex)) : ""
                    opacity: 0.75
                    visible: text.length > 0
                    font.pixelSize: 12
                }
            }

            ToolButton {
                text: "×"
                Accessible.name: "Close"
                onClicked: root.closeRequested()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: theme.spacing
            rowSpacing: Math.max(4, Math.round(theme.spacing / 2))

            Label { text: "Accent"; opacity: 0.85 }
            Rectangle {
                width: 26
                height: 18
                radius: 4
                color: root.hasWorkspace && root.workspaces.accentColorAt ? root.workspaces.accentColorAt(root.workspaceIndex) : theme.accentColor
                border.color: Qt.rgba(0, 0, 0, 0.18)
                border.width: 1
            }
            Button {
                text: "Pick"
                enabled: root.hasWorkspace && root.workspaces.setAccentColorAt
                onClicked: accentDialog.open()
            }

            Label { text: "Theme override"; opacity: 0.85 }
            Label {
                Layout.fillWidth: true
                text: root.themeOverrideId.length > 0 ? root.themeOverrideId : ("(Global: " + (root.settings ? root.settings.themeId : "workspace") + ")")
                opacity: 0.75
                elide: Text.ElideRight
            }
            Item { width: 1; height: 1 }
        }

        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(280, list.contentHeight + theme.spacing * 2)

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: root.themes

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                header: ItemDelegate {
                    width: ListView.view.width
                    highlighted: root.themeOverrideId.length === 0
                    text: "Use global theme"
                    onClicked: {
                        if (!root.hasWorkspace || !root.workspaces.setThemeOverrideAt) {
                            return
                        }
                        root.workspaces.setThemeOverrideAt(root.workspaceIndex, "")
                        root.refreshState()
                    }
                }

                delegate: ItemDelegate {
                    width: ListView.view.width
                    highlighted: root.themeOverrideId === themeId
                    text: name + " (" + version + ")"
                    onClicked: {
                        if (!root.hasWorkspace || !root.workspaces.setThemeOverrideAt) {
                            return
                        }
                        root.workspaces.setThemeOverrideAt(root.workspaceIndex, themeId)
                        root.refreshState()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            Item { Layout.fillWidth: true }

            Button {
                text: "Close"
                onClicked: root.closeRequested()
            }
        }
    }
}
