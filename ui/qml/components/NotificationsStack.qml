import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var notifications

    implicitHeight: list.implicitHeight
    visible: list.count > 0

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        height: implicitHeight
        implicitHeight: Math.min(contentHeight, 170)
        clip: true
        spacing: theme.spacing
        model: root.notifications

        delegate: Rectangle {
            width: ListView.view.width
            radius: theme.cornerRadius
            color: Qt.rgba(1, 1, 1, 0.96)
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitHeight: content.implicitHeight + 12

            ColumnLayout {
                id: content
                anchors.fill: parent
                anchors.margins: theme.spacing
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: message
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: theme.spacing

                    Button {
                        visible: actionText && actionText.length > 0
                        text: actionText
                        onClicked: {
                            if (actionCommand && actionCommand.length > 0) {
                                commands.invoke(actionCommand, actionArgs || {})
                            } else if (actionUrl && actionUrl.length > 0) {
                                Qt.openUrlExternally(actionUrl)
                            }
                            root.notifications.dismiss(notificationId)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ToolButton {
                        text: "\u00D7"
                        onClicked: root.notifications.dismiss(notificationId)
                    }
                }
            }
        }
    }
}
