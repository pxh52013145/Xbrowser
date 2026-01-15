import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var settings
    required property var themes

    signal closeRequested()

    property int currentIndex: 0

    readonly property var sections: [
        { title: "Appearance" },
        { title: "Running" },
        { title: "Shortcuts" },
        { title: "About" },
    ]

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(900, parent.width - 80)
        height: Math.min(620, parent.height - 80)
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.98)
        border.color: Qt.rgba(0, 0, 0, 0.12)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: theme.spacing

            Frame {
                Layout.preferredWidth: 220
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.spacing

                        Label {
                            Layout.fillWidth: true
                            text: "Settings"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        ToolButton {
                            text: "×"
                            onClicked: root.closeRequested()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(0, 0, 0, 0.08)
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.sections
                        currentIndex: root.currentIndex

                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            text: modelData.title
                            highlighted: ListView.isCurrentItem
                            onClicked: root.currentIndex = index
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackLayout {
                    id: pages
                    anchors.fill: parent
                    currentIndex: root.currentIndex

                    // Appearance
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: appearanceColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: appearanceColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Appearance"; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                Label { Layout.fillWidth: true; text: "Theme"; opacity: 0.85 }

                                Label {
                                    text: root.settings ? root.settings.themeId : ""
                                    opacity: 0.65
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: "Manage…"
                                    onClicked: commands.invoke("open-themes")
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Reduce motion"
                                    checked: root.settings ? root.settings.reduceMotion : false
                                    onToggled: if (root.settings) root.settings.reduceMotion = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Compact mode"
                                    checked: root.settings ? root.settings.compactMode : false
                                    onToggled: if (root.settings) root.settings.compactMode = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Show address bar"
                                    checked: root.settings ? root.settings.addressBarVisible : true
                                    onToggled: if (root.settings) root.settings.addressBarVisible = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Show menu bar"
                                    checked: root.settings ? root.settings.showMenuBar : false
                                    onToggled: if (root.settings) root.settings.showMenuBar = checked
                                }
                            }
                        }
                    }

                    // Running
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: runningColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: runningColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Running"; font.bold: true; font.pixelSize: 14 }

                            Label {
                                Layout.fillWidth: true
                                text: "Preferred launchers:\n- scripts\\\\dev.cmd (build + run)\n- scripts\\\\run.cmd (run existing build)\n- scripts\\\\deploy.cmd (one-time deploy for double-clickable exe)"
                                wrapMode: Text.Wrap
                                opacity: 0.8
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Essentials: Close resets (instead of closing)"
                                    checked: root.settings ? root.settings.essentialCloseResets : false
                                    onToggled: if (root.settings) root.settings.essentialCloseResets = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Back closes tab when no history"
                                    checked: root.settings ? root.settings.closeTabOnBackNoHistory : true
                                    onToggled: if (root.settings) root.settings.closeTabOnBackNoHistory = checked
                                }
                            }
                        }
                    }

                    // Shortcuts
                    Item {
                        anchors.fill: parent

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: theme.spacing

                            Label { text: "Shortcuts"; font.bold: true; font.pixelSize: 14 }

                            ShortcutsEditor {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                store: shortcutStore
                            }
                        }
                    }

                    // About
                    ColumnLayout {
                        spacing: theme.spacing

                        Label { text: "About"; font.bold: true; font.pixelSize: 14 }

                        Label {
                            Layout.fillWidth: true
                            text: "XBrowser " + Qt.application.version
                            opacity: 0.85
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Qt " + Qt.version
                            opacity: 0.75
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.spacing

                            Button {
                                text: "Welcome"
                                onClicked: commands.invoke("open-welcome")
                            }

                            Button {
                                text: "Diagnostics…"
                                onClicked: commands.invoke("open-diagnostics")
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}
