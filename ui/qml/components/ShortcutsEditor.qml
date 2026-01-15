import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var store

    property string searchText: ""

    function resetAll() {
        if (root.store && root.store.resetAll) {
            root.store.resetAll()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            TextField {
                Layout.fillWidth: true
                placeholderText: "Search shortcuts"
                text: root.searchText
                selectByMouse: true
                onTextChanged: root.searchText = text
            }

            Button {
                text: "Reset all"
                enabled: root.store
                onClicked: root.resetAll()
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.store && root.store.lastError && root.store.lastError.length > 0
            text: root.store ? root.store.lastError : ""
            wrapMode: Text.Wrap
            color: "#b91c1c"
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: root.store
                spacing: 6

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: ItemDelegate {
                    required property string entryId
                    required property string group
                    required property string title
                    required property string keySequence
                    required property string defaultKeySequence
                    required property bool customized

                    readonly property bool matchesSearch: {
                        const q = (root.searchText || "").trim().toLowerCase()
                        if (q.length === 0) {
                            return true
                        }
                        const hay = (String(title || "") + " " + String(group || "") + " " + String(keySequence || "")).toLowerCase()
                        return hay.indexOf(q) >= 0
                    }

                    width: ListView.view.width
                    height: matchesSearch ? implicitHeight : 0
                    visible: matchesSearch
                    hoverEnabled: true

                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                    }

                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.spacing

                            Label {
                                Layout.fillWidth: true
                                text: title
                                elide: Text.ElideRight
                            }

                            Label {
                                text: group
                                opacity: 0.6
                                font.pixelSize: 11
                                visible: group && group.length > 0
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.spacing

                            TextField {
                                id: seqField
                                Layout.preferredWidth: 200
                                placeholderText: "Shortcut"
                                selectByMouse: true

                                Binding on text {
                                    when: !seqField.activeFocus
                                    value: keySequence
                                }

                                onEditingFinished: {
                                    if (!root.store || !root.store.setUserSequence) {
                                        return
                                    }
                                    root.store.setUserSequence(entryId, text)
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: defaultKeySequence && defaultKeySequence.length > 0 ? ("Default: " + defaultKeySequence) : ""
                                opacity: 0.55
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Button {
                                text: "Reset"
                                enabled: customized
                                onClicked: {
                                    if (root.store && root.store.resetSequence) {
                                        root.store.resetSequence(entryId)
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

