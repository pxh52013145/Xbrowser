import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Item {
    id: root
    anchors.fill: parent

    required property var downloads
    required property var downloadController

    signal closeRequested()

    property bool embedded: false
    property string searchText: ""

    function formatBytes(bytes) {
        const value = Math.max(0, Number(bytes || 0))
        if (value < 1024) return Math.round(value) + " B"
        const units = ["KB", "MB", "GB", "TB"]
        let v = value
        let idx = -1
        while (v >= 1024 && idx < units.length - 1) {
            v = v / 1024
            idx++
        }
        const decimals = v < 10 ? 1 : 0
        return v.toFixed(decimals) + " " + units[idx]
    }

    function progressText(bytesReceived, totalBytes) {
        const received = Math.max(0, Number(bytesReceived || 0))
        const total = Math.max(0, Number(totalBytes || 0))
        if (total > 0) {
            const pct = Math.max(0, Math.min(100, Math.floor((received / total) * 100)))
            return pct + "% (" + root.formatBytes(received) + " / " + root.formatBytes(total) + ")"
        }
        if (received > 0) {
            return root.formatBytes(received)
        }
        return ""
    }

    DownloadFilterModel {
        id: inProgress
        sourceDownloads: root.downloads
        searchText: root.searchText
        stateFilter: "in-progress"
    }

    DownloadFilterModel {
        id: completed
        sourceDownloads: root.downloads
        searchText: root.searchText
        stateFilter: "completed"
    }

    DownloadFilterModel {
        id: failed
        sourceDownloads: root.downloads
        searchText: root.searchText
        stateFilter: "failed"
    }

    Rectangle {
        id: panel
        readonly property real preferredWidth: root.embedded ? parent.width : Math.min(620, parent.width - 80)
        readonly property real preferredHeight: root.embedded ? parent.height : Math.min(480, parent.height - 80)

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
                    text: {
                        const total = root.downloads ? root.downloads.count() : 0
                        if (total <= 0) return "No downloads yet"
                        if (root.downloads.activeCount > 0) return "Active: " + root.downloads.activeCount + " / Total: " + total
                        return "No active downloads · Total: " + total
                    }
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

                ColumnLayout {
                    anchors.fill: parent
                    spacing: theme.spacing

                    TextField {
                        Layout.fillWidth: true
                        placeholderText: "Search downloads"
                        text: root.searchText
                        selectByMouse: true
                        onTextChanged: root.searchText = text
                    }

                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        clip: true
                        contentWidth: width
                        contentHeight: contentColumn.implicitHeight
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: contentColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label {
                                Layout.fillWidth: true
                                opacity: 0.75
                                wrapMode: Text.Wrap
                                text: root.searchText && root.searchText.trim().length > 0 ? "No matching downloads." : "No downloads yet."
                                visible: (root.downloads ? root.downloads.count() : 0) === 0 || (inProgressList.count + completedList.count + failedList.count) === 0
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(4, Math.round(theme.spacing / 2))
                                visible: inProgressList.count > 0

                                Label {
                                    Layout.fillWidth: true
                                    text: "In progress (" + inProgressList.count + ")"
                                    opacity: 0.85
                                    font.bold: true
                                }

                                ListView {
                                    id: inProgressList
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.min(240, contentHeight)
                                    clip: true
                                    model: inProgress
                                    spacing: 4
                                    boundsBehavior: Flickable.StopAtBounds
                                    interactive: contentHeight > height

                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                                    delegate: ItemDelegate {
                                        width: ListView.view.width

                                        contentItem: RowLayout {
                                            spacing: theme.spacing

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: Math.max(4, Math.round(theme.spacing / 2))

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: uri && uri.length > 0 ? uri : filePath
                                                    elide: Text.ElideRight
                                                }
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: {
                                                        if (paused) return "Paused"
                                                        if (canResume) {
                                                            return interruptReason && interruptReason.length > 0 ? ("Interrupted: " + interruptReason) : "Interrupted"
                                                        }
                                                        const t = root.progressText(bytesReceived, totalBytes)
                                                        return t && t.length > 0 ? t : "In progress"
                                                    }
                                                    opacity: 0.7
                                                    font.pixelSize: 12
                                                }

                                                ProgressBar {
                                                    Layout.fillWidth: true
                                                    from: 0
                                                    to: 1
                                                    value: {
                                                        const total = Math.max(0, Number(totalBytes || 0))
                                                        if (total <= 0) return 0
                                                        return Math.max(0, Math.min(1, Number(bytesReceived || 0) / total))
                                                    }
                                                    indeterminate: Number(totalBytes || 0) <= 0 && !paused && !canResume
                                                }
                                            }

                                            Button {
                                                text: (paused || canResume) ? "Resume" : "Pause"
                                                enabled: root.downloadController
                                                onClicked: {
                                                    if (!root.downloadController) return
                                                    if (paused || canResume) {
                                                        root.downloadController.resumeDownload(downloadId)
                                                    } else {
                                                        root.downloadController.pauseDownload(downloadId)
                                                    }
                                                }
                                            }

                                            Button {
                                                text: "Cancel"
                                                enabled: root.downloadController
                                                onClicked: root.downloadController.cancelDownload(downloadId)
                                            }

                                            Button {
                                                text: "Copy URL"
                                                enabled: uri && uri.length > 0
                                                onClicked: commands.invoke("copy-text", { text: uri })
                                            }

                                            Button {
                                                text: "Folder"
                                                enabled: filePath && filePath.length > 0
                                                onClicked: {
                                                    const ok = nativeUtils.openFolder(filePath)
                                                    if (!ok) {
                                                        const err = nativeUtils.lastError || ""
                                                        toast.showToast(err.length > 0 ? ("Open folder failed: " + err) : "Open folder failed")
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(4, Math.round(theme.spacing / 2))
                                visible: completedList.count > 0

                                Label {
                                    Layout.fillWidth: true
                                    text: "Completed (" + completedList.count + ")"
                                    opacity: 0.85
                                    font.bold: true
                                }

                                ListView {
                                    id: completedList
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.min(240, contentHeight)
                                    clip: true
                                    model: completed
                                    spacing: 4
                                    boundsBehavior: Flickable.StopAtBounds
                                    interactive: contentHeight > height

                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

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
                                                onClicked: {
                                                    const ok = nativeUtils.openPath(filePath)
                                                    if (!ok) {
                                                        const err = nativeUtils.lastError || ""
                                                        toast.showToast(err.length > 0 ? ("Open failed: " + err) : "Open failed")
                                                    }
                                                }
                                            }

                                            Button {
                                                text: "Copy URL"
                                                enabled: uri && uri.length > 0
                                                onClicked: commands.invoke("copy-text", { text: uri })
                                            }

                                            Button {
                                                text: "Folder"
                                                enabled: filePath && filePath.length > 0
                                                onClicked: {
                                                    const ok = nativeUtils.openFolder(filePath)
                                                    if (!ok) {
                                                        const err = nativeUtils.lastError || ""
                                                        toast.showToast(err.length > 0 ? ("Open folder failed: " + err) : "Open folder failed")
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Math.max(4, Math.round(theme.spacing / 2))
                                visible: failedList.count > 0

                                Label {
                                    Layout.fillWidth: true
                                    text: "Failed (" + failedList.count + ")"
                                    opacity: 0.85
                                    font.bold: true
                                }

                                ListView {
                                    id: failedList
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.min(200, contentHeight)
                                    clip: true
                                    model: failed
                                    spacing: 4
                                    boundsBehavior: Flickable.StopAtBounds
                                    interactive: contentHeight > height

                                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

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
                                                    text: interruptReason && interruptReason.length > 0 ? ("Failed: " + interruptReason) : "Failed"
                                                    opacity: 0.7
                                                    font.pixelSize: 12
                                                }
                                            }

                                            Button {
                                                text: "Copy URL"
                                                enabled: uri && uri.length > 0
                                                onClicked: commands.invoke("copy-text", { text: uri })
                                            }

                                            Button {
                                                text: "Folder"
                                                enabled: filePath && filePath.length > 0
                                                onClicked: {
                                                    const ok = nativeUtils.openFolder(filePath)
                                                    if (!ok) {
                                                        const err = nativeUtils.lastError || ""
                                                        toast.showToast(err.length > 0 ? ("Open folder failed: " + err) : "Open folder failed")
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
            }
        }
    }
}
