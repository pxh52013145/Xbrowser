import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0
import "components"

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    title: (browser.tabs.activeIndex >= 0 ? browser.tabs.titleAt(browser.tabs.activeIndex) : "XBrowser")
    flags: Qt.Window | Qt.FramelessWindowHint

    property url glanceUrl: ""
    property bool compactTopHover: false
    property bool compactSidebarHover: false
    property bool topBarHovered: false
    property bool sidebarHovered: false

    readonly property int windowControlButtonWidth: 46
    readonly property int windowControlButtonHeight: 32

    readonly property bool showTopBar: browser.settings.addressBarVisible
                                        && (!browser.settings.compactMode || topBarHovered || compactTopHover
                                            || addressField.activeFocus || omniboxPopup.opened)
    readonly property bool showSidebar: browser.settings.sidebarExpanded
                                        && (!browser.settings.compactMode || sidebarHovered || compactSidebarHover)

    readonly property int uiRadius: theme.cornerRadius
    readonly property int uiSpacing: theme.spacing

    readonly property var mediaView: (splitView.enabled && webSecondary.documentPlayingAudio) ? webSecondary : web

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: theme.backgroundFrom }
            GradientStop { position: 1.0; color: theme.backgroundTo }
        }
    }

    WindowChromeController {
        id: windowChrome
    }

    menuBar: MenuBar {
        visible: browser.settings.showMenuBar

        Menu {
            title: "File"
            MenuItem { text: "New Tab"; onTriggered: commands.invoke("new-tab") }
            MenuItem { text: "Close Tab"; onTriggered: commands.invoke("close-tab") }
            MenuSeparator { }
            MenuItem { text: "Share URL"; onTriggered: commands.invoke("share-url") }
            MenuSeparator { }
            MenuItem { text: "Quit"; onTriggered: Qt.quit() }
        }

        Menu {
            title: "View"
            MenuItem { text: "Toggle Sidebar"; onTriggered: commands.invoke("toggle-sidebar") }
            MenuItem { text: "Toggle Address Bar"; onTriggered: commands.invoke("toggle-addressbar") }
            MenuItem { text: "Toggle Compact Mode"; onTriggered: commands.invoke("toggle-compact-mode") }
            MenuItem {
                text: "Back closes tab"
                checkable: true
                checked: browser.settings.closeTabOnBackNoHistory
                onTriggered: commands.invoke("toggle-back-close")
            }
            MenuItem { text: "Toggle Menu Bar"; onTriggered: commands.invoke("toggle-menubar") }
            MenuSeparator { }
            MenuItem { text: splitView.enabled ? "Unsplit View" : "Split View"; onTriggered: commands.invoke("toggle-split-view") }
        }

        Menu {
            title: "Tools"
            MenuItem { text: "Themes"; onTriggered: themesDialog.open() }
            MenuItem { text: "Downloads"; onTriggered: downloadsDialog.open() }
            MenuItem { text: "DevTools"; onTriggered: commands.invoke("open-devtools") }
        }

        Menu {
            title: "Help"
            MenuItem { text: "Welcome"; onTriggered: onboardingDialog.open() }
            MenuItem { text: "About"; onTriggered: toast.showToast("XBrowser " + Qt.application.version) }
        }
    }

    function tabIdForActiveIndex() {
        const idx = browser.tabs.activeIndex
        return idx >= 0 ? browser.tabs.tabIdAt(idx) : 0
    }

    function urlForTabId(tabId) {
        const idx = browser.tabs.indexOfTabId(tabId)
        return idx >= 0 ? browser.tabs.urlAt(idx) : null
    }

    function navigateViewToTabId(view, tabId) {
        if (!view || tabId <= 0) {
            return
        }
        const url = urlForTabId(tabId)
        if (url && url.toString().length > 0) {
            view.navigate(url)
        }
    }

    function updateOmnibox() {
        if (!addressField.activeFocus) {
            return
        }

        const raw = addressField.text || ""
        if (!raw.startsWith(">")) {
            omniboxPopup.close()
            return
        }

        const query = raw.slice(1).trim().toLowerCase()
        const actions = [
            { title: "New Tab", command: "new-tab", args: {} },
            { title: "Close Tab", command: "close-tab", args: {} },
            { title: "Toggle Sidebar", command: "toggle-sidebar", args: {} },
            { title: "Toggle Address Bar", command: "toggle-addressbar", args: {} },
            { title: splitView.enabled ? "Unsplit View" : "Split View", command: "toggle-split-view", args: {} },
            { title: "Copy URL", command: "copy-url", args: {} },
            { title: "New Workspace", command: "new-workspace", args: {} },
        ]

        for (let i = 0; i < browser.workspaces.count(); i++) {
            actions.push({
                title: "Switch Workspace: " + browser.workspaces.nameAt(i),
                command: "switch-workspace",
                args: { index: i },
            })
        }

        omniboxModel.clear()
        for (const a of actions) {
            if (!query || a.title.toLowerCase().includes(query)) {
                omniboxModel.append(a)
            }
        }

        if (omniboxModel.count > 0) {
            omniboxPopup.open()
        } else {
            omniboxPopup.close()
        }
    }

    function insertEmojiInto(field, emoji) {
        if (!field || !emoji || emoji.length === 0) {
            return
        }
        const t = field.text || ""
        const pos = field.cursorPosition || 0
        field.text = t.slice(0, pos) + emoji + t.slice(pos)
        field.cursorPosition = pos + emoji.length
        field.forceActiveFocus()
    }

    Component {
        id: mainMenuComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: root.uiRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 220
            implicitHeight: menuColumn.implicitHeight + 16

            ColumnLayout {
                id: menuColumn
                anchors.fill: parent
                anchors.margins: root.uiSpacing
                spacing: root.uiSpacing

                Button {
                    text: browser.settings.sidebarExpanded ? "Collapse Sidebar" : "Expand Sidebar"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-sidebar")
                    }
                }

                Button {
                    text: browser.settings.addressBarVisible ? "Hide Address Bar" : "Show Address Bar"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-addressbar")
                    }
                }

                Button {
                    text: browser.settings.essentialCloseResets ? "Essentials: Close Resets" : "Essentials: Close Closes"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-essentials-close-resets")
                    }
                }

                Button {
                    text: "DevTools"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-devtools")
                    }
                }

                Button {
                    text: splitView.enabled ? "Unsplit View" : "Split View"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-split-view")
                    }
                }

                Button {
                    text: browser.settings.compactMode ? "Disable Compact Mode" : "Enable Compact Mode"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-compact-mode")
                    }
                }

                Button {
                    text: browser.settings.showMenuBar ? "Hide Menu Bar" : "Show Menu Bar"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-menubar")
                    }
                }

                Button {
                    text: browser.settings.closeTabOnBackNoHistory ? "Back: Close Tab" : "Back: Show Toast"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-back-close")
                    }
                }

                Button {
                    text: "Themes"
                    onClicked: {
                        popupManager.close()
                        themesDialog.open()
                    }
                }

                Button {
                    text: "Downloads"
                    onClicked: {
                        popupManager.close()
                        downloadsDialog.open()
                    }
                }

                Button {
                    text: "Welcome"
                    onClicked: {
                        popupManager.close()
                        onboardingDialog.open()
                    }
                }
            }
        }
    }

    Component {
        id: sitePanelComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: root.uiRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 260
            implicitHeight: panelColumn.implicitHeight + 16

            ColumnLayout {
                id: panelColumn
                anchors.fill: parent
                anchors.margins: root.uiSpacing
                spacing: root.uiSpacing

                Label {
                    text: web.currentUrl.host.length > 0 ? web.currentUrl.host : web.currentUrl.toString()
                    elide: Text.ElideRight
                    font.bold: true
                }

                Button {
                    text: "Copy URL"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("copy-url")
                    }
                }

                Button {
                    text: "Share URL"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("share-url")
                    }
                }

                Button {
                    text: "Permissions"
                    onClicked: {
                        popupManager.close()
                        toast.showToast("Permissions panel not implemented")
                    }
                }

                Button {
                    text: "DevTools"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-devtools")
                    }
                }
            }
        }
    }

    Component {
        id: glanceOverlayComponent

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(1, 1, 1, 0)

            Rectangle {
                id: glanceCard
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 900)
                height: Math.min(parent.height - 80, 620)
                radius: root.uiRadius
                color: Qt.rgba(1, 1, 1, 1)
                border.color: Qt.rgba(0, 0, 0, 0.12)
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.uiSpacing
                    spacing: root.uiSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: root.uiSpacing

                        Label {
                            Layout.fillWidth: true
                            text: root.glanceUrl.toString()
                            elide: Text.ElideRight
                            font.bold: true
                        }

                        Button {
                            text: "Open Tab"
                            onClicked: {
                                commands.invoke("new-tab", { url: root.glanceUrl.toString() })
                                overlayHost.hide()
                            }
                        }

                        Button {
                            text: "Open Split"
                            onClicked: {
                                const idx = browser.newTab(root.glanceUrl)
                                if (idx >= 0) {
                                    splitView.enabled = true
                                    splitView.secondaryTabId = browser.tabs.tabIdAt(idx)
                                }
                                overlayHost.hide()
                            }
                        }

                        ToolButton {
                            text: "\u00D7"
                            onClicked: overlayHost.hide()
                        }
                    }

                    WebView2View {
                        id: glanceWeb
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Component.onCompleted: navigate(root.glanceUrl)
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        windowChrome.attach(root)
        windowChrome.setCaptionItem(topBar)
        windowChrome.setCaptionExcludeItems([
            sidebarButton,
            backButton,
            forwardButton,
            reloadButton,
            sitePanelButton,
            addressField,
            emojiButton,
            newTabButton,
            mainMenuButton
        ])
        windowChrome.setMinimizeButtonItem(windowMinimizeButton)
        windowChrome.setMaximizeButtonItem(windowMaximizeButton)
        windowChrome.setCloseButtonItem(windowCloseButton)

        if (browser.tabs.count() === 0) {
            commands.invoke("new-tab", { url: "https://example.com" })
        }

        if (!browser.settings.onboardingSeen) {
            onboardingDialog.open()
        }

        const glanceScript = `
(() => {
  if (window.__xbrowserGlanceInstalled) return;
  window.__xbrowserGlanceInstalled = true;
  document.addEventListener('click', (e) => {
    if (!e.altKey) return;
    const t = e.target;
    const a = t && t.closest ? t.closest('a[href]') : null;
    if (!a) return;
    e.preventDefault();
    e.stopPropagation();
    try {
      window.chrome?.webview?.postMessage({ type: 'glance', href: a.href });
    } catch (_) {}
  }, true);
})();
`
        web.addScriptOnDocumentCreated(glanceScript)
        webSecondary.addScriptOnDocumentCreated(glanceScript)
    }

    header: ToolBar {
        id: topBar
        height: showTopBar ? implicitHeight : root.windowControlButtonHeight
        leftPadding: 6
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        HoverHandler {
            onHoveredChanged: root.topBarHovered = hovered
        }

        RowLayout {
            anchors.fill: parent
            spacing: 6

            ToolButton {
                id: sidebarButton
                visible: showTopBar
                text: browser.settings.sidebarExpanded ? "<" : ">"
                onClicked: commands.invoke("toggle-sidebar")
            }

            ToolButton {
                id: backButton
                visible: showTopBar
                text: "←"
                enabled: web.canGoBack
                onClicked: commands.invoke("nav-back")
            }
            ToolButton {
                id: forwardButton
                visible: showTopBar
                text: "→"
                enabled: web.canGoForward
                onClicked: commands.invoke("nav-forward")
            }
            ToolButton {
                id: reloadButton
                visible: showTopBar
                text: web.isLoading ? "⟳…" : "⟳"
                onClicked: commands.invoke("nav-reload")
            }

            ToolButton {
                id: sitePanelButton
                visible: showTopBar
                text: "ⓘ"
                onClicked: popupManager.openAtItem(sitePanelComponent, sitePanelButton)
            }

            TextField {
                id: addressField
                visible: showTopBar
                Layout.fillWidth: true
                Layout.minimumWidth: 240
                Layout.preferredWidth: 640
                Layout.maximumWidth: 800
                placeholderText: "Search or enter address"
                selectByMouse: true
                onTextChanged: updateOmnibox()
                onActiveFocusChanged: {
                    if (!activeFocus) {
                        omniboxPopup.close()
                    }
                }
                onAccepted: {
                    if (omniboxPopup.opened) {
                        omniboxView.activateCurrent()
                        return
                    }
                    web.navigate(text)
                }
            }

            Item {
                id: windowDragRegion
                Layout.fillWidth: true
                Layout.minimumWidth: 80
                Layout.preferredWidth: 200
                Layout.alignment: Qt.AlignVCenter
                height: topBar.height
            }

            ToolButton {
                id: emojiButton
                visible: showTopBar
                text: "😊"
                onClicked: {
                    const pos = emojiButton.mapToItem(root.contentItem, 0, emojiButton.height)
                    emojiPicker.x = Math.max(8, Math.min(root.contentItem.width - emojiPicker.implicitWidth - 8, Math.round(pos.x)))
                    emojiPicker.y = Math.max(8, Math.min(root.contentItem.height - emojiPicker.implicitHeight - 8, Math.round(pos.y)))
                    emojiPicker.open()
                }
            }

            ToolButton {
                id: newTabButton
                visible: showTopBar
                text: "+"
                onClicked: {
                    commands.invoke("new-tab", { url: "about:blank" })
                    commands.invoke("focus-address")
                }
            }

            ToolButton {
                id: mainMenuButton
                text: "⋮"
                onClicked: popupManager.openAtItem(mainMenuComponent, mainMenuButton)
            }

            Row {
                Layout.alignment: Qt.AlignVCenter
                spacing: 0

                Item {
                    id: windowMinimizeButton
                    width: root.windowControlButtonWidth
                    height: topBar.height

                    readonly property bool hovered: windowChrome.hoveredButton === WindowChromeController.Minimize
                    readonly property bool pressed: windowChrome.pressedButton === WindowChromeController.Minimize

                    Rectangle {
                        anchors.fill: parent
                        color: pressed ? Qt.rgba(0, 0, 0, 0.12) : (hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent")
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "–"
                        color: "#1f1f1f"
                        font.pixelSize: 16
                    }
                }

                Item {
                    id: windowMaximizeButton
                    width: root.windowControlButtonWidth
                    height: topBar.height

                    readonly property bool hovered: windowChrome.hoveredButton === WindowChromeController.Maximize
                    readonly property bool pressed: windowChrome.pressedButton === WindowChromeController.Maximize

                    Rectangle {
                        anchors.fill: parent
                        color: pressed ? Qt.rgba(0, 0, 0, 0.12) : (hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent")
                    }
                    Text {
                        anchors.centerIn: parent
                        text: (root.visibility === Window.Maximized) ? "❐" : "□"
                        color: "#1f1f1f"
                        font.pixelSize: 14
                    }
                }

                Item {
                    id: windowCloseButton
                    width: root.windowControlButtonWidth
                    height: topBar.height

                    readonly property bool hovered: windowChrome.hoveredButton === WindowChromeController.Close
                    readonly property bool pressed: windowChrome.pressedButton === WindowChromeController.Close

                    Rectangle {
                        anchors.fill: parent
                        color: pressed ? "#c50f1f" : (hovered ? "#e81123" : "transparent")
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: hovered || pressed ? "#ffffff" : "#1f1f1f"
                        font.pixelSize: 16
                    }
                }
            }
        }
    }

    ListModel {
        id: omniboxModel
    }

    Popup {
        id: omniboxPopup
        parent: root.contentItem
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        x: addressField.mapToItem(root.contentItem, 0, addressField.height).x
        y: addressField.mapToItem(root.contentItem, 0, addressField.height).y
        width: addressField.width

        onClosed: omniboxModel.clear()

        contentItem: ListView {
            id: omniboxView
            implicitHeight: Math.min(contentHeight, 240)
            clip: true
            model: omniboxModel
            currentIndex: 0

            function activateCurrent() {
                if (currentIndex < 0 || currentIndex >= count) {
                    return
                }
                const item = omniboxModel.get(currentIndex)
                if (!item || !item.command) {
                    return
                }
                omniboxPopup.close()
                commands.invoke(item.command, item.args || {})
                addressField.text = ""
                addressField.forceActiveFocus()
            }

            delegate: ItemDelegate {
                width: ListView.view.width
                text: title
                onClicked: {
                    omniboxView.currentIndex = index
                    omniboxView.activateCurrent()
                }
            }
        }
    }

    EmojiPicker {
        id: emojiPicker
        parent: root.contentItem
        onEmojiSelected: (emoji) => {
            insertEmojiInto(addressField, emoji)
            emojiPicker.close()
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 6
        hoverEnabled: true
        visible: browser.settings.compactMode && browser.settings.addressBarVisible
        z: 2000
        onEntered: root.compactTopHover = true
        onExited: root.compactTopHover = false
    }

    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        hoverEnabled: true
        visible: browser.settings.compactMode && browser.settings.sidebarExpanded
        z: 2000
        onEntered: root.compactSidebarHover = true
        onExited: root.compactSidebarHover = false
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: showSidebar ? browser.settings.sidebarWidth : 0
            Layout.fillHeight: true
            visible: showSidebar
            color: Qt.rgba(0, 0, 0, 0.04)

            HoverHandler {
                onHoveredChanged: root.sidebarHovered = hovered
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                TabFilterModel {
                    id: essentialsTabs
                    sourceTabs: browser.tabs
                    includeEssentials: true
                    includeRegular: false
                }

                TabFilterModel {
                    id: regularTabs
                    sourceTabs: browser.tabs
                    includeEssentials: false
                    includeRegular: true
                    groupId: 0
                }

                Label {
                    text: "Essentials"
                    font.pixelSize: 12
                    opacity: 0.7
                    visible: essentialsView.count > 0
                }

                ListView {
                    id: essentialsView
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 120)
                    clip: true
                    model: essentialsTabs
                    visible: count > 0

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: title
                        highlighted: isActive
                        onClicked: browser.activateTabById(tabId)

                        contentItem: RowLayout {
                            spacing: 6
                            Label {
                                Layout.fillWidth: true
                                text: title
                                elide: Text.ElideRight
                            }
                            ToolButton {
                                text: "\u2605"
                                onClicked: commands.invoke("toggle-essential", { tabId: tabId })
                            }
                            ToolButton {
                                text: "\u00D7"
                                onClicked: commands.invoke("close-tab", { tabId: tabId })
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: "Shortcuts"
                        font.pixelSize: 12
                        opacity: 0.7
                    }

                    ToolButton {
                        text: "+"
                        enabled: web.currentUrl.toString().length > 0
                        onClicked: quickLinks.addLink(web.currentUrl, web.title)
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 100)
                    clip: true
                    model: quickLinks

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: title
                        onClicked: commands.invoke("navigate", { url: url })
                    }
                }

                Label {
                    text: "Tabs"
                    font.pixelSize: 12
                    opacity: 0.7
                }

                Repeater {
                    model: browser.tabGroups

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        property bool searching: false

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            ToolButton {
                                text: collapsed ? "+" : "−"
                                onClicked: browser.tabGroups.setCollapsedAt(index, !collapsed)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: name
                                elide: Text.ElideRight
                                font.bold: true
                            }

                            ToolButton {
                                text: searching ? "\u2715" : "\u2315"
                                onClicked: {
                                    searching = !searching
                                    if (!searching) {
                                        searchField.text = ""
                                    }
                                }
                            }

                            ToolButton {
                                text: "\u00D7"
                                onClicked: browser.deleteTabGroup(groupId)
                            }
                        }

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            visible: searching
                            placeholderText: "Search in group"
                            selectByMouse: true
                        }

                        TabFilterModel {
                            id: groupTabs
                            sourceTabs: browser.tabs
                            includeEssentials: false
                            includeRegular: true
                            groupId: groupId
                            searchText: searchField.text
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(contentHeight, 160)
                            clip: true
                            visible: !collapsed && count > 0
                            model: groupTabs

                            delegate: ItemDelegate {
                                width: ListView.view.width
                                text: title
                                highlighted: isActive

                                Drag.active: tabDrag.active
                                Drag.hotSpot.x: width / 2
                                Drag.hotSpot.y: height / 2
                                Drag.keys: ["tab"]
                                Drag.mimeData: ({ tabId: tabId })
                                Drag.supportedActions: Qt.MoveAction

                                DragHandler {
                                    id: tabDrag
                                    acceptedButtons: Qt.LeftButton
                                }

                                onClicked: browser.activateTabById(tabId)

                                Component {
                                    id: tabContextMenuComponent

                                    Rectangle {
                                        color: Qt.rgba(1, 1, 1, 0.98)
                                        radius: root.uiRadius
                                        border.color: Qt.rgba(0, 0, 0, 0.08)
                                        border.width: 1

                                        implicitWidth: 220
                                        implicitHeight: menuColumn.implicitHeight + 16

                                        ColumnLayout {
                                            id: menuColumn
                                            anchors.fill: parent
                                            anchors.margins: root.uiSpacing
                                            spacing: root.uiSpacing

                                            Button {
                                                text: "Close Tab"
                                                onClicked: {
                                                    popupManager.close()
                                                    commands.invoke("close-tab", { tabId: tabId })
                                                }
                                            }

                                            Button {
                                                text: "Ungroup"
                                                onClicked: {
                                                    popupManager.close()
                                                    browser.ungroupTab(tabId)
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: (mouse) => {
                                        const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                        popupManager.openAtPoint(tabContextMenuComponent, pos.x, pos.y)
                                    }
                                }

                                DropArea {
                                    anchors.fill: parent
                                    keys: ["tab"]
                                    onDropped: (drop) => {
                                        const dragged = Number(drop.mimeData.tabId || 0)
                                        if (dragged > 0 && dragged !== tabId) {
                                            browser.moveTabBefore(dragged, tabId)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: tabList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: regularTabs

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: title
                        highlighted: isActive

                        property bool renaming: false

                        Drag.active: tabDrag.active
                        Drag.hotSpot.x: width / 2
                        Drag.hotSpot.y: height / 2
                        Drag.keys: ["tab"]
                        Drag.mimeData: ({ tabId: tabId })
                        Drag.supportedActions: Qt.MoveAction

                        function finishRename() {
                            if (!renaming) {
                                return
                            }
                            renaming = false
                            browser.setTabCustomTitleById(tabId, renameField.text)
                            toast.showToast("Renamed tab")
                        }

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onDoubleTapped: {
                                renaming = true
                                renameField.forceActiveFocus()
                                renameField.selectAll()
                            }
                        }

                        DragHandler {
                            id: tabDrag
                            acceptedButtons: Qt.LeftButton
                        }

                        onClicked: {
                            browser.activateTabById(tabId)
                        }

                        Component {
                            id: tabContextMenuComponent

                            Rectangle {
                                color: Qt.rgba(1, 1, 1, 0.98)
                                radius: root.uiRadius
                                border.color: Qt.rgba(0, 0, 0, 0.08)
                                border.width: 1

                                implicitWidth: 220
                                implicitHeight: menuColumn.implicitHeight + 16

                                ColumnLayout {
                                    id: menuColumn
                                    anchors.fill: parent
                                    anchors.margins: root.uiSpacing
                                    spacing: root.uiSpacing

                                    Button {
                                        text: "Close Tab"
                                        onClicked: {
                                            popupManager.close()
                                            commands.invoke("close-tab", { tabId: tabId })
                                        }
                                    }

                                    Button {
                                        text: isEssential ? "Unpin from Essentials" : "Pin to Essentials"
                                        onClicked: {
                                            popupManager.close()
                                            commands.invoke("toggle-essential", { tabId: tabId })
                                        }
                                    }

                                    Button {
                                        text: "New Group"
                                        onClicked: {
                                            popupManager.close()
                                            browser.createTabGroupForTab(tabId)
                                        }
                                    }

                                    Repeater {
                                        model: browser.tabGroups

                                        delegate: Button {
                                            text: "Move to " + name
                                            onClicked: {
                                                popupManager.close()
                                                browser.moveTabToGroup(tabId, groupId)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: (mouse) => {
                                const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                popupManager.openAtPoint(tabContextMenuComponent, pos.x, pos.y)
                            }
                        }

                        DropArea {
                            anchors.fill: parent
                            keys: ["tab"]
                            onDropped: (drop) => {
                                const dragged = Number(drop.mimeData.tabId || 0)
                                if (dragged > 0 && dragged !== tabId) {
                                    browser.moveTabBefore(dragged, tabId)
                                }
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 6
                            TextField {
                                id: renameField
                                Layout.fillWidth: true
                                visible: renaming
                                text: title
                                selectByMouse: true
                                onAccepted: finishRename()
                                onEditingFinished: finishRename()
                                Keys.onEscapePressed: renaming = false
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !renaming
                                text: title
                                elide: Text.ElideRight
                            }
                            ToolButton {
                                text: isEssential ? "\u2605" : "\u2606"
                                onClicked: commands.invoke("toggle-essential", { tabId: tabId })
                            }
                            ToolButton {
                                text: "×"
                                onClicked: {
                                    commands.invoke("close-tab", { tabId: tabId })
                                }
                            }
                        }
                    }
                }

                NotificationsStack {
                    Layout.fillWidth: true
                    notifications: notifications
                }

                MediaControls {
                    Layout.fillWidth: true
                    view: root.mediaView
                }

                SidebarFooter {
                    Layout.fillWidth: true
                    workspaces: browser.workspaces
                    settings: browser.settings
                    popupHost: popupManager
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            visible: showSidebar
            color: Qt.rgba(0, 0, 0, 0)

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.SplitHCursor
                hoverEnabled: true
                preventStealing: true

                property real startSceneX: 0
                property int startWidth: 0

                onPressed: (mouse) => {
                    startSceneX = mouse.sceneX
                    startWidth = browser.settings.sidebarWidth
                }

                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }
                    browser.settings.sidebarWidth = startWidth + Math.round(mouse.sceneX - startSceneX)
                }
            }
        }

        Item {
            id: contentHost
            Layout.fillWidth: true
            Layout.fillHeight: true

            property int splitPos: Math.round(width * 0.5)
            readonly property int handleWidth: 4

            onWidthChanged: {
                splitPos = Math.max(200, Math.min(width - 200, splitPos))
            }

            WebView2View {
                id: web
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: splitView.enabled ? contentHost.splitPos : contentHost.width
            }

            Rectangle {
                id: splitHandle
                visible: splitView.enabled
                x: contentHost.splitPos
                width: contentHost.handleWidth
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: Qt.rgba(0, 0, 0, 0)

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    hoverEnabled: true
                    preventStealing: true

                    property real startSceneX: 0
                    property int startPos: 0

                    onPressed: (mouse) => {
                        startSceneX = mouse.sceneX
                        startPos = contentHost.splitPos
                    }

                    onPositionChanged: (mouse) => {
                        if (!pressed) {
                            return
                        }
                        const next = startPos + Math.round(mouse.sceneX - startSceneX)
                        contentHost.splitPos = Math.max(200, Math.min(contentHost.width - 200, next))
                    }
                }
            }

            WebView2View {
                id: webSecondary
                visible: splitView.enabled
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: splitHandle.right
                anchors.right: parent.right
            }

            Item {
                id: splitDropzone
                anchors.fill: parent
                z: 20
                visible: dropArea.containsDrag

                property string zone: ""

                DropArea {
                    id: dropArea
                    anchors.fill: parent
                    keys: ["tab"]

                    onPositionChanged: (drag) => {
                        const threshold = 80
                        if (drag.x < threshold) {
                            splitDropzone.zone = "left"
                        } else if (drag.x > width - threshold) {
                            splitDropzone.zone = "right"
                        } else {
                            splitDropzone.zone = ""
                        }
                    }

                    onExited: splitDropzone.zone = ""

                    onDropped: (drop) => {
                        const tabId = Number(drop.mimeData.tabId || 0)
                        if (tabId <= 0) {
                            splitDropzone.zone = ""
                            return
                        }

                        if (splitDropzone.zone === "left") {
                            splitView.enabled = true
                            splitView.primaryTabId = tabId
                        } else if (splitDropzone.zone === "right") {
                            splitView.enabled = true
                            splitView.secondaryTabId = tabId
                        }

                        splitDropzone.zone = ""
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, splitDropzone.zone === "left" ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, splitDropzone.zone === "right" ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }
            }
        }
    }

    OverlayHost {
        id: overlayHost
    }

    PopupManager {
        id: popupManager
    }

    ToastHost { }

    Shortcuts { }

    Connections {
        target: web

        function onTitleChanged() {
            const tabId = splitView.enabled ? splitView.primaryTabId : tabIdForActiveIndex()
            if (tabId > 0) {
                browser.setTabTitleById(tabId, web.title)
            }
        }

        function onCurrentUrlChanged() {
            const tabId = splitView.enabled ? splitView.primaryTabId : tabIdForActiveIndex()
            if (tabId > 0) {
                browser.setTabUrlById(tabId, web.currentUrl)
            }
            if (!addressField.activeFocus) {
                addressField.text = web.currentUrl.toString()
            }
        }

        function onWebMessageReceived(json) {
            let msg = null
            try {
                msg = JSON.parse(json)
            } catch (e) {
                return
            }

            if (msg && msg.type === "glance" && msg.href) {
                root.glanceUrl = msg.href
                overlayHost.show(glanceOverlayComponent)
            }
        }

        function onDownloadStarted(uri, resultFilePath) {
            toast.showToast("Download started")
            downloads.addStarted(uri, resultFilePath)
        }

        function onDownloadFinished(uri, resultFilePath, success) {
            toast.showToast(success ? "Download finished" : "Download failed")
            downloads.markFinished(uri, resultFilePath, success)
        }
    }

    Connections {
        target: webSecondary

        function onTitleChanged() {
            if (splitView.enabled && splitView.secondaryTabId > 0) {
                browser.setTabTitleById(splitView.secondaryTabId, webSecondary.title)
            }
        }

        function onCurrentUrlChanged() {
            if (splitView.enabled && splitView.secondaryTabId > 0) {
                browser.setTabUrlById(splitView.secondaryTabId, webSecondary.currentUrl)
            }
        }
    }

    Connections {
        target: commands

        function onCommandInvoked(id, args) {
            if (id === "focus-address") {
                addressField.forceActiveFocus()
                addressField.selectAll()
                return
            }
            if (id === "nav-back") {
                const tabId = tabIdForActiveIndex()
                if (browser.handleBackRequested(tabId, web.canGoBack)) {
                    return
                }
                if (!web.canGoBack) {
                    toast.showToast("No history")
                    return
                }
                web.goBack()
                return
            }
            if (id === "nav-forward") {
                web.goForward()
                return
            }
            if (id === "nav-reload") {
                web.reload()
                return
            }
            if (id === "navigate") {
                web.navigate(args.url)
                return
            }
            if (id === "open-devtools") {
                web.openDevTools()
                return
            }
            if (id === "theme-update") {
                if (args && args.themeId) {
                    themes.updateTheme(args.themeId)
                }
                return
            }
        }
    }

    ThemesDialog {
        id: themesDialog
        parent: root.contentItem
        themes: themes
        settings: browser.settings
    }

    DownloadsDialog {
        id: downloadsDialog
        parent: root.contentItem
        downloads: downloads
    }

    OnboardingDialog {
        id: onboardingDialog
        parent: root.contentItem
        settings: browser.settings
    }

    function syncSplitViews() {
        if (!splitView.enabled) {
            return
        }

        if (splitView.primaryTabId > 0) {
            browser.activateTabById(splitView.primaryTabId)
            navigateViewToTabId(web, splitView.primaryTabId)
        }

        if (splitView.secondaryTabId > 0) {
            navigateViewToTabId(webSecondary, splitView.secondaryTabId)
        }
    }

    Connections {
        target: splitView
        onEnabledChanged: syncSplitViews()
        onTabsChanged: syncSplitViews()
    }

    Connections {
        target: browser.tabs

        function onActiveIndexChanged() {
            const idx = browser.tabs.activeIndex
            if (idx < 0) {
                return
            }
            const tabId = browser.tabs.tabIdAt(idx)
            if (splitView.enabled && splitView.primaryTabId !== tabId) {
                splitView.primaryTabId = tabId
            }
            navigateViewToTabId(web, tabId)
        }
    }
}
