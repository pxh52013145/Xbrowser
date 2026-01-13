import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent
    z: 1000

    visible: toast.visible

    Rectangle {
        id: toastBox
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: theme.spacing * 2
        radius: theme.cornerRadius
        color: Qt.rgba(0.12, 0.12, 0.12, 0.92)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        implicitWidth: Math.min(parent.width - 24, row.implicitWidth + 24)
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

    Behavior on visible {
        NumberAnimation { duration: 120 }
    }
}
