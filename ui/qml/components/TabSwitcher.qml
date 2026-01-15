import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Rectangle {
    id: root

    property var tabs: null
    property var browser: null
    property string query: ""

    signal closeRequested()

    implicitWidth: 620
    implicitHeight: 460

    function openSwitcher() {
        root.query = ""
        Qt.callLater(() => field.forceActiveFocus())
        list.currentIndex = 0
    }

    function activateCurrent() {
        if (!root.browser || !list.currentItem || !list.currentItem.tabId) {
            return
        }
        root.browser.activateTabById(list.currentItem.tabId)
        root.closeRequested()
    }

    TabSwitcherModel {
        id: filtered
        sourceTabs: root.tabs
        searchText: root.query
    }

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
                text: "Switch tab"
                font.bold: true
                font.pixelSize: 14
            }

            ToolButton {
                text: "×"
                onClicked: root.closeRequested()
            }
        }

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: "Search tabs"
            text: root.query
            selectByMouse: true
            onTextChanged: {
                root.query = text
                list.currentIndex = 0
            }
            onAccepted: root.activateCurrent()

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down) {
                    list.incrementCurrentIndex()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Up) {
                    list.decrementCurrentIndex()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Escape) {
                    root.closeRequested()
                    event.accepted = true
                    return
                }
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: filtered
                currentIndex: 0
                highlightMoveDuration: 0

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: ItemDelegate {
                    required property int tabId
                    required property string title
                    required property url url

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    onClicked: {
                        if (root.browser) {
                            root.browser.activateTabById(tabId)
                        }
                        root.closeRequested()
                    }

                    contentItem: RowLayout {
                        spacing: theme.spacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Math.max(2, Math.round(theme.spacing / 4))

                            Label {
                                Layout.fillWidth: true
                                text: title && title.length > 0 ? title : "Tab"
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
                    }
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.activateCurrent()
                        event.accepted = true
                        return
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No matching tabs"
                    opacity: 0.6
                    visible: list.count === 0
                }
            }
        }
    }
}
