import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Downloads"
    modal: true
    standardButtons: Dialog.Close

    required property var downloads

    implicitWidth: 620
    implicitHeight: 480

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: theme.spacing

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
