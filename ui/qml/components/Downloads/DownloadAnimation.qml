import QtQuick
import QtQuick.Controls

Item {
    id: root

    implicitWidth: 18
    implicitHeight: 18

    required property var downloads
    required property bool reduceMotion

    readonly property int activeCount: root.downloads ? Math.max(0, Number(root.downloads.activeCount || 0)) : 0
    property int lastActiveCount: 0

    Component.onCompleted: root.lastActiveCount = root.activeCount

    onActiveCountChanged: {
        const prev = root.lastActiveCount
        root.lastActiveCount = root.activeCount
        if (root.reduceMotion) {
            return
        }
        if (root.activeCount > prev) {
            startPulse.restart()
        } else if (root.activeCount < prev) {
            finishPulse.restart()
        }
    }

    Rectangle {
        id: badge
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: Math.round(width / 2)
        color: theme.accentColor
        opacity: 0.95
        visible: root.activeCount > 0
    }

    Text {
        anchors.centerIn: parent
        visible: root.activeCount > 0
        text: root.activeCount > 99 ? "99+" : String(root.activeCount)
        color: "#ffffff"
        font.pixelSize: 10
        font.bold: true
    }

    Rectangle {
        id: ring
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: Math.round(width / 2)
        color: "transparent"
        border.width: 2
        border.color: theme.accentColor
        opacity: 0.0
        scale: 0.8
    }

    ParallelAnimation {
        id: startPulse
        running: false

        ScriptAction {
            script: {
                ring.border.color = theme.accentColor
                ring.opacity = 0.75
                ring.scale = 0.75
            }
        }

        PropertyAnimation {
            target: ring
            property: "opacity"
            to: 0.0
            duration: root.reduceMotion ? 0 : 240
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: ring
            property: "scale"
            to: 1.6
            duration: root.reduceMotion ? 0 : 240
            easing.type: Easing.OutCubic
        }
    }

    ParallelAnimation {
        id: finishPulse
        running: false

        ScriptAction {
            script: {
                ring.border.color = "#2e7d32"
                ring.opacity = 0.75
                ring.scale = 0.75
            }
        }

        PropertyAnimation {
            target: ring
            property: "opacity"
            to: 0.0
            duration: root.reduceMotion ? 0 : 260
            easing.type: Easing.OutCubic
        }

        PropertyAnimation {
            target: ring
            property: "scale"
            to: 1.7
            duration: root.reduceMotion ? 0 : 260
            easing.type: Easing.OutCubic
        }
    }
}

