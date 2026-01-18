import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Item {
    id: root
    anchors.fill: parent

    required property var view

    signal closeRequested()

    property string pdfPath: ""
    property bool pdfPending: false

    Connections {
        target: root.view
        function onPrintToPdfFinished(filePath, success, error) {
            root.pdfPending = false
            if (success) {
                toast.showToast("Saved PDF")
                root.closeRequested()
            } else {
                toast.showToast(error && error.length > 0 ? ("Save PDF failed: " + error) : "Save PDF failed")
            }
        }
    }

    Platform.FileDialog {
        id: saveDialog
        title: "Save as PDF"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["PDF files (*.pdf)"]
        onAccepted: {
            const url = saveDialog.file
            root.pdfPath = url && url.toString().startsWith("file:") ? url.toLocalFile() : url.toString()
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(620, parent.width - 80)
        height: Math.min(360, parent.height - 80)
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
                    text: "Print"
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
                text: "Print uses the current focused pane."
                opacity: 0.75
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    text: "Print…"
                    enabled: root.view && root.view.showPrintUI
                    onClicked: {
                        if (root.view && root.view.showPrintUI) {
                            root.view.showPrintUI()
                            root.closeRequested()
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Qt.rgba(0, 0, 0, 0.08)
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "PDF output path"
                    text: root.pdfPath
                    selectByMouse: true
                    onTextChanged: root.pdfPath = text
                }

                Button {
                    text: "Browse…"
                    onClicked: saveDialog.open()
                }

                Button {
                    text: root.pdfPending ? "Saving…" : "Save PDF"
                    enabled: !root.pdfPending && root.view && root.view.printToPdf && root.pdfPath.trim().length > 0
                    onClicked: {
                        if (!root.view || !root.view.printToPdf) {
                            return
                        }
                        root.pdfPending = true
                        root.view.printToPdf(root.pdfPath.trim())
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}
