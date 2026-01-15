import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var view: null
    property string query: ""

    signal closeRequested()

    implicitWidth: 420
    implicitHeight: layout.implicitHeight + theme.spacing * 2

    radius: theme.cornerRadius
    color: Qt.rgba(1, 1, 1, 0.98)
    border.color: Qt.rgba(0, 0, 0, 0.12)
    border.width: 1

    function focusQuery() {
        Qt.callLater(() => field.forceActiveFocus())
    }

    function find(backwards) {
        const q = (root.query || "").trim()
        if (!root.view || !root.view.executeScript || q.length === 0) {
            return
        }
        const js = `(function(){ try { return window.find(${JSON.stringify(q)}, false, ${backwards ? "true" : "false"}, true, false, true, false); } catch (e) { return false; } })();`
        root.view.executeScript(js)
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: Math.max(6, Math.round(theme.spacing * 0.75))

        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: "Find in page"
            text: root.query
            selectByMouse: true
            onTextChanged: root.query = text

            onAccepted: root.find(false)

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.find((event.modifiers & Qt.ShiftModifier) !== 0)
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Escape) {
                    root.closeRequested()
                    event.accepted = true
                    return
                }
            }
        }

        ToolButton {
            text: "Prev"
            enabled: (root.query || "").trim().length > 0
            onClicked: root.find(true)
        }

        ToolButton {
            text: "Next"
            enabled: (root.query || "").trim().length > 0
            onClicked: root.find(false)
        }

        ToolButton {
            text: "×"
            onClicked: root.closeRequested()
        }
    }
}
