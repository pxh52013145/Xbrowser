import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var diagnostics

    signal closeRequested()

    Rectangle {
        id: panel
        readonly property real preferredWidth: Math.min(680, parent.width - 80)
        readonly property real preferredHeight: Math.min(520, parent.height - 80)

        width: Math.max(1, Math.round(preferredWidth))
        height: Math.max(1, Math.round(preferredHeight))
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

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
                    text: "Diagnostics"
                    font.bold: true
                    font.pixelSize: 14
                }

                Button {
                    text: "Refresh"
                    enabled: root.diagnostics && root.diagnostics.refreshWebView2Version
                    onClicked: root.diagnostics.refreshWebView2Version()
                }

                ToolButton {
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            Label { text: "XBrowser " + Qt.application.version; opacity: 0.85 }
            Label { text: "Qt " + Qt.version; opacity: 0.75 }

            Label {
                Layout.fillWidth: true
                text: root.diagnostics && root.diagnostics.webView2Version && root.diagnostics.webView2Version.length > 0
                      ? ("WebView2 " + root.diagnostics.webView2Version)
                      : "WebView2 (unknown)"
                opacity: 0.75
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: root.diagnostics ? root.diagnostics.webView2Error : ""
                color: "#b91c1c"
                wrapMode: Text.Wrap
                visible: root.diagnostics && root.diagnostics.webView2Error && root.diagnostics.webView2Error.length > 0
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(0, 0, 0, 0.08) }

            Label {
                Layout.fillWidth: true
                text: root.diagnostics ? ("Data: " + root.diagnostics.dataDir) : ""
                opacity: 0.75
                wrapMode: Text.Wrap
                visible: root.diagnostics && root.diagnostics.dataDir && root.diagnostics.dataDir.length > 0
            }

            Label {
                Layout.fillWidth: true
                text: root.diagnostics ? ("Log: " + root.diagnostics.logFilePath) : ""
                opacity: 0.75
                wrapMode: Text.Wrap
                visible: root.diagnostics && root.diagnostics.logFilePath && root.diagnostics.logFilePath.length > 0
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    text: "Open log folder"
                    enabled: root.diagnostics && root.diagnostics.openLogFolder
                    onClicked: root.diagnostics.openLogFolder()
                }

                Button {
                    text: "Open log file"
                    enabled: root.diagnostics && root.diagnostics.openLogFile
                    onClicked: root.diagnostics.openLogFile()
                }

                Button {
                    text: "Open data folder"
                    enabled: root.diagnostics && root.diagnostics.openDataFolder
                    onClicked: root.diagnostics.openDataFolder()
                }

                Item { Layout.fillWidth: true }
            }

            Label {
                Layout.fillWidth: true
                text: "Tip: If you see missing Qt DLL / platform plugin errors, use scripts\\\\run.cmd (auto-deploy) or scripts\\\\deploy.cmd (one-time deploy)."
                opacity: 0.7
                wrapMode: Text.Wrap
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}

