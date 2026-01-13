import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var view

    implicitHeight: row.implicitHeight + theme.spacing * 2
    visible: root.view && root.view.documentPlayingAudio

    Rectangle {
        anchors.fill: parent
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.92)
        border.color: Qt.rgba(0, 0, 0, 0.08)
        border.width: 1
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: theme.spacing

        Label {
            Layout.fillWidth: true
            text: "Media"
            font.bold: true
            elide: Text.ElideRight
        }

        ToolButton {
            text: root.view && root.view.muted ? "Unmute" : "Mute"
            onClicked: root.view.muted = !root.view.muted
        }

        ToolButton {
            text: "Play/Pause"
            onClicked: {
                const toggleScript = `
(() => {
  const el = document.querySelector('video, audio');
  if (!el) return false;
  if (el.paused) { el.play(); return true; }
  el.pause(); return true;
})();`
                root.view.executeScript(toggleScript)
                toast.showToast("Sent media toggle")
            }
        }
    }
}
