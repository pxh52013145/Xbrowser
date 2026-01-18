import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var extensions

    signal closeRequested()

    property bool embedded: false
    property string folderDraft: ""
    property string searchText: ""

    function refreshNow() {
        if (root.extensions && root.extensions.refresh) {
            root.extensions.refresh()
        }
    }

    function selectFirstIfNeeded() {
        if (!root.extensions || list.currentIndex >= 0 || list.count === 0) {
            return
        }
        list.currentIndex = 0
    }

    Rectangle {
        id: panel
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(980, parent.width - 80)
        readonly property real preferredHeight: root.embedded ? parent.height : Math.min(660, parent.height - 80)

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
                    text: "Extensions"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "Refresh"
                    onClicked: root.refreshNow()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            Label {
                Layout.fillWidth: true
                visible: root.extensions && root.extensions.lastError && root.extensions.lastError.length > 0
                text: root.extensions ? root.extensions.lastError : ""
                wrapMode: Text.Wrap
                color: "#b91c1c"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Search extensions"
                    text: root.searchText
                    selectByMouse: true
                    onTextChanged: root.searchText = text
                }

                Button {
                    text: "Install…"
                    enabled: root.extensions && root.extensions.installFromFolder
                    onClicked: installRow.visible = !installRow.visible
                }
            }

            RowLayout {
                id: installRow
                Layout.fillWidth: true
                spacing: theme.spacing
                visible: false

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Path to unpacked extension folder (manifest.json)"
                    text: root.folderDraft
                    selectByMouse: true
                    onTextChanged: root.folderDraft = text
                }

                Button {
                    text: "Install"
                    enabled: root.extensions && root.extensions.installFromFolder && root.folderDraft.trim().length > 0
                    onClicked: {
                        root.extensions.installFromFolder(root.folderDraft)
                        root.folderDraft = ""
                        installRow.visible = false
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridLayout {
                    anchors.fill: parent
                    columns: root.embedded ? 1 : 2
                    rowSpacing: theme.spacing
                    columnSpacing: theme.spacing

                    Frame {
                        Layout.row: 0
                        Layout.column: 0
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.minimumHeight: 0
                        Layout.preferredWidth: root.embedded ? 0 : 340
                        Layout.fillHeight: !root.embedded
                        Layout.preferredHeight: root.embedded ? Math.min(280, Math.round(panel.height * 0.45)) : 0

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: theme.spacing

                            ListView {
                                id: list
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                model: root.extensions
                                currentIndex: -1
                                highlightMoveDuration: 0

                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                delegate: ItemDelegate {
                                    required property string extensionId
                                    required property string name
                                    required property bool enabled
                                    required property bool pinned
                                    required property var iconUrl
                                    required property string popupUrl
                                    required property string optionsUrl
                                    required property string version
                                    required property string description
                                    required property string installPath
                                    required property var permissions
                                    required property var hostPermissions
                                    required property bool updateAvailable

                                    readonly property bool matchesSearch: {
                                        const q = (root.searchText || "").trim().toLowerCase()
                                        if (q.length === 0) {
                                            return true
                                        }
                                        const hay = (String(name || "") + " " + String(extensionId || "")).toLowerCase()
                                        return hay.indexOf(q) >= 0
                                    }

                                    width: ListView.view.width
                                    height: matchesSearch ? implicitHeight : 0
                                    visible: matchesSearch
                                    highlighted: ListView.isCurrentItem
                                    hoverEnabled: true
                                    onClicked: list.currentIndex = index

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.highlighted ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16)
                                                                 : (parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent")
                                        border.color: parent.highlighted ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : "transparent"
                                        border.width: parent.highlighted ? 1 : 0
                                    }

                                    contentItem: RowLayout {
                                        spacing: theme.spacing

                                        Item {
                                            width: 22
                                            height: 22
                                            Layout.alignment: Qt.AlignVCenter

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 6
                                                color: Qt.rgba(0, 0, 0, 0.06)
                                                border.width: 1
                                                border.color: Qt.rgba(0, 0, 0, 0.08)
                                                visible: !(iconUrl && iconUrl.toString().length > 0)

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: name && name.length > 0 ? name[0].toUpperCase() : ""
                                                    font.pixelSize: 10
                                                    opacity: 0.65
                                                }
                                            }

                                            Image {
                                                anchors.fill: parent
                                                source: iconUrl
                                                asynchronous: true
                                                cache: true
                                                fillMode: Image.PreserveAspectFit
                                                visible: iconUrl && iconUrl.toString().length > 0
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: name && name.length > 0 ? name : extensionId
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    text: version && version.length > 0 ? ("v" + version) : ""
                                                    opacity: 0.6
                                                    font.pixelSize: 11
                                                    visible: version && version.length > 0
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: extensionId
                                                    opacity: 0.55
                                                    font.pixelSize: 11
                                                    elide: Text.ElideRight
                                                }

                                                Label {
                                                    text: updateAvailable ? "Update" : ""
                                                    color: "#b45309"
                                                    font.pixelSize: 11
                                                    visible: updateAvailable
                                                }
                                            }
                                        }

                                        Label {
                                            text: enabled ? "" : "Off"
                                            opacity: enabled ? 0.0 : 0.65
                                            visible: !enabled
                                        }

                                        Label {
                                            text: pinned ? "Pin" : ""
                                            opacity: 0.65
                                            visible: pinned
                                        }
                                    }
                                }

                                Label {
                                    anchors.centerIn: parent
                                    text: "No extensions installed"
                                    opacity: 0.6
                                    visible: list.count === 0 && (!root.extensions || root.extensions.ready === true)
                                }
                            }
                        }
                    }

                    Frame {
                        Layout.row: root.embedded ? 1 : 0
                        Layout.column: root.embedded ? 0 : 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 0
                        Layout.minimumHeight: 0

                        Item {
                            id: detailsHost
                            anchors.fill: parent

                            readonly property var entry: list.currentItem ? list.currentItem : null

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: theme.spacing

                                Label {
                                    Layout.fillWidth: true
                                    text: detailsHost.entry ? (detailsHost.entry.name || detailsHost.entry.extensionId) : "Select an extension"
                                    font.bold: true
                                    font.pixelSize: 14
                                    elide: Text.ElideRight
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: actionsFlow.implicitHeight
                                    visible: !!detailsHost.entry

                                    Flow {
                                        id: actionsFlow
                                        width: parent.width
                                        spacing: theme.spacing

                                        Switch {
                                            checked: detailsHost.entry ? detailsHost.entry.enabled === true : false
                                            onToggled: root.extensions.setExtensionEnabled(detailsHost.entry.extensionId, checked)
                                        }

                                        Button {
                                            text: detailsHost.entry && detailsHost.entry.pinned ? "Unpin" : "Pin"
                                            onClicked: root.extensions.setExtensionPinned(detailsHost.entry.extensionId, !(detailsHost.entry && detailsHost.entry.pinned))
                                        }

                                        Button {
                                            text: "Popup"
                                            enabled: detailsHost.entry && detailsHost.entry.popupUrl && detailsHost.entry.popupUrl.length > 0
                                            onClicked: commands.invoke("new-tab", { url: detailsHost.entry.popupUrl })
                                        }

                                        Button {
                                            text: "Options"
                                            enabled: detailsHost.entry && detailsHost.entry.optionsUrl && detailsHost.entry.optionsUrl.length > 0
                                            onClicked: commands.invoke("new-tab", { url: detailsHost.entry.optionsUrl })
                                        }

                                        Button {
                                            text: "Remove"
                                            onClicked: {
                                                root.extensions.removeExtension(detailsHost.entry.extensionId)
                                                list.currentIndex = -1
                                                Qt.callLater(root.selectFirstIfNeeded)
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Qt.rgba(0, 0, 0, 0.08)
                                    visible: !!detailsHost.entry
                                }

                                Flickable {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    contentWidth: width
                                    contentHeight: detailsColumn.implicitHeight
                                    boundsBehavior: Flickable.StopAtBounds
                                    flickableDirection: Flickable.VerticalFlick
                                    interactive: contentHeight > height
                                    visible: !!detailsHost.entry

                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    ColumnLayout {
                                        id: detailsColumn
                                        width: parent.width
                                        spacing: theme.spacing

                                        Label {
                                            Layout.fillWidth: true
                                            text: detailsHost.entry && detailsHost.entry.updateAvailable ? "Update detected (manifest changed on disk)." : ""
                                            color: "#b45309"
                                            wrapMode: Text.Wrap
                                            visible: detailsHost.entry && detailsHost.entry.updateAvailable
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: detailsHost.entry ? ("ID: " + detailsHost.entry.extensionId) : ""
                                            opacity: 0.75
                                            wrapMode: Text.Wrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: detailsHost.entry && detailsHost.entry.version && detailsHost.entry.version.length > 0 ? ("Version: " + detailsHost.entry.version) : ""
                                            opacity: 0.75
                                            visible: detailsHost.entry && detailsHost.entry.version && detailsHost.entry.version.length > 0
                                            wrapMode: Text.Wrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: detailsHost.entry && detailsHost.entry.description && detailsHost.entry.description.length > 0 ? detailsHost.entry.description : ""
                                            opacity: 0.8
                                            visible: detailsHost.entry && detailsHost.entry.description && detailsHost.entry.description.length > 0
                                            wrapMode: Text.Wrap
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: detailsHost.entry && detailsHost.entry.installPath && detailsHost.entry.installPath.length > 0 ? ("Install path: " + detailsHost.entry.installPath) : ""
                                            opacity: 0.7
                                            visible: detailsHost.entry && detailsHost.entry.installPath && detailsHost.entry.installPath.length > 0
                                            wrapMode: Text.Wrap
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 1
                                            color: Qt.rgba(0, 0, 0, 0.08)
                                            visible: (detailsHost.entry && ((detailsHost.entry.permissions && detailsHost.entry.permissions.length > 0) || (detailsHost.entry.hostPermissions && detailsHost.entry.hostPermissions.length > 0)))
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 6
                                            visible: detailsHost.entry && detailsHost.entry.permissions && detailsHost.entry.permissions.length > 0

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Permissions"
                                                    font.bold: true
                                                    opacity: 0.9
                                                }

                                                ToolButton {
                                                    text: "Copy"
                                                    onClicked: {
                                                        const list = detailsHost.entry ? (detailsHost.entry.permissions || []) : []
                                                        const text = list.map(v => String(v)).join("\n")
                                                        commands.invoke("copy-text", { text })
                                                    }
                                                }
                                            }

                                            Repeater {
                                                model: detailsHost.entry ? (detailsHost.entry.permissions || []) : []
                                                delegate: Label {
                                                    required property var modelData
                                                    Layout.fillWidth: true
                                                    text: "• " + String(modelData)
                                                    opacity: 0.8
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 6
                                            visible: detailsHost.entry && detailsHost.entry.hostPermissions && detailsHost.entry.hostPermissions.length > 0

                                            readonly property bool hasAllUrls: {
                                                const list = detailsHost.entry ? (detailsHost.entry.hostPermissions || []) : []
                                                for (const v of list) {
                                                    if (String(v) === "<all_urls>") {
                                                        return true
                                                    }
                                                }
                                                return false
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 6

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Host permissions"
                                                    font.bold: true
                                                    opacity: 0.9
                                                }

                                                ToolButton {
                                                    text: "Copy"
                                                    onClicked: {
                                                        const list = detailsHost.entry ? (detailsHost.entry.hostPermissions || []) : []
                                                        const text = list.map(v => String(v)).join("\n")
                                                        commands.invoke("copy-text", { text })
                                                    }
                                                }
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                visible: hasAllUrls
                                                text: "Warning: this extension requests <all_urls> (site access to all websites)."
                                                wrapMode: Text.Wrap
                                                color: "#b91c1c"
                                            }

                                            Repeater {
                                                model: detailsHost.entry ? (detailsHost.entry.hostPermissions || []) : []
                                                delegate: Label {
                                                    required property var modelData
                                                    Layout.fillWidth: true
                                                    text: "• " + String(modelData)
                                                    opacity: 0.8
                                                    wrapMode: Text.Wrap
                                                }
                                            }
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: root.extensions && root.extensions.ready === false
                                    text: "Waiting for WebView2 profile…"
                                    opacity: 0.7
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: root.refreshNow()

    Connections {
        target: root.extensions

        function onModelReset() { Qt.callLater(root.selectFirstIfNeeded) }
        function onRowsInserted() { Qt.callLater(root.selectFirstIfNeeded) }
    }
}
