import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Item {
    id: root
    anchors.fill: parent

    required property var history
    required property var popupManager

    signal closeRequested()

    property bool embedded: false
    property string searchText: ""

    property var selectedById: ({})
    property int selectedCount: 0
    property int selectionRevision: 0

    property int contextMenuTargetId: 0
    property url contextMenuTargetUrl: ""
    property string contextMenuTargetTitle: ""
    property var contextMenuItems: []
    property real contextMenuX: 0
    property real contextMenuY: 0

    HistoryFilterModel {
        id: filtered
        sourceHistory: root.history
        searchText: root.searchText
    }

    function isSelected(historyId) {
        root.selectionRevision
        return !!root.selectedById[String(historyId)]
    }

    function setSelected(historyId, selected) {
        const key = String(historyId || 0)
        if (!key || key === "0") {
            return
        }
        const had = !!root.selectedById[key]
        if (selected && !had) {
            root.selectedById[key] = true
            root.selectedCount += 1
            root.selectionRevision += 1
        } else if (!selected && had) {
            delete root.selectedById[key]
            root.selectedCount = Math.max(0, root.selectedCount - 1)
            root.selectionRevision += 1
        }
    }

    function clearSelection() {
        root.selectedById = ({})
        root.selectedCount = 0
        root.selectionRevision += 1
    }

    function deleteSelected() {
        if (!root.history || !root.history.removeById) {
            return
        }
        const keys = Object.keys(root.selectedById || {})
        for (const k of keys) {
            root.history.removeById(Number(k))
        }
        root.clearSelection()
    }

    function clearToday() {
        if (!root.history || !root.history.clearRange) {
            return
        }
        const now = new Date()
        const start = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
        const end = start + 24 * 60 * 60 * 1000
        root.history.clearRange(start, end)
    }

    function openContextMenuAtPoint(historyId, url, title, x, y) {
        root.contextMenuTargetId = Number(historyId || 0)
        root.contextMenuTargetUrl = url || ""
        root.contextMenuTargetTitle = String(title || "")
        root.contextMenuX = Number(x || 0)
        root.contextMenuY = Number(y || 0)

        const urlText = root.contextMenuTargetUrl ? String(root.contextMenuTargetUrl) : ""
        const items = []
        items.push({ action: "open", text: "Open", enabled: urlText.length > 0 })
        items.push({ action: "copy-url", text: "Copy URL", enabled: urlText.length > 0 })
        items.push({ separator: true })
        items.push({ action: "delete", text: "Delete", enabled: root.contextMenuTargetId > 0 })
        root.contextMenuItems = items

        if (root.popupManager && root.popupManager.openAtPoint) {
            root.popupManager.openAtPoint(contextMenuComponent, x, y, root)
        }
    }

    function handleContextMenuAction(action, args) {
        const id = root.contextMenuTargetId
        const urlText = root.contextMenuTargetUrl ? String(root.contextMenuTargetUrl) : ""

        if (action === "open" && urlText.length > 0) {
            commands.invoke("new-tab", { url: urlText })
            root.closeRequested()
            return
        }

        if (action === "copy-url" && urlText.length > 0) {
            commands.invoke("copy-text", { text: urlText })
            return
        }

        if (action === "delete" && id > 0 && root.history && root.history.removeById) {
            root.history.removeById(id)
            root.setSelected(id, false)
        }
    }

    Component {
        id: contextMenuComponent

        ContextMenu {
            items: root.contextMenuItems
            onActionTriggered: (action, args) => {
                if (root.popupManager) {
                    root.popupManager.close()
                }
                root.handleContextMenuAction(action, args)
            }
        }
    }

    Rectangle {
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(760, parent.width - 80)
        readonly property real preferredHeight: root.embedded ? parent.height : Math.min(580, parent.height - 80)

        width: Math.max(1, Math.round(preferredWidth))
        height: Math.max(1, Math.round(preferredHeight))
        x: root.embedded ? 0 : Math.round((parent.width - width) / 2)
        y: root.embedded ? 0 : Math.round((parent.height - height) / 2)
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.98)
        border.color: Qt.rgba(0, 0, 0, 0.12)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: theme.spacing

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: "History"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "Clear today"
                    enabled: root.history && root.history.count > 0
                    onClicked: root.clearToday()
                }

                Button {
                    text: "Clear all"
                    enabled: root.history && root.history.count > 0
                    onClicked: root.history.clearAll()
                }

                Button {
                    text: "Delete selected"
                    visible: root.selectedCount > 0
                    enabled: root.selectedCount > 0
                    onClicked: root.deleteSelected()
                }

                Button {
                    text: "Clear selection"
                    visible: root.selectedCount > 0
                    enabled: root.selectedCount > 0
                    onClicked: root.clearSelection()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: "Search history"
                text: root.searchText
                selectByMouse: true
                onTextChanged: root.searchText = text
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: filtered
                    spacing: 4

                    section.property: "dayKey"
                    section.criteria: ViewSection.FullString
                    section.delegate: Item {
                        width: ListView.view.width
                        height: headerLabel.implicitHeight + 10

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: Qt.rgba(0, 0, 0, 0.04)
                            border.color: Qt.rgba(0, 0, 0, 0.08)
                            border.width: 1
                        }

                        Label {
                            id: headerLabel
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 10
                            text: section
                            font.pixelSize: 12
                            opacity: 0.8
                        }
                    }

                    delegate: ItemDelegate {
                        required property string title
                        required property url url
                        required property double visitedMs
                        required property int historyId

                        width: ListView.view.width

                        contentItem: RowLayout {
                            spacing: theme.spacing

                            CheckBox {
                                checked: root.isSelected(historyId)
                                onToggled: root.setSelected(historyId, checked)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(2, Math.round(theme.spacing / 4))

                                Label {
                                    Layout.fillWidth: true
                                    text: title
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: url ? url.toString() : ""
                                    opacity: 0.65
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                text: visitedMs > 0 ? Qt.formatDateTime(new Date(visitedMs), "hh:mm") : ""
                                opacity: 0.6
                                font.pixelSize: 11
                            }

                            Button {
                                text: "Open"
                                onClicked: {
                                    if (url && url.toString().length > 0) {
                                        commands.invoke("new-tab", { url: url.toString() })
                                        root.closeRequested()
                                    }
                                }
                            }

                            ToolButton {
                                text: "⋯"
                                onClicked: {
                                    if (!root.popupManager) {
                                        return
                                    }
                                    const pos = mapToItem(root.popupManager, width, height)
                                    root.openContextMenuAtPoint(historyId, url, title, pos.x, pos.y)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: (mouse) => {
                                    if (!root.popupManager) {
                                        return
                                    }
                                    const pos = mapToItem(root.popupManager, mouse.x, mouse.y)
                                    root.openContextMenuAtPoint(historyId, url, title, pos.x, pos.y)
                                }
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: "No history"
                        opacity: 0.6
                        visible: list.count === 0
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.history ? root.history.lastError : ""
                color: "#b00020"
                wrapMode: Text.Wrap
                visible: root.history && root.history.lastError && root.history.lastError.length > 0
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}
