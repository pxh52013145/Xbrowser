import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    implicitHeight: row.implicitHeight

    required property var browser
    required property var workspaces
    required property var settings
    required property var popupHost

    property int renameWorkspaceIndex: -1
    property string renameDraft: ""

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: theme.spacing

        ToolButton {
            text: settings.sidebarExpanded ? "<" : ">"
            onClicked: commands.invoke("toggle-sidebar")
        }

        ListView {
            id: workspaceStrip
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            clip: true
            orientation: ListView.Horizontal
            spacing: 6
            model: root.workspaces
            currentIndex: root.workspaces.activeIndex
            interactive: contentWidth > width

            delegate: Item {
                height: workspaceStrip.height
                width: Math.min(180, Math.max(64, pillText.implicitWidth + 20))

                property bool dropAfter: false

                DragHandler {
                    id: wsDrag
                    acceptedButtons: Qt.LeftButton
                }

                Drag.active: wsDrag.active
                Drag.supportedActions: Qt.MoveAction
                Drag.hotSpot.x: Math.round(width * 0.5)
                Drag.hotSpot.y: Math.round(height * 0.5)
                Drag.keys: ["workspace"]
                Drag.mimeData: ({ workspaceIndex: index })

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: isActive ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.22) : Qt.rgba(0, 0, 0, 0.06)
                    border.color: isActive ? accentColor : Qt.rgba(0, 0, 0, 0.08)
                    border.width: 1
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 3
                    color: accentColor
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                }

                Text {
                    id: pillText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 20
                    anchors.rightMargin: 8
                    text: name
                    elide: Text.ElideRight
                    color: "#1f1f1f"
                    font.pixelSize: 12
                }

                ToolTip.visible: hover.hovered
                ToolTip.delay: 500
                ToolTip.text: name

                HoverHandler {
                    id: hover
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: root.workspaces.activeIndex = index
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }

                DropArea {
                    id: wsDrop
                    anchors.fill: parent
                    keys: ["tab"]
                    onDropped: (drop) => {
                        const dragged = Number(drop.mimeData.tabId || 0)
                        if (dragged > 0) {
                            root.browser.moveTabToWorkspace(dragged, index)
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: Qt.rgba(0.2, 0.5, 1.0, 0.14)
                    visible: wsDrop.containsDrag
                }

                DropArea {
                    id: wsReorderDrop
                    anchors.fill: parent
                    keys: ["workspace"]
                    onPositionChanged: (drag) => dropAfter = drag.x > width * 0.5
                    onDropped: (drop) => {
                        const fromIndex = Number(drop.mimeData.workspaceIndex)
                        const targetIndex = index
                        if (isNaN(fromIndex) || fromIndex < 0 || fromIndex >= root.workspaces.count()) {
                            return
                        }
                        if (targetIndex < 0 || targetIndex >= root.workspaces.count()) {
                            return
                        }

                        let toIndex = targetIndex
                        if (dropAfter) {
                            toIndex = fromIndex < targetIndex ? targetIndex : (targetIndex + 1)
                        } else {
                            toIndex = fromIndex < targetIndex ? (targetIndex - 1) : targetIndex
                        }

                        toIndex = Math.max(0, Math.min(root.workspaces.count() - 1, toIndex))
                        if (toIndex !== fromIndex) {
                            root.workspaces.moveWorkspace(fromIndex, toIndex)
                        }
                    }
                }

                Rectangle {
                    width: 4
                    height: parent.height
                    radius: 2
                    color: accentColor
                    opacity: wsReorderDrop.containsDrag ? 0.85 : 0.0
                    visible: opacity > 0
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: dropAfter ? (parent.width - width - 1) : 1

                    Behavior on opacity {
                        NumberAnimation { duration: settings.reduceMotion ? 0 : theme.motionFastMs }
                    }
                }
            }
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
                        root.renameWorkspaceIndex = root.workspaces.activeIndex
                        root.renameDraft = root.workspaces.nameAt(root.renameWorkspaceIndex)
                        root.popupHost.openAtItem(renameDialogComponent, workspaceMenuButton)
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

    Component {
        id: renameDialogComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: theme.cornerRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 320
            implicitHeight: layout.implicitHeight + theme.spacing * 2

            ColumnLayout {
                id: layout
                anchors.fill: parent
                anchors.margins: theme.spacing
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: "Rename Workspace"
                    font.bold: true
                }

                TextField {
                    id: renameField
                    Layout.fillWidth: true
                    selectByMouse: true
                    text: root.renameDraft
                    onTextChanged: root.renameDraft = text

                    Component.onCompleted: {
                        forceActiveFocus()
                        selectAll()
                    }

                    Keys.onReturnPressed: okButton.clicked()
                    Keys.onEscapePressed: root.popupHost.close()
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.spacing

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Cancel"
                        onClicked: root.popupHost.close()
                    }

                    Button {
                        id: okButton
                        text: "OK"
                        enabled: root.renameDraft.trim().length > 0 && root.renameWorkspaceIndex >= 0
                        onClicked: {
                            const idx = root.renameWorkspaceIndex
                            const name = root.renameDraft.trim()
                            if (idx >= 0 && name.length > 0) {
                                root.workspaces.setNameAt(idx, name)
                            }
                            root.popupHost.close()
                        }
                    }
                }
            }
        }
    }
}
