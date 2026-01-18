import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import QtQuick.Window
import Qt.labs.platform as Platform

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
    readonly property bool fullscreenActive: visibility === Window.FullScreen
    property int preFullscreenVisibility: Window.Windowed
    property int contentFullscreenTabId: 0
    property bool contentFullscreenToggled: false
    property string viewSourceToken: ""
    property int viewSourceTabId: 0
    property var viewSourceTargetView: null

    property var activeDownloadIdByKey: ({})
    property var activeDownloadOpById: ({})
    property string lastFailedDownloadUri: ""

    Timer {
        id: compactTopEnterTimer
        interval: 120
        repeat: false
        onTriggered: layoutController.compactTopHover = true
    }

    Timer {
        id: compactTopExitTimer
        interval: 300
        repeat: false
        onTriggered: layoutController.compactTopHover = false
    }

    Timer {
        id: compactSidebarEnterTimer
        interval: 120
        repeat: false
        onTriggered: layoutController.compactSidebarHover = true
    }

    Timer {
        id: compactSidebarExitTimer
        interval: 300
        repeat: false
        onTriggered: layoutController.compactSidebarHover = false
    }

    Timer {
        id: omniboxUpdateTimer
        interval: 60
        repeat: false
        onTriggered: root.updateOmnibox()
    }

    readonly property int windowControlButtonWidth: 46
    readonly property int windowControlButtonHeight: 32

    readonly property bool showTopBar: layoutController.showTopBar
    readonly property bool showSidebar: layoutController.showSidebar

    readonly property bool sidebarIconOnly: layoutController.sidebarIconOnly
    property string sidebarPanel: "tabs"

    readonly property int uiRadius: theme.cornerRadius
    readonly property int uiSpacing: theme.spacing

    property string glanceScript: ""
    property var glanceView: null
    property var extensionPopupView: null
    property string omniboxQuery: ""
    property bool suppressNextOmniboxUpdate: false

    property int webContextMenuTabId: 0
    property var webContextMenuInfo: ({})
    property var webContextMenuItems: []

    property string popupManagerContext: ""
    property string toolWindowManagerContext: ""
    property string overlayHostContext: ""

    onPopupManagerContextChanged: layoutController.popupManagerContext = popupManagerContext
    onToolWindowManagerContextChanged: layoutController.toolWindowManagerContext = toolWindowManagerContext
    onFullscreenActiveChanged: layoutController.fullscreen = fullscreenActive

    property string extensionsPanelSearch: ""
    property string extensionPopupExtensionId: ""
    property string extensionPopupName: ""
    property string extensionPopupUrl: ""
    property string extensionPopupOptionsUrl: ""

    property url webPanelUrl: "about:blank"
    property string webPanelTitle: ""

    readonly property var browserModel: browser
    readonly property var notificationsModel: notifications
    readonly property var downloadsModel: downloads
    readonly property var bookmarksModel: bookmarks
    readonly property var historyModel: history
    readonly property var webPanelsModel: webPanels
    readonly property var themesModel: themes
    readonly property var modsModel: mods

    property int pendingPermissionTabId: 0
    property int pendingPermissionRequestId: 0
    property string pendingPermissionOrigin: ""
    property int pendingPermissionKind: 0
    property bool pendingPermissionUserInitiated: false

    QtObject {
        id: tabViews
        property var byId: ({})
        property int revision: 0
    }

    readonly property int focusedTabId: splitView.enabled ? splitView.tabIdForPane(splitView.focusedPane) : tabIdForActiveIndex()

    readonly property var primaryView: {
        tabViews.revision
        const tabId = splitView.enabled ? splitView.tabIdForPane(0) : tabIdForActiveIndex()
        return tabViews.byId[tabId] || null
    }

    readonly property var secondaryView: {
        tabViews.revision
        if (!splitView.enabled || splitView.paneCount < 2) {
            return null
        }
        const tabId = splitView.tabIdForPane(1)
        return tabViews.byId[tabId] || null
    }

    readonly property var focusedView: {
        tabViews.revision
        return tabViews.byId[focusedTabId] || null
    }

    readonly property var mediaView: {
        tabViews.revision
        if (splitView.enabled && splitView.paneTabIds) {
            const ids = splitView.paneTabIds
            for (const tabId of ids) {
                const view = tabViews.byId[Number(tabId || 0)]
                if (view && view.documentPlayingAudio) {
                    return view
                }
            }
        }
        return focusedView || primaryView || secondaryView
    }

    onFocusedViewChanged: {
        syncAddressFieldFromFocused()
        syncExtensionsHost()
    }

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
            MenuItem { text: "New Window"; onTriggered: commands.invoke("new-window") }
            MenuItem { text: "New Incognito Window"; onTriggered: commands.invoke("new-incognito-window") }
            MenuItem { text: "Open File…"; onTriggered: commands.invoke("open-file") }
            MenuItem { text: "Close Tab"; onTriggered: commands.invoke("close-tab") }
            MenuItem {
                text: "Restore Closed Tab"
                enabled: browser.recentlyClosedCount > 0
                onTriggered: commands.invoke("restore-closed-tab")
            }
            Menu {
                id: recentlyClosedMenu
                title: "Recently Closed"
                enabled: browser.recentlyClosedCount > 0

                Instantiator {
                    model: {
                        browser.recentlyClosedCount
                        return browser.recentlyClosedItems(10)
                    }
                    onObjectAdded: recentlyClosedMenu.insertItem(index, object)
                    onObjectRemoved: recentlyClosedMenu.removeItem(object)
                    delegate: MenuItem {
                        required property var modelData
                        text: String(modelData.title || modelData.url || "")
                        onTriggered: browser.restoreRecentlyClosed(index)
                    }
                }

                MenuSeparator { }
                MenuItem {
                    text: "Clear Recently Closed"
                    enabled: browser.recentlyClosedCount > 0
                    onTriggered: browser.clearRecentlyClosed()
                }
            }
            MenuSeparator { }
            MenuItem { text: "Open in Default Browser"; onTriggered: commands.invoke("open-external-url", { tabId: root.focusedTabId }) }
            MenuItem {
                text: (shareController && shareController.canShare) ? "Share URL" : "Copy URL"
                enabled: root.focusedView
                         && root.focusedView.currentUrl
                         && root.focusedView.currentUrl.toString
                         && root.focusedView.currentUrl.toString().length > 0
                         && root.focusedView.currentUrl.toString() !== "about:blank"
                onTriggered: commands.invoke((shareController && shareController.canShare) ? "share-url" : "copy-url", { tabId: root.focusedTabId })
            }
            MenuSeparator { }
            MenuItem { text: "Print"; onTriggered: commands.invoke("open-print") }
            MenuSeparator { }
            MenuItem { text: "Quit"; onTriggered: Qt.quit() }
        }

        Menu {
            title: "View"
            MenuItem { text: root.fullscreenActive ? "Exit Fullscreen" : "Fullscreen"; onTriggered: commands.invoke("toggle-fullscreen") }
            MenuItem { text: "Toggle Sidebar"; onTriggered: commands.invoke("toggle-sidebar") }
            MenuItem { text: "Toggle Address Bar"; onTriggered: commands.invoke("toggle-addressbar") }
            MenuItem { text: "Toggle Compact Mode"; onTriggered: commands.invoke("toggle-compact-mode") }
            MenuItem {
                text: "Reduce Motion"
                checkable: true
                checked: browser.settings.reduceMotion
                onTriggered: commands.invoke("toggle-reduce-motion")
            }
            MenuItem {
                text: "Back closes tab"
                checkable: true
                checked: browser.settings.closeTabOnBackNoHistory
                onTriggered: commands.invoke("toggle-back-close")
            }
            MenuItem { text: "Toggle Menu Bar"; onTriggered: commands.invoke("toggle-menubar") }
            MenuSeparator { }
            MenuItem { text: "Zoom In"; onTriggered: commands.invoke("zoom-in") }
            MenuItem { text: "Zoom Out"; onTriggered: commands.invoke("zoom-out") }
            MenuItem { text: "Reset Zoom"; onTriggered: commands.invoke("zoom-reset") }
            MenuSeparator { }
            MenuItem { text: splitView.enabled ? "Unsplit View" : "Split View"; onTriggered: commands.invoke("toggle-split-view") }
            MenuItem { visible: splitView.enabled; text: "Swap Split Panes"; onTriggered: commands.invoke("split-swap") }
            MenuItem { visible: splitView.enabled; text: "Close Split Pane"; onTriggered: commands.invoke("split-close-pane") }
            MenuItem { visible: splitView.enabled; text: "Focus Next Split Pane"; onTriggered: commands.invoke("split-focus-next") }
        }

        Menu {
            title: "Tools"
            MenuItem { text: "Settings"; onTriggered: commands.invoke("open-settings") }
            MenuItem { text: "Themes"; onTriggered: root.openOverlay(themesDialogComponent, "themes") }
            MenuItem { text: "Mods"; onTriggered: commands.invoke("open-mods") }
            MenuItem { text: "Extensions"; onTriggered: commands.invoke("open-extensions") }
            MenuItem { text: "Downloads"; onTriggered: root.openOverlay(downloadsDialogComponent, "downloads") }
            MenuItem { text: "Bookmarks"; onTriggered: commands.invoke("open-bookmarks") }
            MenuItem { text: "History"; onTriggered: commands.invoke("open-history") }
            MenuItem { text: "Permissions"; onTriggered: commands.invoke("open-permissions") }
            MenuItem { text: "Diagnostics"; onTriggered: commands.invoke("open-diagnostics") }
            MenuItem { text: "View Source"; onTriggered: commands.invoke("view-source") }
            MenuItem { text: "DevTools"; onTriggered: commands.invoke("open-devtools") }
        }

        Menu {
            title: "Help"
            MenuItem { text: "Welcome"; onTriggered: root.openOverlay(onboardingDialogComponent, "onboarding") }
            MenuItem { text: "About"; onTriggered: toast.showToast("XBrowser " + Qt.application.version) }
        }
    }

    function toggleFullscreen() {
        if (root.visibility === Window.FullScreen) {
            root.visibility = root.preFullscreenVisibility || Window.Windowed
            return
        }

        root.preFullscreenVisibility = root.visibility
        root.visibility = Window.FullScreen
    }

    function handleContentFullscreen(tabId, enabled) {
        const resolvedTabId = Number(tabId || 0)
        if (resolvedTabId <= 0 || resolvedTabId !== root.focusedTabId) {
            return
        }

        const active = enabled === true
        if (active) {
            root.contentFullscreenTabId = resolvedTabId
            if (!root.fullscreenActive) {
                root.contentFullscreenToggled = true
                root.toggleFullscreen()
            } else {
                root.contentFullscreenToggled = false
            }

            toast.showToast("Entered fullscreen", "Exit", "toggle-fullscreen", 4000)
            return
        }

        if (root.contentFullscreenTabId === resolvedTabId) {
            root.contentFullscreenTabId = 0
            if (root.contentFullscreenToggled && root.fullscreenActive) {
                root.contentFullscreenToggled = false
                root.toggleFullscreen()
            } else {
                root.contentFullscreenToggled = false
            }
        }
    }

    function openViewSourceForTab(tabId) {
        const resolvedTabId = Number(tabId || root.focusedTabId || 0)
        const view = tabViews.byId[resolvedTabId] || root.focusedView

        if (!view || !view.executeScript) {
            toast.showToast("No active tab")
            return
        }
        if (!view.initialized) {
            toast.showToast("Tab is not ready")
            return
        }
        if (root.viewSourceToken && root.viewSourceToken.length > 0) {
            toast.showToast("Already fetching source")
            return
        }

        const token = "vs-" + Date.now() + "-" + Math.floor(Math.random() * 1000000000)
        root.viewSourceToken = token
        root.viewSourceTabId = resolvedTabId
        root.viewSourceTargetView = view

        const js = `(function(){
  const token = ${JSON.stringify(token)};
  try {
    const doctype = (document.doctype && window.XMLSerializer) ? (new XMLSerializer()).serializeToString(document.doctype) : "";
    const html = document.documentElement ? document.documentElement.outerHTML : "";
    const source = (doctype && doctype.length > 0 ? (doctype + "\\n") : "") + html;
    return { __xb_viewSourceToken: token, url: String(location.href || ""), source: source };
  } catch (e) {
    return { __xb_viewSourceToken: token, error: String(e) };
  }
})();`

        view.executeScript(js)
        toast.showToast("Fetching page source...")
    }

    function handleViewSourceScriptExecuted(resultJson) {
        const token = String(root.viewSourceToken || "")
        if (token.length === 0) {
            return
        }

        let payload = null
        try {
            payload = JSON.parse(resultJson)
        } catch (e) {
            return
        }

        if (!payload || payload.__xb_viewSourceToken !== token) {
            return
        }

        root.viewSourceToken = ""
        root.viewSourceTabId = 0
        root.viewSourceTargetView = null

        if (payload.error) {
            toast.showToast("View source failed")
            return
        }

        const sourceText = payload.source ? String(payload.source) : ""
        if (sourceText.trim().length === 0) {
            toast.showToast("No page source")
            return
        }

        if (!sourceViewer || !sourceViewer.createViewSourcePage) {
            toast.showToast("Source viewer unavailable")
            return
        }

        const pageUrl = payload.url ? payload.url : ""
        const fileUrl = sourceViewer.createViewSourcePage(pageUrl, sourceText)
        if (!fileUrl || fileUrl.toString().length === 0) {
            toast.showToast("Failed to create source viewer")
            return
        }

        const idx = browser.newTab(fileUrl)
        if (idx < 0) {
            toast.showToast("Failed to open tab")
            return
        }

        const tabId = browser.tabs.tabIdAt(idx)
        if (tabId > 0) {
            const label = String(pageUrl || "")
            const title = label.length > 0 ? ("View Source - " + label) : "View Source"
            browser.setTabCustomTitleById(tabId, title)
        }
    }

    Connections {
        target: root.viewSourceTargetView
        function onScriptExecuted(resultJson) {
            root.handleViewSourceScriptExecuted(resultJson)
        }
    }

    Platform.FileDialog {
        id: openFileDialog
        title: "Open File"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: ["Web pages (*.html *.htm)", "Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.svg)", "All files (*)"]
        onAccepted: {
            const url = openFileDialog.file
            if (!url || url.toString().length === 0) {
                return
            }
            if (root.focusedView) {
                root.focusedView.navigate(url)
                return
            }
            commands.invoke("new-tab", { url: url })
        }
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.WindowShortcut
        onActivated: {
            if (root.fullscreenActive) {
                root.toggleFullscreen()
                return
            }
            if (overlayHost.active || popupManager.opened || toolWindowManager.opened) {
                return
            }
            if (!layoutController.addressFieldFocused && root.focusedView && root.focusedView.isLoading) {
                commands.invoke("nav-stop")
            }
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

    function currentFocusedUrlString() {
        const view = focusedView
        if (view && view.currentUrl) {
            return view.currentUrl.toString()
        }

        const url = urlForTabId(focusedTabId)
        return url ? url.toString() : ""
    }

    function currentFocusedOriginString() {
        const view = focusedView
        let url = null
        if (view && view.currentUrl) {
            url = view.currentUrl
        }
        if (!url) {
            url = urlForTabId(focusedTabId)
        }
        if (!url) {
            return ""
        }

        if (url.scheme && url.scheme.length > 0 && url.host && url.host.length > 0) {
            let origin = url.scheme + "://" + url.host
            if (url.port && url.port > 0) {
                origin += ":" + url.port
            }
            return origin
        }

        return url.toString()
    }

    function singleToolbarActive() {
        return browser.settings.useSingleToolbar && browser.settings.sidebarExpanded && !root.sidebarIconOnly
    }

    function activeAddressField() {
        return singleToolbarActive() ? sidebarAddressField : addressField
    }

    function syncAddressFocusState() {
        const topFocused = addressField && addressField.activeFocus
        const sidebarFocused = sidebarAddressField && sidebarAddressField.activeFocus
        layoutController.addressFieldFocused = (topFocused || sidebarFocused) === true
    }

    function syncAddressFieldFromFocused(force) {
        const shouldForce = force === true
        const nextText = currentFocusedUrlString()

        const fields = [addressField, sidebarAddressField]
        for (const field of fields) {
            if (!field) {
                continue
            }
            if (!shouldForce && field.activeFocus) {
                continue
            }
            if (field.text === nextText) {
                continue
            }

            if (field.activeFocus) {
                suppressNextOmniboxUpdate = true
            }
            field.text = nextText
        }
    }

    function interpretOmniboxInput(text) {
        const raw = (text || "").trim()
        if (!raw) {
            return { kind: "empty", url: "", display: "" }
        }

        const lower = raw.toLowerCase()
        const looksLikeUrl = lower.includes("://")
                             || lower.startsWith("about:")
                             || lower.startsWith("file:")
                             || lower.startsWith("edge:")
                             || (!lower.includes(" ") && (lower.includes(".") || lower.startsWith("localhost")))

        if (looksLikeUrl) {
            const url = (lower.includes("://") || lower.startsWith("about:") || lower.startsWith("file:") || lower.startsWith("edge:"))
                            ? raw
                            : ("https://" + raw)
            return { kind: "url", url: url, display: url }
        }

        const q = encodeURIComponent(raw)
        const searchUrl = "https://duckduckgo.com/?q=" + q
        return { kind: "search", url: searchUrl, display: raw }
    }

    function navigateFromOmnibox(text) {
        const parsed = interpretOmniboxInput(text)
        if (!parsed || !parsed.url || parsed.url.length === 0) {
            return
        }

        if (root.focusedView) {
            root.focusedView.navigate(parsed.url)
        } else {
            commands.invoke("new-tab", { url: parsed.url })
        }
    }

    function desiredZoomForUrl(url) {
        if (!browser || !browser.settings) {
            return 1.0
        }

        const settings = browser.settings
        if (settings.zoomForUrl) {
            return settings.zoomForUrl(url)
        }

        return settings.defaultZoom || 1.0
    }

    function applyZoomForView(view) {
        if (!view) {
            return
        }

        const desired = desiredZoomForUrl(view.currentUrl)
        const current = view.zoomFactor || 1.0
        if (Math.abs(current - desired) < 0.001) {
            return
        }
        view.zoomFactor = desired
    }

    function syncTabViews() {
        if (!browser.tabs) {
            return
        }

        const wanted = {}
        for (let i = 0; i < browser.tabs.count(); i++) {
            const tabId = browser.tabs.tabIdAt(i)
            if (tabId <= 0) {
                continue
            }

            const key = String(tabId)
            wanted[key] = true

            if (!tabViews.byId[tabId]) {
                const view = tabWebViewComponent.createObject(contentHost, { tabId: tabId })
                if (view) {
                    tabViews.byId[tabId] = view
                }
            }
        }

        const toRemove = []
        for (const key in tabViews.byId) {
            if (!wanted[key]) {
                toRemove.push(key)
            }
        }

        for (const key of toRemove) {
            const view = tabViews.byId[key]
            if (view) {
                view.destroy()
            }
            delete tabViews.byId[key]
        }

        tabViews.revision++
        syncAddressFieldFromFocused()
        syncExtensionsHost()
    }

    function syncExtensionsHost() {
        if (!extensions) {
            return
        }
        const view = root.focusedView || root.primaryView || root.secondaryView
        extensions.hostView = view ? view : null
    }

    function openExtensionsPanel(anchorItem) {
        if (!anchorItem) {
            return
        }
        popupManager.openAtItem(extensionsPanelComponent, anchorItem)
        root.popupManagerContext = "extensions-panel"
    }

    function openExtensionPopup(anchorItem, extensionId, name, popupUrl, optionsUrl) {
        const id = String(extensionId || "")
        const popup = String(popupUrl || "")
        const options = String(optionsUrl || "")

        if (!popup || popup.length === 0) {
            if (options && options.length > 0) {
                commands.invoke("new-tab", { url: options })
            } else {
                commands.invoke("new-tab", { url: "edge://extensions" })
            }
            return
        }

        if (!anchorItem) {
            anchorItem = extensionsButton
        }

        extensionPopupExtensionId = id
        extensionPopupName = String(name || id || "Extension")
        extensionPopupUrl = popup
        extensionPopupOptionsUrl = options

        popupManager.openAtItem(extensionPopupComponent, anchorItem)
        root.popupManagerContext = "extension-popup"
    }

    function toggleTopBarPopup(contextId, component, anchorItem) {
        const ctx = String(contextId || "").trim()
        if (!ctx || !component || !anchorItem) {
            return
        }

        const hasPopup = popupManager.opened || popupManager.pendingOpen || popupManager.popupItem
        if (hasPopup && root.popupManagerContext === ctx) {
            popupManager.close()
            return
        }

        if (hasPopup) {
            popupManager.close()
        }

        root.popupManagerContext = ctx
        Qt.callLater(() => {
            if (root.popupManagerContext !== ctx) {
                return
            }
            popupManager.openAtItem(component, anchorItem)
            root.popupManagerContext = ctx
        })
    }

    function toggleTabSwitcherPopup() {
        const ctx = "tab-switcher"

        const hasPopup = popupManager.opened || popupManager.pendingOpen || popupManager.popupItem
        if (hasPopup && root.popupManagerContext === ctx) {
            popupManager.close()
            return
        }

        if (hasPopup) {
            popupManager.close()
        }

        root.popupManagerContext = ctx
        Qt.callLater(() => {
            if (root.popupManagerContext !== ctx) {
                return
            }
            const w = 620
            const x = Math.max(8, Math.round((root.width - w) / 2))
            const y = Math.round(topBar.height + 24)
            popupManager.openAtPoint(tabSwitcherComponent, x, y, root)
            root.popupManagerContext = ctx
        })
    }

    function toggleSidebarToolPopup(toolId, component, anchorItem) {
        if (!component || !anchorItem) {
            return
        }

        const ctx = "sidebar-tool-" + String(toolId || "")
        if (popupManager.opened && root.popupManagerContext === ctx) {
            popupManager.close()
            return
        }

        const anchorPos = anchorItem.mapToItem(popupManager, 0, 0)
        const sidebarPos = sidebarPane ? sidebarPane.mapToItem(popupManager, sidebarPane.width, 0) : ({ x: 0, y: 0 })
        const gap = 8
        const ax = Math.max(0, Math.round((sidebarPos.x + gap) - anchorPos.x))
        popupManager.openAtItem(component, anchorItem, null, ax, anchorItem.height)
        root.popupManagerContext = ctx
    }

    function openWebPanel(url, title) {
        if (!url) {
            return
        }
        const urlText = url.toString ? url.toString() : String(url || "")
        if (!urlText || urlText.length === 0 || urlText === "about:blank") {
            return
        }

        root.webPanelUrl = url
        root.webPanelTitle = String(title || "")
        browser.settings.webPanelVisible = true
        browser.settings.webPanelUrl = url
        browser.settings.webPanelTitle = root.webPanelTitle

        if (webPanelWeb && webPanelWeb.navigate) {
            webPanelWeb.navigate(url)
        }
    }

    function closeWebPanel() {
        browser.settings.webPanelVisible = false
    }

    function buildExtensionContextMenuItems(extensionId, name, enabled, pinned, popupUrl, optionsUrl) {
        const id = String(extensionId || "")
        const displayName = String(name || id || "Extension")
        const popup = String(popupUrl || "")
        const options = String(optionsUrl || "")
        const isEnabled = enabled === true
        const isPinned = pinned === true

        if (!id || id.length === 0) {
            return []
        }

        const baseArgs = {
            extensionId: id,
            name: displayName,
            popupUrl: popup,
            optionsUrl: options,
            enabled: isEnabled,
            pinned: isPinned,
        }

        const items = []
        items.push({
            action: "ext-open-popup",
            text: popup && popup.length > 0 ? "Open popup" : "Open",
            enabled: true,
            args: baseArgs,
        })
        if (options && options.length > 0) {
            items.push({ action: "ext-open-options", text: "Options", enabled: true, args: baseArgs })
        }
        items.push({ separator: true })
        items.push({
            action: isPinned ? "ext-unpin" : "ext-pin",
            text: isPinned ? "Unpin from toolbar" : "Pin to toolbar",
            enabled: true,
            args: baseArgs,
        })
        items.push({
            action: isEnabled ? "ext-disable" : "ext-enable",
            text: isEnabled ? "Disable" : "Enable",
            enabled: true,
            args: baseArgs,
        })
        items.push({ separator: true })
        items.push({ action: "ext-remove", text: "Remove", enabled: true, args: baseArgs })
        items.push({ action: "ext-manage", text: "Manage extensions", enabled: true, args: {} })
        return items
    }

    function handleExtensionContextMenuAction(action, args, anchorItem) {
        if (!extensions) {
            return
        }

        const id = String(args && args.extensionId ? args.extensionId : "")
        if (!id || id.length === 0) {
            return
        }

        const displayName = String(args && args.name ? args.name : id)
        const popupUrl = String(args && args.popupUrl ? args.popupUrl : "")
        const optionsUrl = String(args && args.optionsUrl ? args.optionsUrl : "")
        const isEnabled = args && args.enabled === true
        const isPinned = args && args.pinned === true

        if (action === "ext-open-popup") {
            if (!isEnabled) {
                toast.showToast("Extension is disabled")
                commands.invoke("open-extensions")
                return
            }
            root.openExtensionPopup(anchorItem, id, displayName, popupUrl, optionsUrl)
            return
        }

        if (action === "ext-open-options") {
            if (optionsUrl && optionsUrl.length > 0) {
                commands.invoke("new-tab", { url: optionsUrl })
            } else {
                commands.invoke("new-tab", { url: "edge://extensions" })
            }
            return
        }

        if (action === "ext-pin") {
            extensions.setExtensionPinned(id, true)
            return
        }

        if (action === "ext-unpin") {
            extensions.setExtensionPinned(id, false)
            return
        }

        if (action === "ext-enable") {
            extensions.setExtensionEnabled(id, true)
            return
        }

        if (action === "ext-disable") {
            extensions.setExtensionEnabled(id, false)
            return
        }

        if (action === "ext-remove") {
            extensions.removeExtension(id)
            return
        }

        if (action === "ext-manage") {
            commands.invoke("open-extensions")
            return
        }
    }

    function pushModsCss(targetView) {
        const css = root.modsModel && root.modsModel.combinedCss ? String(root.modsModel.combinedCss) : ""

        if (targetView && targetView.setUserCss) {
            targetView.setUserCss(css)
            return
        }

        for (const key in tabViews.byId) {
            const view = tabViews.byId[key]
            if (view && view.setUserCss) {
                view.setUserCss(css)
            }
        }

        if (webPanelWeb && webPanelWeb.setUserCss) {
            webPanelWeb.setUserCss(css)
        }

        if (root.glanceView && root.glanceView.setUserCss) {
            root.glanceView.setUserCss(css)
        }

        if (root.extensionPopupView && root.extensionPopupView.setUserCss) {
            root.extensionPopupView.setUserCss(css)
        }
    }

    function handleWebMessage(tabId, json, senderView) {
        let msg = null
        try {
            msg = JSON.parse(json)
        } catch (e) {
            return
        }

        if (msg && msg.type === "glance" && msg.href) {
            root.glanceUrl = msg.href
            root.openOverlay(glanceOverlayComponent, "glance")
        }
    }

    function downloadOpKey(tabId, downloadOperationId) {
        return String(tabId) + ":" + String(downloadOperationId)
    }

    function handleDownloadStarted(tabId, downloadOperationId, uri, resultFilePath, totalBytes) {
        const downloadId = downloads.addStarted(uri, resultFilePath)
        if (downloadId > 0) {
            const key = downloadOpKey(tabId, downloadOperationId)
            activeDownloadIdByKey[key] = downloadId
            activeDownloadOpById[downloadId] = { tabId: tabId, opId: downloadOperationId }
            downloads.updateProgress(downloadId, 0, Number(totalBytes || 0), false, false, "")
        }
        toast.showToast("Download started", "Downloads", "open-downloads", 3500)
    }

    function handleDownloadProgress(tabId, downloadOperationId, bytesReceived, totalBytes, paused, canResume, interruptReason) {
        const key = downloadOpKey(tabId, downloadOperationId)
        const downloadId = Number(activeDownloadIdByKey[key] || 0)
        if (downloadId > 0) {
            downloads.updateProgress(
                        downloadId,
                        Number(bytesReceived || 0),
                        Number(totalBytes || 0),
                        !!paused,
                        !!canResume,
                        String(interruptReason || ""))
        }
    }

    function handleDownloadFinished(tabId, downloadOperationId, uri, resultFilePath, success, interruptReason) {
        const key = downloadOpKey(tabId, downloadOperationId)
        const downloadId = Number(activeDownloadIdByKey[key] || 0)
        if (downloadId > 0) {
            delete activeDownloadIdByKey[key]
            delete activeDownloadOpById[downloadId]
            downloads.markFinishedById(downloadId, !!success, String(interruptReason || ""))
        } else {
            downloads.markFinished(uri, resultFilePath, success)
        }

        if (success) {
            toast.showToast("Download finished", "Open Folder", "open-latest-download-folder", 6000)
        } else {
            root.lastFailedDownloadUri = uri ? String(uri) : ""
            if (root.lastFailedDownloadUri && root.lastFailedDownloadUri.trim().length > 0) {
                toast.showToast("Download failed", "Retry", "retry-last-download", 6000)
            } else {
                toast.showToast("Download failed", "Downloads", "open-downloads", 6000)
            }
        }
    }

    function pauseDownload(downloadId) {
        const op = activeDownloadOpById[downloadId]
        if (!op) {
            return
        }
        const view = tabViews.byId[Number(op.tabId || 0)]
        if (view && view.pauseDownload) {
            view.pauseDownload(Number(op.opId || 0))
        }
    }

    function resumeDownload(downloadId) {
        const op = activeDownloadOpById[downloadId]
        if (!op) {
            return
        }
        const view = tabViews.byId[Number(op.tabId || 0)]
        if (view && view.resumeDownload) {
            view.resumeDownload(Number(op.opId || 0))
        }
    }

    function cancelDownload(downloadId) {
        const op = activeDownloadOpById[downloadId]
        if (!op) {
            return
        }
        const view = tabViews.byId[Number(op.tabId || 0)]
        if (view && view.cancelDownload) {
            view.cancelDownload(Number(op.opId || 0))
        }
    }

    function openWebContextMenu(tabId, info) {
        const view = tabViews.byId[tabId]
        if (!view || !info) {
            return
        }

        popupManager.close()

        webContextMenuTabId = tabId
        webContextMenuInfo = info

        const items = []
        items.push({ action: "back", text: "Back", enabled: view.canGoBack })
        items.push({ action: "forward", text: "Forward", enabled: view.canGoForward })
        items.push({ action: "reload", text: "Reload", enabled: true })
        items.push({
            action: "open-external-page",
            text: "Open in default browser",
            enabled: view.currentUrl && view.currentUrl.toString && view.currentUrl.toString().length > 0 && view.currentUrl.toString() !== "about:blank"
        })
        if (shareController && shareController.canShare) {
            items.push({
                action: "share-page",
                text: "Share page",
                enabled: view.currentUrl && view.currentUrl.toString && view.currentUrl.toString().length > 0 && view.currentUrl.toString() !== "about:blank"
            })
        } else {
            items.push({
                action: "copy-page",
                text: "Copy page address",
                enabled: view.currentUrl && view.currentUrl.toString && view.currentUrl.toString().length > 0 && view.currentUrl.toString() !== "about:blank"
            })
        }

        const link = info.linkUri ? String(info.linkUri) : ""
        const src = info.sourceUri ? String(info.sourceUri) : ""
        if (link.length > 0 || src.length > 0) {
            items.push({ separator: true })
        }
        if (link.length > 0) {
            items.push({ action: "open-link-new-tab", text: "Open link in new tab", enabled: true })
            items.push({ action: "open-link-split", text: "Open link in split view", enabled: true })
            items.push({ action: "open-link-external", text: "Open link in default browser", enabled: true })
            items.push({ action: "copy-link", text: "Copy link address", enabled: true })
        }
        if (src.length > 0 && (!link || src !== link)) {
            items.push({ action: "open-source-new-tab", text: "Open media in new tab", enabled: true })
            items.push({ action: "copy-source", text: "Copy media address", enabled: true })
        }

        items.push({ separator: true })
        items.push({ action: "view-source", text: "View page source", enabled: true })

        webContextMenuItems = items

        const x = Number(info.x || 0)
        const y = Number(info.y || 0)
        const pos = root.contentItem.mapToItem(popupManager, x, y)
        popupManager.openAtPoint(webContextMenuComponent, pos.x, pos.y)
        popupManagerContext = "web-context-menu"
    }

    function handleWebContextMenuAction(action) {
        const view = tabViews.byId[webContextMenuTabId]
        const info = webContextMenuInfo || {}
        const link = info.linkUri ? String(info.linkUri) : ""
        const src = info.sourceUri ? String(info.sourceUri) : ""

        if (action === "back" && view) {
            view.goBack()
        } else if (action === "forward" && view) {
            view.goForward()
        } else if (action === "reload" && view) {
            view.reload()
        } else if (action === "open-external-page") {
            commands.invoke("open-external-url", { tabId: webContextMenuTabId })
        } else if (action === "share-page") {
            commands.invoke("share-url", { tabId: webContextMenuTabId })
        } else if (action === "copy-page") {
            commands.invoke("copy-url", { tabId: webContextMenuTabId })
        } else if (action === "open-link-new-tab" && link.length > 0) {
            commands.invoke("new-tab", { url: link })
        } else if (action === "open-link-split" && link.length > 0) {
            splitView.openUrlInNewPane(link)
        } else if (action === "open-link-external" && link.length > 0) {
            commands.invoke("open-external-link", { url: link })
        } else if (action === "copy-link" && link.length > 0) {
            commands.invoke("copy-text", { text: link })
        } else if (action === "open-source-new-tab" && src.length > 0) {
            commands.invoke("new-tab", { url: src })
        } else if (action === "copy-source" && src.length > 0) {
            commands.invoke("copy-text", { text: src })
        } else if (action === "view-source") {
            commands.invoke("view-source", { tabId: webContextMenuTabId })
        }
    }

    function tabIdsForSelectionArgs(args) {
        if (args && args.tabIds !== undefined && args.tabIds !== null) {
            return args.tabIds
        }
        if (browser.tabs && browser.tabs.selectedTabIds) {
            return browser.tabs.selectedTabIds()
        }
        return []
    }

    function selectedTabsAllEssential(tabIds) {
        if (!browser.tabs || !browser.tabs.indexOfTabId || !browser.tabs.isEssentialAt) {
            return false
        }
        for (const id of tabIds) {
            const idx = browser.tabs.indexOfTabId(Number(id))
            if (idx < 0) {
                continue
            }
            if (!browser.tabs.isEssentialAt(idx)) {
                return false
            }
        }
        return tabIds.length > 0
    }

    function prepareContextMenuSelection(tabId) {
        if (!browser.tabs || !browser.tabs.selectOnlyById || !browser.tabs.isSelectedById) {
            return
        }
        const resolved = Number(tabId || 0)
        if (resolved <= 0) {
            return
        }

        const count = Number(browser.tabs.selectedCount || 0)
        if (count <= 0) {
            browser.tabs.selectOnlyById(resolved)
            return
        }

        if (!browser.tabs.isSelectedById(resolved)) {
            browser.tabs.selectOnlyById(resolved)
        }
    }

    function handleTabRowClick(listView, model, index, tabId, modifiers, activateOnPlainClick) {
        if (!browser.tabs) {
            return
        }

        const resolvedTabId = Number(tabId || 0)
        if (resolvedTabId <= 0) {
            return
        }

        const mods = Number(modifiers || 0)
        const ctrl = (mods & Qt.ControlModifier) !== 0
        const shift = (mods & Qt.ShiftModifier) !== 0

        if (listView) {
            listView.currentIndex = index
            listView.forceActiveFocus()
        }

        if (shift && model && model.tabIdsInRange && browser.tabs.setSelectionByIds) {
            if (listView && listView.selectionAnchorIndex === undefined) {
                listView.selectionAnchorIndex = -1
            }
            if (listView && listView.selectionAnchorIndex < 0) {
                listView.selectionAnchorIndex = index
            }

            const ids = model.tabIdsInRange(listView ? listView.selectionAnchorIndex : index, index)
            browser.tabs.setSelectionByIds(ids, !ctrl)
            return
        }

        if (listView && listView.selectionAnchorIndex !== undefined) {
            listView.selectionAnchorIndex = index
        }

        if (ctrl && browser.tabs.setSelectedById && browser.tabs.isSelectedById) {
            browser.tabs.setSelectedById(resolvedTabId, !browser.tabs.isSelectedById(resolvedTabId))
            return
        }

        if (browser.tabs.selectOnlyById) {
            browser.tabs.selectOnlyById(resolvedTabId)
        }
        if (activateOnPlainClick) {
            browser.activateTabById(resolvedTabId)
        }
    }

    function buildTabContextMenuItems(tabId, isEssential, groupId) {
        const resolvedTabId = Number(tabId || 0)
        const pinned = isEssential === true
        const resolvedGroupId = Number(groupId || 0)

        const selectionCount = browser.tabs ? Number(browser.tabs.selectedCount || 0) : 0
        const isSelected = browser.tabs && browser.tabs.isSelectedById ? browser.tabs.isSelectedById(resolvedTabId) : false
        const useSelection = selectionCount > 1 && isSelected
        const selectedIds = useSelection ? (browser.tabs.selectedTabIds ? browser.tabs.selectedTabIds() : []) : []
        const selectionAllPinned = useSelection ? selectedTabsAllEssential(selectedIds) : false

        const items = []
        if (useSelection) {
            items.push({ action: "close-tabs", text: "Close " + selectionCount + " Tabs", enabled: true, args: { tabIds: selectedIds } })
            items.push({ action: "duplicate-tabs", text: "Duplicate " + selectionCount + " Tabs", enabled: true, args: { tabIds: selectedIds } })
            items.push({ separator: true })
            items.push({
                action: selectionAllPinned ? "unpin-tabs" : "pin-tabs",
                text: selectionAllPinned ? "Unpin from Essentials" : "Pin to Essentials",
                enabled: true,
                args: { tabIds: selectedIds }
            })
            items.push({ separator: true })
            items.push({ action: "new-group-tabs", text: "New Group from Selection", enabled: true, args: { tabIds: selectedIds } })
            items.push({ action: "ungroup-tabs", text: "Ungroup Selection", enabled: true, args: { tabIds: selectedIds } })
        } else {
            items.push({ action: "close-tab", text: "Close Tab", enabled: true, args: { tabId: resolvedTabId } })
            items.push({ action: "duplicate-tab", text: "Duplicate Tab", enabled: true, args: { tabId: resolvedTabId } })
            items.push({ separator: true })
            items.push({ action: "copy-url", text: "Copy URL", enabled: true, args: { tabId: resolvedTabId } })
            items.push({ action: "copy-title", text: "Copy Title", enabled: true, args: { tabId: resolvedTabId } })
            items.push({ separator: true })
            items.push({ action: "open-split-right", text: "Open in Split (Right)", enabled: true, args: { tabId: resolvedTabId } })
            items.push({
                action: "toggle-essential",
                text: pinned ? "Unpin from Essentials" : "Pin to Essentials",
                enabled: true,
                args: { tabId: resolvedTabId }
            })
            items.push({ separator: true })
            if (resolvedGroupId > 0) {
                items.push({ action: "ungroup-tab", text: "Ungroup", enabled: true, args: { tabId: resolvedTabId } })
            } else {
                items.push({ action: "new-group", text: "New Group", enabled: true, args: { tabId: resolvedTabId } })
            }
        }

        const groupCount = browser.tabGroups ? browser.tabGroups.count() : 0
        for (let i = 0; i < groupCount; i++) {
            const name = browser.tabGroups.nameAt(i)
            const groupId = browser.tabGroups.groupIdAt(i)
            if (useSelection) {
                items.push({ action: "move-tabs-to-group", text: "Move Selection to " + name, enabled: true, args: { tabIds: selectedIds, groupId: groupId } })
            } else {
                items.push({ action: "move-to-group", text: "Move to " + name, enabled: true, args: { tabId: resolvedTabId, groupId: groupId } })
            }
        }

        const wsCount = browser.workspaces ? browser.workspaces.count() : 0
        for (let i = 0; i < wsCount; i++) {
            const name = browser.workspaces.nameAt(i)
            const isCurrent = i === browser.workspaces.activeIndex
            items.push({
                action: useSelection ? "move-tabs-to-workspace" : "move-to-workspace",
                text: useSelection
                          ? (isCurrent ? ("Move Selection to Workspace: " + name + " (Current)") : ("Move Selection to Workspace: " + name))
                          : (isCurrent ? ("Move to Workspace: " + name + " (Current)") : ("Move to Workspace: " + name)),
                enabled: !isCurrent,
                args: useSelection ? ({ tabIds: selectedIds, workspaceIndex: i }) : ({ tabId: resolvedTabId, workspaceIndex: i })
            })
        }

        return items
    }

    function handleTabContextMenuAction(action, args) {
        if (action === "close-tabs") {
            const tabIds = tabIdsForSelectionArgs(args)
            for (const id of tabIds) {
                const tabId = Number(id)
                if (tabId > 0) {
                    commands.invoke("close-tab", { tabId: tabId })
                }
            }
            if (browser.tabs && browser.tabs.clearSelection) {
                browser.tabs.clearSelection()
            }
            return
        }

        if (action === "duplicate-tabs") {
            const tabIds = tabIdsForSelectionArgs(args)
            for (const id of tabIds) {
                const tabId = Number(id)
                if (tabId > 0) {
                    commands.invoke("duplicate-tab", { tabId: tabId })
                }
            }
            return
        }

        if (action === "pin-tabs" || action === "unpin-tabs") {
            const tabIds = tabIdsForSelectionArgs(args)
            const essential = action === "pin-tabs"
            for (const id of tabIds) {
                const tabId = Number(id)
                const idx = browser.tabs.indexOfTabId(tabId)
                if (idx >= 0) {
                    browser.tabs.setEssentialAt(idx, essential)
                }
            }
            return
        }

        if (action === "new-group-tabs") {
            const tabIds = tabIdsForSelectionArgs(args)
            if (tabIds.length === 0) {
                return
            }
            const first = Number(tabIds[0])
            if (first <= 0) {
                return
            }
            const groupId = browser.createTabGroupForTab(first)
            if (groupId > 0) {
                for (let i = 1; i < tabIds.length; i++) {
                    const tabId = Number(tabIds[i])
                    if (tabId > 0) {
                        browser.moveTabToGroup(tabId, groupId)
                    }
                }
            }
            return
        }

        if (action === "ungroup-tabs") {
            const tabIds = tabIdsForSelectionArgs(args)
            for (const id of tabIds) {
                const tabId = Number(id)
                if (tabId > 0) {
                    browser.ungroupTab(tabId)
                }
            }
            return
        }

        if (action === "move-tabs-to-group") {
            const tabIds = tabIdsForSelectionArgs(args)
            const groupId = Number(args && args.groupId !== undefined ? args.groupId : 0)
            if (groupId <= 0) {
                return
            }
            for (const id of tabIds) {
                const tabId = Number(id)
                if (tabId > 0) {
                    browser.moveTabToGroup(tabId, groupId)
                }
            }
            return
        }

        if (action === "move-tabs-to-workspace") {
            const tabIds = tabIdsForSelectionArgs(args)
            if (!args || args.workspaceIndex === undefined) {
                return
            }
            const workspaceIndex = Number(args.workspaceIndex)
            if (workspaceIndex < 0 || workspaceIndex >= browser.workspaces.count()) {
                return
            }
            for (const id of tabIds) {
                const tabId = Number(id)
                if (tabId > 0) {
                    browser.moveTabToWorkspace(tabId, workspaceIndex)
                }
            }
            return
        }

        const tabId = Number(args && args.tabId !== undefined ? args.tabId : 0)
        if (tabId <= 0) {
            return
        }

        if (action === "close-tab") {
            commands.invoke("close-tab", { tabId: tabId })
        } else if (action === "duplicate-tab") {
            commands.invoke("duplicate-tab", { tabId: tabId })
        } else if (action === "copy-url") {
            commands.invoke("copy-url", { tabId: tabId })
        } else if (action === "copy-title") {
            commands.invoke("copy-title", { tabId: tabId })
        } else if (action === "open-split-right") {
            if (!splitView.enabled) {
                splitView.paneCount = 2
                splitView.setTabIdForPane(0, root.tabIdForActiveIndex())
                splitView.enabled = true
            } else if (splitView.paneCount < 2) {
                splitView.paneCount = 2
            }

            splitView.setTabIdForPane(1, tabId)
            splitView.focusedPane = 1
        } else if (action === "toggle-essential") {
            commands.invoke("toggle-essential", { tabId: tabId })
        } else if (action === "new-group") {
            browser.createTabGroupForTab(tabId)
        } else if (action === "ungroup-tab") {
            browser.ungroupTab(tabId)
        } else if (action === "move-to-group") {
            const groupId = Number(args && args.groupId !== undefined ? args.groupId : 0)
            if (groupId > 0) {
                browser.moveTabToGroup(tabId, groupId)
            }
        } else if (action === "move-to-workspace") {
            if (!args || args.workspaceIndex === undefined) {
                return
            }
            const workspaceIndex = Number(args.workspaceIndex)
            if (workspaceIndex < 0 || workspaceIndex >= browser.workspaces.count()) {
                return
            }
            browser.moveTabToWorkspace(tabId, workspaceIndex)
        }
    }

    function clearPendingPermission() {
        pendingPermissionTabId = 0
        pendingPermissionRequestId = 0
        pendingPermissionOrigin = ""
        pendingPermissionKind = 0
        pendingPermissionUserInitiated = false
    }

    function respondPendingPermission(state, remember) {
        const view = tabViews.byId[pendingPermissionTabId]
        const reqId = pendingPermissionRequestId
        clearPendingPermission()
        if (view && reqId > 0) {
            view.respondToPermissionRequest(reqId, state, remember === true)
        }
        popupManager.close()
    }

    function showPermissionDoorhanger(tabId, requestId, origin, kind, userInitiated) {
        if (requestId <= 0) {
            return
        }

        popupManager.close()

        pendingPermissionTabId = tabId
        pendingPermissionRequestId = requestId
        pendingPermissionOrigin = origin || ""
        pendingPermissionKind = Number(kind || 0)
        pendingPermissionUserInitiated = userInitiated === true

        if (!showTopBar || !sitePanelButton.visible) {
            const x = Math.round(root.contentItem.width * 0.5 - 160)
            const y = 56
            const pos = root.contentItem.mapToItem(popupManager, x, y)
            popupManager.openAtPoint(permissionDoorhangerComponent, pos.x, pos.y)
        } else {
            popupManager.openAtItem(permissionDoorhangerComponent, sitePanelButton)
        }
        popupManagerContext = "permission-doorhanger"
    }

    function omniboxPopupOpen() {
        return root.popupManagerContext === "omnibox" && popupManager.opened
    }

    function currentOmniboxView() {
        if (!omniboxPopupOpen()) {
            return null
        }
        const item = popupManager.popupItem
        return item && item.listView ? item.listView : null
    }

    function openOmniboxPopup() {
        const field = activeAddressField()
        const trimmed = String(field && field.text || "").trim()
        if (trimmed.length === 0) {
            return
        }

        const pos = field.mapToItem(popupManager, 0, field.height)
        popupManager.openAtPoint(omniboxPopupComponent, pos.x, pos.y)
        root.popupManagerContext = "omnibox"
    }

    function closeOmniboxPopup() {
        if (root.popupManagerContext === "omnibox") {
            popupManager.close()
        }
    }

    function openOverlay(component, context) {
        if (!component) {
            return
        }
        popupManager.close()
        if (overlayHost.active) {
            overlayHost.hide()
        }
        overlayHost.show(component)
        root.overlayHostContext = context || ""
    }

    function scheduleOmniboxUpdate() {
        omniboxUpdateTimer.stop()
        omniboxUpdateTimer.start()
    }

    function updateOmnibox() {
        omniboxUpdateTimer.stop()
        const field = activeAddressField()
        if (!field || !field.activeFocus) {
            return
        }

        if (suppressNextOmniboxUpdate) {
            suppressNextOmniboxUpdate = false
            return
        }

        const raw = field.text || ""
        omniboxQuery = raw

        const trimmed = raw.trim()
        const focusedUrl = currentFocusedUrlString()

        if (omniboxUtils && omniboxUtils.setWebSuggestionsEnabled) {
            omniboxUtils.setWebSuggestionsEnabled(browser && browser.settings ? browser.settings.webSuggestionsEnabled : false)
        }

        omniboxModel.clear()

        if (!trimmed) {
            root.closeOmniboxPopup()
            return
        }

        const isCommandMode = trimmed.startsWith(">")
        if (isCommandMode) {
            const query = trimmed.slice(1).trim()
            const baseCommands = [
                { group: "Tabs", title: "New Tab", command: "new-tab", args: {}, shortcut: "Ctrl+T" },
                { group: "Windows", title: "New Window", command: "new-window", args: {}, shortcut: "Ctrl+N" },
                { group: "Windows", title: "New Incognito Window", command: "new-incognito-window", args: {}, shortcut: "Ctrl+Shift+N" },
                { group: "Tabs", title: "Close Tab", command: "close-tab", args: { tabId: root.focusedTabId }, shortcut: "Ctrl+W" },
                { group: "Tabs", title: "Restore Closed Tab", command: "restore-closed-tab", args: {}, shortcut: "Ctrl+Shift+T" },
                { group: "Tabs", title: "Duplicate Tab", command: "duplicate-tab", args: { tabId: root.focusedTabId }, shortcut: "" },
                { group: "Tabs", title: "Switch Tab", command: "open-tab-switcher", args: {}, shortcut: "Ctrl+K" },
                { group: "Navigation", title: "Reload", command: "nav-reload", args: {}, shortcut: "Ctrl+R" },
                { group: "Navigation", title: "Stop Loading", command: "nav-stop", args: {}, shortcut: "Esc" },
                { group: "Navigation", title: "Find in Page", command: "open-find", args: {}, shortcut: "Ctrl+F" },
                 { group: "View", title: "Toggle Sidebar", command: "toggle-sidebar", args: {}, shortcut: "Ctrl+B" },
                 { group: "View", title: "Toggle Address Bar", command: "toggle-addressbar", args: {}, shortcut: "Ctrl+Shift+L" },
                 { group: "View", title: root.fullscreenActive ? "Exit Fullscreen" : "Fullscreen", command: "toggle-fullscreen", args: {}, shortcut: "F11" },
                 { group: "View", title: "Zoom In", command: "zoom-in", args: {}, shortcut: "Ctrl++" },
                 { group: "View", title: "Zoom Out", command: "zoom-out", args: {}, shortcut: "Ctrl+-" },
                 { group: "View", title: "Reset Zoom", command: "zoom-reset", args: {}, shortcut: "Ctrl+0" },
                 { group: "View", title: splitView.enabled ? "Unsplit View" : "Split View", command: "toggle-split-view", args: {}, shortcut: "Ctrl+E" },
                 { group: "Tools", title: "Settings", command: "open-settings", args: {}, shortcut: "Ctrl+," },
                 { group: "Tools", title: "Open File", command: "open-file", args: {}, shortcut: "Ctrl+O" },
                 { group: "Output", title: "Print / Save PDF", command: "open-print", args: {}, shortcut: "Ctrl+P" },
                 { group: "Tools", title: "Downloads", command: "open-downloads", args: {}, shortcut: "Ctrl+J" },
                 { group: "Tools", title: "Bookmarks", command: "open-bookmarks", args: {}, shortcut: "Ctrl+D" },
                 { group: "Tools", title: "History", command: "open-history", args: {}, shortcut: "Ctrl+H" },
                 { group: "Tools", title: "Mods", command: "open-mods", args: {}, shortcut: "" },
                 { group: "Tools", title: "Themes", command: "open-themes", args: {}, shortcut: "" },
                 { group: "Tools", title: "Extensions", command: "open-extensions", args: {}, shortcut: "" },
                 { group: "Tools", title: "DevTools", command: "open-devtools", args: {}, shortcut: "F12" },
                 { group: "Workspaces", title: "New Workspace", command: "new-workspace", args: {}, shortcut: "" },
             ]

            for (let i = 0; i < browser.workspaces.count(); i++) {
                baseCommands.push({
                    group: "Workspaces",
                    title: "Switch Workspace: " + browser.workspaces.nameAt(i),
                    command: "switch-workspace",
                    args: { index: i },
                    shortcut: i < 9 ? ("Alt+" + (i + 1)) : "",
                })
            }

            const rows = []
            for (const cmd of baseCommands) {
                const score = omniboxUtils.fuzzyScore(query, cmd.title)
                if (score >= 0) {
                    rows.push({ score: score, cmd: cmd })
                }
            }
            rows.sort((a, b) => b.score - a.score)

            const grouped = {}
            for (const row of rows) {
                const g = row.cmd.group || "Commands"
                if (!grouped[g]) {
                    grouped[g] = []
                }
                grouped[g].push(row.cmd)
            }

            const groupOrder = ["Tabs", "Navigation", "View", "Tools", "Workspaces", "Commands"]
            for (const g of groupOrder) {
                const items = grouped[g]
                if (!items || items.length === 0) {
                    continue
                }
                omniboxModel.append({ type: "header", title: g })
                for (const item of items.slice(0, 12)) {
                    const range = omniboxUtils.matchRange(query, item.title)
                    omniboxModel.append({
                        type: "item",
                        kind: "command",
                        title: item.title,
                        subtitle: item.command,
                        command: item.command,
                        args: item.args || {},
                        shortcut: item.shortcut || "",
                        matchStart: range.start,
                        matchLength: range.length,
                    })
                }
            }

            if (omniboxModel.count > 0) {
                if (!root.omniboxPopupOpen()) {
                    root.openOmniboxPopup()
                }

                Qt.callLater(() => {
                    const view = root.currentOmniboxView()
                    if (view) {
                        view.currentIndex = view.firstSelectableIndex()
                    }
                })
            } else {
                root.closeOmniboxPopup()
            }
            return
        }

        if (raw === focusedUrl && field.selectedText && field.selectedText.length === raw.length) {
            root.closeOmniboxPopup()
            return
        }

        const parsed = interpretOmniboxInput(trimmed)
        const navTitle = parsed.kind === "search" ? ("Search: " + parsed.display) : ("Go to: " + parsed.display)
        const navFaviconKey = faviconCache ? faviconCache.faviconKeyForUrl(parsed.url, 32) : ""
        const navFaviconUrl = faviconCache ? faviconCache.faviconUrlFor(parsed.url, 32) : ""
        omniboxModel.append({ type: "header", title: parsed.kind === "search" ? "Search" : "Navigate" })
        omniboxModel.append({
            type: "item",
            kind: parsed.kind,
            title: navTitle,
            subtitle: parsed.kind === "search" ? parsed.url : parsed.display,
            url: parsed.url,
            faviconKey: navFaviconKey,
            faviconUrl: navFaviconUrl,
            shortcut: "",
            matchStart: -1,
            matchLength: 0,
        })

        if (browser.settings.omniboxActionsEnabled) {
            omniboxModel.append({ type: "header", title: "Actions" })
            omniboxModel.append({
                type: "item",
                kind: "action",
                title: "Open in Split (Right)",
                subtitle: "Open in split view",
                action: "open-split-right",
                url: parsed.url,
                shortcut: "",
                matchStart: -1,
                matchLength: 0,
            })
            omniboxModel.append({
                type: "item",
                kind: "action",
                title: "Open in New Workspace",
                subtitle: "Create workspace and open",
                action: "open-new-workspace",
                url: parsed.url,
                shortcut: "",
                matchStart: -1,
                matchLength: 0,
            })
            omniboxModel.append({
                type: "item",
                kind: "action",
                title: "Copy URL",
                subtitle: "",
                action: "copy-url",
                url: parsed.url,
                shortcut: "",
                matchStart: -1,
                matchLength: 0,
            })
        }

        const bookmarkHits = omniboxUtils.bookmarkSuggestions(bookmarksModel, trimmed, 6)
        if (bookmarkHits && bookmarkHits.length > 0) {
            omniboxModel.append({ type: "header", title: "Bookmarks" })
            for (const b of bookmarkHits) {
                const faviconKey = faviconCache ? faviconCache.faviconKeyForUrl(b.url, 32) : ""
                const faviconUrl = faviconCache ? faviconCache.faviconUrlFor(b.url, 32) : ""
                omniboxModel.append({
                    type: "item",
                    kind: "bookmark",
                    title: b.title,
                    subtitle: b.subtitle,
                    url: b.url,
                    faviconKey: faviconKey,
                    faviconUrl: faviconUrl,
                    shortcut: "",
                    matchStart: b.matchStart,
                    matchLength: b.matchLength,
                })
            }
        }

        const historyHits = omniboxUtils.historySuggestions(historyModel, trimmed, 6)
        if (historyHits && historyHits.length > 0) {
            omniboxModel.append({ type: "header", title: "History" })
            for (const h of historyHits) {
                const faviconKey = faviconCache ? faviconCache.faviconKeyForUrl(h.url, 32) : ""
                const faviconUrl = faviconCache ? faviconCache.faviconUrlFor(h.url, 32) : ""
                omniboxModel.append({
                    type: "item",
                    kind: "history",
                    title: h.title,
                    subtitle: h.subtitle,
                    url: h.url,
                    faviconKey: faviconKey,
                    faviconUrl: faviconUrl,
                    shortcut: "",
                    matchStart: h.matchStart,
                    matchLength: h.matchLength,
                })
            }
        }

        const tabHits = omniboxUtils.tabSuggestions(browser.tabs, trimmed, 8)
        if (tabHits && tabHits.length > 0) {
            omniboxModel.append({ type: "header", title: "Tabs" })
            for (const t of tabHits) {
                omniboxModel.append({
                    type: "item",
                    kind: "tab",
                    title: t.title,
                    subtitle: t.subtitle,
                    tabId: t.tabId,
                    faviconUrl: t.faviconUrl,
                    shortcut: "",
                    matchStart: t.matchStart,
                    matchLength: t.matchLength,
                })
            }
        }

        const wsHits = omniboxUtils.workspaceSuggestions(browser.workspaces, trimmed, 6)
        if (wsHits && wsHits.length > 0) {
            omniboxModel.append({ type: "header", title: "Workspaces" })
            for (const ws of wsHits) {
                omniboxModel.append({
                    type: "item",
                    kind: "workspace",
                    title: ws.title,
                    subtitle: "Switch workspace",
                    workspaceIndex: ws.workspaceIndex,
                    shortcut: ws.shortcut,
                    matchStart: ws.matchStart,
                    matchLength: ws.matchLength,
                })
            }
        }

        if (browser.settings.webSuggestionsEnabled && parsed.kind === "search") {
            omniboxUtils.requestWebSuggestions(trimmed, 6)
        }

        if (omniboxModel.count > 0) {
            if (!root.omniboxPopupOpen()) {
                root.openOmniboxPopup()
            }

            Qt.callLater(() => {
                const view = root.currentOmniboxView()
                if (!view) {
                    return
                }
                if (!view.isSelectableIndex(view.currentIndex)) {
                    view.currentIndex = view.firstSelectableIndex()
                }
            })
        } else {
            root.closeOmniboxPopup()
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
                    text: root.fullscreenActive ? "Exit Fullscreen" : "Fullscreen"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("toggle-fullscreen")
                    }
                }

                Button {
                    text: "Open File…"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-file")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        Layout.fillWidth: true
                        text: "Zoom In"
                        onClicked: commands.invoke("zoom-in")
                    }

                    Button {
                        Layout.fillWidth: true
                        text: "Zoom Out"
                        onClicked: commands.invoke("zoom-out")
                    }
                }

                Button {
                    text: "Reset Zoom"
                    onClicked: commands.invoke("zoom-reset")
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
                        root.openOverlay(themesDialogComponent, "themes")
                    }
                }

                Button {
                    text: "Settings"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-settings")
                    }
                }

                Button {
                    text: "Extensions"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-extensions")
                    }
                }

                Button {
                    text: "Downloads"
                    onClicked: {
                        root.openOverlay(downloadsDialogComponent, "downloads")
                    }
                }

                Button {
                    text: "Bookmarks"
                    onClicked: {
                        popupManager.close()
                        commands.invoke("open-bookmarks")
                    }
                }

                Button {
                    text: "Welcome"
                    onClicked: {
                        root.openOverlay(onboardingDialogComponent, "onboarding")
                    }
                }
            }
        }
    }

    Component {
        id: sitePanelComponent

        SiteInfoPanel {
            cornerRadius: root.uiRadius
            spacing: root.uiSpacing
            pageUrl: root.focusedView ? root.focusedView.currentUrl : ""
            canShare: shareController && shareController.canShare
            onCloseRequested: popupManager.close()
            onPermissionsRequested: root.toggleTopBarPopup("site-permissions", sitePermissionsPanelComponent, sitePanelButton)
            onSiteDataRequested: root.toggleTopBarPopup("site-data", siteDataPanelComponent, sitePanelButton)
            onCopyUrlRequested: {
                popupManager.close()
                commands.invoke("copy-url", { tabId: root.focusedTabId })
            }
            onShareUrlRequested: {
                popupManager.close()
                commands.invoke("share-url", { tabId: root.focusedTabId })
            }
            onExtensionsRequested: {
                popupManager.close()
                root.openExtensionsPanel(sitePanelButton)
            }
            onDevToolsRequested: {
                popupManager.close()
                commands.invoke("open-devtools")
            }
        }
    }

    Component {
        id: siteDataPanelComponent

        SiteDataPanel {
            cornerRadius: root.uiRadius
            spacing: root.uiSpacing
            view: root.focusedView
            pageUrl: root.focusedView ? root.focusedView.currentUrl : ""
            host: root.focusedView ? root.focusedView.currentUrl.host : ""
            onCloseRequested: popupManager.close()
        }
    }

    Component {
        id: sitePermissionsPanelComponent

        SitePermissionsPanel {
            cornerRadius: root.uiRadius
            spacing: root.uiSpacing
            store: sitePermissions
            origin: root.currentFocusedOriginString()
            onCloseRequested: popupManager.close()
        }
    }

    Component {
        id: extensionsPanelComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: root.uiRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 360
            implicitHeight: Math.min(520, panelColumn.implicitHeight + 16)

            ColumnLayout {
                id: panelColumn
                anchors.fill: parent
                anchors.margins: root.uiSpacing
                spacing: root.uiSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.uiSpacing

                    Label {
                        Layout.fillWidth: true
                        text: "Extensions"
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    ToolButton {
                        text: "Manage"
                        onClicked: {
                            popupManager.close()
                            commands.invoke("open-extensions")
                        }
                    }

                    ToolButton {
                        text: "×"
                        onClicked: popupManager.close()
                    }
                }

                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Search extensions"
                    text: root.extensionsPanelSearch
                    selectByMouse: true
                    onTextChanged: root.extensionsPanelSearch = text
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ListView {
                        id: extList
                        anchors.fill: parent
                        clip: true
                        model: extensions

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            hoverEnabled: true
                            visible: {
                                const q = String(root.extensionsPanelSearch || "").trim().toLowerCase()
                                if (!q) {
                                    return true
                                }
                                const n = String(name || "").toLowerCase()
                                const id = String(extensionId || "").toLowerCase()
                                return n.indexOf(q) >= 0 || id.indexOf(q) >= 0
                            }
                            height: visible ? implicitHeight : 0

                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"
                            }

                            contentItem: RowLayout {
                                anchors.fill: parent
                                spacing: root.uiSpacing

                                Item {
                                    width: 20
                                    height: 20
                                    Layout.alignment: Qt.AlignVCenter

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 6
                                        color: Qt.rgba(0, 0, 0, 0.06)
                                        border.width: 1
                                        border.color: Qt.rgba(0, 0, 0, 0.08)
                                        visible: !(iconUrl && iconUrl.toString().length > 0)

                                        Text {
                                            anchors.centerIn: parent
                                            text: name && name.length > 0 ? name[0].toUpperCase() : ""
                                            font.pixelSize: 10
                                            opacity: 0.65
                                        }
                                    }

                                    Image {
                                        anchors.fill: parent
                                        source: iconUrl
                                        asynchronous: true
                                        cache: true
                                        fillMode: Image.PreserveAspectFit
                                        visible: iconUrl && iconUrl.toString().length > 0
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: name && name.length > 0 ? name : extensionId
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: extensionId
                                        opacity: 0.55
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        visible: extensionId && extensionId.length > 0
                                    }
                                }

                                ToolButton {
                                    text: pinned ? "Unpin" : "Pin"
                                    onClicked: extensions.setExtensionPinned(extensionId, !pinned)
                                }

                                Switch {
                                    checked: enabled === true
                                    onToggled: extensions.setExtensionEnabled(extensionId, checked)
                                }

                                ToolButton {
                                    text: "Open"
                                    enabled: enabled === true
                                    onClicked: {
                                        popupManager.close()
                                        root.openExtensionPopup(extensionsButton, extensionId, name, popupUrl, optionsUrl)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: extensionPopupComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            border.color: Qt.rgba(0, 0, 0, 0.10)
            border.width: 1

            implicitWidth: 420
            implicitHeight: 560

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    color: Qt.rgba(0, 0, 0, 0.04)
                    border.color: Qt.rgba(0, 0, 0, 0.08)
                    border.width: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: root.extensionPopupName
                            elide: Text.ElideRight
                        }

                        ToolButton {
                            text: "Options"
                            visible: root.extensionPopupOptionsUrl && root.extensionPopupOptionsUrl.length > 0
                            onClicked: {
                                popupManager.close()
                                commands.invoke("new-tab", { url: root.extensionPopupOptionsUrl })
                            }
                        }

                        ToolButton {
                            text: "Open tab"
                            visible: root.extensionPopupUrl && root.extensionPopupUrl.length > 0
                            onClicked: {
                                popupManager.close()
                                commands.invoke("new-tab", { url: root.extensionPopupUrl })
                            }
                        }

                        ToolButton {
                            text: "×"
                            onClicked: popupManager.close()
                        }
                    }
                }

                WebView2View {
                    id: extPopupWeb
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Component.onCompleted: {
                        root.extensionPopupView = extPopupWeb
                        root.pushModsCss(extPopupWeb)
                        if (root.extensionPopupUrl && root.extensionPopupUrl.length > 0) {
                            navigate(root.extensionPopupUrl)
                        }
                    }

                    Component.onDestruction: {
                        if (root.extensionPopupView === extPopupWeb) {
                            root.extensionPopupView = null
                        }
                    }
                }
            }
        }
    }

    Component {
        id: downloadsPanelComponent

        Rectangle {
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: root.uiRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            implicitWidth: 420
            implicitHeight: panelColumn.implicitHeight + root.uiSpacing * 2

            ColumnLayout {
                id: panelColumn
                anchors.fill: parent
                anchors.margins: root.uiSpacing
                spacing: root.uiSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.uiSpacing

                    Label {
                        Layout.fillWidth: true
                        text: "Downloads"
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Button {
                        text: "Clear"
                        enabled: downloadsList.count > downloads.activeCount
                        onClicked: downloads.clearFinished()
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: downloads.activeCount > 0 ? ("Active: " + downloads.activeCount) : "No active downloads"
                    opacity: 0.75
                    font.pixelSize: 12
                }

                Frame {
                    Layout.fillWidth: true

                    ListView {
                        id: downloadsList
                        width: parent.width
                        implicitHeight: Math.min(contentHeight, 360)
                        clip: true
                        model: downloads

                        delegate: ItemDelegate {
                            width: ListView.view.width

                            contentItem: RowLayout {
                                spacing: theme.spacing

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Math.max(2, Math.round(theme.spacing / 4))

                                    Label {
                                        Layout.fillWidth: true
                                        text: uri && uri.length > 0 ? uri : filePath
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: state
                                        opacity: 0.7
                                        font.pixelSize: 12
                                    }
                                }

                                Button {
                                    text: "Open"
                                    enabled: filePath && filePath.length > 0
                                    onClicked: {
                                        const ok = nativeUtils.openPath(filePath)
                                        if (!ok) {
                                            const err = nativeUtils.lastError || ""
                                            toast.showToast(err.length > 0 ? ("Open failed: " + err) : "Open failed")
                                        }
                                    }
                                }

                                Button {
                                    text: "Folder"
                                    enabled: filePath && filePath.length > 0
                                    onClicked: {
                                        const ok = nativeUtils.openFolder(filePath)
                                        if (!ok) {
                                            const err = nativeUtils.lastError || ""
                                            toast.showToast(err.length > 0 ? ("Open folder failed: " + err) : "Open folder failed")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: "Open Full List…"
                    onClicked: {
                        root.openOverlay(downloadsDialogComponent, "downloads")
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
                                    const tabId = browser.tabs.tabIdAt(idx)
                                    if (!splitView.enabled) {
                                        splitView.paneCount = 2
                                        splitView.setTabIdForPane(0, root.tabIdForActiveIndex())
                                        splitView.enabled = true
                                    } else if (splitView.paneCount < 2) {
                                        splitView.paneCount = 2
                                    }
                                    splitView.setTabIdForPane(1, tabId)
                                    splitView.focusedPane = 1
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
                        Component.onCompleted: {
                            root.glanceView = glanceWeb
                            root.pushModsCss(glanceWeb)
                            navigate(root.glanceUrl)
                        }

                        Component.onDestruction: {
                            if (root.glanceView === glanceWeb) {
                                root.glanceView = null
                            }
                        }

                        onWebMessageReceived: (json) => root.handleWebMessage(0, json, glanceWeb)

                        Rectangle {
                            anchors.fill: parent
                            visible: !glanceWeb.initialized && glanceWeb.initError && glanceWeb.initError.length > 0
                            color: Qt.rgba(1, 1, 1, 0.96)
                            border.color: Qt.rgba(0, 0, 0, 0.08)
                            border.width: 1
                            radius: theme.cornerRadius

                            ColumnLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 48, 520)
                                spacing: theme.spacing

                                Label {
                                    Layout.fillWidth: true
                                    text: "Web engine unavailable"
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: glanceWeb.initError
                                    wrapMode: Text.Wrap
                                    opacity: 0.85
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.spacing

                                    Button {
                                        text: "Install WebView2 Runtime"
                                        onClicked: Qt.openUrlExternally("https://go.microsoft.com/fwlink/p/?LinkId=2124703")
                                    }

                                    Button {
                                        text: "Retry"
                                        onClicked: glanceWeb.retryInitialize()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        layoutController.fullscreen = root.fullscreenActive
        windowChrome.attach(root)
        windowChrome.setCaptionItem(topBar)
        windowChrome.setCaptionExcludeItems([
            sidebarButton,
            backButton,
            forwardButton,
            reloadButton,
            sitePanelButton,
            addressField,
            bookmarkButton,
            emojiButton,
            newTabButton,
            extensionsToolbar,
            extensionsButton,
            downloadsButton,
            mainMenuButton
        ])
        windowChrome.setMinimizeButtonItem(windowMinimizeButton)
        windowChrome.setMaximizeButtonItem(windowMaximizeButton)
        windowChrome.setCloseButtonItem(windowCloseButton)

        if (browser.tabs.count() === 0) {
            commands.invoke("new-tab", { url: "https://example.com" })
        }

        if (!browser.settings.onboardingSeen) {
            root.openOverlay(onboardingDialogComponent, "onboarding")
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
        root.glanceScript = glanceScript

        root.webPanelUrl = browser.settings.webPanelUrl
        root.webPanelTitle = browser.settings.webPanelTitle
        if (browser.settings.webPanelVisible
                && root.webPanelUrl
                && root.webPanelUrl.toString().length > 0
                && root.webPanelUrl.toString() !== "about:blank"
                && webPanelWeb
                && webPanelWeb.navigate) {
            webPanelWeb.navigate(root.webPanelUrl)
        }

        if (splitView) {
            splitView.enabledChanged.connect(syncSplitViews)
            splitView.tabsChanged.connect(syncSplitViews)
            splitView.focusedPaneChanged.connect(syncSplitViews)
        }
        Qt.callLater(syncTabViews)
    }

    header: ToolBar {
        id: topBar
        readonly property int expandedHeight: 56
        height: showTopBar ? expandedHeight : (root.fullscreenActive ? 0 : root.windowControlButtonHeight)
        leftPadding: 6
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        Behavior on height {
            NumberAnimation {
                duration: browser.settings.reduceMotion ? 0 : theme.motionNormalMs
                easing.type: Easing.InOutCubic
            }
        }

        HoverHandler {
            onHoveredChanged: layoutController.topBarHovered = hovered
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
                enabled: root.focusedView ? root.focusedView.canGoBack : false
                onClicked: commands.invoke("nav-back")
            }
            ToolButton {
                id: forwardButton
                visible: showTopBar
                text: "→"
                enabled: root.focusedView ? root.focusedView.canGoForward : false
                onClicked: commands.invoke("nav-forward")
            }
            ToolButton {
                id: reloadButton
                visible: showTopBar
                text: (root.focusedView && root.focusedView.isLoading) ? "\u00D7" : "\u27F3"
                onClicked: commands.invoke((root.focusedView && root.focusedView.isLoading) ? "nav-stop" : "nav-reload")
            }

            ToolButton {
                id: sitePanelButton
                visible: showTopBar
                text: "ⓘ"
                onClicked: root.toggleTopBarPopup("site-panel", sitePanelComponent, sitePanelButton)
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
                onTextChanged: scheduleOmniboxUpdate()
                onActiveFocusChanged: {
                    root.syncAddressFocusState()
                    if (activeFocus) {
                        Qt.callLater(() => addressField.selectAll())
                        return
                    }
                    if (!layoutController.addressFieldFocused) {
                        root.closeOmniboxPopup()
                    }
                }
                onAccepted: {
                    if (root.omniboxPopupOpen()) {
                        const view = root.currentOmniboxView()
                        if (view) {
                            view.activateCurrent()
                        }
                        return
                    }
                    if ((text || "").trim().startsWith(">")) {
                        return
                    }
                    root.navigateFromOmnibox(text)
                    addressField.focus = false
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Down) {
                        if (!root.omniboxPopupOpen()) {
                            updateOmnibox()
                            Qt.callLater(() => {
                                const view = root.currentOmniboxView()
                                if (view) {
                                    view.currentIndex = view.firstSelectableIndex()
                                }
                            })
                        } else {
                            const view = root.currentOmniboxView()
                            if (view) {
                                view.moveSelection(1)
                            }
                        }
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Up) {
                        const view = root.currentOmniboxView()
                        if (view) {
                            view.moveSelection(-1)
                            event.accepted = true
                        }
                        return
                    }
                    if (event.key === Qt.Key_Tab) {
                        const view = root.currentOmniboxView()
                        if (view) {
                            view.activateCurrent()
                            event.accepted = true
                        }
                        return
                    }
                }

                Keys.onEscapePressed: (event) => {
                    if (root.omniboxPopupOpen()) {
                        root.closeOmniboxPopup()
                        event.accepted = true
                        return
                    }
                    const currentUrl = root.currentFocusedUrlString()
                    if (addressField.text !== currentUrl) {
                        root.syncAddressFieldFromFocused(true)
                        Qt.callLater(() => addressField.selectAll())
                        event.accepted = true
                        return
                    }
                    addressField.focus = false
                    event.accepted = true
                }
            }

            ToolButton {
                id: zoomIndicatorButton
                visible: showTopBar && root.focusedView && Math.abs((root.focusedView.zoomFactor || 1.0) - 1.0) > 0.001
                text: {
                    const view = root.focusedView
                    const factor = view ? (view.zoomFactor || 1.0) : 1.0
                    return Math.round(factor * 100) + "%"
                }
                onClicked: commands.invoke("zoom-reset")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: "Reset zoom"
            }

            ToolButton {
                id: bookmarkButton
                visible: showTopBar
                text: {
                    const view = root.focusedView
                    if (!view || !bookmarks) {
                        return "☆"
                    }
                    return bookmarks.isBookmarked(view.currentUrl) ? "★" : "☆"
                }
                enabled: root.focusedView && root.focusedView.currentUrl && root.focusedView.currentUrl.toString().length > 0
                onClicked: {
                    const view = root.focusedView
                    if (!view || !bookmarks) {
                        return
                    }
                    const wasBookmarked = bookmarks.isBookmarked(view.currentUrl)
                    bookmarks.toggleBookmark(view.currentUrl, view.title)
                    toast.showToast(wasBookmarked ? "Bookmark removed" : "Bookmarked")
                }
            }

            ToolButton {
                id: shareUrlButton
                visible: showTopBar
                text: (shareController && shareController.canShare) ? "Share" : "Copy"
                enabled: root.focusedView
                         && root.focusedView.currentUrl
                         && root.focusedView.currentUrl.toString
                         && root.focusedView.currentUrl.toString().length > 0
                         && root.focusedView.currentUrl.toString() !== "about:blank"
                onClicked: commands.invoke((shareController && shareController.canShare) ? "share-url" : "copy-url", { tabId: root.focusedTabId })
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: (shareController && shareController.canShare) ? "Share URL" : "Copy URL"
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
                onClicked: root.toggleTopBarPopup("emoji-picker", emojiPickerComponent, emojiButton)
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

            ExtensionsFilterModel {
                id: pinnedExtensionsModel
                sourceExtensions: extensions
                pinnedOnly: true
            }

            RowLayout {
                id: extensionsToolbar
                visible: showTopBar
                spacing: 2
                readonly property int maxWidth: Math.max(extensionsOverflowButton.width, Math.min(320, Math.round(topBar.width * 0.25)))
                Layout.maximumWidth: maxWidth
                clip: true

                readonly property int buttonSize: 34
                readonly property int pinnedCount: pinnedExtensionsRepeater.count
                readonly property int availableWidth: Math.max(0, maxWidth)
                readonly property int visiblePinnedCount: extensionsStore.visiblePinnedCountForWidth(
                                                        pinnedCount,
                                                        availableWidth,
                                                        buttonSize,
                                                        spacing,
                                                        extensionsOverflowButton.width)
                readonly property bool overflowNeeded: pinnedCount > visiblePinnedCount

                function buildOverflowMenuItems() {
                    const items = []
                    const start = Math.max(0, visiblePinnedCount)
                    for (let i = start; i < pinnedExtensionsRepeater.count; ++i) {
                        const button = pinnedExtensionsRepeater.itemAt(i)
                        if (!button) {
                            continue
                        }

                        const id = String(button.extensionId || "")
                        if (!id || id.length === 0) {
                            continue
                        }

                        const displayName = String(button.name || id || "Extension")
                        items.push({
                            action: "ext-open-popup",
                            text: displayName + (button.enabled === true ? "" : " (disabled)"),
                            enabled: true,
                            args: {
                                extensionId: id,
                                name: displayName,
                                popupUrl: String(button.popupUrl || ""),
                                optionsUrl: String(button.optionsUrl || ""),
                                enabled: button.enabled === true,
                                pinned: true,
                            },
                        })
                    }
                    return items
                }

                Repeater {
                    id: pinnedExtensionsRepeater
                    model: pinnedExtensionsModel

                    delegate: ToolButton {
                        id: pinnedExtensionButton
                        required property string extensionId
                        required property string name
                        required property bool enabled
                        required property bool pinned
                        required property var iconUrl
                        required property string popupUrl
                        required property string optionsUrl

                        visible: index < extensionsToolbar.visiblePinnedCount
                        width: extensionsToolbar.buttonSize
                        height: extensionsToolbar.buttonSize
                        display: (iconUrl && iconUrl.toString().length > 0) ? AbstractButton.IconOnly : AbstractButton.TextOnly
                        icon.source: iconUrl
                        icon.width: 18
                        icon.height: 18
                        text: name && name.length > 0 ? name[0].toUpperCase() : ""
                        opacity: enabled ? 1.0 : 0.45
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: (name && name.length > 0 ? name : extensionId) + (enabled ? "" : " (disabled)")

                        onClicked: {
                            if (!enabled) {
                                toast.showToast("Extension is disabled")
                                commands.invoke("open-extensions")
                                return
                            }
                            root.openExtensionPopup(pinnedExtensionButton, extensionId, name, popupUrl, optionsUrl)
                        }

                        Component {
                            id: extensionContextMenuComponent

                            ContextMenu {
                                cornerRadius: root.uiRadius
                                spacing: root.uiSpacing
                                implicitWidth: 240
                                items: root.buildExtensionContextMenuItems(extensionId, name, enabled, pinned, popupUrl, optionsUrl)
                                onActionTriggered: (action, args) => {
                                    popupManager.close()
                                    root.handleExtensionContextMenuAction(action, args, pinnedExtensionButton)
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: (mouse) => {
                                const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                popupManager.openAtPoint(extensionContextMenuComponent, pos.x, pos.y)
                                root.popupManagerContext = "extensions-context-menu"
                            }
                        }
                    }
                }

                ToolButton {
                    id: extensionsOverflowButton
                    visible: extensionsToolbar.overflowNeeded
                    width: extensionsToolbar.buttonSize
                    height: extensionsToolbar.buttonSize
                    text: "\u22EF"
                    onClicked: root.toggleTopBarPopup("extensions-overflow", extensionsOverflowMenuComponent, extensionsOverflowButton)
                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: "More extensions"
                }

                Component {
                    id: extensionsOverflowMenuComponent

                    ContextMenu {
                        cornerRadius: root.uiRadius
                        spacing: root.uiSpacing
                        implicitWidth: 260
                        items: extensionsToolbar.buildOverflowMenuItems()
                        onActionTriggered: (action, args) => {
                            popupManager.close()
                            root.handleExtensionContextMenuAction(action, args, extensionsOverflowButton)
                        }
                    }
                }
            }

            ToolButton {
                id: extensionsButton
                visible: showTopBar
                text: "Ext"
                enabled: extensions && extensions.ready === true
                onClicked: root.openExtensionsPanel(extensionsButton)
            }

            ToolButton {
                id: downloadsButton
                visible: showTopBar
                text: downloads.activeCount > 0 ? ("↓" + downloads.activeCount) : "↓"
                onClicked: root.toggleTopBarPopup("downloads-panel", downloadsPanelComponent, downloadsButton)
            }

            ToolButton {
                id: mainMenuButton
                text: "⋮"
                onClicked: root.toggleTopBarPopup("main-menu", mainMenuComponent, mainMenuButton)
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
                        color: parent.pressed
                                   ? Qt.rgba(0, 0, 0, 0.12)
                                   : (parent.hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent")
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
                        color: parent.pressed
                                   ? Qt.rgba(0, 0, 0, 0.12)
                                   : (parent.hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent")
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
                        color: parent.pressed ? "#c50f1f" : (parent.hovered ? "#e81123" : "transparent")
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: parent.hovered || parent.pressed ? "#ffffff" : "#1f1f1f"
                        font.pixelSize: 16
                    }
                }
            }
        }
    }

    ListModel {
        id: omniboxModel
    }

    Connections {
        target: omniboxUtils

        function onWebSuggestionsReady(query, suggestions) {
            const field = root.activeAddressField()
            if (!field || !field.activeFocus) {
                return
            }

            const currentQuery = String(field.text || "").trim()
            const expectedQuery = String(query || "").trim()
            if (!currentQuery || !expectedQuery || currentQuery !== expectedQuery) {
                return
            }

            if (currentQuery.startsWith(">") || !browser.settings.webSuggestionsEnabled) {
                return
            }

            for (let i = omniboxModel.count - 1; i >= 0; i--) {
                const it = omniboxModel.get(i)
                if (it && it.group === "web-suggestions") {
                    omniboxModel.remove(i)
                }
            }

            if (!suggestions || suggestions.length === 0) {
                return
            }

             omniboxModel.append({ type: "header", title: "Web Suggestions", group: "web-suggestions" })
             for (const s of suggestions) {
                 const text = String(s || "").trim()
                 if (!text) {
                     continue
                 }

                 const range = omniboxUtils.matchRange(currentQuery, text)
                 const url = "https://duckduckgo.com/?q=" + encodeURIComponent(text)
                 const faviconKey = faviconCache ? faviconCache.faviconKeyForUrl(url, 32) : ""
                 const faviconUrl = faviconCache ? faviconCache.faviconUrlFor(url, 32) : ""
                 omniboxModel.append({
                     type: "item",
                     kind: "search",
                     title: text,
                     subtitle: "",
                     url: url,
                     faviconKey: faviconKey,
                     faviconUrl: faviconUrl,
                     group: "web-suggestions",
                     shortcut: "",
                     matchStart: range.start,
                     matchLength: range.length,
                 })
             }
        }
    }

    Connections {
        target: faviconCache

        function onFaviconAvailable(key, faviconUrl) {
            const k = String(key || "")
            if (!k || !faviconUrl || faviconUrl.toString().length === 0) {
                return
            }

            for (let i = 0; i < omniboxModel.count; i++) {
                const item = omniboxModel.get(i)
                if (item && item.faviconKey === k) {
                    omniboxModel.setProperty(i, "faviconUrl", faviconUrl)
                }
            }
        }
    }

    Component {
        id: omniboxPopupComponent

        Rectangle {
            id: omniboxPopupRoot
            color: Qt.rgba(1, 1, 1, 0.98)
            radius: root.uiRadius
            border.color: Qt.rgba(0, 0, 0, 0.08)
            border.width: 1

            readonly property var anchorField: root.activeAddressField()
            implicitWidth: Math.max(1, (anchorField ? anchorField.width : addressField.width))
            implicitHeight: omniboxView.implicitHeight
            width: implicitWidth
            height: implicitHeight

            property alias listView: omniboxView

            ListView {
                id: omniboxView
                anchors.fill: parent
                implicitHeight: Math.min(contentHeight, 240)
                clip: true
                model: omniboxModel
                currentIndex: -1

            function isSelectableIndex(idx) {
                if (idx < 0 || idx >= omniboxModel.count) {
                    return false
                }
                const item = omniboxModel.get(idx)
                return item && item.type !== "header"
            }

            function firstSelectableIndex() {
                for (let i = 0; i < omniboxModel.count; i++) {
                    if (isSelectableIndex(i)) {
                        return i
                    }
                }
                return -1
            }

            function moveSelection(delta) {
                const count = omniboxModel.count
                if (count <= 0) {
                    return
                }

                let idx = currentIndex
                if (idx < 0) {
                    idx = firstSelectableIndex()
                    if (idx < 0) {
                        return
                    }
                    currentIndex = idx
                    positionViewAtIndex(idx, ListView.Visible)
                    return
                }

                for (let step = 0; step < count; step++) {
                    idx = (idx + delta + count) % count
                    if (isSelectableIndex(idx)) {
                        currentIndex = idx
                        positionViewAtIndex(idx, ListView.Visible)
                        return
                    }
                }
            }

            function activateCurrent() {
                let idx = currentIndex
                if (!isSelectableIndex(idx)) {
                    idx = firstSelectableIndex()
                    if (!isSelectableIndex(idx)) {
                        return
                    }
                    currentIndex = idx
                }
                if (idx < 0 || idx >= omniboxModel.count) {
                    return
                }
                const item = omniboxModel.get(idx)
                if (!item) {
                    return
                }
                root.closeOmniboxPopup()

                if (item.kind === "command") {
                    commands.invoke(item.command, item.args || {})
                    root.suppressNextOmniboxUpdate = true
                    const field = root.activeAddressField()
                    if (field) {
                        field.text = ""
                        field.forceActiveFocus()
                    }
                    return
                 }
 
                 const field = root.activeAddressField()
                 if (item.kind === "action") {
                     const action = item.action || ""
                     const url = item.url || ""

                     if (action === "copy-url") {
                         commands.invoke("copy-text", { text: url })
                         return
                     }

                     if (!url || url.length === 0) {
                         return
                     }

                     if (action === "open-new-workspace") {
                         const workspaces = browser.workspaces
                         if (!workspaces) {
                             return
                         }
                         const nextNumber = workspaces.count() + 1
                         const wsIndex = workspaces.addWorkspace("Workspace " + nextNumber)
                         workspaces.activeIndex = wsIndex
                         browser.newTab(url)
                         if (field) {
                             field.focus = false
                         }
                         return
                     }

                     if (action === "open-split-right") {
                         const leftTabId = root.tabIdForActiveIndex()
                         const idx = browser.newTab(url)
                         const model = browser.tabs
                         const rightTabId = (idx >= 0 && model) ? model.tabIdAt(idx) : 0
                         if (rightTabId <= 0) {
                             return
                         }

                         if (!splitView.enabled) {
                             splitView.paneCount = 2
                             splitView.setTabIdForPane(0, leftTabId > 0 ? leftTabId : rightTabId)
                             splitView.enabled = true
                         } else if (splitView.paneCount < 2) {
                             splitView.paneCount = 2
                         }

                         splitView.setTabIdForPane(1, rightTabId)
                         splitView.focusedPane = 1
                         if (field) {
                             field.focus = false
                         }
                         return
                     }

                     return
                 }
                 if (item.kind === "tab") {
                     browser.activateTabById(item.tabId)
                     if (field) {
                         field.focus = false
                     }
                    return
                }

                if (item.kind === "workspace") {
                    commands.invoke("switch-workspace", { index: item.workspaceIndex })
                    if (field) {
                        field.focus = false
                    }
                    return
                }

                if (item.kind === "url" || item.kind === "search" || item.kind === "bookmark" || item.kind === "history") {
                    if (root.focusedView) {
                        root.focusedView.navigate(item.url)
                    } else {
                        commands.invoke("new-tab", { url: item.url })
                    }
                    if (field) {
                        field.focus = false
                    }
                    return
                }
            }

            delegate: ItemDelegate {
                width: ListView.view.width
                height: type === "header" ? 28 : 44
                enabled: type !== "header"
                hoverEnabled: enabled
                highlighted: enabled && ListView.isCurrentItem

                background: Rectangle {
                    radius: 6
                    color: highlighted ? Qt.rgba(0, 0, 0, 0.06) : "transparent"
                }

                onClicked: {
                    if (!enabled) {
                        return
                    }
                    omniboxView.currentIndex = index
                    omniboxView.activateCurrent()
                }

                contentItem: Item {
                    anchors.fill: parent
                    anchors.margins: 8

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            visible: type === "header"
                            text: title
                            font.pixelSize: 12
                            opacity: 0.7
                        }

                        Item {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            visible: type !== "header"

                             Rectangle {
                                 anchors.fill: parent
                                 radius: 6
                                 color: Qt.rgba(0, 0, 0, 0.06)
                                 visible: !(faviconUrl && faviconUrl.toString().length > 0)

                                     Text {
                                         anchors.centerIn: parent
                                         text: kind === "command"
                                               ? ">"
                                          : (kind === "action"
                                                 ? "A"
                                                 : (kind === "tab"
                                                  ? "T"
                                                  : (kind === "workspace"
                                                         ? "W"
                                                         : (kind === "bookmark"
                                                                ? "\u2605"
                                                                : (kind === "history" ? "H" : (kind === "search" ? "S" : "U")))))
                                                )
                                         opacity: 0.7
                                         font.pixelSize: 10
                                     }
                                 }

                             Image {
                                 anchors.fill: parent
                                 source: faviconUrl
                                 asynchronous: true
                                 cache: true
                                 fillMode: Image.PreserveAspectFit
                                 visible: faviconUrl && faviconUrl.toString().length > 0
                             }
                         }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: type !== "header"
                            spacing: 2

                            Item {
                                Layout.fillWidth: true
                                height: titleRow.implicitHeight
                                clip: true

                                Row {
                                    id: titleRow
                                    anchors.fill: parent
                                    spacing: 0

                                    readonly property string textValue: title || ""
                                    readonly property int start: matchStart !== undefined ? matchStart : -1
                                    readonly property int length: matchLength !== undefined ? matchLength : 0
                                    readonly property string pre: start >= 0 ? textValue.slice(0, start) : textValue
                                    readonly property string mid: start >= 0 ? textValue.slice(start, start + length) : ""
                                    readonly property string suf: start >= 0 ? textValue.slice(start + length) : ""

                                    Text {
                                        id: titlePrefix
                                        text: titleRow.pre
                                        font.pixelSize: 13
                                        color: "#1f1f1f"
                                    }
                                    Text {
                                        id: titleMatch
                                        text: titleRow.mid
                                        font.pixelSize: 13
                                        font.bold: true
                                        color: "#1f1f1f"
                                    }
                                    Text {
                                        text: titleRow.suf
                                        font.pixelSize: 13
                                        color: "#1f1f1f"
                                        elide: Text.ElideRight
                                        width: Math.max(0, titleRow.width - titlePrefix.implicitWidth - titleMatch.implicitWidth)
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: subtitle || ""
                                font.pixelSize: 11
                                opacity: 0.65
                                elide: Text.ElideRight
                                visible: subtitle && subtitle.length > 0
                            }
                        }

                        Text {
                            text: shortcut || ""
                            visible: type !== "header" && shortcut && shortcut.length > 0
                            opacity: 0.6
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }
            }
        }
    }
    }

    Component {
        id: webContextMenuComponent

        ContextMenu {
            cornerRadius: root.uiRadius
            spacing: root.uiSpacing
            maxHeight: Math.max(200, popupManager.height - root.uiSpacing * 2)
            items: root.webContextMenuItems
            onActionTriggered: (action, args) => {
                popupManager.close()
                root.handleWebContextMenuAction(action)
            }
        }
    }

    Component {
        id: permissionDoorhangerComponent

        PermissionDoorhanger {
            cornerRadius: root.uiRadius
            spacing: root.uiSpacing
            origin: root.pendingPermissionOrigin
            permissionKind: root.pendingPermissionKind
            userInitiated: root.pendingPermissionUserInitiated
            onResponded: (state, remember) => root.respondPendingPermission(state, remember)
        }
    }

    Component {
        id: emojiPickerComponent

        EmojiPicker {
            onEmojiSelected: (emoji) => {
                insertEmojiInto(addressField, emoji)
                popupManager.close()
            }
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 6
        hoverEnabled: true
        visible: (browser.settings.compactMode || root.fullscreenActive) && browser.settings.addressBarVisible
        z: 2000
        onEntered: {
            compactTopExitTimer.stop()
            compactTopEnterTimer.stop()
            compactTopEnterTimer.start()
        }
        onExited: {
            compactTopEnterTimer.stop()
            compactTopExitTimer.stop()
            compactTopExitTimer.start()
        }
    }

    MouseArea {
        anchors.left: browser.settings.sidebarOnRight ? undefined : parent.left
        anchors.right: browser.settings.sidebarOnRight ? parent.right : undefined
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        hoverEnabled: true
        visible: (browser.settings.compactMode || root.fullscreenActive) && browser.settings.sidebarExpanded
        z: 2000
        onEntered: {
            compactSidebarExitTimer.stop()
            compactSidebarEnterTimer.stop()
            compactSidebarEnterTimer.start()
        }
        onExited: {
            compactSidebarEnterTimer.stop()
            compactSidebarExitTimer.stop()
            compactSidebarExitTimer.start()
        }
    }

    GridLayout {
        anchors.fill: parent
        columns: 5
        columnSpacing: 0
        rowSpacing: 0

        Rectangle {
            id: sidebarPane
            Layout.column: browser.settings.sidebarOnRight ? 4 : 0
            Layout.preferredWidth: showSidebar ? browser.settings.sidebarWidth : 0
            Layout.fillHeight: true
            visible: true
            opacity: showSidebar ? 1.0 : 0.0
            clip: true
            color: Qt.rgba(0, 0, 0, 0.04)

            Behavior on Layout.preferredWidth {
                NumberAnimation {
                    duration: browser.settings.reduceMotion ? 0 : theme.motionNormalMs
                    easing.type: Easing.InOutCubic
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                    easing.type: Easing.OutCubic
                }
            }

            HoverHandler {
                onHoveredChanged: layoutController.sidebarHovered = hovered
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    visible: root.singleToolbarActive() && browser.settings.addressBarVisible

                    ToolButton {
                        text: "←"
                        enabled: root.focusedView ? root.focusedView.canGoBack : false
                        onClicked: commands.invoke("nav-back")
                    }
                    ToolButton {
                        text: "→"
                        enabled: root.focusedView ? root.focusedView.canGoForward : false
                        onClicked: commands.invoke("nav-forward")
                    }
                    ToolButton {
                        text: (root.focusedView && root.focusedView.isLoading) ? "\u00D7" : "\u27F3"
                        onClicked: commands.invoke((root.focusedView && root.focusedView.isLoading) ? "nav-stop" : "nav-reload")
                    }

                    TextField {
                        id: sidebarAddressField
                        Layout.fillWidth: true
                        placeholderText: "Search or enter address"
                        selectByMouse: true
                        onTextChanged: scheduleOmniboxUpdate()
                        onActiveFocusChanged: {
                            root.syncAddressFocusState()
                            if (activeFocus) {
                                Qt.callLater(() => sidebarAddressField.selectAll())
                                return
                            }
                            if (!layoutController.addressFieldFocused) {
                                root.closeOmniboxPopup()
                            }
                        }
                        onAccepted: {
                            if (root.omniboxPopupOpen()) {
                                const view = root.currentOmniboxView()
                                if (view) {
                                    view.activateCurrent()
                                }
                                return
                            }
                            if ((text || "").trim().startsWith(">")) {
                                return
                            }
                            root.navigateFromOmnibox(text)
                            sidebarAddressField.focus = false
                        }

                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Down) {
                                if (!root.omniboxPopupOpen()) {
                                    updateOmnibox()
                                    Qt.callLater(() => {
                                        const view = root.currentOmniboxView()
                                        if (view) {
                                            view.currentIndex = view.firstSelectableIndex()
                                        }
                                    })
                                } else {
                                    const view = root.currentOmniboxView()
                                    if (view) {
                                        view.moveSelection(1)
                                    }
                                }
                                event.accepted = true
                                return
                            }
                            if (event.key === Qt.Key_Up) {
                                const view = root.currentOmniboxView()
                                if (view) {
                                    view.moveSelection(-1)
                                    event.accepted = true
                                }
                                return
                            }
                            if (event.key === Qt.Key_Tab) {
                                const view = root.currentOmniboxView()
                                if (view) {
                                    view.activateCurrent()
                                    event.accepted = true
                                }
                                return
                            }
                        }

                        Keys.onEscapePressed: (event) => {
                            if (root.omniboxPopupOpen()) {
                                root.closeOmniboxPopup()
                                event.accepted = true
                                return
                            }
                            const currentUrl = root.currentFocusedUrlString()
                            if (sidebarAddressField.text !== currentUrl) {
                                root.syncAddressFieldFromFocused(true)
                                Qt.callLater(() => sidebarAddressField.selectAll())
                                event.accepted = true
                                return
                            }
                            sidebarAddressField.focus = false
                            event.accepted = true
                        }
                    }

                    ToolButton {
                        visible: root.focusedView && Math.abs((root.focusedView.zoomFactor || 1.0) - 1.0) > 0.001
                        text: Math.round((root.focusedView.zoomFactor || 1.0) * 100) + "%"
                        onClicked: commands.invoke("zoom-reset")
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Reset zoom"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    function buttonColor(active) {
                        return active ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16) : Qt.rgba(0, 0, 0, 0.04)
                    }

                    function buttonBorder(active) {
                        return active ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : Qt.rgba(0, 0, 0, 0.08)
                    }

                    ToolButton {
                        id: sidebarTabsButton
                        text: root.sidebarIconOnly ? "T" : "Tabs"
                        onClicked: {
                            if (popupManager.opened && root.popupManagerContext.startsWith("sidebar-tool-")) {
                                popupManager.close()
                            }
                        }
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(!(popupManager.opened && root.popupManagerContext.startsWith("sidebar-tool-")))
                            border.color: buttonBorder(!(popupManager.opened && root.popupManagerContext.startsWith("sidebar-tool-")))
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Tabs"
                    }

                    ToolButton {
                        id: sidebarBookmarksButton
                        text: root.sidebarIconOnly ? "\u2605" : "Bookmarks"
                        onClicked: root.toggleSidebarToolPopup("bookmarks", sidebarBookmarksPopupComponent, sidebarBookmarksButton)
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(root.popupManagerContext === "sidebar-tool-bookmarks")
                            border.color: buttonBorder(root.popupManagerContext === "sidebar-tool-bookmarks")
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Bookmarks"
                    }

                    ToolButton {
                        id: sidebarHistoryButton
                        text: root.sidebarIconOnly ? "H" : "History"
                        onClicked: root.toggleSidebarToolPopup("history", sidebarHistoryPopupComponent, sidebarHistoryButton)
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(root.popupManagerContext === "sidebar-tool-history")
                            border.color: buttonBorder(root.popupManagerContext === "sidebar-tool-history")
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "History"
                    }

                    ToolButton {
                        id: sidebarDownloadsButton
                        text: root.sidebarIconOnly ? "D" : "Downloads"
                        onClicked: root.toggleSidebarToolPopup("downloads", sidebarDownloadsPopupComponent, sidebarDownloadsButton)
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(root.popupManagerContext === "sidebar-tool-downloads")
                            border.color: buttonBorder(root.popupManagerContext === "sidebar-tool-downloads")
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Downloads"
                    }

                    ToolButton {
                        id: sidebarPanelsButton
                        text: root.sidebarIconOnly ? "P" : "Panels"
                        enabled: root.webPanelsModel !== null && root.webPanelsModel !== undefined
                        onClicked: root.toggleSidebarToolPopup("panels", sidebarPanelsPopupComponent, sidebarPanelsButton)
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(root.popupManagerContext === "sidebar-tool-panels")
                            border.color: buttonBorder(root.popupManagerContext === "sidebar-tool-panels")
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Panels"
                    }

                    ToolButton {
                        id: sidebarExtensionsButton
                        text: root.sidebarIconOnly ? "E" : "Extensions"
                        enabled: extensions && extensions.ready === true
                        onClicked: root.toggleSidebarToolPopup("extensions", sidebarExtensionsPopupComponent, sidebarExtensionsButton)
                        background: Rectangle {
                            radius: 8
                            color: buttonColor(root.popupManagerContext === "sidebar-tool-extensions")
                            border.color: buttonBorder(root.popupManagerContext === "sidebar-tool-extensions")
                            border.width: 1
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: "Extensions"
                    }

                    Item { Layout.fillWidth: true }
                }

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
                    visible: root.sidebarPanel === "tabs" && !root.sidebarIconOnly && essentialsView.count > 0
                }

                GridView {
                    id: essentialsGrid
                    Layout.fillWidth: true
                    Layout.preferredHeight: active ? Math.min(contentHeight, 120) : 0
                    clip: true
                    model: essentialsTabs
                    readonly property bool active: root.sidebarPanel === "tabs" && root.sidebarIconOnly && count > 0
                    visible: active
                    activeFocusOnTab: true
                    currentIndex: -1
                    cellWidth: 40
                    cellHeight: 40
                    property int selectionAnchorIndex: -1

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            if (browser.tabs && browser.tabs.clearSelection) {
                                browser.tabs.clearSelection()
                            }
                            event.accepted = true
                            return
                        }

                        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                            if (browser.tabs && browser.tabs.setSelectionByIds) {
                                browser.tabs.setSelectionByIds(essentialsTabs.tabIds(), true)
                            }
                            essentialsGrid.selectionAnchorIndex = essentialsGrid.currentIndex >= 0 ? essentialsGrid.currentIndex : 0
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (essentialsGrid.currentItem && essentialsGrid.currentItem.tabId) {
                                browser.activateTabById(essentialsGrid.currentItem.tabId)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Delete) {
                            const selectedCount = browser.tabs ? Number(browser.tabs.selectedCount || 0) : 0
                            if (selectedCount > 1) {
                                root.handleTabContextMenuAction("close-tabs", { tabIds: browser.tabs.selectedTabIds() })
                            } else if (essentialsGrid.currentItem && essentialsGrid.currentItem.tabId) {
                                commands.invoke("close-tab", { tabId: essentialsGrid.currentItem.tabId })
                            }
                            event.accepted = true
                            return
                        }
                    }

                    delegate: ItemDelegate {
                        width: essentialsGrid.cellWidth
                        height: essentialsGrid.cellHeight
                        hoverEnabled: true
                        highlighted: isActive || isSelected || (essentialsGrid.activeFocus && GridView.isCurrentItem)

                        background: Rectangle {
                            radius: 10
                            color: isActive
                                       ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16)
                                       : (isSelected ? Qt.rgba(0, 0, 0, 0.10) : (parent.hovered ? Qt.rgba(0, 0, 0, 0.06) : "transparent"))
                            border.color: isActive ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : "transparent"
                            border.width: isActive ? 1 : 0
                        }

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: url && url.toString().length > 0 ? (title + "\n" + url.toString()) : title

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.handleTabRowClick(essentialsGrid, essentialsTabs, index, tabId, point.modifiers, true)
                        }

                        TapHandler {
                            acceptedButtons: Qt.MiddleButton
                            onTapped: commands.invoke("close-tab", { tabId: tabId })
                        }

                        contentItem: Item {
                            anchors.fill: parent
                            anchors.margins: 6

                            Item {
                                anchors.centerIn: parent
                                width: 22
                                height: 22

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 6
                                    color: Qt.rgba(0, 0, 0, 0.08)
                                    visible: !(faviconUrl && faviconUrl.toString().length > 0)

                                    Text {
                                        anchors.centerIn: parent
                                        text: title && title.length > 0 ? title[0].toUpperCase() : ""
                                        font.pixelSize: 10
                                        opacity: 0.6
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    source: faviconUrl
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectFit
                                    visible: faviconUrl && faviconUrl.toString().length > 0
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: essentialsView
                    Layout.fillWidth: true
                    Layout.preferredHeight: active ? Math.min(contentHeight, 120) : 0
                    clip: true
                    model: essentialsTabs
                    readonly property bool active: root.sidebarPanel === "tabs" && !root.sidebarIconOnly && count > 0
                    visible: active
                    activeFocusOnTab: true
                    currentIndex: -1
                    property int selectionAnchorIndex: -1

                    add: Transition {
                        NumberAnimation {
                            properties: "y,opacity"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }
                    remove: Transition {
                        NumberAnimation {
                            properties: "y,opacity"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.InCubic
                        }
                    }
                    move: Transition {
                        NumberAnimation {
                            properties: "y"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }
                    displaced: Transition {
                        NumberAnimation {
                            properties: "y"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            if (browser.tabs && browser.tabs.clearSelection) {
                                browser.tabs.clearSelection()
                            }
                            event.accepted = true
                            return
                        }

                        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                            if (browser.tabs && browser.tabs.setSelectionByIds) {
                                browser.tabs.setSelectionByIds(essentialsTabs.tabIds(), true)
                            }
                            essentialsView.selectionAnchorIndex = essentialsView.currentIndex >= 0 ? essentialsView.currentIndex : 0
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Up) {
                            const next = essentialsView.currentIndex > 0 ? (essentialsView.currentIndex - 1) : 0
                            if (event.modifiers & Qt.ShiftModifier) {
                                if (essentialsView.selectionAnchorIndex < 0) {
                                    essentialsView.selectionAnchorIndex = essentialsView.currentIndex >= 0 ? essentialsView.currentIndex : next
                                }
                                browser.tabs.setSelectionByIds(essentialsTabs.tabIdsInRange(essentialsView.selectionAnchorIndex, next), true)
                                essentialsView.currentIndex = next
                                essentialsView.positionViewAtIndex(essentialsView.currentIndex, ListView.Visible)
                            } else if (essentialsView.currentIndex > 0) {
                                essentialsView.currentIndex = next
                                essentialsView.positionViewAtIndex(essentialsView.currentIndex, ListView.Visible)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Down) {
                            const next = essentialsView.currentIndex < essentialsView.count - 1 ? (essentialsView.currentIndex + 1) : (essentialsView.count - 1)
                            if (event.modifiers & Qt.ShiftModifier) {
                                if (essentialsView.selectionAnchorIndex < 0) {
                                    essentialsView.selectionAnchorIndex = essentialsView.currentIndex >= 0 ? essentialsView.currentIndex : next
                                }
                                browser.tabs.setSelectionByIds(essentialsTabs.tabIdsInRange(essentialsView.selectionAnchorIndex, next), true)
                                essentialsView.currentIndex = next
                                essentialsView.positionViewAtIndex(essentialsView.currentIndex, ListView.Visible)
                            } else if (essentialsView.currentIndex < essentialsView.count - 1) {
                                essentialsView.currentIndex = next
                                essentialsView.positionViewAtIndex(essentialsView.currentIndex, ListView.Visible)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (essentialsView.currentItem && essentialsView.currentItem.tabId) {
                                browser.activateTabById(essentialsView.currentItem.tabId)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Delete) {
                            const selectedCount = browser.tabs ? Number(browser.tabs.selectedCount || 0) : 0
                            if (selectedCount > 1) {
                                root.handleTabContextMenuAction("close-tabs", { tabIds: browser.tabs.selectedTabIds() })
                            } else if (essentialsView.currentItem && essentialsView.currentItem.tabId) {
                                commands.invoke("close-tab", { tabId: essentialsView.currentItem.tabId })
                            }
                            event.accepted = true
                            return
                        }
                    }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: title
                        highlighted: isActive || isSelected || (essentialsView.activeFocus && ListView.isCurrentItem)
                        hoverEnabled: true
                        readonly property bool showActions: !root.sidebarIconOnly && (hovered || isActive || isSelected || (essentialsView.activeFocus && ListView.isCurrentItem))

                        background: Rectangle {
                            radius: 8
                            color: isActive
                                       ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16)
                                       : (isSelected ? Qt.rgba(0, 0, 0, 0.08) : (parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"))
                            border.color: isActive ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : "transparent"
                            border.width: isActive ? 1 : 0
                        }

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: url && url.toString().length > 0 ? (title + "\n" + url.toString()) : title
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.handleTabRowClick(essentialsView, essentialsTabs, index, tabId, point.modifiers, true)
                        }

                        TapHandler {
                            acceptedButtons: Qt.MiddleButton
                            onTapped: commands.invoke("close-tab", { tabId: tabId })
                        }

                        contentItem: RowLayout {
                            spacing: 6
                            Item {
                                width: 18
                                height: 18
                                Layout.alignment: Qt.AlignVCenter

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    color: Qt.rgba(0, 0, 0, 0.08)
                                    visible: !(faviconUrl && faviconUrl.toString().length > 0)

                                    Text {
                                        anchors.centerIn: parent
                                        text: title && title.length > 0 ? title[0].toUpperCase() : ""
                                        font.pixelSize: 10
                                        opacity: 0.6
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    source: faviconUrl
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectFit
                                    visible: faviconUrl && faviconUrl.toString().length > 0
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !root.sidebarIconOnly
                                text: title
                                elide: Text.ElideRight
                            }
                            Text {
                                text: "⟳"
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignVCenter
                                visible: !root.sidebarIconOnly
                                opacity: isLoading ? 0.85 : 0.0

                                NumberAnimation on rotation {
                                    from: 0
                                    to: 360
                                    duration: 900
                                    loops: Animation.Infinite
                                    running: isLoading && !browser.settings.reduceMotion
                                }

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            Text {
                                text: isMuted ? "🔇" : "🔊"
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignVCenter
                                visible: !root.sidebarIconOnly
                                opacity: isAudioPlaying ? 0.85 : 0.0

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            ToolButton {
                                text: "\u2605"
                                visible: !root.sidebarIconOnly
                                opacity: showActions ? 1.0 : 0.0
                                enabled: showActions
                                onClicked: commands.invoke("toggle-essential", { tabId: tabId })

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            ToolButton {
                                text: "\u00D7"
                                visible: !root.sidebarIconOnly
                                opacity: showActions ? 1.0 : 0.0
                                enabled: showActions
                                onClicked: commands.invoke("close-tab", { tabId: tabId })

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
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
                        enabled: root.focusedView ? (root.focusedView.currentUrl.toString().length > 0) : false
                        onClicked: {
                            if (!root.focusedView) {
                                return
                            }
                            quickLinks.addLink(root.focusedView.currentUrl, root.focusedView.title)
                        }
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

                Item {
                    Layout.fillWidth: true
                    implicitHeight: tabsLabel.implicitHeight
                    visible: root.sidebarPanel === "tabs"

                    Label {
                        id: tabsLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Tabs"
                        font.pixelSize: 12
                        opacity: 0.7
                    }

                    DropArea {
                        id: ungroupDropArea
                        anchors.fill: parent
                        keys: ["tab"]
                        onDropped: (drop) => {
                            const dragged = Number(drop.mimeData.tabId || 0)
                            if (dragged > 0) {
                                browser.ungroupTab(dragged)
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: Qt.rgba(0.2, 0.5, 1.0, 0.12)
                        visible: ungroupDropArea.containsDrag
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    visible: root.sidebarPanel === "tabs" && !root.sidebarIconOnly && browser.tabs && Number(browser.tabs.selectedCount || 0) > 1
                    padding: 8

                    background: Rectangle {
                        radius: 10
                        color: Qt.rgba(0, 0, 0, 0.05)
                        border.color: Qt.rgba(0, 0, 0, 0.08)
                        border.width: 1
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: browser.tabs.selectedCount + " selected"
                            elide: Text.ElideRight
                        }

                        ToolButton {
                            text: "Pin"
                            onClicked: root.handleTabContextMenuAction(root.selectedTabsAllEssential(browser.tabs.selectedTabIds()) ? "unpin-tabs" : "pin-tabs", { tabIds: browser.tabs.selectedTabIds() })
                        }

                        ToolButton {
                            text: "Group"
                            onClicked: root.handleTabContextMenuAction("new-group-tabs", { tabIds: browser.tabs.selectedTabIds() })
                        }

                        ToolButton {
                            text: "Close"
                            onClicked: root.handleTabContextMenuAction("close-tabs", { tabIds: browser.tabs.selectedTabIds() })
                        }

                        ToolButton {
                            text: "Clear"
                            onClicked: browser.tabs.clearSelection()
                        }
                    }
                }

                Repeater {
                    model: browser.tabGroups

                    delegate: ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: root.sidebarPanel === "tabs"

                        property bool searching: false
                        property bool renaming: false
                        property var groupColorPalette: ["#7c3aed", "#2563eb", "#059669", "#ea580c", "#db2777", "#0ea5e9"]

                        function finishRename() {
                            if (!renaming) {
                                return
                            }
                            renaming = false
                            browser.tabGroups.setNameAt(index, groupRenameField.text)
                        }

                        function cycleGroupColor() {
                            const current = groupColor && groupColor.toString ? groupColor.toString().toLowerCase() : ""
                            let i = groupColorPalette.indexOf(current)
                            if (i < 0) {
                                i = 0
                            } else {
                                i = (i + 1) % groupColorPalette.length
                            }
                            browser.tabGroups.setColorAt(index, groupColorPalette[i])
                        }

                         Item {
                             id: groupHeaderHost
                             Layout.fillWidth: true
                             implicitHeight: groupHeaderRow.implicitHeight

                             function buildGroupHeaderContextMenuItems() {
                                const items = []
                                items.push({ action: "rename-group", text: "Rename Group", enabled: true, args: {} })
                                items.push({ action: "delete-group", text: "Delete Group", enabled: true, args: {} })
                                items.push({ separator: true })
                                items.push({ action: "collapse-all-groups", text: "Collapse All Groups", enabled: true, args: {} })
                                items.push({ action: "expand-all-groups", text: "Expand All Groups", enabled: true, args: {} })
                                return items
                            }

                            function handleGroupHeaderContextMenuAction(action) {
                                const a = String(action || "")
                                if (!a) {
                                    return
                                }

                                if (a === "rename-group") {
                                    renaming = true
                                    groupRenameField.text = name
                                    groupRenameField.forceActiveFocus()
                                    groupRenameField.selectAll()
                                    return
                                }

                                if (a === "delete-group") {
                                    browser.deleteTabGroup(groupId)
                                    return
                                }

                                if (a === "collapse-all-groups" || a === "expand-all-groups") {
                                    const shouldCollapse = a === "collapse-all-groups"
                                    const count = browser.tabGroups ? browser.tabGroups.count() : 0
                                    for (let i = 0; i < count; i++) {
                                        browser.tabGroups.setCollapsedAt(i, shouldCollapse)
                                    }
                                 }
                             }

                             HoverHandler {
                                 id: groupHeaderHoverHandler
                                 acceptedDevices: PointerDevice.Mouse
                             }

                             Rectangle {
                                 anchors.fill: parent
                                 radius: 6
                                 color: Qt.rgba(0.2, 0.5, 1.0, 0.12)
                                 visible: groupHeaderDrop.containsDrag
                             }

                             Rectangle {
                                 anchors.fill: parent
                                 radius: 6
                                 color: Qt.rgba(0, 0, 0, 0.04)
                                 visible: groupHeaderHoverHandler.hovered
                             }

                            RowLayout {
                                id: groupHeaderRow
                                anchors.fill: parent
                                spacing: 6

                                ToolButton {
                                    text: collapsed ? "+" : "−"
                                    onClicked: browser.tabGroups.setCollapsedAt(index, !collapsed)
                                }

                                Rectangle {
                                    width: 10
                                    height: 10
                                    radius: 3
                                    color: groupColor
                                    border.width: 1
                                    border.color: Qt.rgba(0, 0, 0, 0.12)
                                    Layout.alignment: Qt.AlignVCenter

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: cycleGroupColor()
                                    }
                                }

                                TextField {
                                    id: groupRenameField
                                    Layout.fillWidth: true
                                    visible: renaming
                                    text: name
                                    selectByMouse: true
                                    onAccepted: finishRename()
                                    onEditingFinished: finishRename()
                                    Keys.onEscapePressed: renaming = false
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: !renaming
                                    text: name
                                    elide: Text.ElideRight
                                    font.bold: true

                                    TapHandler {
                                        acceptedButtons: Qt.LeftButton
                                        onDoubleTapped: {
                                            renaming = true
                                            groupRenameField.text = name
                                            groupRenameField.forceActiveFocus()
                                            groupRenameField.selectAll()
                                        }
                                    }
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

                            Component {
                                id: groupHeaderContextMenuComponent

                                ContextMenu {
                                    cornerRadius: root.uiRadius
                                    spacing: root.uiSpacing
                                    implicitWidth: 240
                                    maxHeight: Math.max(200, sidebarPane.height - root.uiSpacing * 2)
                                    items: groupHeaderHost.buildGroupHeaderContextMenuItems()
                                    onActionTriggered: (action, args) => {
                                        popupManager.close()
                                        groupHeaderHost.handleGroupHeaderContextMenuAction(action)
                                    }
                                }
                            }

                            DropArea {
                                id: groupHeaderDrop
                                anchors.fill: parent
                                keys: ["tab"]
                                onDropped: (drop) => {
                                    const dragged = Number(drop.mimeData.tabId || 0)
                                    if (dragged > 0) {
                                        browser.moveTabToGroup(dragged, groupId)
                                    }
                                }
                            }

                             MouseArea {
                                 id: groupHeaderContextArea
                                 anchors.fill: parent
                                 acceptedButtons: Qt.RightButton
                                 onClicked: (mouse) => {
                                     const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                     popupManager.openAtPoint(groupHeaderContextMenuComponent, pos.x, pos.y, sidebarPane)
                                     root.popupManagerContext = "sidebar-context-menu"
                                 }
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
                            id: groupTabList
                            Layout.fillWidth: true
                            Layout.preferredHeight: collapsed ? 0 : Math.min(contentHeight, 160)
                            clip: true
                            opacity: collapsed ? 0.0 : 1.0
                            enabled: !collapsed
                            interactive: !collapsed
                            model: groupTabs
                            activeFocusOnTab: true
                            currentIndex: -1
                            property int selectionAnchorIndex: -1

                            Behavior on Layout.preferredHeight {
                                NumberAnimation {
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }

                            add: Transition {
                                NumberAnimation {
                                    properties: "y,opacity"
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }
                            remove: Transition {
                                NumberAnimation {
                                    properties: "y,opacity"
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.InCubic
                                }
                            }
                            move: Transition {
                                NumberAnimation {
                                    properties: "y"
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }
                            displaced: Transition {
                                NumberAnimation {
                                    properties: "y"
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Keys.onPressed: (event) => {
                                if (event.key === Qt.Key_Escape) {
                                    if (browser.tabs && browser.tabs.clearSelection) {
                                        browser.tabs.clearSelection()
                                    }
                                    event.accepted = true
                                    return
                                }

                                if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                                    if (browser.tabs && browser.tabs.setSelectionByIds) {
                                        browser.tabs.setSelectionByIds(groupTabs.tabIds(), true)
                                    }
                                    groupTabList.selectionAnchorIndex = groupTabList.currentIndex >= 0 ? groupTabList.currentIndex : 0
                                    event.accepted = true
                                    return
                                }

                                if (event.key === Qt.Key_Up) {
                                    const next = groupTabList.currentIndex > 0 ? (groupTabList.currentIndex - 1) : 0
                                    if (event.modifiers & Qt.ShiftModifier) {
                                        if (groupTabList.selectionAnchorIndex < 0) {
                                            groupTabList.selectionAnchorIndex = groupTabList.currentIndex >= 0 ? groupTabList.currentIndex : next
                                        }
                                        browser.tabs.setSelectionByIds(groupTabs.tabIdsInRange(groupTabList.selectionAnchorIndex, next), true)
                                        groupTabList.currentIndex = next
                                        groupTabList.positionViewAtIndex(groupTabList.currentIndex, ListView.Visible)
                                    } else if (groupTabList.currentIndex > 0) {
                                        groupTabList.currentIndex = next
                                        groupTabList.positionViewAtIndex(groupTabList.currentIndex, ListView.Visible)
                                    }
                                    event.accepted = true
                                    return
                                }

                                if (event.key === Qt.Key_Down) {
                                    const next = groupTabList.currentIndex < groupTabList.count - 1 ? (groupTabList.currentIndex + 1) : (groupTabList.count - 1)
                                    if (event.modifiers & Qt.ShiftModifier) {
                                        if (groupTabList.selectionAnchorIndex < 0) {
                                            groupTabList.selectionAnchorIndex = groupTabList.currentIndex >= 0 ? groupTabList.currentIndex : next
                                        }
                                        browser.tabs.setSelectionByIds(groupTabs.tabIdsInRange(groupTabList.selectionAnchorIndex, next), true)
                                        groupTabList.currentIndex = next
                                        groupTabList.positionViewAtIndex(groupTabList.currentIndex, ListView.Visible)
                                    } else if (groupTabList.currentIndex < groupTabList.count - 1) {
                                        groupTabList.currentIndex = next
                                        groupTabList.positionViewAtIndex(groupTabList.currentIndex, ListView.Visible)
                                    }
                                    event.accepted = true
                                    return
                                }

                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    if (groupTabList.currentItem && groupTabList.currentItem.tabId) {
                                        browser.activateTabById(groupTabList.currentItem.tabId)
                                    }
                                    event.accepted = true
                                    return
                                }

                                if (event.key === Qt.Key_Delete) {
                                    const selectedCount = browser.tabs ? Number(browser.tabs.selectedCount || 0) : 0
                                    if (selectedCount > 1) {
                                        root.handleTabContextMenuAction("close-tabs", { tabIds: browser.tabs.selectedTabIds() })
                                    } else if (groupTabList.currentItem && groupTabList.currentItem.tabId) {
                                        commands.invoke("close-tab", { tabId: groupTabList.currentItem.tabId })
                                    }
                                    event.accepted = true
                                    return
                                }
                            }

                            delegate: ItemDelegate {
                                width: ListView.view.width
                                text: title
                                highlighted: isActive || isSelected || (ListView.view.activeFocus && ListView.isCurrentItem)
                                hoverEnabled: true
                                property bool dropAfter: false
                                readonly property bool showActions: !root.sidebarIconOnly && (hovered || isActive || isSelected || (ListView.view.activeFocus && ListView.isCurrentItem))

                                Timer {
                                    id: tabPreviewDelayTimer
                                    interval: 300
                                    repeat: false
                                    onTriggered: {
                                        if (!parent.hovered || tabId <= 0) {
                                            return
                                        }
                                        root.openTabPreview(parent, tabId, title, url, thumbnailUrl)
                                    }
                                }

                                onHoveredChanged: {
                                    if (hovered) {
                                        tabPreviewDelayTimer.restart()
                                    } else {
                                        tabPreviewDelayTimer.stop()
                                        root.closeTabPreviewIfTabId(tabId)
                                    }
                                }

                                background: Rectangle {
                                    radius: 8
                                    color: isActive
                                               ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16)
                                               : (isSelected ? Qt.rgba(0, 0, 0, 0.08) : (parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"))
                                    border.color: isActive ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : "transparent"
                                    border.width: isActive ? 1 : 0
                                }

                                Drag.active: groupTabDrag.active
                                Drag.hotSpot.x: width / 2
                                Drag.hotSpot.y: height / 2
                                Drag.keys: ["tab"]
                                Drag.mimeData: ({ tabId: tabId })
                                Drag.supportedActions: Qt.MoveAction

                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    onTapped: root.handleTabRowClick(ListView.view, groupTabs, index, tabId, point.modifiers, true)
                                }

                                DragHandler {
                                    id: groupTabDrag
                                    acceptedButtons: Qt.LeftButton
                                }

                                TapHandler {
                                    acceptedButtons: Qt.MiddleButton
                                    onTapped: commands.invoke("close-tab", { tabId: tabId })
                                }

                                Component {
                                    id: groupTabContextMenuComponent

                                    ContextMenu {
                                        cornerRadius: root.uiRadius
                                        spacing: root.uiSpacing
                                        implicitWidth: 240
                                        maxHeight: Math.max(200, sidebarPane.height - root.uiSpacing * 2)
                                        items: root.buildTabContextMenuItems(tabId, isEssential, groupId)
                                        onActionTriggered: (action, args) => {
                                            popupManager.close()
                                            root.handleTabContextMenuAction(action, args)
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: (mouse) => {
                                        ListView.view.currentIndex = index
                                        ListView.view.forceActiveFocus()
                                        root.prepareContextMenuSelection(tabId)
                                        const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                        popupManager.openAtPoint(groupTabContextMenuComponent, pos.x, pos.y, sidebarPane)
                                        root.popupManagerContext = "sidebar-context-menu"
                                    }
                                }

                                DropArea {
                                    id: groupReorderDrop
                                    anchors.fill: parent
                                    keys: ["tab"]
                                    onPositionChanged: (drag) => dropAfter = drag.y > height * 0.5
                                    onDropped: (drop) => {
                                        const dragged = Number(drop.mimeData.tabId || 0)
                                        if (dragged > 0 && dragged !== tabId) {
                                            if (dropAfter) {
                                                browser.moveTabAfter(dragged, tabId)
                                            } else {
                                                browser.moveTabBefore(dragged, tabId)
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: 2
                                    y: dropAfter ? (parent.height - height) : 0
                                    color: theme.accentColor
                                    opacity: groupReorderDrop.containsDrag ? 0.85 : 0.0
                                    visible: opacity > 0
                                    z: 10

                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }

                                Timer {
                                    interval: 16
                                    repeat: true
                                    running: groupTabDrag.active
                                    onTriggered: {
                                        const view = ListView.view
                                        if (!view || view.contentHeight <= view.height) {
                                            return
                                        }
                                        const pos = mapToItem(view, groupTabDrag.centroid.position.x, groupTabDrag.centroid.position.y)
                                        const edge = 28
                                        const speed = 14
                                        let dir = 0
                                        if (pos.y < edge) {
                                            dir = -1
                                        } else if (pos.y > view.height - edge) {
                                            dir = 1
                                        }
                                        if (dir === 0) {
                                            return
                                        }
                                        view.contentY = Math.max(0, Math.min(view.contentHeight - view.height, view.contentY + dir * speed))
                                    }
                                }

                                contentItem: RowLayout {
                                    spacing: 6
                                    Item {
                                        width: 18
                                        height: 18
                                        Layout.alignment: Qt.AlignVCenter

                                        Rectangle {
                                            anchors.fill: parent
                                            radius: 4
                                            color: Qt.rgba(0, 0, 0, 0.08)
                                            visible: !(faviconUrl && faviconUrl.toString().length > 0)

                                            Text {
                                                anchors.centerIn: parent
                                                text: title && title.length > 0 ? title[0].toUpperCase() : ""
                                                font.pixelSize: 10
                                                opacity: 0.6
                                            }
                                        }

                                        Image {
                                            anchors.fill: parent
                                            source: faviconUrl
                                            asynchronous: true
                                            cache: true
                                            fillMode: Image.PreserveAspectFit
                                            visible: faviconUrl && faviconUrl.toString().length > 0
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: !root.sidebarIconOnly
                                        text: title
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: "⟳"
                                        Layout.preferredWidth: 18
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: !root.sidebarIconOnly
                                        opacity: isLoading ? 0.85 : 0.0

                                        NumberAnimation on rotation {
                                            from: 0
                                            to: 360
                                            duration: 900
                                            loops: Animation.Infinite
                                            running: isLoading && !browser.settings.reduceMotion
                                        }

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                    Text {
                                        text: isMuted ? "🔇" : "🔊"
                                        Layout.preferredWidth: 18
                                        horizontalAlignment: Text.AlignHCenter
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: !root.sidebarIconOnly
                                        opacity: isAudioPlaying ? 0.85 : 0.0

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                    ToolButton {
                                        id: groupMoreButton
                                        text: "⋯"
                                        visible: !root.sidebarIconOnly
                                        opacity: showActions ? 1.0 : 0.0
                                        enabled: showActions
                                        onClicked: {
                                            root.prepareContextMenuSelection(tabId)
                                            popupManager.openAtItem(groupTabContextMenuComponent, groupMoreButton, sidebarPane)
                                            root.popupManagerContext = "sidebar-context-menu"
                                        }

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                    ToolButton {
                                        text: isEssential ? "★" : "☆"
                                        visible: !root.sidebarIconOnly
                                        opacity: showActions ? 1.0 : 0.0
                                        enabled: showActions
                                        onClicked: commands.invoke("toggle-essential", { tabId: tabId })

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                    ToolButton {
                                        text: "×"
                                        visible: !root.sidebarIconOnly
                                        opacity: showActions ? 1.0 : 0.0
                                        enabled: showActions
                                        onClicked: commands.invoke("close-tab", { tabId: tabId })

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                                easing.type: Easing.OutCubic
                                            }
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
                    visible: root.sidebarPanel === "tabs"
                    clip: true
                    model: regularTabs
                    activeFocusOnTab: true
                    currentIndex: -1
                    property int selectionAnchorIndex: -1

                    add: Transition {
                        NumberAnimation {
                            properties: "y,opacity"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }
                    remove: Transition {
                        NumberAnimation {
                            properties: "y,opacity"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.InCubic
                        }
                    }
                    move: Transition {
                        NumberAnimation {
                            properties: "y"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }
                    displaced: Transition {
                        NumberAnimation {
                            properties: "y"
                            duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                            easing.type: Easing.OutCubic
                        }
                    }

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Escape) {
                            if (browser.tabs && browser.tabs.clearSelection) {
                                browser.tabs.clearSelection()
                            }
                            event.accepted = true
                            return
                        }

                        if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_A) {
                            if (browser.tabs && browser.tabs.setSelectionByIds) {
                                browser.tabs.setSelectionByIds(regularTabs.tabIds(), true)
                            }
                            tabList.selectionAnchorIndex = tabList.currentIndex >= 0 ? tabList.currentIndex : 0
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Up) {
                            const next = tabList.currentIndex > 0 ? (tabList.currentIndex - 1) : 0
                            if (event.modifiers & Qt.ShiftModifier) {
                                if (tabList.selectionAnchorIndex < 0) {
                                    tabList.selectionAnchorIndex = tabList.currentIndex >= 0 ? tabList.currentIndex : next
                                }
                                browser.tabs.setSelectionByIds(regularTabs.tabIdsInRange(tabList.selectionAnchorIndex, next), true)
                                tabList.currentIndex = next
                                tabList.positionViewAtIndex(tabList.currentIndex, ListView.Visible)
                            } else if (tabList.currentIndex > 0) {
                                tabList.currentIndex = next
                                tabList.positionViewAtIndex(tabList.currentIndex, ListView.Visible)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Down) {
                            const next = tabList.currentIndex < tabList.count - 1 ? (tabList.currentIndex + 1) : (tabList.count - 1)
                            if (event.modifiers & Qt.ShiftModifier) {
                                if (tabList.selectionAnchorIndex < 0) {
                                    tabList.selectionAnchorIndex = tabList.currentIndex >= 0 ? tabList.currentIndex : next
                                }
                                browser.tabs.setSelectionByIds(regularTabs.tabIdsInRange(tabList.selectionAnchorIndex, next), true)
                                tabList.currentIndex = next
                                tabList.positionViewAtIndex(tabList.currentIndex, ListView.Visible)
                            } else if (tabList.currentIndex < tabList.count - 1) {
                                tabList.currentIndex = next
                                tabList.positionViewAtIndex(tabList.currentIndex, ListView.Visible)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (tabList.currentItem && tabList.currentItem.tabId) {
                                browser.activateTabById(tabList.currentItem.tabId)
                            }
                            event.accepted = true
                            return
                        }

                        if (event.key === Qt.Key_Delete) {
                            const selectedCount = browser.tabs ? Number(browser.tabs.selectedCount || 0) : 0
                            if (selectedCount > 1) {
                                root.handleTabContextMenuAction("close-tabs", { tabIds: browser.tabs.selectedTabIds() })
                            } else if (tabList.currentItem && tabList.currentItem.tabId) {
                                commands.invoke("close-tab", { tabId: tabList.currentItem.tabId })
                            }
                            event.accepted = true
                            return
                        }
                    }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        text: title
                        highlighted: isActive || isSelected || (tabList.activeFocus && ListView.isCurrentItem)
                        property bool renaming: false
                        property bool dropAfter: false
                        readonly property bool showActions: !root.sidebarIconOnly && (hovered || isActive || isSelected || (tabList.activeFocus && ListView.isCurrentItem))
                        hoverEnabled: true

                        Timer {
                            id: tabPreviewDelayTimer
                            interval: 300
                            repeat: false
                            onTriggered: {
                                if (!parent.hovered || renaming || tabDrag.active || tabId <= 0) {
                                    return
                                }
                                root.openTabPreview(parent, tabId, title, url, thumbnailUrl)
                            }
                        }

                        onHoveredChanged: {
                            if (hovered && !renaming && !tabDrag.active) {
                                tabPreviewDelayTimer.restart()
                            } else {
                                tabPreviewDelayTimer.stop()
                                root.closeTabPreviewIfTabId(tabId)
                            }
                        }

                        onRenamingChanged: {
                            if (renaming) {
                                tabPreviewDelayTimer.stop()
                                root.closeTabPreviewIfTabId(tabId)
                                return
                            }
                            if (hovered && !tabDrag.active) {
                                tabPreviewDelayTimer.restart()
                            }
                        }

                        background: Rectangle {
                            radius: 8
                            color: isActive
                                       ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.16)
                                       : (isSelected ? Qt.rgba(0, 0, 0, 0.08) : (parent.hovered ? Qt.rgba(0, 0, 0, 0.04) : "transparent"))
                            border.color: isActive ? Qt.rgba(theme.accentColor.r, theme.accentColor.g, theme.accentColor.b, 0.35) : "transparent"
                            border.width: isActive ? 1 : 0
                        }

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
                            onTapped: root.handleTabRowClick(tabList, regularTabs, index, tabId, point.modifiers, true)
                            onDoubleTapped: {
                                if (root.sidebarIconOnly) {
                                    return
                                }
                                if ((point.modifiers & (Qt.ControlModifier | Qt.ShiftModifier)) !== 0) {
                                    return
                                }
                                renaming = true
                                renameField.forceActiveFocus()
                                renameField.selectAll()
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.MiddleButton
                            onTapped: commands.invoke("close-tab", { tabId: tabId })
                        }

                        DragHandler {
                            id: tabDrag
                            acceptedButtons: Qt.LeftButton
                        }

                        Component {
                            id: tabContextMenuComponent

                            ContextMenu {
                                cornerRadius: root.uiRadius
                                spacing: root.uiSpacing
                                implicitWidth: 240
                                maxHeight: Math.max(200, sidebarPane.height - root.uiSpacing * 2)
                                items: root.buildTabContextMenuItems(tabId, isEssential, groupId)
                                onActionTriggered: (action, args) => {
                                    popupManager.close()
                                    root.handleTabContextMenuAction(action, args)
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: (mouse) => {
                                tabList.currentIndex = index
                                tabList.forceActiveFocus()
                                root.prepareContextMenuSelection(tabId)
                                const pos = mapToItem(popupManager, mouse.x, mouse.y)
                                popupManager.openAtPoint(tabContextMenuComponent, pos.x, pos.y, sidebarPane)
                                root.popupManagerContext = "sidebar-context-menu"
                            }
                        }

                        DropArea {
                            id: reorderDrop
                            anchors.fill: parent
                            keys: ["tab"]
                            onPositionChanged: (drag) => dropAfter = drag.y > height * 0.5
                            onDropped: (drop) => {
                                const dragged = Number(drop.mimeData.tabId || 0)
                                if (dragged > 0 && dragged !== tabId) {
                                    if (dropAfter) {
                                        browser.moveTabAfter(dragged, tabId)
                                    } else {
                                        browser.moveTabBefore(dragged, tabId)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 2
                            y: dropAfter ? (parent.height - height) : 0
                            color: theme.accentColor
                            opacity: reorderDrop.containsDrag ? 0.85 : 0.0
                            visible: opacity > 0
                            z: 10

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        Timer {
                            interval: 16
                            repeat: true
                            running: tabDrag.active
                            onTriggered: {
                                const view = ListView.view
                                if (!view || view.contentHeight <= view.height) {
                                    return
                                }
                                const pos = mapToItem(view, tabDrag.centroid.position.x, tabDrag.centroid.position.y)
                                const edge = 28
                                const speed = 14
                                let dir = 0
                                if (pos.y < edge) {
                                    dir = -1
                                } else if (pos.y > view.height - edge) {
                                    dir = 1
                                }
                                if (dir === 0) {
                                    return
                                }
                                view.contentY = Math.max(0, Math.min(view.contentHeight - view.height, view.contentY + dir * speed))
                            }
                        }

                        contentItem: RowLayout {
                            spacing: 6
                            Item {
                                width: 18
                                height: 18
                                Layout.alignment: Qt.AlignVCenter

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    color: Qt.rgba(0, 0, 0, 0.08)
                                    visible: !(faviconUrl && faviconUrl.toString().length > 0)

                                    Text {
                                        anchors.centerIn: parent
                                        text: title && title.length > 0 ? title[0].toUpperCase() : ""
                                        font.pixelSize: 10
                                        opacity: 0.6
                                    }
                                }

                                Image {
                                    anchors.fill: parent
                                    source: faviconUrl
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectFit
                                    visible: faviconUrl && faviconUrl.toString().length > 0
                                }
                            }
                            TextField {
                                id: renameField
                                Layout.fillWidth: true
                                visible: renaming && !root.sidebarIconOnly
                                text: title
                                selectByMouse: true
                                onAccepted: finishRename()
                                onEditingFinished: finishRename()
                                Keys.onEscapePressed: renaming = false
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !renaming && !root.sidebarIconOnly
                                text: title
                                elide: Text.ElideRight
                            }
                            Text {
                                id: loadingIcon
                                text: "⟳"
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignVCenter
                                visible: !root.sidebarIconOnly
                                opacity: isLoading ? 0.85 : 0.0

                                NumberAnimation on rotation {
                                    from: 0
                                    to: 360
                                    duration: 900
                                    loops: Animation.Infinite
                                    running: isLoading && !browser.settings.reduceMotion
                                }

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            Text {
                                id: audioIcon
                                text: isMuted ? "🔇" : "🔊"
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignVCenter
                                visible: !root.sidebarIconOnly
                                opacity: isAudioPlaying ? 0.85 : 0.0

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            ToolButton {
                                id: moreButton
                                text: "⋯"
                                visible: !root.sidebarIconOnly
                                opacity: showActions ? 1.0 : 0.0
                                enabled: showActions
                                onClicked: {
                                    root.prepareContextMenuSelection(tabId)
                                    popupManager.openAtItem(tabContextMenuComponent, moreButton, sidebarPane)
                                    root.popupManagerContext = "sidebar-context-menu"
                                }

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            ToolButton {
                                text: isEssential ? "\u2605" : "\u2606"
                                visible: !root.sidebarIconOnly
                                opacity: showActions ? 1.0 : 0.0
                                enabled: showActions
                                onClicked: commands.invoke("toggle-essential", { tabId: tabId })

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                            ToolButton {
                                text: "×"
                                visible: !root.sidebarIconOnly
                                opacity: showActions ? 1.0 : 0.0
                                enabled: showActions
                                onClicked: {
                                    commands.invoke("close-tab", { tabId: tabId })
                                }

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }
                }

                NotificationsStack {
                    Layout.fillWidth: true
                    visible: root.sidebarPanel === "tabs"
                    notifications: root.notificationsModel
                }

                MediaControls {
                    Layout.fillWidth: true
                    visible: root.sidebarPanel === "tabs"
                    view: root.mediaView
                }

                SidebarFooter {
                    Layout.fillWidth: true
                    browser: root.browserModel
                    workspaces: root.browserModel.workspaces
                    settings: root.browserModel.settings
                    popupHost: popupManager
                    popupContextHost: root
                }
            }
        }

        Rectangle {
            Layout.column: browser.settings.sidebarOnRight ? 3 : 1
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

                onDoubleClicked: browser.settings.sidebarWidth = 260

                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }
                    const delta = Math.round(mouse.sceneX - startSceneX)
                    browser.settings.sidebarWidth = browser.settings.sidebarOnRight ? (startWidth - delta) : (startWidth + delta)
                }
            }
        }

        Item {
            id: contentHost
            Layout.column: 2
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property int handleWidth: 4
            readonly property int splitMinPx: 200
            readonly property int splitPos: {
                const w = width
                if (w <= 0) {
                    return 0
                }
                const minPos = Math.min(splitMinPx, Math.round(w * 0.5))
                const maxPos = Math.max(minPos, w - minPos)
                const desired = Math.round(w * splitView.splitRatio)
                return Math.max(minPos, Math.min(maxPos, desired))
            }
            readonly property int gridSplitPosX: {
                const gap = handleWidth
                const available = Math.max(0, width - gap)
                if (available <= 0) {
                    return 0
                }
                const minPos = Math.min(splitMinPx, Math.round(available * 0.5))
                const maxPos = Math.max(minPos, available - minPos)
                const desired = Math.round(available * splitView.gridSplitRatioX)
                return Math.max(minPos, Math.min(maxPos, desired))
            }
            readonly property int gridSplitPosY: {
                const gap = handleWidth
                const available = Math.max(0, height - gap)
                if (available <= 0) {
                    return 0
                }
                const minPos = Math.min(splitMinPx, Math.round(available * 0.5))
                const maxPos = Math.max(minPos, available - minPos)
                const desired = Math.round(available * splitView.gridSplitRatioY)
                return Math.max(minPos, Math.min(maxPos, desired))
            }

            function paneRect(paneIndex) {
                if (!splitView.enabled) {
                    return { x: 0, y: 0, width: contentHost.width, height: contentHost.height }
                }

                const count = Number(splitView.paneCount || 2)
                if (count <= 2) {
                    const leftW = Math.max(0, contentHost.splitPos)
                    const rightX = contentHost.splitPos + contentHost.handleWidth
                    const rightW = Math.max(0, contentHost.width - contentHost.splitPos - contentHost.handleWidth)
                    if (paneIndex === 0) {
                        return { x: 0, y: 0, width: leftW, height: contentHost.height }
                    }
                    if (paneIndex === 1) {
                        return { x: rightX, y: 0, width: rightW, height: contentHost.height }
                    }
                    return { x: 0, y: 0, width: 0, height: 0 }
                }

                const gap = contentHost.handleWidth
                const availableW = Math.max(0, contentHost.width - gap)
                const availableH = Math.max(0, contentHost.height - gap)

                const leftW = Math.max(0, Math.min(availableW, contentHost.gridSplitPosX))
                const rightW = Math.max(0, availableW - leftW)
                const topH = Math.max(0, Math.min(availableH, contentHost.gridSplitPosY))
                const bottomH = Math.max(0, availableH - topH)
                const rightX = leftW + gap
                const bottomY = topH + gap

                if (paneIndex === 0) {
                    return { x: 0, y: 0, width: leftW, height: topH }
                }
                if (paneIndex === 1) {
                    return { x: rightX, y: 0, width: rightW, height: topH }
                }
                if (paneIndex === 2) {
                    return { x: 0, y: bottomY, width: leftW, height: bottomH }
                }
                if (paneIndex === 3) {
                    return { x: rightX, y: bottomY, width: rightW, height: bottomH }
                }
                return { x: 0, y: 0, width: 0, height: 0 }
            }

            Component {
                id: tabWebViewComponent

                WebView2View {
                    id: tabWeb
                    required property int tabId
                    property int lastThumbnailCaptureMs: 0

                    readonly property int paneIndex: splitView.enabled
                                                          ? splitView.paneIndexForTabId(tabId)
                                                          : (tabId === root.tabIdForActiveIndex() ? 0 : -1)
                    readonly property var paneRect: paneIndex >= 0 ? contentHost.paneRect(paneIndex) : ({ x: 0, y: 0, width: 0, height: 0 })

                    visible: paneIndex >= 0
                    x: paneRect.x
                    y: paneRect.y
                    width: paneRect.width
                    height: paneRect.height

                    Component.onCompleted: {
                        root.pushModsCss(tabWeb)
                        if (root.glanceScript && root.glanceScript.length > 0) {
                            addScriptOnDocumentCreated(root.glanceScript)
                        }
                        const url = root.urlForTabId(tabId)
                        if (url && url.toString().length > 0) {
                            navigate(url)
                        }
                    }

                    onTitleChanged: {
                        if (tabId > 0) {
                            browser.setTabTitleById(tabId, title)
                        }
                    }

                    onCurrentUrlChanged: {
                        if (tabId > 0) {
                            browser.setTabUrlById(tabId, currentUrl)
                        }
                        if (tabId === root.focusedTabId) {
                            root.syncAddressFieldFromFocused()
                        }
                        root.applyZoomForView(tabWeb)
                    }

                    onIsLoadingChanged: {
                        if (tabId > 0) {
                            browser.setTabIsLoadingById(tabId, isLoading)
                        }
                        if (!isLoading) {
                            scheduleThumbnailCapture()
                        }
                    }

                    onNavigationCommitted: (success) => {
                        if (!success) {
                            return
                        }
                        const store = root.historyModel
                        if (store && store.addVisit) {
                            store.addVisit(currentUrl, title)
                        }
                    }

                    onZoomFactorChanged: {
                        const settings = browser && browser.settings ? browser.settings : null
                        if (!settings || !settings.rememberZoomPerSite || !settings.setZoomForUrl) {
                            return
                        }

                        const desired = root.desiredZoomForUrl(currentUrl)
                        if (Math.abs((zoomFactor || 1.0) - desired) < 0.001) {
                            return
                        }

                        settings.setZoomForUrl(currentUrl, zoomFactor || 1.0)
                    }

                    onDocumentPlayingAudioChanged: {
                        if (tabId > 0) {
                            browser.setTabAudioStateById(tabId, documentPlayingAudio, muted)
                        }
                    }

                    onMutedChanged: {
                        if (tabId > 0) {
                            browser.setTabAudioStateById(tabId, documentPlayingAudio, muted)
                        }
                    }

                    onFaviconUrlChanged: {
                        if (tabId > 0) {
                            browser.setTabFaviconUrlById(tabId, faviconUrl)
                        }
                    }

                    Timer {
                        id: thumbnailCaptureTimer
                        interval: browser && browser.settings && browser.settings.reduceMotion ? 650 : 250
                        repeat: false
                        onTriggered: {
                            if (!tabWeb.visible || !tabWeb.initialized || tabWeb.tabId <= 0) {
                                return
                            }
                            if (!tabWeb.capturePreview || !diagnostics || !diagnostics.dataDir || diagnostics.dataDir.length === 0) {
                                return
                            }

                            const now = Date.now()
                            const minInterval = browser && browser.settings && browser.settings.reduceMotion ? 2000 : 800
                            if (now - tabWeb.lastThumbnailCaptureMs < minInterval) {
                                thumbnailCaptureTimer.restart()
                                return
                            }

                            tabWeb.lastThumbnailCaptureMs = now
                            const base = String(diagnostics.dataDir || "")
                            const path = base + "/thumbnails/tab_" + tabWeb.tabId + ".png"
                            tabWeb.capturePreview(path)
                        }
                    }

                    function scheduleThumbnailCapture() {
                        if (!tabWeb.visible || !tabWeb.initialized || tabWeb.tabId <= 0) {
                            return
                        }
                        thumbnailCaptureTimer.restart()
                    }

                    onCapturePreviewFinished: (filePath, success, error) => {
                        if (!success) {
                            return
                        }

                        if (tabId > 0 && browser && browser.tabs && browser.tabs.setThumbnailPathById) {
                            browser.tabs.setThumbnailPathById(tabId, String(filePath || ""))
                        }
                    }

                    onVisibleChanged: {
                        if (visible && !isLoading) {
                            scheduleThumbnailCapture()
                        }
                    }

                    onFocusReceived: {
                        if (!splitView.enabled) {
                            splitView.focusedPane = 0
                            scheduleThumbnailCapture()
                            return
                        }
                        if (paneIndex >= 0) {
                            splitView.focusedPane = paneIndex
                            scheduleThumbnailCapture()
                        }
                    }

                    onContainsFullScreenElementChanged: root.handleContentFullscreen(tabId, containsFullScreenElement)

                    onContextMenuRequested: (info) => root.openWebContextMenu(tabId, info)

                    onPermissionRequested: (requestId, origin, kind, userInitiated) => {
                        root.showPermissionDoorhanger(tabId, requestId, origin, kind, userInitiated)
                    }

                    onWebMessageReceived: (json) => root.handleWebMessage(tabId, json, tabWeb)
                    onDownloadStarted: (downloadOperationId, uri, resultFilePath, totalBytes) => {
                        root.handleDownloadStarted(tabId, downloadOperationId, uri, resultFilePath, totalBytes)
                    }
                    onDownloadProgress: (downloadOperationId, bytesReceived, totalBytes, paused, canResume, interruptReason) => {
                        root.handleDownloadProgress(tabId, downloadOperationId, bytesReceived, totalBytes, paused, canResume, interruptReason)
                    }
                    onDownloadFinished: (downloadOperationId, uri, resultFilePath, success, interruptReason) => {
                        root.handleDownloadFinished(tabId, downloadOperationId, uri, resultFilePath, success, interruptReason)
                    }

                        Rectangle {
                            anchors.fill: parent
                            visible: !tabWeb.initialized && tabWeb.initError && tabWeb.initError.length > 0
                            color: Qt.rgba(1, 1, 1, 0.96)
                            border.color: Qt.rgba(0, 0, 0, 0.08)
                            border.width: 1
                            radius: theme.cornerRadius

                            ColumnLayout {
                                anchors.centerIn: parent
                                width: Math.min(parent.width - 48, 560)
                                spacing: theme.spacing

                                Label {
                                    Layout.fillWidth: true
                                    text: "Web engine unavailable"
                                    font.bold: true
                                    wrapMode: Text.Wrap
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: tabWeb.initError
                                    wrapMode: Text.Wrap
                                    opacity: 0.85
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: theme.spacing

                                    Button {
                                        text: "Install WebView2 Runtime"
                                        onClicked: Qt.openUrlExternally("https://go.microsoft.com/fwlink/p/?LinkId=2124703")
                                    }

                                    Button {
                                        text: "Retry"
                                        onClicked: tabWeb.retryInitialize()
                                    }
                                }
                            }
                        }
                    }
                }

            Repeater {
                model: splitView.enabled ? (splitView.paneCount > 2 ? 4 : splitView.paneCount) : 0

                delegate: Rectangle {
                    readonly property var rect: contentHost.paneRect(index)
                    readonly property int tabId: splitView.tabIdForPane(index)
                    readonly property bool emptyPane: tabId <= 0

                    x: rect.x
                    y: rect.y
                    width: rect.width
                    height: rect.height
                    color: "transparent"
                    border.width: splitView.focusedPane === index ? 2 : 1
                    border.color: emptyPane ? Qt.rgba(0, 0, 0, 0.10) : (splitView.focusedPane === index ? theme.accentColor : Qt.rgba(0, 0, 0, 0.08))
                    z: 5

                    Label {
                        anchors.centerIn: parent
                        text: "Empty pane\nDrop a tab here"
                        horizontalAlignment: Text.AlignHCenter
                        visible: emptyPane
                        opacity: 0.6
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: emptyPane
                        hoverEnabled: enabled
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            const idx = browser.newTab(QUrl("about:blank"))
                            if (idx >= 0) {
                                splitView.paneCount = Math.max(splitView.paneCount, index + 1)
                                splitView.setTabIdForPane(index, browser.tabs.tabIdAt(idx))
                                splitView.focusedPane = index
                                splitView.enabled = true
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: gridSplitHandleX
                visible: splitView.enabled && splitView.paneCount > 2
                x: contentHost.gridSplitPosX
                y: 0
                width: contentHost.handleWidth
                height: contentHost.height
                color: Qt.rgba(0, 0, 0, (gridSplitHandleXArea.containsMouse || gridSplitHandleXArea.pressed) ? 0.10 : 0.06)
                z: 6

                MouseArea {
                    id: gridSplitHandleXArea
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    hoverEnabled: true
                    preventStealing: true

                    property real startSceneX: 0
                    property int startPos: 0

                    onPressed: (mouse) => {
                        startSceneX = mouse.sceneX
                        startPos = contentHost.gridSplitPosX
                    }

                    onDoubleClicked: splitView.gridSplitRatioX = 0.5

                    onPositionChanged: (mouse) => {
                        if (!pressed) {
                            return
                        }
                        const available = Math.max(1, contentHost.width - contentHost.handleWidth)
                        const minPos = Math.min(contentHost.splitMinPx, Math.round(available * 0.5))
                        const maxPos = Math.max(minPos, available - minPos)
                        const next = startPos + Math.round(mouse.sceneX - startSceneX)
                        const clamped = Math.max(minPos, Math.min(maxPos, next))
                        splitView.gridSplitRatioX = clamped / available
                    }
                }
            }

            Rectangle {
                id: gridSplitHandleY
                visible: splitView.enabled && splitView.paneCount > 2
                x: 0
                y: contentHost.gridSplitPosY
                width: contentHost.width
                height: contentHost.handleWidth
                color: Qt.rgba(0, 0, 0, (gridSplitHandleYArea.containsMouse || gridSplitHandleYArea.pressed) ? 0.10 : 0.06)
                z: 6

                MouseArea {
                    id: gridSplitHandleYArea
                    anchors.fill: parent
                    cursorShape: Qt.SplitVCursor
                    hoverEnabled: true
                    preventStealing: true

                    property real startSceneY: 0
                    property int startPos: 0

                    onPressed: (mouse) => {
                        startSceneY = mouse.sceneY
                        startPos = contentHost.gridSplitPosY
                    }

                    onDoubleClicked: splitView.gridSplitRatioY = 0.5

                    onPositionChanged: (mouse) => {
                        if (!pressed) {
                            return
                        }
                        const available = Math.max(1, contentHost.height - contentHost.handleWidth)
                        const minPos = Math.min(contentHost.splitMinPx, Math.round(available * 0.5))
                        const maxPos = Math.max(minPos, available - minPos)
                        const next = startPos + Math.round(mouse.sceneY - startSceneY)
                        const clamped = Math.max(minPos, Math.min(maxPos, next))
                        splitView.gridSplitRatioY = clamped / available
                    }
                }
            }

            Rectangle {
                id: splitHandle
                visible: splitView.enabled && splitView.paneCount === 2
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

                    onDoubleClicked: {
                        splitView.splitRatio = 0.5
                    }

                    onPositionChanged: (mouse) => {
                        if (!pressed) {
                            return
                        }
                        if (contentHost.width <= 0) {
                            return
                        }
                        const minPos = Math.min(contentHost.splitMinPx, Math.round(contentHost.width * 0.5))
                        const maxPos = Math.max(minPos, contentHost.width - minPos)
                        const next = startPos + Math.round(mouse.sceneX - startSceneX)
                        const clamped = Math.max(minPos, Math.min(maxPos, next))
                        splitView.splitRatio = clamped / contentHost.width
                    }
                }
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
                        const left = drag.x < threshold
                        const right = drag.x > width - threshold
                        const top = drag.y < threshold
                        const bottom = drag.y > height - threshold

                        if (left && top) {
                            splitDropzone.zone = "top-left"
                        } else if (right && top) {
                            splitDropzone.zone = "top-right"
                        } else if (left && bottom) {
                            splitDropzone.zone = "bottom-left"
                        } else if (right && bottom) {
                            splitDropzone.zone = "bottom-right"
                        } else if (left) {
                            splitDropzone.zone = "left"
                        } else if (right) {
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

                        const current = root.tabIdForActiveIndex()
                        const zone = splitDropzone.zone

                        if (!splitView.enabled) {
                            if (zone === "bottom-right") {
                                splitView.paneCount = 4
                                splitView.setTabIdForPane(0, current)
                                splitView.setTabIdForPane(3, tabId)
                                splitView.focusedPane = 3
                            } else if (zone === "bottom-left") {
                                splitView.paneCount = 3
                                splitView.setTabIdForPane(0, current)
                                splitView.setTabIdForPane(2, tabId)
                                splitView.focusedPane = 2
                            } else if (zone === "right" || zone === "top-right") {
                                splitView.paneCount = 2
                                splitView.setTabIdForPane(0, current)
                                splitView.setTabIdForPane(1, tabId)
                                splitView.focusedPane = 1
                            } else {
                                splitView.paneCount = 2
                                splitView.setTabIdForPane(0, tabId)
                                splitView.setTabIdForPane(1, current)
                                splitView.focusedPane = 0
                            }
                            splitView.enabled = true
                        } else {
                            if (zone === "left" || zone === "top-left") {
                                splitView.setTabIdForPane(0, tabId)
                                splitView.focusedPane = 0
                            } else if (zone === "right" || zone === "top-right") {
                                splitView.setTabIdForPane(1, tabId)
                                splitView.focusedPane = 1
                            } else if (zone === "bottom-left") {
                                splitView.setTabIdForPane(2, tabId)
                                splitView.focusedPane = 2
                            } else if (zone === "bottom-right") {
                                splitView.setTabIdForPane(3, tabId)
                                splitView.focusedPane = 3
                            }
                        }

                        splitDropzone.zone = ""
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, (splitDropzone.zone === "left" || splitDropzone.zone.endsWith("left")) ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, (splitDropzone.zone === "right" || splitDropzone.zone.endsWith("right")) ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, splitDropzone.zone.startsWith("top") ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 80
                    color: Qt.rgba(0.2, 0.5, 1.0, splitDropzone.zone.startsWith("bottom") ? 0.18 : 0.08)
                    visible: dropArea.containsDrag
                }
            }
        }

        Rectangle {
            Layout.column: browser.settings.sidebarOnRight ? 1 : 3
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            visible: browser.settings.webPanelVisible
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
                    startWidth = browser.settings.webPanelWidth
                }

                onDoubleClicked: browser.settings.webPanelWidth = 360

                onPositionChanged: (mouse) => {
                    if (!pressed) {
                        return
                    }
                    const delta = Math.round(mouse.sceneX - startSceneX)
                    browser.settings.webPanelWidth = browser.settings.sidebarOnRight ? (startWidth + delta) : (startWidth - delta)
                }
            }
        }

        Rectangle {
            id: webPanelPane
            Layout.column: browser.settings.sidebarOnRight ? 0 : 4
            Layout.preferredWidth: browser.settings.webPanelVisible ? browser.settings.webPanelWidth : 0
            Layout.fillHeight: true
            visible: true
            opacity: browser.settings.webPanelVisible ? 1.0 : 0.0
            clip: true
            color: Qt.rgba(0, 0, 0, 0.03)

            Behavior on Layout.preferredWidth {
                NumberAnimation {
                    duration: browser.settings.reduceMotion ? 0 : theme.motionNormalMs
                    easing.type: Easing.InOutCubic
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: browser.settings.reduceMotion ? 0 : theme.motionFastMs
                    easing.type: Easing.OutCubic
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    color: Qt.rgba(1, 1, 1, 0.92)
                    border.color: Qt.rgba(0, 0, 0, 0.08)
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: root.webPanelTitle && root.webPanelTitle.length > 0
                                      ? root.webPanelTitle
                                      : (root.webPanelUrl && root.webPanelUrl.toString().length > 0 ? root.webPanelUrl.toString() : "Panel")
                            elide: Text.ElideRight
                        }

                        ToolButton {
                            text: "Open tab"
                            enabled: root.webPanelUrl && root.webPanelUrl.toString().length > 0 && root.webPanelUrl.toString() !== "about:blank"
                            onClicked: commands.invoke("new-tab", { url: root.webPanelUrl })
                        }

                        ToolButton {
                            text: "×"
                            onClicked: root.closeWebPanel()
                        }
                    }
                }

                WebView2View {
                    id: webPanelWeb
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Component.onCompleted: {
                        root.pushModsCss(webPanelWeb)
                        if (root.webPanelUrl && root.webPanelUrl.toString().length > 0 && root.webPanelUrl.toString() !== "about:blank") {
                            navigate(root.webPanelUrl)
                        }
                    }

                    onCurrentUrlChanged: {
                        if (!currentUrl || currentUrl.toString().length === 0 || currentUrl.toString() === "about:blank") {
                            return
                        }
                        root.webPanelUrl = currentUrl
                        browser.settings.webPanelUrl = currentUrl
                    }

                    onTitleChanged: {
                        if (!currentUrl || currentUrl.toString().length === 0 || currentUrl.toString() === "about:blank") {
                            return
                        }
                        root.webPanelTitle = title
                        browser.settings.webPanelTitle = title
                    }
                }
            }
        }
    }

    OverlayHost {
        id: overlayHost
    }

    Connections {
        target: overlayHost

        function onClosed() {
            if (root.overlayHostContext === "glance") {
                root.glanceUrl = ""
            } else if (root.overlayHostContext === "onboarding") {
                browser.settings.onboardingSeen = true
            }

            root.overlayHostContext = ""
        }
    }

    PopupManager {
        id: popupManager
    }

    ToolWindowManager {
        id: toolWindowManager
    }

    ToolWindowManager {
        id: tabPreviewManager
    }

    QtObject {
        id: tabPreviewState
        property int tabId: 0
        property string title: ""
        property url url: ""
        property url thumbnailUrl: ""
    }

    function openTabPreview(anchorItem, tabId, title, url, thumbnailUrl) {
        if (!anchorItem || tabId <= 0) {
            return
        }

        tabPreviewState.tabId = tabId
        tabPreviewState.title = String(title || "")
        tabPreviewState.url = url
        tabPreviewState.thumbnailUrl = thumbnailUrl

        if (browser && browser.tabs && browser.tabs.markThumbnailUsedById) {
            browser.tabs.markThumbnailUsedById(tabId)
        }

        const pos = anchorItem.mapToItem(tabPreviewManager, anchorItem.width + 12, 0)
        tabPreviewManager.openAtPoint(tabPreviewComponent, pos.x, pos.y, root.contentItem)
    }

    function closeTabPreviewIfTabId(tabId) {
        if (tabPreviewState.tabId === tabId) {
            tabPreviewManager.close()
        }
    }

    Connections {
        target: tabPreviewManager

        function onClosed() {
            tabPreviewState.tabId = 0
            tabPreviewState.title = ""
            tabPreviewState.url = ""
            tabPreviewState.thumbnailUrl = ""
        }
    }

    Component {
        id: tabPreviewComponent

        Rectangle {
            id: card
            implicitWidth: 360
            implicitHeight: 260
            radius: root.uiRadius
            color: Qt.rgba(1, 1, 1, 0.98)
            border.color: Qt.rgba(0, 0, 0, 0.12)
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.uiSpacing
                spacing: Math.max(6, Math.round(root.uiSpacing / 2))

                Rectangle {
                    id: previewFrame
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    radius: Math.max(6, Math.round(root.uiRadius * 0.8))
                    color: Qt.rgba(0, 0, 0, 0.06)
                    clip: true

                    Image {
                        id: previewImage
                        anchors.fill: parent
                        visible: tabPreviewState.thumbnailUrl && tabPreviewState.thumbnailUrl.toString().length > 0
                        source: tabPreviewState.thumbnailUrl
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: 640
                        sourceSize.height: 360
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "No preview"
                        opacity: 0.65
                        visible: !previewImage.visible
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: tabPreviewState.title && tabPreviewState.title.length > 0 ? tabPreviewState.title : "Tab"
                    elide: Text.ElideRight
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: tabPreviewState.url ? tabPreviewState.url.toString() : ""
                    visible: text.length > 0
                    opacity: 0.65
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    Component {
        id: findBarComponent

        FindBar {
            view: root.focusedView
            onCloseRequested: toolWindowManager.close()
            Component.onCompleted: focusQuery()
        }
    }

    Component {
        id: tabSwitcherComponent

        TabSwitcher {
            tabs: root.browserModel.tabs
            browser: root.browserModel
            onCloseRequested: popupManager.close()
            Component.onCompleted: openSwitcher()
        }
    }

    Connections {
        target: popupManager

        function onClosed() {
            if (root.popupManagerContext === "web-context-menu") {
                root.webContextMenuItems = []
                root.webContextMenuTabId = 0
                root.webContextMenuInfo = ({})
            } else if (root.popupManagerContext === "permission-doorhanger") {
                const view = tabViews.byId[root.pendingPermissionTabId]
                const reqId = root.pendingPermissionRequestId
                root.clearPendingPermission()
                if (view && reqId > 0) {
                    view.respondToPermissionRequest(reqId, 0, false)
                }
            } else if (root.popupManagerContext === "omnibox") {
                omniboxModel.clear()
            } else if (root.popupManagerContext === "extensions-panel") {
                root.extensionsPanelSearch = ""
            } else if (root.popupManagerContext === "extension-popup") {
                root.extensionPopupExtensionId = ""
                root.extensionPopupName = ""
                root.extensionPopupUrl = ""
                root.extensionPopupOptionsUrl = ""
            }

            root.popupManagerContext = ""
        }
    }

    Connections {
        target: toolWindowManager

        function onClosed() {
            if (root.toolWindowManagerContext === "find-bar") {
                // no persistent state yet
            }
            root.toolWindowManagerContext = ""
        }
    }

    ToastHost { }

    Shortcuts { }

    Connections {
        target: root.modsModel

        function onCombinedCssChanged() {
            root.pushModsCss(null)
        }
    }

    Connections {
        target: browser

        function onTabsChanged() {
            Qt.callLater(syncTabViews)
        }
    }

    Connections {
        target: browser.tabs

        function onRowsInserted() { Qt.callLater(syncTabViews) }
        function onRowsRemoved() { Qt.callLater(syncTabViews) }
        function onRowsMoved() { Qt.callLater(syncTabViews) }
        function onModelReset() { Qt.callLater(syncTabViews) }

        function onDataChanged(topLeft, bottomRight, roles) {
            if (!browser.tabs) {
                return
            }

            if (roles && roles.length > 0 && roles.indexOf(TabModel.UrlRole) < 0) {
                return
            }

            const firstRow = topLeft ? topLeft.row : 0
            const lastRow = bottomRight ? bottomRight.row : browser.tabs.count() - 1

            for (let row = firstRow; row <= lastRow; row++) {
                const tabId = browser.tabs.tabIdAt(row)
                const view = tabViews.byId[tabId]
                if (!view) {
                    continue
                }

                const url = browser.tabs.urlAt(row)
                if (url && url.toString() !== view.currentUrl.toString()) {
                    view.navigate(url)
                }
            }

            syncAddressFieldFromFocused()
        }
    }

    Connections {
        target: commands

        function onCommandInvoked(id, args) {
            if (id === "focus-address") {
                const field = root.activeAddressField()
                if (field) {
                    field.forceActiveFocus()
                    field.selectAll()
                }
                return
            }
            if (id === "nav-back") {
                const view = root.focusedView
                const tabId = root.focusedTabId
                if (!view) {
                    toast.showToast("No active tab")
                    return
                }
                if (browser.handleBackRequested(tabId, view.canGoBack)) {
                    return
                }
                if (!view.canGoBack) {
                    toast.showToast("No history")
                    return
                }
                view.goBack()
                return
            }
            if (id === "nav-forward") {
                if (root.focusedView) {
                    root.focusedView.goForward()
                }
                return
            }
            if (id === "nav-reload") {
                if (root.focusedView) {
                    root.focusedView.reload()
                }
                return
            }
            if (id === "nav-stop") {
                if (root.focusedView) {
                    root.focusedView.stop()
                }
                return
            }
            if (id === "navigate") {
                if (root.focusedView) {
                    root.focusedView.navigate(args.url)
                }
                return
            }
            if (id === "zoom-in") {
                if (root.focusedView) {
                    root.focusedView.zoomIn()
                }
                return
            }
            if (id === "zoom-out") {
                if (root.focusedView) {
                    root.focusedView.zoomOut()
                }
                return
            }
            if (id === "zoom-reset") {
                if (root.focusedView) {
                    root.focusedView.zoomReset()
                }
                return
            }
            if (id === "open-file") {
                openFileDialog.open()
                return
            }
            if (id === "toggle-fullscreen") {
                root.toggleFullscreen()
                return
            }
            if (id === "view-source") {
                const tabId = args && args.tabId !== undefined ? Number(args.tabId) : root.focusedTabId
                root.openViewSourceForTab(tabId)
                return
            }
            if (id === "open-devtools") {
                if (root.focusedView) {
                    root.focusedView.openDevTools()
                }
                return
            }
            if (id === "open-settings") {
                root.openOverlay(settingsDialogComponent, "settings")
                return
            }
            if (id === "open-downloads") {
                root.toggleTopBarPopup("downloads-panel", downloadsPanelComponent, downloadsButton)
                return
            }
            if (id === "open-bookmarks") {
                root.openOverlay(bookmarksPanelComponent, "bookmarks")
                return
            }
            if (id === "open-history") {
                root.openOverlay(historyPanelComponent, "history")
                return
            }
            if (id === "open-permissions") {
                root.openOverlay(permissionsCenterComponent, "permissions")
                return
            }
            if (id === "open-find") {
                if (root.toolWindowManagerContext === "find-bar" && toolWindowManager.opened && toolWindowManager.popupItem) {
                    if (toolWindowManager.popupItem.focusQuery) {
                        toolWindowManager.popupItem.focusQuery()
                    }
                    return
                }
                root.toolWindowManagerContext = "find-bar"
                const x = Math.max(8, Math.round(root.width - 16))
                const expectsExpandedTopBar = browser.settings.addressBarVisible && !root.singleToolbarActive()
                const y = Math.round((expectsExpandedTopBar ? topBar.expandedHeight : topBar.height) + 12)
                toolWindowManager.openAtPoint(findBarComponent, x, y, root)
                return
            }
            if (id === "open-tab-switcher") {
                root.toggleTabSwitcherPopup()
                return
            }
            if (id === "open-print") {
                root.openOverlay(printDialogComponent, "print")
                return
            }
            if (id === "open-mods") {
                root.openOverlay(modsDialogComponent, "mods")
                return
            }
            if (id === "open-themes") {
                root.openOverlay(themesDialogComponent, "themes")
                return
            }
            if (id === "open-clear-data") {
                root.openOverlay(clearDataDialogComponent, "clear-data")
                return
            }
            if (id === "open-extensions") {
                root.openOverlay(extensionsDialogComponent, "extensions")
                return
            }
            if (id === "open-welcome") {
                root.openOverlay(onboardingDialogComponent, "onboarding")
                return
            }
            if (id === "open-diagnostics") {
                root.openOverlay(diagnosticsDialogComponent, "diagnostics")
                return
            }
            if (id === "open-latest-download-file") {
                const path = downloads && downloads.latestFinishedFilePath ? downloads.latestFinishedFilePath() : ""
                if (path && String(path).length > 0) {
                    const ok = nativeUtils.openPath(path)
                    if (!ok) {
                        const err = nativeUtils.lastError || ""
                        toast.showToast(err.length > 0 ? ("Open failed: " + err) : "Open failed")
                    }
                } else {
                    toast.showToast("No finished downloads")
                }
                return
            }
            if (id === "open-latest-download-folder") {
                const folder = downloads && downloads.latestFinishedFolderPath ? downloads.latestFinishedFolderPath() : ""
                if (folder && String(folder).length > 0) {
                    const ok = nativeUtils.openFolder(folder)
                    if (!ok) {
                        const err = nativeUtils.lastError || ""
                        toast.showToast(err.length > 0 ? ("Open folder failed: " + err) : "Open folder failed")
                    }
                } else {
                    toast.showToast("No finished downloads")
                }
                return
            }
            if (id === "retry-last-download") {
                const url = root.lastFailedDownloadUri ? String(root.lastFailedDownloadUri) : ""
                if (url.trim().length > 0) {
                    browser.newTab(url)
                } else {
                    root.toggleTopBarPopup("downloads-panel", downloadsPanelComponent, downloadsButton)
                }
                return
            }
            if (id === "focus-split-primary") {
                if (splitView.enabled) {
                    splitView.focusedPane = 0
                }
                return
            }
            if (id === "focus-split-secondary") {
                if (splitView.enabled) {
                    splitView.focusedPane = 1
                }
                return
            }
            if (id === "split-swap") {
                if (splitView.enabled) {
                    splitView.swapPanes()
                }
                return
            }
            if (id === "split-close-pane") {
                if (splitView.enabled) {
                    splitView.closeFocusedPane()
                }
                return
            }
            if (id === "split-focus-next") {
                if (splitView.enabled) {
                    splitView.focusNextPane()
                }
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

    Component {
        id: themesDialogComponent

        ThemesDialog {
            themes: root.themesModel
            settings: root.browserModel.settings
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: clearDataDialogComponent

        ClearDataDialog {
            view: root.focusedView
            history: root.historyModel
            downloads: root.downloadsModel
            sitePermissions: sitePermissions
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: settingsDialogComponent

        SettingsDialog {
            settings: root.browserModel.settings
            themes: root.themesModel
            bookmarks: root.bookmarksModel
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: downloadsDialogComponent

        DownloadsDialog {
            downloads: root.downloadsModel
            downloadController: root
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: sidebarBookmarksPopupComponent

        Item {
            property int maxHeight: 0
            implicitWidth: 720
            implicitHeight: maxHeight > 0 ? Math.min(560, maxHeight) : 560

            BookmarksPanel {
                anchors.fill: parent
                bookmarks: root.bookmarksModel
                popupManager: popupManager
                embedded: true
                onCloseRequested: popupManager.close()
            }
        }
    }

    Component {
        id: sidebarHistoryPopupComponent

        Item {
            property int maxHeight: 0
            implicitWidth: 760
            implicitHeight: maxHeight > 0 ? Math.min(580, maxHeight) : 580

            HistoryPanel {
                anchors.fill: parent
                history: root.historyModel
                popupManager: popupManager
                embedded: true
                onCloseRequested: popupManager.close()
            }
        }
    }

    Component {
        id: sidebarDownloadsPopupComponent

        Item {
            property int maxHeight: 0
            implicitWidth: 620
            implicitHeight: maxHeight > 0 ? Math.min(480, maxHeight) : 480

            DownloadsDialog {
                anchors.fill: parent
                downloads: root.downloadsModel
                downloadController: root
                embedded: true
                onCloseRequested: popupManager.close()
            }
        }
    }

    Component {
        id: sidebarPanelsPopupComponent

        Item {
            property int maxHeight: 0
            implicitWidth: 520
            implicitHeight: maxHeight > 0 ? Math.min(560, maxHeight) : 560

            WebPanelsDialog {
                anchors.fill: parent
                panels: root.webPanelsModel
                embedded: true
                currentUrl: root.focusedView ? root.focusedView.currentUrl : "about:blank"
                currentTitle: root.focusedView ? root.focusedView.title : ""
                onCloseRequested: popupManager.close()
                onOpenRequested: (url, title) => {
                    popupManager.close()
                    root.openWebPanel(url, title)
                }
            }
        }
    }

    Component {
        id: sidebarExtensionsPopupComponent

        Item {
            property int maxHeight: 0
            implicitWidth: 980
            implicitHeight: maxHeight > 0 ? Math.min(660, maxHeight) : 660

            ExtensionsDialog {
                anchors.fill: parent
                extensions: extensions
                embedded: true
                onCloseRequested: popupManager.close()
            }
        }
    }

    Component {
        id: sidebarBookmarksPanelComponent

        BookmarksPanel {
            bookmarks: root.bookmarksModel
            popupManager: popupManager
            embedded: true
            onCloseRequested: root.sidebarPanel = "tabs"
        }
    }

    Component {
        id: sidebarHistoryPanelComponent

        HistoryPanel {
            history: root.historyModel
            popupManager: popupManager
            embedded: true
            onCloseRequested: root.sidebarPanel = "tabs"
        }
    }

    Component {
        id: sidebarDownloadsPanelComponent

        DownloadsDialog {
            downloads: root.downloadsModel
            downloadController: root
            embedded: true
            onCloseRequested: root.sidebarPanel = "tabs"
        }
    }

    Component {
        id: sidebarExtensionsPanelComponent

        ExtensionsDialog {
            extensions: extensions
            embedded: true
            onCloseRequested: root.sidebarPanel = "tabs"
        }
    }

    Component {
        id: bookmarksPanelComponent

        BookmarksPanel {
            bookmarks: root.bookmarksModel
            popupManager: popupManager
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: historyPanelComponent

        HistoryPanel {
            history: root.historyModel
            popupManager: popupManager
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: permissionsCenterComponent

        PermissionsCenter {
            store: sitePermissions
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: printDialogComponent

        PrintDialog {
            view: root.focusedView
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: modsDialogComponent

        ModsDialog {
            mods: root.modsModel
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: extensionsDialogComponent

        ExtensionsDialog {
            extensions: extensions
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: diagnosticsDialogComponent

        DiagnosticsDialog {
            diagnostics: diagnostics
            view: root.focusedView
            onCloseRequested: overlayHost.hide()
        }
    }

    Component {
        id: onboardingDialogComponent

        OnboardingDialog {
            settings: root.browserModel.settings
            onAccepted: overlayHost.hide()
            onRejected: overlayHost.hide()
            onCloseRequested: overlayHost.hide()
        }
    }

    function syncSplitViews() {
        if (!splitView.enabled) {
            splitView.focusedPane = 0
            syncAddressFieldFromFocused()
            return
        }

        const tabId = splitView.tabIdForPane(splitView.focusedPane)
        if (tabId > 0) {
            browser.activateTabById(tabId)
        } else {
            splitView.focusedPane = 0
        }

        syncAddressFieldFromFocused()
    }

    Connections {
        target: browser.tabs

        function onActiveIndexChanged() {
            const idx = browser.tabs.activeIndex
            if (idx < 0) {
                return
            }
            const tabId = browser.tabs.tabIdAt(idx)
            if (splitView.enabled) {
                splitView.setTabIdForPane(splitView.focusedPane, tabId)
            }
            syncAddressFieldFromFocused()
        }
    }
}
