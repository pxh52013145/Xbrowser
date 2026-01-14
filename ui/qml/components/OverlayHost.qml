import QtQuick
import QtQuick.Window

Item {
    id: root
    anchors.fill: parent
    z: 900

    signal closed()

    readonly property bool active: overlayLoader.sourceComponent !== null

    function show(component) {
        if (!component) {
            return
        }

        overlayLoader.sourceComponent = component
        overlayWindow.requestActivate()
        focusRoot.forceActiveFocus()
    }

    function hide() {
        if (!active) {
            return
        }

        overlayLoader.sourceComponent = null
        if (root.window) {
            root.window.requestActivate()
        }
        closed()
    }

    Window {
        id: overlayWindow
        visible: opacity > 0.01
        opacity: root.active ? 1.0 : 0.0
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "transparent"
        transientParent: root.window ? root.window : null

        x: root.window ? root.window.x : 0
        y: root.window ? root.window.y : 0
        width: root.window ? Math.max(1, root.window.width) : 1
        height: root.window ? Math.max(1, root.window.height) : 1

        Behavior on opacity {
            NumberAnimation { duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs }
        }

        FocusScope {
            id: focusRoot
            anchors.fill: parent

            Keys.onEscapePressed: {
                root.hide()
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.35)

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.hide()
                }
            }

            Loader {
                id: overlayLoader
                anchors.fill: parent
            }
        }
    }
}
