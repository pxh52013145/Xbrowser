import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int cornerRadius: 10
    property int spacing: 8
    property int maxHeight: 0
    property var items: []

    signal actionTriggered(string action, var args)

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 280
    readonly property real contentHeight: menuColumn.implicitHeight + spacing * 2
    implicitHeight: maxHeight > 0 ? Math.min(contentHeight, maxHeight) : contentHeight

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: root.spacing
        clip: true
        contentWidth: width
        contentHeight: menuColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: menuColumn
            width: flick.width
            spacing: 2

            Repeater {
                model: root.items || []

                delegate: Item {
                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: modelData && modelData.separator ? 9 : 34

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: 1
                        visible: !!(modelData && modelData.separator)
                        color: Qt.rgba(0, 0, 0, 0.08)
                    }

                    ItemDelegate {
                        anchors.fill: parent
                        visible: !(modelData && modelData.separator)
                        enabled: modelData ? (modelData.enabled !== false) : false
                        hoverEnabled: visible

                        background: Rectangle {
                            radius: 6
                            color: (parent.hovered || parent.highlighted) ? Qt.rgba(0, 0, 0, 0.06) : "transparent"
                        }

                        onClicked: {
                            if (modelData && modelData.action) {
                                const args = modelData.args !== undefined ? modelData.args : ({})
                                root.actionTriggered(String(modelData.action), args)
                            }
                        }

                        contentItem: RowLayout {
                            anchors.fill: parent
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: modelData ? (modelData.text || "") : ""
                                elide: Text.ElideRight
                            }

                            Text {
                                text: modelData ? (modelData.shortcut || "") : ""
                                visible: !!(modelData && modelData.shortcut && modelData.shortcut.length > 0)
                                opacity: 0.6
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
