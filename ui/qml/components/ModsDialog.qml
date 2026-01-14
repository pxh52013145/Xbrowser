import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var mods

    signal closeRequested()

    property int selectedIndex: -1
    property string nameDraft: ""
    property string cssDraft: ""
    property bool enabledDraft: true

    function selectIndex(idx) {
        if (!root.mods) {
            root.selectedIndex = -1
            return
        }

        if (idx < 0 || idx >= root.mods.count()) {
            root.selectedIndex = -1
            root.nameDraft = ""
            root.cssDraft = ""
            root.enabledDraft = true
            return
        }

        root.selectedIndex = idx
        root.nameDraft = root.mods.nameAt(idx) || ""
        root.cssDraft = root.mods.cssAt(idx) || ""
        root.enabledDraft = root.mods.enabledAt(idx) === true
    }

    function applyDraft() {
        const idx = root.selectedIndex
        if (!root.mods || idx < 0 || idx >= root.mods.count()) {
            return
        }

        root.mods.setNameAt(idx, root.nameDraft)
        root.mods.setCssAt(idx, root.cssDraft)
        root.mods.setEnabledAt(idx, root.enabledDraft)
        toast.showToast("Updated mod")
    }

    function createMod() {
        if (!root.mods) {
            return
        }
        const idx = root.mods.addMod("New Mod", "")
        root.selectIndex(idx)
        list.currentIndex = idx
        Qt.callLater(() => nameField.forceActiveFocus())
    }

    function deleteSelected() {
        const idx = root.selectedIndex
        if (!root.mods || idx < 0 || idx >= root.mods.count()) {
            return
        }

        root.mods.removeMod(idx)
        toast.showToast("Deleted mod")
        root.selectIndex(Math.min(idx, root.mods.count() - 1))
        list.currentIndex = root.selectedIndex
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(920, parent.width - 80)
        height: Math.min(640, parent.height - 80)
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
                    text: "Mods (CSS)"
                    font.bold: true
                    font.pixelSize: 14
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Enabled mods are injected into every page via WebView2 user scripts."
                wrapMode: Text.Wrap
                opacity: 0.8
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: theme.spacing

                Frame {
                    Layout.preferredWidth: 300
                    Layout.fillHeight: true

                    ListView {
                        id: list
                        anchors.fill: parent
                        clip: true
                        model: root.mods
                        currentIndex: root.selectedIndex

                        onCurrentIndexChanged: root.selectIndex(currentIndex)

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            highlighted: index === root.selectedIndex

                            contentItem: RowLayout {
                                spacing: theme.spacing

                                CheckBox {
                                    checked: enabled
                                    onToggled: root.mods.setEnabledAt(index, checked)
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: name
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: enabled ? "Enabled" : "Disabled"
                                        opacity: 0.6
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            onClicked: {
                                list.currentIndex = index
                                root.selectIndex(index)
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: theme.spacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.spacing

                        TextField {
                            id: nameField
                            Layout.fillWidth: true
                            placeholderText: root.selectedIndex >= 0 ? "Mod name" : "Select a mod"
                            selectByMouse: true
                            enabled: root.selectedIndex >= 0
                            text: root.nameDraft
                            onTextChanged: root.nameDraft = text
                        }

                        CheckBox {
                            text: "Enabled"
                            enabled: root.selectedIndex >= 0
                            checked: root.enabledDraft
                            onToggled: root.enabledDraft = checked
                        }
                    }

                    Frame {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        TextArea {
                            id: cssArea
                            anchors.fill: parent
                            selectByMouse: true
                            wrapMode: TextArea.Wrap
                            placeholderText: root.selectedIndex >= 0 ? "CSS to inject into every page..." : ""
                            enabled: root.selectedIndex >= 0
                            text: root.cssDraft
                            onTextChanged: root.cssDraft = text
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.spacing

                        Button {
                            text: "Add"
                            onClicked: root.createMod()
                        }

                        Button {
                            text: "Delete"
                            enabled: root.selectedIndex >= 0
                            onClicked: root.deleteSelected()
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Apply"
                            enabled: root.selectedIndex >= 0
                            onClicked: root.applyDraft()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.mods && root.mods.lastError ? root.mods.lastError : ""
                        color: "#b00020"
                        wrapMode: Text.Wrap
                        visible: root.mods && root.mods.lastError && root.mods.lastError.length > 0
                    }
                }
            }
        }
    }
}

