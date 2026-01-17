import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Item {
    id: root
    anchors.fill: parent

    required property var bookmarks
    required property var popupManager

    signal closeRequested()

    property bool embedded: false
    property string searchText: ""

    property int editingId: 0
    property string editingText: ""

    property int contextMenuTargetId: 0
    property bool contextMenuTargetIsFolder: false
    property url contextMenuTargetUrl: ""
    property string contextMenuTargetTitle: ""
    property bool contextMenuTargetExpanded: false
    property bool contextMenuTargetHasChildren: false
    property var contextMenuItems: []
    property real contextMenuX: 0
    property real contextMenuY: 0

    property int moveTargetId: 0
    property var folderChoices: []

    BookmarksFilterModel {
        id: filtered
        sourceBookmarks: root.bookmarks
        searchText: root.searchText
    }

    function startRename(itemId, currentTitle) {
        root.editingId = Number(itemId || 0)
        root.editingText = String(currentTitle || "")
    }

    function commitRename() {
        if (!root.bookmarks || root.editingId <= 0) {
            root.editingId = 0
            root.editingText = ""
            return
        }
        root.bookmarks.renameItem(root.editingId, root.editingText)
        root.editingId = 0
        root.editingText = ""
    }

    function cancelRename() {
        root.editingId = 0
        root.editingText = ""
    }

    function openContextMenuAtPoint(itemId, isFolder, url, title, expanded, hasChildren, x, y) {
        root.contextMenuTargetId = Number(itemId || 0)
        root.contextMenuTargetIsFolder = !!isFolder
        root.contextMenuTargetUrl = url || ""
        root.contextMenuTargetTitle = String(title || "")
        root.contextMenuTargetExpanded = !!expanded
        root.contextMenuTargetHasChildren = !!hasChildren
        root.contextMenuX = Number(x || 0)
        root.contextMenuY = Number(y || 0)

        const items = []
        if (root.contextMenuTargetIsFolder) {
            items.push({ action: "toggle-expanded", text: root.contextMenuTargetExpanded ? "Collapse" : "Expand", enabled: root.contextMenuTargetHasChildren })
            items.push({ separator: true })
            items.push({ action: "new-folder", text: "New folder inside", enabled: true })
            items.push({ action: "rename", text: "Rename", enabled: true })
            items.push({ action: "delete", text: "Delete folder", enabled: true })
        } else {
            const urlText = root.contextMenuTargetUrl ? String(root.contextMenuTargetUrl) : ""
            items.push({ action: "open", text: "Open", enabled: urlText.length > 0 })
            items.push({ action: "copy-url", text: "Copy URL", enabled: urlText.length > 0 })
            items.push({ separator: true })
            items.push({ action: "move", text: "Move to folder...", enabled: true })
            items.push({ action: "rename", text: "Rename", enabled: true })
            items.push({ action: "delete", text: "Delete", enabled: true })
        }

        root.contextMenuItems = items
        if (root.popupManager && root.popupManager.openAtPoint) {
            root.popupManager.openAtPoint(contextMenuComponent, x, y, root)
        }
    }

    function openMoveToFolder(itemId, x, y) {
        if (!root.bookmarks || !root.popupManager) {
            return
        }
        root.moveTargetId = Number(itemId || 0)

        const folders = root.bookmarks.folders ? root.bookmarks.folders() : []
        const options = []
        options.push({ id: 0, title: "Root", depth: 0 })
        for (const f of folders) {
            const fid = Number(f.id || 0)
            if (fid > 0) {
                options.push({ id: fid, title: String(f.title || ""), depth: Number(f.depth || 0) })
            }
        }

        root.folderChoices = options
        root.popupManager.openAtPoint(moveToFolderComponent, x, y, root)
    }

    function handleContextMenuAction(action, args) {
        const id = root.contextMenuTargetId
        const isFolder = root.contextMenuTargetIsFolder
        const urlText = root.contextMenuTargetUrl ? String(root.contextMenuTargetUrl) : ""

        if (action === "open" && !isFolder && urlText.length > 0) {
            commands.invoke("new-tab", { url: urlText })
            root.closeRequested()
            return
        }

        if (action === "copy-url" && !isFolder && urlText.length > 0) {
            commands.invoke("copy-text", { text: urlText })
            return
        }

        if (action === "toggle-expanded" && isFolder && id > 0) {
            root.bookmarks.toggleExpanded(id)
            return
        }

        if (action === "new-folder" && isFolder && id > 0) {
            const newId = root.bookmarks.createFolder("New folder", id)
            if (newId > 0) {
                root.startRename(newId, "New folder")
            }
            return
        }

        if (action === "move" && !isFolder && id > 0) {
            root.openMoveToFolder(id, root.contextMenuX, root.contextMenuY)
            return
        }

        if (action === "rename" && id > 0) {
            root.startRename(id, root.contextMenuTargetTitle)
            return
        }

        if (action === "delete" && id > 0) {
            root.bookmarks.removeById(id)
            return
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

    Component {
        id: moveToFolderComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: theme.cornerRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 320
            implicitHeight: Math.min(420, list.contentHeight + theme.spacing * 2)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: theme.spacing
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: "Move to folder"
                    font.bold: true
                }

                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.folderChoices || []
                    spacing: 0
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: ItemDelegate {
                        required property var modelData

                        width: ListView.view.width
                        leftPadding: 10 + Number(modelData.depth || 0) * 16
                        text: String(modelData.title || "")
                        onClicked: {
                            if (!root.bookmarks) {
                                return
                            }
                            const folderId = Number(modelData.id || 0)
                            root.bookmarks.moveItem(root.moveTargetId, folderId)
                            if (root.popupManager) {
                                root.popupManager.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(820, parent.width - 80)
        readonly property real preferredHeight: root.embedded ? parent.height : Math.min(600, parent.height - 80)

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
                    text: "Bookmarks"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "New folder"
                    enabled: root.bookmarks && root.bookmarks.createFolder
                    onClicked: {
                        const id = root.bookmarks.createFolder("New folder", 0)
                        if (id > 0) {
                            root.startRename(id, "New folder")
                        }
                    }
                }

                Button {
                    text: "Clear"
                    enabled: root.bookmarks && root.bookmarks.count > 0
                    onClicked: root.bookmarks.clearAll()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: "Search bookmarks"
                text: root.searchText
                selectByMouse: true
                onTextChanged: root.searchText = text
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Item {
                    anchors.fill: parent

                    ListView {
                        id: treeList
                        anchors.fill: parent
                        visible: !root.searchText || root.searchText.trim().length === 0
                        clip: true
                        model: root.bookmarks
                        spacing: 0
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            width: ListView.view.width
                            height: visibleRole ? Math.max(1, content.implicitHeight) : 0

                            readonly property bool visibleRole: !!treeVisible

                            ItemDelegate {
                                id: content
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                visible: parent.visibleRole
                                enabled: parent.visibleRole
                                leftPadding: 10 + depth * 16

                                onClicked: {
                                    if (isFolder) {
                                        root.bookmarks.toggleExpanded(bookmarkId)
                                    } else if (url && url.toString().length > 0) {
                                        commands.invoke("new-tab", { url: url.toString() })
                                        root.closeRequested()
                                    }
                                }

                                contentItem: RowLayout {
                                    spacing: theme.spacing

                                    ToolButton {
                                        visible: isFolder
                                        enabled: hasChildren
                                        text: expanded ? "▾" : "▸"
                                        onClicked: root.bookmarks.toggleExpanded(bookmarkId)
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Math.max(2, Math.round(theme.spacing / 4))

                                        Label {
                                            Layout.fillWidth: true
                                            text: title
                                            font.bold: isFolder
                                            elide: Text.ElideRight
                                            visible: bookmarkId !== root.editingId
                                        }

                                        TextField {
                                            Layout.fillWidth: true
                                            visible: bookmarkId === root.editingId
                                            text: root.editingText
                                            selectByMouse: true
                                            onTextChanged: root.editingText = text
                                            onAccepted: root.commitRename()
                                            Keys.onEscapePressed: root.cancelRename()
                                            Component.onCompleted: {
                                                if (visible) {
                                                    forceActiveFocus()
                                                    selectAll()
                                                }
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: !isFolder && bookmarkId !== root.editingId
                                            text: url ? url.toString() : ""
                                            opacity: 0.65
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Button {
                                        visible: !isFolder
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
                                            root.openContextMenuAtPoint(bookmarkId, isFolder, url, title, expanded, hasChildren, pos.x, pos.y)
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
                                            root.openContextMenuAtPoint(bookmarkId, isFolder, url, title, expanded, hasChildren, pos.x, pos.y)
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            text: "No bookmarks yet."
                            opacity: 0.7
                            visible: root.bookmarks && root.bookmarks.count === 0
                        }
                    }

                    ListView {
                        id: searchList
                        anchors.fill: parent
                        visible: root.searchText && root.searchText.trim().length > 0
                        clip: true
                        model: filtered
                        spacing: 4
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            visible: !isFolder
                            height: visible ? implicitHeight : 0

                            contentItem: RowLayout {
                                spacing: theme.spacing

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

                                Button {
                                    text: "Open"
                                    onClicked: {
                                        if (url && url.toString().length > 0) {
                                            commands.invoke("new-tab", { url: url.toString() })
                                            root.closeRequested()
                                        }
                                    }
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            text: "No matches."
                            opacity: 0.7
                            visible: searchList.count === 0
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.bookmarks ? root.bookmarks.lastError : ""
                color: "#b00020"
                wrapMode: Text.Wrap
                visible: root.bookmarks && root.bookmarks.lastError && root.bookmarks.lastError.length > 0
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}
