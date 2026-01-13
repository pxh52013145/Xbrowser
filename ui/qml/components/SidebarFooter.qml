import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    implicitHeight: row.implicitHeight

    required property var workspaces
    required property var settings

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: 6

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
            text: "⋮"
            menu: workspaceMenu
        }

        ToolButton {
            text: "New"
            onClicked: commands.invoke("new-tab", { url: "about:blank" })
        }
    }

    Menu {
        id: workspaceMenu

        MenuItem {
            text: "Rename Workspace"
            onTriggered: {
                if (root.workspaces.activeIndex < 0) {
                    return
                }
                renameField.text = root.workspaces.nameAt(root.workspaces.activeIndex)
                renameDialog.open()
            }
        }

        MenuItem {
            text: "Delete Workspace"
            enabled: root.workspaces.count() > 1 && root.workspaces.activeIndex >= 0
            onTriggered: root.workspaces.closeWorkspace(root.workspaces.activeIndex)
        }
    }

    Dialog {
        id: renameDialog
        title: "Rename Workspace"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 8
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
