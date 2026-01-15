import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var view

    implicitHeight: layout.implicitHeight + theme.spacing * 2
    visible: root.view && root.view.documentPlayingAudio

    Rectangle {
        anchors.fill: parent
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.92)
        border.color: Qt.rgba(0, 0, 0, 0.08)
        border.width: 1
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: Math.max(6, Math.round(theme.spacing * 0.75))

        Label { text: "Media"; font.bold: true; elide: Text.ElideRight }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            ToolButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.view && root.view.muted ? "Unmute" : "Mute"
                onClicked: root.view.muted = !root.view.muted
            }

            ToolButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
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
}
