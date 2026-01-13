import QtQuick

Item {
    id: root
    anchors.fill: parent
    z: 900

    readonly property bool active: overlayLoader.sourceComponent !== null

    function show(component) {
        overlayLoader.sourceComponent = component
    }

    function hide() {
        overlayLoader.sourceComponent = null
    }

    signal closed()

    visible: active

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)

        MouseArea {
            anchors.fill: parent
            onClicked: {
                root.hide()
                root.closed()
            }
        }
    }

    Loader {
        id: overlayLoader
        anchors.fill: parent
    }
}

