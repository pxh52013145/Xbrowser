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
        sequence: "Ctrl+Shift+T"
        onActivated: commands.invoke("restore-closed-tab")
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

    Shortcut {
        sequence: "Ctrl+R"
        onActivated: commands.invoke("nav-reload")
    }

    Shortcut {
        sequence: "F5"
        onActivated: commands.invoke("nav-reload")
    }

    Shortcut {
        sequence: "Alt+Left"
        onActivated: commands.invoke("nav-back")
    }

    Shortcut {
        sequence: "Alt+Right"
        onActivated: commands.invoke("nav-forward")
    }

    Shortcut {
        sequence: "Ctrl+Tab"
        onActivated: commands.invoke("next-tab")
    }

    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: commands.invoke("prev-tab")
    }

    Shortcut {
        sequence: "Ctrl+J"
        onActivated: commands.invoke("open-downloads")
    }

    Shortcut {
        sequence: "Ctrl+Alt+Left"
        onActivated: commands.invoke("focus-split-primary")
    }

    Shortcut {
        sequence: "Ctrl+Alt+Right"
        onActivated: commands.invoke("focus-split-secondary")
    }

    Shortcut {
        sequence: "Alt+1"
        onActivated: commands.invoke("switch-workspace", { index: 0 })
    }

    Shortcut {
        sequence: "Alt+2"
        onActivated: commands.invoke("switch-workspace", { index: 1 })
    }

    Shortcut {
        sequence: "Alt+3"
        onActivated: commands.invoke("switch-workspace", { index: 2 })
    }

    Shortcut {
        sequence: "Alt+4"
        onActivated: commands.invoke("switch-workspace", { index: 3 })
    }

    Shortcut {
        sequence: "Alt+5"
        onActivated: commands.invoke("switch-workspace", { index: 4 })
    }

    Shortcut {
        sequence: "Alt+6"
        onActivated: commands.invoke("switch-workspace", { index: 5 })
    }

    Shortcut {
        sequence: "Alt+7"
        onActivated: commands.invoke("switch-workspace", { index: 6 })
    }

    Shortcut {
        sequence: "Alt+8"
        onActivated: commands.invoke("switch-workspace", { index: 7 })
    }

    Shortcut {
        sequence: "Alt+9"
        onActivated: commands.invoke("switch-workspace", { index: 8 })
    }
}
