import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    implicitHeight: row.implicitHeight

    required property var browser
    required property var workspaces
    required property var settings
    required property var themes
    required property var popupHost
    required property var popupContextHost

    property int renameWorkspaceIndex: -1
    property string renameDraft: ""
    property string renameIconTypeDraft: ""
    property string renameIconValueDraft: ""

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: theme.spacing

        function buttonColor(active, hovered, enabled) {
            if (!enabled) {
                return Qt.rgba(0, 0, 0, 0.02)
            }
            if (active) {
                return Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, hovered ? 0.22 : 0.18)
            }
            return hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent"
        }

        function buttonBorder(active, hovered, enabled) {
            if (!enabled) {
                return Qt.rgba(0, 0, 0, 0.06)
            }
            if (active) {
                return Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35)
            }
            return hovered ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(0, 0, 0, 0.08)
        }

        function buttonRadius() {
            return Math.max(6, Math.round(theme.cornerRadius * 0.8))
        }

        ToolButton {
            text: settings.sidebarExpanded ? "<" : ">"
            onClicked: commands.invoke("toggle-sidebar")
            background: Rectangle {
                radius: row.buttonRadius()
                color: row.buttonColor(false, parent.hovered, parent.enabled)
                border.color: row.buttonBorder(false, parent.hovered, parent.enabled)
                border.width: parent.activeFocus ? 2 : 1
            }
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: settings.sidebarExpanded ? "Collapse Sidebar" : "Expand Sidebar"
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
                    radius: theme.cornerRadius
                    color: isActive
                               ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, hover.hovered ? 0.26 : 0.22)
                               : (hover.hovered ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(0, 0, 0, 0.04))
                    border.color: isActive ? accentColor : (hover.hovered ? Qt.rgba(0, 0, 0, 0.12) : Qt.rgba(0, 0, 0, 0.08))
                    border.width: 1
                }

                Item {
                    width: 18
                    height: 18
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 8

                    Text {
                        id: wsIconText
                        anchors.centerIn: parent
                        text: iconValue && iconValue.length > 0 ? iconValue : ""
                        visible: text.length > 0
                        font.pixelSize: 14
                    }

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 3
                        color: accentColor
                        anchors.centerIn: parent
                        visible: !wsIconText.visible
                    }
                }

                Text {
                    id: pillText
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 30
                    anchors.rightMargin: 8
                    text: name
                    elide: Text.ElideRight
                    color: "#1f1f1f"
                    font.pixelSize: 12
                }

                ToolTip.visible: hover.hovered
                ToolTip.delay: 300
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

                Timer {
                    id: hoverSwitchTimer
                    interval: root.settings ? Number(root.settings.dndHoverSwitchWorkspaceDelayMs || 500) : 500
                    repeat: false
                    onTriggered: {
                        if (!wsDrop.containsDrag) {
                            return
                        }
                        if (!root.settings || !root.settings.dndHoverSwitchWorkspaceEnabled) {
                            return
                        }
                        if (root.workspaces.activeIndex === index) {
                            return
                        }
                        root.workspaces.activeIndex = index
                    }
                }

                DropArea {
                    id: wsDrop
                    anchors.fill: parent
                    keys: ["tab"]
                    onContainsDragChanged: {
                        if (!root.settings || !root.settings.dndHoverSwitchWorkspaceEnabled) {
                            hoverSwitchTimer.stop()
                            return
                        }
                        if (containsDrag && root.workspaces.activeIndex !== index) {
                            hoverSwitchTimer.restart()
                        } else {
                            hoverSwitchTimer.stop()
                        }
                    }
                    onDropped: (drop) => {
                        hoverSwitchTimer.stop()
                        let draggedIds = drop.mimeData.tabIds || []
                        draggedIds = draggedIds.map(v => Number(v || 0)).filter(v => v > 0)
                        if (draggedIds.length === 0) {
                            const dragged = Number(drop.mimeData.tabId || 0)
                            if (dragged > 0) {
                                draggedIds = [dragged]
                            }
                        }
                        if (draggedIds.length === 0) {
                            return
                        }
                        if (draggedIds.length === 1) {
                            root.browser.moveTabToWorkspace(draggedIds[0], index)
                            return
                        }
                        root.browser.moveTabsToWorkspace(draggedIds, index)
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
            background: Rectangle {
                radius: row.buttonRadius()
                color: row.buttonColor(false, parent.hovered, parent.enabled)
                border.color: row.buttonBorder(false, parent.hovered, parent.enabled)
                border.width: parent.activeFocus ? 2 : 1
            }
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: "New Workspace"
        }

        ToolButton {
            id: workspaceMenuButton
            text: "\u22EE"
            onClicked: root.popupContextHost.togglePopupWithContext("sidebar-workspace-menu", () => {
                root.popupHost.openAtItem(workspaceMenuComponent, workspaceMenuButton)
            })
            background: Rectangle {
                radius: row.buttonRadius()
                readonly property bool active: root.popupHost.opened && root.popupContextHost && (root.popupContextHost.popupManagerContext === "sidebar-workspace-menu")
                color: row.buttonColor(active, parent.hovered, parent.enabled)
                border.color: parent.activeFocus ? theme.accentColor : row.buttonBorder(active, parent.hovered, parent.enabled)
                border.width: parent.activeFocus ? 2 : 1
            }
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: "Workspace Menu"
        }

        ToolButton {
            text: "New"
            onClicked: commands.invoke("new-tab", { url: "about:blank" })
            background: Rectangle {
                radius: row.buttonRadius()
                color: row.buttonColor(false, parent.hovered, parent.enabled)
                border.color: row.buttonBorder(false, parent.hovered, parent.enabled)
                border.width: parent.activeFocus ? 2 : 1
            }
            ToolTip.visible: hovered
            ToolTip.delay: 300
            ToolTip.text: "New Tab"
        }
    }

    Component {
        id: workspaceMenuComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: theme.cornerRadius
            border.color: Qt.rgba(0, 0, 0, 0.12)
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
                        root.renameIconTypeDraft = root.workspaces.iconTypeAt(root.renameWorkspaceIndex)
                        root.renameIconValueDraft = root.workspaces.iconValueAt(root.renameWorkspaceIndex)
                        root.popupContextHost.openPopupWithContext("sidebar-workspace-rename", () => {
                            root.popupHost.openAtItem(renameDialogComponent, workspaceMenuButton)
                        })
                    }
                }

                Button {
                    text: "Theme & Accent"
                    enabled: root.workspaces.activeIndex >= 0 && !!root.themes
                    onClicked: {
                        root.popupHost.close()
                        if (root.workspaces.activeIndex < 0) {
                            return
                        }
                        root.popupContextHost.openPopupWithContext("sidebar-workspace-theme", () => {
                            root.popupHost.openAtItem(workspaceThemeDialogComponent, workspaceMenuButton)
                        })
                    }
                }

                Button {
                    text: "Background Gradient"
                    enabled: root.workspaces.activeIndex >= 0 && root.workspaces.setBackgroundGradient2At
                    onClicked: {
                        root.popupHost.close()
                        if (root.workspaces.activeIndex < 0) {
                            return
                        }
                        root.popupContextHost.openPopupWithContext("sidebar-workspace-gradient", () => {
                            root.popupHost.openAtItem(gradientDialogComponent, workspaceMenuButton)
                        })
                    }
                }

                Button {
                    text: "Duplicate Workspace"
                    enabled: root.workspaces.activeIndex >= 0
                    onClicked: {
                        root.popupHost.close()
                        const src = root.workspaces.activeIndex
                        if (src < 0) {
                            return
                        }
                        const created = root.workspaces.duplicateWorkspace(src)
                        if (created >= 0) {
                            root.workspaces.activeIndex = created
                        }
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
        id: workspaceThemeDialogComponent

        WorkspaceThemeDialog {
            workspaces: root.workspaces
            themes: root.themes
            settings: root.settings
            workspaceIndex: root.workspaces.activeIndex
            onCloseRequested: root.popupHost.close()
        }
    }

    Component {
        id: gradientDialogComponent

        GradientGeneratorDialog {
            workspaces: root.workspaces
            workspaceIndex: root.workspaces.activeIndex
            onCloseRequested: root.popupHost.close()
        }
    }

    Component {
        id: renameDialogComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: theme.cornerRadius
            border.color: Qt.rgba(0, 0, 0, 0.12)
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

                Label {
                    Layout.fillWidth: true
                    text: "Icon"
                    opacity: 0.85
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Repeater {
                        model: ["💼", "🏠", "⭐", "📚", "🧪", "🎨"]

                        delegate: ToolButton {
                            required property var modelData
                            text: modelData
                            onClicked: {
                                root.renameIconTypeDraft = "builtin"
                                root.renameIconValueDraft = String(modelData || "")
                                iconField.text = root.renameIconValueDraft
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Clear"
                        onClicked: {
                            root.renameIconTypeDraft = ""
                            root.renameIconValueDraft = ""
                            iconField.text = ""
                        }
                    }
                }

                TextField {
                    id: iconField
                    Layout.fillWidth: true
                    selectByMouse: true
                    placeholderText: "Emoji (e.g., 🚀)"

                    Component.onCompleted: {
                        text = root.renameIconValueDraft
                    }

                    onTextChanged: {
                        const trimmed = text.trim()
                        root.renameIconTypeDraft = trimmed.length > 0 ? "emoji" : ""
                        root.renameIconValueDraft = text
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
                                if (root.workspaces.setIconAt) {
                                    root.workspaces.setIconAt(idx, root.renameIconTypeDraft, root.renameIconValueDraft)
                                }
                            }
                            root.popupHost.close()
                        }
                    }
                }
            }
        }
    }
}
