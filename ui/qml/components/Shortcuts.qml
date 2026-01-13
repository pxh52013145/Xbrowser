import QtQuick
import QtQuick.Controls

Item {
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: commands.invoke("focus-address")
    }

    Shortcut {
        sequence: "Ctrl+T"
        onActivated: commands.invoke("new-tab")
    }

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: commands.invoke("close-tab")
    }

    Shortcut {
        sequence: "Ctrl+B"
        onActivated: commands.invoke("toggle-sidebar")
    }

    Shortcut {
        sequence: "Ctrl+Shift+L"
        onActivated: commands.invoke("toggle-addressbar")
    }

    Shortcut {
        sequence: "F12"
        onActivated: commands.invoke("open-devtools")
    }

    Shortcut {
        sequence: "Ctrl+E"
        onActivated: commands.invoke("toggle-split-view")
    }

    Shortcut {
        sequence: "Ctrl+Shift+C"
        onActivated: commands.invoke("toggle-compact-mode")
    }
}
