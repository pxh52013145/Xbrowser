import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var downloads

    signal closeRequested()

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 80)
        height: Math.min(480, parent.height - 80)
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
                    text: "Downloads"
                    font.bold: true
                    font.pixelSize: 14
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: downloads.activeCount > 0 ? ("Active: " + downloads.activeCount) : "No active downloads"
                    opacity: 0.8
                }

                Button {
                    text: "Clear Finished"
                    enabled: downloads.count() > downloads.activeCount
                    onClicked: downloads.clearFinished()
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    anchors.fill: parent
                    clip: true
                    model: downloads

                    delegate: ItemDelegate {
                        width: ListView.view.width

                        contentItem: RowLayout {
                            spacing: theme.spacing

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(2, Math.round(theme.spacing / 4))

                                Label {
                                    Layout.fillWidth: true
                                    text: uri && uri.length > 0 ? uri : filePath
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: state
                                    opacity: 0.7
                                    font.pixelSize: 12
                                }
                            }

                            Button {
                                text: "Open"
                                enabled: filePath && filePath.length > 0
                                onClicked: downloads.openFile(downloadId)
                            }

                            Button {
                                text: "Folder"
                                enabled: filePath && filePath.length > 0
                                onClicked: downloads.openFolder(downloadId)
                            }
                        }
                    }
                }
            }
        }
    }
}
