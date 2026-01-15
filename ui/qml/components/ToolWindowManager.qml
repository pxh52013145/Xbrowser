import QtQuick
import QtQuick.Controls
import QtQuick.Window

Item {
    id: root
    anchors.fill: parent
    z: 940

    signal closed()

    readonly property bool opened: toolWindow.visible
    readonly property var popupItem: toolLoader.item

    property real pendingX: 0
    property real pendingY: 0
    property var pendingRect: null
    property var pendingAnchorRect: null
    property bool pendingOpen: false
    property bool closing: false

    function constrainRectForItem(item) {
        if (!item) {
            return null
        }
        const pos = item.mapToItem(root, 0, 0)
        return {
            x: Math.round(pos.x),
            y: Math.round(pos.y),
            width: Math.round(item.width),
            height: Math.round(item.height),
        }
    }

    function clampToRect(x, y, w, h, rect) {
        const margin = 8
        const bounds = rect || ({ x: 0, y: 0, width: root.width, height: root.height })

        const minX = bounds.x + margin
        const minY = bounds.y + margin
        const maxX = bounds.x + bounds.width - w - margin
        const maxY = bounds.y + bounds.height - h - margin

        let nextX = Math.round(x)
        let nextY = Math.round(y)

        if (nextX > maxX) {
            const anchor = pendingAnchorRect
            nextX = anchor ? Math.round(anchor.x + anchor.width - w) : Math.round(nextX - w)
        }

        if (nextY > maxY) {
            const anchor = pendingAnchorRect
            nextY = anchor ? Math.round(anchor.y - h) : Math.round(nextY - h)
        }

        const clampedX = maxX < minX ? minX : Math.max(minX, Math.min(maxX, nextX))
        const clampedY = maxY < minY ? minY : Math.max(minY, Math.min(maxY, nextY))

        return { x: clampedX, y: clampedY }
    }

    function applyItemConstraints(rect) {
        if (!toolLoader.item) {
            return
        }

        const bounds = rect || ({ x: 0, y: 0, width: root.width, height: root.height })
        const margin = 8
        const maxHeight = Math.max(1, Math.round(bounds.height - margin * 2))

        if ("maxHeight" in toolLoader.item) {
            if (toolLoader.item.maxHeight === 0 || toolLoader.item.maxHeight > maxHeight) {
                toolLoader.item.maxHeight = maxHeight
            }
        }
    }

    function close() {
        if (closing) {
            return
        }
        closing = true
        const hadPopup = pendingOpen || toolWindow.visible || toolLoader.sourceComponent !== null
        toolWindow.visible = false
        toolLoader.sourceComponent = null
        pendingOpen = false
        pendingAnchorRect = null
        if (hadPopup) {
            closed()
        }
        closing = false
    }

    function openAtItem(component, anchorItem, constrainItem) {
        if (!component || !anchorItem) {
            return
        }

        close()
        toolLoader.sourceComponent = component
        const pos = anchorItem.mapToItem(root, 0, anchorItem.height)
        pendingX = pos.x
        pendingY = pos.y
        pendingRect = constrainRectForItem(constrainItem)
        pendingAnchorRect = constrainRectForItem(anchorItem)
        pendingOpen = true
        Qt.callLater(applyPendingGeometry)
    }

    function openAtPoint(component, x, y, constrainItem) {
        if (!component) {
            return
        }

        close()
        toolLoader.sourceComponent = component
        pendingX = x
        pendingY = y
        pendingRect = constrainRectForItem(constrainItem)
        pendingAnchorRect = ({ x: Math.round(x), y: Math.round(y), width: 1, height: 1 })
        pendingOpen = true
        Qt.callLater(applyPendingGeometry)
    }

    function applyPendingGeometry() {
        if ((!pendingOpen && !toolWindow.visible) || !toolLoader.item) {
            return
        }

        applyItemConstraints(pendingRect)

        const w = Math.max(1, toolWindow.width)
        const h = Math.max(1, toolWindow.height)
        const p = clampToRect(pendingX, pendingY, w, h, pendingRect)
        const global = root.mapToGlobal(p.x, p.y)

        toolWindow.x = Math.round(global.x)
        toolWindow.y = Math.round(global.y)
        toolWindow.visible = true
        toolWindow.raise()

        pendingOpen = false
    }

    Connections {
        target: root.window

        function onXChanged() { Qt.callLater(root.applyPendingGeometry) }
        function onYChanged() { Qt.callLater(root.applyPendingGeometry) }
        function onWidthChanged() { Qt.callLater(root.applyPendingGeometry) }
        function onHeightChanged() { Qt.callLater(root.applyPendingGeometry) }
    }

    Window {
        id: toolWindow
        visible: false
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "transparent"
        transientParent: root.window ? root.window : null

        width: toolLoader.item ? Math.max(1, toolLoader.item.implicitWidth) : 1
        height: toolLoader.item ? Math.max(1, toolLoader.item.implicitHeight) : 1

        onWidthChanged: {
            if (visible && !root.closing) {
                Qt.callLater(root.applyPendingGeometry)
            }
        }

        onHeightChanged: {
            if (visible && !root.closing) {
                Qt.callLater(root.applyPendingGeometry)
            }
        }

        Shortcut {
            enabled: toolWindow.visible
            sequence: StandardKey.Cancel
            context: Qt.WindowShortcut
            onActivated: root.close()
        }

        Loader {
            id: toolLoader
            anchors.fill: parent
            onLoaded: Qt.callLater(root.applyPendingGeometry)
        }
    }
}
