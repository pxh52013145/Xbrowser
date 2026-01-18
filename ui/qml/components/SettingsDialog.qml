import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Item {
    id: root
    anchors.fill: parent

    required property var settings
    required property var themes
    required property var bookmarks

    signal closeRequested()

    property int currentIndex: 0
    property string pendingImportPath: ""

    function dialogPath(url) {
        if (!url) {
            return ""
        }
        const s = url.toString()
        if (!s || s.length === 0) {
            return ""
        }
        return s.startsWith("file:") ? url.toLocalFile() : s
    }

    function doExportBookmarks(path) {
        if (!root.bookmarks) {
            toast.showToast("Bookmarks not available")
            return
        }
        const ok = root.bookmarks.exportToHtml(path)
        if (ok) {
            toast.showToast("Exported bookmarks")
            return
        }
        const err = root.bookmarks.lastError || ""
        toast.showToast(err.length > 0 ? ("Export failed: " + err) : "Export failed")
    }

    function doImportBookmarks(path) {
        if (!root.bookmarks) {
            toast.showToast("Bookmarks not available")
            return
        }
        const ok = root.bookmarks.importFromHtml(path)
        if (ok) {
            toast.showToast("Imported bookmarks")
            return
        }
        const err = root.bookmarks.lastError || ""
        toast.showToast(err.length > 0 ? ("Import failed: " + err) : "Import failed")
    }

    Platform.FileDialog {
        id: importBookmarksDialog
        title: "Import Bookmarks"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["Bookmarks HTML (*.html *.htm)", "All files (*)"]
        onAccepted: {
            root.pendingImportPath = root.dialogPath(importBookmarksDialog.file)
            if (root.pendingImportPath.length > 0) {
                importBookmarksConfirm.open()
            }
        }
    }

    Dialog {
        id: importBookmarksConfirm
        modal: true
        title: "Import bookmarks"
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: root.doImportBookmarks(root.pendingImportPath)

        contentItem: ColumnLayout {
            width: 420
            spacing: theme.spacing

            Label {
                Layout.fillWidth: true
                text: "Importing will replace your current bookmarks."
                wrapMode: Text.Wrap
                opacity: 0.85
            }
        }
    }

    Platform.FileDialog {
        id: exportBookmarksDialog
        title: "Export Bookmarks"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: ["Bookmarks HTML (*.html)", "All files (*)"]
        onAccepted: {
            const path = root.dialogPath(exportBookmarksDialog.file)
            if (path.length > 0) {
                root.doExportBookmarks(path)
            }
        }
    }

    readonly property var sections: [
        { title: "Appearance" },
        { title: "Running" },
        { title: "Privacy" },
        { title: "Bookmarks" },
        { title: "Shortcuts" },
        { title: "About" },
    ]

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(900, parent.width - 80)
        height: Math.min(620, parent.height - 80)
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.98)
        border.color: Qt.rgba(0, 0, 0, 0.12)
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: theme.spacing

            Frame {
                Layout.preferredWidth: 220
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.spacing

                        Label {
                            Layout.fillWidth: true
                            text: "Settings"
                            font.bold: true
                            font.pixelSize: 14
                        }

                        ToolButton {
                            text: "×"
                            onClicked: root.closeRequested()
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(0, 0, 0, 0.08)
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.sections
                        currentIndex: root.currentIndex

                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            text: modelData.title
                            highlighted: ListView.isCurrentItem
                            onClicked: root.currentIndex = index
                        }
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackLayout {
                    id: pages
                    anchors.fill: parent
                    currentIndex: root.currentIndex

                    // Appearance
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: appearanceColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: appearanceColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Appearance"; font.bold: true; font.pixelSize: 14 }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                Label { Layout.fillWidth: true; text: "Theme"; opacity: 0.85 }

                                Label {
                                    text: root.settings ? root.settings.themeId : ""
                                    opacity: 0.65
                                    elide: Text.ElideRight
                                }

                                Button {
                                    text: "Manage…"
                                    onClicked: commands.invoke("open-themes")
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Reduce motion"
                                    checked: root.settings ? root.settings.reduceMotion : false
                                    onToggled: if (root.settings) root.settings.reduceMotion = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Compact mode"
                                    checked: root.settings ? root.settings.compactMode : false
                                    onToggled: if (root.settings) root.settings.compactMode = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                Label {
                                    Layout.fillWidth: true
                                    text: "Default zoom"
                                    opacity: 0.85
                                }

                                SpinBox {
                                    from: 25
                                    to: 500
                                    stepSize: 5
                                    editable: true
                                    value: root.settings ? Math.round((root.settings.defaultZoom || 1.0) * 100) : 100
                                    onValueModified: if (root.settings) root.settings.defaultZoom = value / 100.0
                                }

                                Label { text: "%"; opacity: 0.75 }

                                Button {
                                    text: "Reset"
                                    onClicked: if (root.settings) root.settings.defaultZoom = 1.0
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Remember zoom per site"
                                    checked: root.settings ? root.settings.rememberZoomPerSite : false
                                    onToggled: if (root.settings) root.settings.rememberZoomPerSite = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Show address bar"
                                    checked: root.settings ? root.settings.addressBarVisible : true
                                    onToggled: if (root.settings) root.settings.addressBarVisible = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Show menu bar"
                                    checked: root.settings ? root.settings.showMenuBar : false
                                    onToggled: if (root.settings) root.settings.showMenuBar = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Sidebar on right"
                                    checked: root.settings ? root.settings.sidebarOnRight : false
                                    onToggled: if (root.settings) root.settings.sidebarOnRight = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Use single toolbar (omnibox in sidebar)"
                                    checked: root.settings ? root.settings.useSingleToolbar : false
                                    onToggled: if (root.settings) root.settings.useSingleToolbar = checked
                                }
                            }
                        }
                    }

                    // Running
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: runningColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: runningColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Running"; font.bold: true; font.pixelSize: 14 }

                            Label {
                                Layout.fillWidth: true
                                text: "Preferred launchers:\n- scripts\\\\dev.cmd (build + run)\n- scripts\\\\run.cmd (run existing build)\n- scripts\\\\deploy.cmd (one-time deploy for double-clickable exe)"
                                wrapMode: Text.Wrap
                                opacity: 0.8
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Essentials: Close resets (instead of closing)"
                                    checked: root.settings ? root.settings.essentialCloseResets : false
                                    onToggled: if (root.settings) root.settings.essentialCloseResets = checked
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                CheckBox {
                                    Layout.fillWidth: true
                                    text: "Back closes tab when no history"
                                    checked: root.settings ? root.settings.closeTabOnBackNoHistory : true
                                    onToggled: if (root.settings) root.settings.closeTabOnBackNoHistory = checked
                                }
                            }
                        }
                    }

                    // Privacy
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: privacyColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: privacyColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Privacy"; font.bold: true; font.pixelSize: 14 }

                            Label {
                                Layout.fillWidth: true
                                text: "Clear browsing data for the selected time range."
                                wrapMode: Text.Wrap
                                opacity: 0.8
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                Button {
                                    text: "Clear browsing dataâ€¦"
                                    onClicked: commands.invoke("open-clear-data")
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    // Bookmarks
                    Flickable {
                        clip: true
                        contentWidth: width
                        contentHeight: bookmarksColumn.implicitHeight
                        boundsBehavior: Flickable.StopAtBounds
                        flickableDirection: Flickable.VerticalFlick
                        interactive: contentHeight > height

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        ColumnLayout {
                            id: bookmarksColumn
                            width: parent.width
                            spacing: theme.spacing

                            Label { text: "Bookmarks"; font.bold: true; font.pixelSize: 14 }

                            Label {
                                Layout.fillWidth: true
                                text: "Export or import bookmarks using the standard Netscape HTML format."
                                wrapMode: Text.Wrap
                                opacity: 0.8
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: theme.spacing

                                Button {
                                    text: "Import"
                                    enabled: !!root.bookmarks
                                    onClicked: importBookmarksDialog.open()
                                }

                                Button {
                                    text: "Export"
                                    enabled: !!root.bookmarks
                                    onClicked: exportBookmarksDialog.open()
                                }

                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    // Shortcuts
                    Item {
                        anchors.fill: parent

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: theme.spacing

                            Label { text: "Shortcuts"; font.bold: true; font.pixelSize: 14 }

                            ShortcutsEditor {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                store: shortcutStore
                            }
                        }
                    }

                    // About
                    ColumnLayout {
                        spacing: theme.spacing

                        Label { text: "About"; font.bold: true; font.pixelSize: 14 }

                        Label {
                            Layout.fillWidth: true
                            text: "XBrowser " + Qt.application.version
                            opacity: 0.85
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Qt " + Qt.version
                            opacity: 0.75
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: theme.spacing

                            Button {
                                text: "Welcome"
                                onClicked: commands.invoke("open-welcome")
                            }

                            Button {
                                text: "Diagnostics…"
                                onClicked: commands.invoke("open-diagnostics")
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }

    Keys.onEscapePressed: root.closeRequested()
}
