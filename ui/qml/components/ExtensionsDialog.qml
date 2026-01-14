import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var extensions

    signal closeRequested()

    property string folderDraft: ""

    function refreshNow() {
        if (root.extensions && root.extensions.refresh) {
            root.extensions.refresh()
        }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(820, parent.width - 80)
        height: Math.min(620, parent.height - 80)
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

            Label {
                Layout.fillWidth: true
                visible: root.extensions && root.extensions.ready === false
                text: "Waiting for WebView2 profile…"
                opacity: 0.7
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                TextField {
                    id: folderField
                    Layout.fillWidth: true
                    placeholderText: "Path to unpacked extension folder (manifest.json)"
                    text: root.folderDraft
                    selectByMouse: true
                    onTextChanged: root.folderDraft = text
                }

                Button {
                    text: "Install"
                    enabled: root.extensions && root.extensions.installFromFolder && root.folderDraft.trim().length > 0
                    onClicked: root.extensions.installFromFolder(root.folderDraft)
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: root.extensions

                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        hoverEnabled: true

                        background: Rectangle {
                            radius: 8
                            color: parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
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

                            Switch {
                                checked: enabled === true
                                onToggled: root.extensions.setExtensionEnabled(extensionId, checked)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    text: name && name.length > 0 ? name : extensionId
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: extensionId
                                    opacity: 0.6
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    visible: extensionId && extensionId.length > 0
                                }
                            }

                            ToolButton {
                                text: pinned ? "Unpin" : "Pin"
                                onClicked: root.extensions.setExtensionPinned(extensionId, !pinned)
                            }

                            ToolButton {
                                text: "Popup"
                                visible: popupUrl && popupUrl.length > 0
                                onClicked: commands.invoke("new-tab", { url: popupUrl })
                            }

                            ToolButton {
                                text: "Options"
                                visible: optionsUrl && optionsUrl.length > 0
                                onClicked: commands.invoke("new-tab", { url: optionsUrl })
                            }

                            ToolButton {
                                text: "Remove"
                                onClicked: root.extensions.removeExtension(extensionId)
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
    }

    Component.onCompleted: root.refreshNow()
}
