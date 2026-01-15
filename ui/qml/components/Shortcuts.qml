import QtQuick
import QtQuick.Controls
import QtQml

Item {
    Instantiator {
        model: shortcutStore

        delegate: Shortcut {
            sequence: keySequence
            enabled: keySequence && keySequence.length > 0
            context: Qt.ApplicationShortcut
            onActivated: commands.invoke(command, args)
        }
    }
}

