import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var store

    signal closeRequested()

    property bool embedded: false
    property string searchText: ""
    property int selectedKind: 0
    property string selectedOrigin: ""
    property var originsList: []
    property var filteredOrigins: []

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

    function refreshOrigins() {
        if (!root.store || !root.store.origins) {
            root.originsList = []
            root.filteredOrigins = []
            return
        }
        root.originsList = root.store.origins()
        if (root.selectedOrigin && root.selectedOrigin.length > 0) {
            const exists = root.originsList.indexOf(root.selectedOrigin) >= 0
            if (!exists) {
                root.selectedOrigin = ""
            }
        }
        root.applyFilters()
    }

    function applyFilters() {
        const all = root.originsList || []
        const q = (root.searchText || "").trim().toLowerCase()
        const kind = Number(root.selectedKind || 0)

        const out = []
        for (let i = 0; i < all.length; i++) {
            const origin = String(all[i] || "")
            if (origin.length === 0) {
                continue
            }

            if (q.length > 0 && origin.toLowerCase().indexOf(q) < 0) {
                continue
            }

            if (kind > 0 && root.store && root.store.decision) {
                const state = Number(root.store.decision(origin, kind) || 0)
                if (state === 0) {
                    continue
                }
            }

            out.push(origin)
        }

        root.filteredOrigins = out

        if (root.selectedOrigin && root.selectedOrigin.length > 0) {
            if (out.indexOf(root.selectedOrigin) < 0) {
                root.selectedOrigin = ""
            }
        }
    }

    onSearchTextChanged: root.applyFilters()
    onSelectedKindChanged: root.applyFilters()

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
                    text: "Permissions"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "Reload"
                    enabled: root.store && root.store.reload
                    onClicked: root.store.reload()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Search sites"
                    text: root.searchText
                    selectByMouse: true
                    onTextChanged: root.searchText = text
                }

                ComboBox {
                    Layout.preferredWidth: 200
                    enabled: root.store
                    model: [
                        "All types",
                        root.permissionLabel(1),
                        root.permissionLabel(2),
                        root.permissionLabel(3),
                        root.permissionLabel(4),
                        root.permissionLabel(5),
                        root.permissionLabel(6),
                        root.permissionLabel(7),
                        root.permissionLabel(8),
                        root.permissionLabel(9),
                        root.permissionLabel(10),
                        root.permissionLabel(11),
                        root.permissionLabel(12)
                    ]
                    currentIndex: root.selectedKind
                    onActivated: root.selectedKind = currentIndex
                }

                Button {
                    text: "Reset all"
                    enabled: root.store
                    onClicked: {
                        if (root.store) {
                            root.store.clearAll()
                            root.selectedOrigin = ""
                            root.refreshOrigins()
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.filteredOrigins.length + " / " + root.originsList.length + " sites"
                opacity: 0.65
                font.pixelSize: 12
                visible: root.store
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                RowLayout {
                    anchors.fill: parent
                    spacing: theme.spacing

                    Frame {
                        Layout.preferredWidth: 360
                        Layout.fillHeight: true

                        ListView {
                            id: originsView
                            anchors.fill: parent
                            clip: true
                            model: root.filteredOrigins
                            currentIndex: -1

                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: ItemDelegate {
                                required property var modelData
                                required property int index

                                readonly property string origin: String(modelData || "")
                                width: ListView.view.width
                                text: origin
                                highlighted: root.selectedOrigin === origin
                                onClicked: root.selectedOrigin = origin
                            }

                            Label {
                                anchors.centerIn: parent
                                text: "No saved permissions"
                                opacity: 0.6
                                visible: originsView.count === 0
                            }
                        }
                    }

                    Frame {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: theme.spacing

                            Label {
                                Layout.fillWidth: true
                                text: root.selectedOrigin && root.selectedOrigin.length > 0 ? root.selectedOrigin : "Select a site"
                                elide: Text.ElideRight
                                font.bold: true
                            }

                            Flickable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: width
                                contentHeight: permsColumn.implicitHeight
                                boundsBehavior: Flickable.StopAtBounds
                                flickableDirection: Flickable.VerticalFlick
                                interactive: contentHeight > height
                                visible: root.selectedOrigin && root.selectedOrigin.length > 0

                                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                ColumnLayout {
                                    id: permsColumn
                                    width: parent.width
                                    spacing: 10

                                    Repeater {
                                        model: [
                                            { kind: 1 }, { kind: 2 }, { kind: 3 }, { kind: 4 }, { kind: 5 }, { kind: 6 },
                                            { kind: 7 }, { kind: 8 }, { kind: 9 }, { kind: 10 }, { kind: 11 }, { kind: 12 }
                                        ]

                                        delegate: RowLayout {
                                            required property var modelData

                                            Layout.fillWidth: true
                                            spacing: theme.spacing

                                            Label {
                                                Layout.fillWidth: true
                                                text: root.permissionLabel(modelData.kind)
                                                elide: Text.ElideRight
                                            }

                                            ComboBox {
                                                id: stateCombo
                                                model: ["Ask", "Allow", "Deny"]
                                                enabled: root.store && root.selectedOrigin && root.selectedOrigin.length > 0

                                                function syncNow() {
                                                    if (!root.store || !root.selectedOrigin || root.selectedOrigin.length === 0) {
                                                        stateCombo.currentIndex = 0
                                                        return
                                                    }
                                                    stateCombo.currentIndex = root.store.decision(root.selectedOrigin, modelData.kind)
                                                }

                                                Component.onCompleted: syncNow()

                                                Connections {
                                                    target: root.store
                                                    function onRevisionChanged() { stateCombo.syncNow() }
                                                }

                                                Connections {
                                                    target: root
                                                    function onSelectedOriginChanged() { stateCombo.syncNow() }
                                                }

                                                onActivated: {
                                                    if (!root.store || !root.selectedOrigin || root.selectedOrigin.length === 0) {
                                                        return
                                                    }
                                                    root.store.setDecision(root.selectedOrigin, modelData.kind, stateCombo.currentIndex)
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing
                                visible: root.selectedOrigin && root.selectedOrigin.length > 0

                                Button {
                                    Layout.fillWidth: true
                                    text: "Reset this site"
                                    enabled: root.store
                                    onClicked: {
                                        if (root.store) {
                                            root.store.clearOrigin(root.selectedOrigin)
                                            root.selectedOrigin = ""
                                            root.refreshOrigins()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: root.refreshOrigins()

    Connections {
        target: root.store
        function onRevisionChanged() { root.refreshOrigins() }
    }
}
