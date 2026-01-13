import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    signal emojiSelected(string emoji)

    property string filterText: ""

    implicitWidth: 320
    implicitHeight: 260

    readonly property var allEmojis: [
        "😀","😃","😄","😁","😆","😅","😂","🙂","😉","😊","😇","🥰","😍","😘","😋","😜","🤪","🤔","😎",
        "😴","😵","🤯","😱","😡","🤬","🥶","🥵","🤢","🤮","🤧","🤠","🥳","😺","😸","😹","😻",
        "🙈","🙉","🙊","💡","🔥","✨","⭐","🌙","☀️","🌈","❄️","⚡","✅","❌","⚠️","💬","📌","📎",
        "📁","📂","🗂️","📝","📖","🔍","🔒","🔓","🔑","🧩","🧠","🧪","🛠️","⚙️","🖥️","📱","🕹️","🎧",
        "🎵","🎬","📷","📸","🧭","🗺️","🚀","🛰️","🌐","💾","🧾","💳","💰","🧱","🏁","🧲","🧰","🪄"
    ]

    readonly property var filteredEmojis: {
        if (!filterText || filterText.length === 0) {
            return allEmojis
        }
        // Minimal filter: for now just show all (emoji have no text names).
        return allEmojis
    }

    background: Rectangle {
        color: Qt.rgba(1, 1, 1, 0.98)
        radius: theme.cornerRadius
        border.color: Qt.rgba(0, 0, 0, 0.08)
        border.width: 1
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: theme.spacing

        TextField {
            Layout.fillWidth: true
            placeholderText: "Search (coming soon)"
            selectByMouse: true
            text: root.filterText
            onTextChanged: root.filterText = text
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 36
            cellHeight: 36
            model: root.filteredEmojis
            clip: true

            delegate: ItemDelegate {
                width: grid.cellWidth
                height: grid.cellHeight
                text: modelData
                onClicked: root.emojiSelected(modelData)
            }
        }
    }
}
