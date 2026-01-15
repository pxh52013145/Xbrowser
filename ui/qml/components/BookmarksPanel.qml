import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Item {
    id: root
    anchors.fill: parent

    required property var bookmarks

    signal closeRequested()

    property bool embedded: false
    property string searchText: ""

    BookmarksFilterModel {
        id: filtered
        sourceBookmarks: root.bookmarks
        searchText: root.searchText
    }

    Rectangle {
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(720, parent.width - 80)
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
                    text: "Bookmarks"
                    font.bold: true
                    font.pixelSize: 14
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
                        required property int bookmarkId

                        width: ListView.view.width

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

                            ToolButton {
                                text: "×"
                                onClicked: {
                                    const idx = root.bookmarks ? root.bookmarks.indexOfUrl(url) : -1
                                    if (idx >= 0) {
                                        root.bookmarks.removeAt(idx)
                                    }
                                }
                            }
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
