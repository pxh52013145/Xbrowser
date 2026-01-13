import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Welcome to XBrowser"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    required property var settings

    implicitWidth: 560
    implicitHeight: 420

    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            Layout.fillWidth: true
            text: "Vertical tabs, workspaces, split view, and a Zen-inspired UI."
            wrapMode: Text.Wrap
        }

        GroupBox {
            Layout.fillWidth: true
            title: "Shortcuts"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Label { text: "Ctrl+L  Focus address bar" }
                Label { text: "Ctrl+T  New tab" }
                Label { text: "Ctrl+W  Close tab" }
                Label { text: "Ctrl+B  Toggle sidebar" }
                Label { text: "Ctrl+Shift+L  Toggle address bar" }
                Label { text: "Ctrl+E  Toggle split view" }
                Label { text: "Ctrl+Shift+C  Toggle compact mode" }
                Label { text: "F12  DevTools" }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Tip: type \">\" in the address bar for actions."
            opacity: 0.8
            wrapMode: Text.Wrap
        }
    }

    onAccepted: root.settings.onboardingSeen = true
    onRejected: root.settings.onboardingSeen = true
}

