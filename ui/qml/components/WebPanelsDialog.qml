import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var panels

    property bool embedded: false
    property url currentUrl: "about:blank"
    property string currentTitle: ""

    property string urlDraft: ""
    property string titleDraft: ""

    signal closeRequested()
    signal openRequested(url url, string title)

    Keys.onEscapePressed: root.closeRequested()

    function addDraft() {
        if (!root.panels) {
            return
        }
        const urlText = String(urlDraft || "").trim()
        if (urlText.length === 0) {
            return
        }
        let normalized = urlText
        if (!/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(normalized)) {
            normalized = "https://" + normalized
        }
        root.panels.addPanel(normalized, String(titleDraft || ""))
        urlDraft = ""
        titleDraft = ""
    }

    function addCurrent() {
        if (!root.panels) {
            return
        }
        if (!currentUrl || currentUrl.toString().length === 0 || currentUrl.toString() === "about:blank") {
            return
        }
        root.panels.addPanel(currentUrl, currentTitle)
    }

    Rectangle {
        id: panel
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(520, parent.width - 80)
        readonly property real preferredHeight: root.embedded ? parent.height : Math.min(560, parent.height - 80)

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
                    text: "Panels"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "Add current"
                    enabled: root.currentUrl && root.currentUrl.toString().length > 0 && root.currentUrl.toString() !== "about:blank"
                    onClicked: root.addCurrent()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: theme.spacing
                rowSpacing: theme.spacing

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Panel URL"
                    text: root.urlDraft
                    selectByMouse: true
                    onTextChanged: root.urlDraft = text
                    onAccepted: root.addDraft()
                }

                Button {
                    text: "Add"
                    enabled: root.urlDraft && root.urlDraft.trim().length > 0
                    onClicked: root.addDraft()
                }

                TextField {
                    Layout.fillWidth: true
                    Layout.columnSpan: 2
                    placeholderText: "Title (optional)"
                    text: root.titleDraft
                    selectByMouse: true
                    onTextChanged: root.titleDraft = text
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: root.panels
                    spacing: 6

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: ListView.view.width
                        radius: theme.cornerRadius
                        color: Qt.rgba(0, 0, 0, 0.03)
                        border.color: Qt.rgba(0, 0, 0, 0.08)
                        border.width: 1
                        property bool renaming: false
                        property string renameDraft: ""

                        function beginRename() {
                            renaming = true
                            renameDraft = String(title || "")
                            Qt.callLater(() => {
                                renameField.forceActiveFocus()
                                renameField.selectAll()
                            })
                        }

                        function cancelRename() {
                            renaming = false
                        }

                        function commitRename() {
                            if (!root.panels) {
                                renaming = false
                                return
                            }
                            root.panels.updatePanel(panelId, url, String(renameDraft || ""))
                            renaming = false
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: theme.spacing
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: title && title.length > 0 ? title : (url ? url.toString() : "Panel")
                                        elide: Text.ElideRight
                                        font.bold: true
                                        visible: !renaming
                                    }

                                    TextField {
                                        id: renameField
                                        Layout.fillWidth: true
                                        visible: renaming
                                        text: renameDraft
                                        selectByMouse: true
                                        onTextEdited: renameDraft = text
                                        onAccepted: commitRename()
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: url ? url.toString() : ""
                                        elide: Text.ElideRight
                                        opacity: 0.7
                                        font.pixelSize: 11
                                        visible: url && url.toString().length > 0
                                    }
                                }

                                ToolButton {
                                    text: "↑"
                                    enabled: index > 0
                                    onClicked: root.panels.movePanel(index, index - 1)
                                }

                                ToolButton {
                                    text: "↓"
                                    enabled: root.panels && index >= 0 && index < root.panels.count - 1
                                    onClicked: root.panels.movePanel(index, index + 1)
                                }

                                Button {
                                    text: "Open"
                                    enabled: url && url.toString().length > 0
                                    onClicked: root.openRequested(url, title)
                                }

                                ToolButton {
                                    text: renaming ? "Save" : "Rename"
                                    onClicked: {
                                        if (renaming) {
                                            commitRename()
                                        } else {
                                            beginRename()
                                        }
                                    }
                                }

                                ToolButton {
                                    text: renaming ? "Cancel" : "Remove"
                                    onClicked: {
                                        if (renaming) {
                                            cancelRename()
                                        } else {
                                            root.panels.removeAt(index)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    footer: Item {
                        width: ListView.view.width
                        height: root.panels && root.panels.count > 0 ? 0 : emptyLabel.implicitHeight + theme.spacing * 2
                        visible: root.panels && root.panels.count === 0

                        Label {
                            id: emptyLabel
                            anchors.centerIn: parent
                            text: "No panels yet"
                            opacity: 0.6
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.panels ? root.panels.lastError : ""
                color: "#b00020"
                wrapMode: Text.Wrap
                visible: root.panels && root.panels.lastError && root.panels.lastError.length > 0
            }
        }
    }
}
