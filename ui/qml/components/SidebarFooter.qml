import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    implicitHeight: row.implicitHeight

    required property var workspaces
    required property var settings
    required property var popupHost

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: theme.spacing

        ToolButton {
            text: settings.sidebarExpanded ? "<" : ">"
            onClicked: commands.invoke("toggle-sidebar")
        }

        ComboBox {
            Layout.fillWidth: true
            model: root.workspaces
            textRole: "name"
            currentIndex: root.workspaces.activeIndex
            onActivated: (index) => root.workspaces.activeIndex = index
        }

        ToolButton {
            text: "+"
            onClicked: commands.invoke("new-workspace")
        }

        ToolButton {
            id: workspaceMenuButton
            text: "⋯"
            onClicked: root.popupHost.openAtItem(workspaceMenuComponent, workspaceMenuButton)
        }

        ToolButton {
            text: "New"
            onClicked: commands.invoke("new-tab", { url: "about:blank" })
        }
    }

    Component {
        id: workspaceMenuComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: theme.cornerRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 220
            implicitHeight: menuColumn.implicitHeight + theme.spacing * 2

            ColumnLayout {
                id: menuColumn
                anchors.fill: parent
                anchors.margins: theme.spacing
                spacing: theme.spacing

                Button {
                    text: "Rename Workspace"
                    onClicked: {
                        root.popupHost.close()
                        if (root.workspaces.activeIndex < 0) {
                            return
                        }
                        renameField.text = root.workspaces.nameAt(root.workspaces.activeIndex)
                        renameDialog.open()
                    }
                }

                Button {
                    text: "Delete Workspace"
                    enabled: root.workspaces.count() > 1 && root.workspaces.activeIndex >= 0
                    onClicked: {
                        root.popupHost.close()
                        root.workspaces.closeWorkspace(root.workspaces.activeIndex)
                    }
                }
            }
        }
    }

    Dialog {
        id: renameDialog
        title: "Rename Workspace"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: theme.spacing
            TextField {
                id: renameField
                Layout.fillWidth: true
                selectByMouse: true
            }
        }

        onAccepted: {
            if (root.workspaces.activeIndex < 0) {
                return
            }
            root.workspaces.setNameAt(root.workspaces.activeIndex, renameField.text)
        }
    }
}

