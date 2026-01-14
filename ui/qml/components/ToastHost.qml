import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: root
    anchors.fill: parent
    z: 1000

    readonly property real shown: toast.visible ? 1.0 : 0.0

    function toastX() {
        if (!root.window) {
            return 0
        }
        return Math.round(root.window.x + (root.window.width - toastWindow.width) * 0.5)
    }

    function toastY() {
        if (!root.window) {
            return 0
        }
        const base = root.window.y + root.window.height - toastWindow.height - theme.spacing * 2
        return Math.round(base + (1.0 - root.shown) * 12)
    }

    Window {
        id: toastWindow
        visible: opacity > 0.01
        opacity: root.shown
        flags: Qt.ToolTip | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
        color: "transparent"
        transientParent: root.window ? root.window : null

        x: root.toastX()
        y: root.toastY()
        width: Math.max(1, toastBox.implicitWidth)
        height: Math.max(1, toastBox.implicitHeight)

        Behavior on opacity {
            NumberAnimation { duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs }
        }

        Behavior on y {
            NumberAnimation { duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs }
        }

        Rectangle {
            id: toastBox
            anchors.fill: parent
            radius: theme.cornerRadius
            color: Qt.rgba(0.12, 0.12, 0.12, 0.92)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1

            implicitWidth: Math.min((root.window ? root.window.width : 800) - 24, row.implicitWidth + 24)
            implicitHeight: row.implicitHeight + 16

            RowLayout {
                id: row
                anchors.fill: parent
                anchors.margins: theme.spacing
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: toast.message
                    wrapMode: Text.Wrap
                    color: "white"
                    elide: Text.ElideRight
                }

                Button {
                    visible: toast.actionLabel.length > 0
                    text: toast.actionLabel
                    onClicked: toast.activateAction()
                }

                ToolButton {
                    text: "×"
                    onClicked: toast.dismiss()
                }
            }
        }
    }
}
