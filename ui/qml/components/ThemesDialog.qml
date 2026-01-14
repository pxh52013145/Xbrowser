import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var themes
    required property var settings

    signal closeRequested()

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 80)
        height: Math.min(520, parent.height - 80)
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
                    text: "Themes"
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
                text: "Select a theme. Built-in themes cannot be uninstalled."
                wrapMode: Text.Wrap
                opacity: 0.8
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    anchors.fill: parent
                    clip: true
                    model: root.themes

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        highlighted: root.settings.themeId === themeId

                        property bool builtInFlag: builtIn
                        property string themeIdValue: themeId
                        property string themeName: name
                        property string themeVersion: version

                        onClicked: {
                            list.currentIndex = index
                            root.settings.themeId = themeId
                        }

                        contentItem: ColumnLayout {
                            spacing: Math.max(4, Math.round(theme.spacing / 2))
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                RadioButton {
                                    checked: root.settings.themeId === themeId
                                    text: name + " (" + version + ")"
                                    onClicked: {
                                        list.currentIndex = index
                                        root.settings.themeId = themeId
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: builtIn ? "Built-in" : "Local"
                                    opacity: 0.7
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: description
                                wrapMode: Text.Wrap
                                opacity: 0.75
                                visible: description && description.length > 0
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                TextField {
                    id: urlField
                    Layout.fillWidth: true
                    placeholderText: "Install theme from URL (https://...)"
                    selectByMouse: true
                    enabled: !root.themes.busy
                }

                Button {
                    text: root.themes.busy ? "Installing..." : "Install"
                    enabled: !root.themes.busy && urlField.text.length > 0
                    onClicked: root.themes.installFromUrl(urlField.text)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    text: "Check Updates"
                    enabled: !root.themes.busy
                    onClicked: root.themes.checkForUpdates()
                }

                Button {
                    text: "Uninstall"
                    enabled: list.currentItem && !list.currentItem.builtInFlag
                    onClicked: {
                        const themeId = list.currentItem.themeIdValue
                        root.themes.uninstallTheme(themeId)
                        if (root.settings.themeId === themeId) {
                            root.settings.themeId = "workspace"
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: root.themes.lastError
                    color: "#b00020"
                    wrapMode: Text.Wrap
                    visible: root.themes.lastError && root.themes.lastError.length > 0
                }
            }

            Connections {
                target: root.themes

                function onInstallSucceeded(themeId) {
                    urlField.text = ""
                    toast.showToast("Installed theme: " + themeId)
                }

                function onInstallFailed(error) {
                    toast.showToast("Theme install failed")
                }

                function onUpdateAvailable(themeId, newVersion) {
                    notifications.push(
                        "Theme update available: " + themeId + " (" + newVersion + ")",
                        "Update",
                        "",
                        "theme-update",
                        { themeId: themeId }
                    )
                }

                function onUpdateCheckFinished() {
                    toast.showToast("Theme update check finished")
                }
            }
        }
    }
}
