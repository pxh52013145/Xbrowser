import QtQuick
import QtQuick.Controls
import QtQuick.Window

Item {
    id: root
    anchors.fill: parent
    z: 950

    signal closed()

    readonly property bool opened: popupWindow.visible
    readonly property var popupItem: popupLoader.item

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
        if (!popupLoader.item) {
            return
        }

        const bounds = rect || ({ x: 0, y: 0, width: root.width, height: root.height })
        const margin = 8
        const maxHeight = Math.max(1, Math.round(bounds.height - margin * 2))

        if ("maxHeight" in popupLoader.item) {
            if (popupLoader.item.maxHeight === 0 || popupLoader.item.maxHeight > maxHeight) {
                popupLoader.item.maxHeight = maxHeight
            }
        }
    }

    function close() {
        if (closing) {
            return
        }
        closing = true
        const hadPopup = pendingOpen || popupWindow.visible || popupLoader.sourceComponent !== null
        popupWindow.visible = false
        popupLoader.sourceComponent = null
        pendingOpen = false
        pendingAnchorRect = null
        if (hadPopup) {
            closed()
        }
        closing = false
    }

    function openAtItem(component, anchorItem, constrainItem, anchorX, anchorY) {
        if (!component || !anchorItem) {
            return
        }

        close()
        popupLoader.sourceComponent = component
        const ax = (anchorX !== undefined && anchorX !== null) ? anchorX : 0
        const ay = (anchorY !== undefined && anchorY !== null) ? anchorY : anchorItem.height
        const pos = anchorItem.mapToItem(root, ax, ay)
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
        popupLoader.sourceComponent = component
        pendingX = x
        pendingY = y
        pendingRect = constrainRectForItem(constrainItem)
        pendingAnchorRect = ({ x: Math.round(x), y: Math.round(y), width: 1, height: 1 })
        pendingOpen = true
        Qt.callLater(applyPendingGeometry)
    }

    function applyPendingGeometry() {
        if ((!pendingOpen && !popupWindow.visible) || !popupLoader.item) {
            return
        }

        applyItemConstraints(pendingRect)

        const w = Math.max(1, popupWindow.width)
        const h = Math.max(1, popupWindow.height)
        const p = clampToRect(pendingX, pendingY, w, h, pendingRect)
        const global = root.mapToGlobal(p.x, p.y)

        popupWindow.x = Math.round(global.x)
        popupWindow.y = Math.round(global.y)
        popupWindow.visible = true
        popupWindow.raise()
        popupWindow.requestActivate()

        pendingOpen = false
    }

    Window {
        id: popupWindow
        visible: false
        flags: Qt.Popup | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "transparent"
        transientParent: root.window ? root.window : null

        width: popupLoader.item ? popupLoader.item.implicitWidth : 1
        height: popupLoader.item ? popupLoader.item.implicitHeight : 1

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

        onActiveChanged: {
            if (!active && visible) {
                root.close()
            }
        }

        onVisibleChanged: {
            if (!visible && !root.closing && popupLoader.sourceComponent !== null) {
                root.close()
            }
        }

        Shortcut {
            enabled: popupWindow.visible
            sequence: StandardKey.Cancel
            context: Qt.WindowShortcut
            onActivated: root.close()
        }

        Loader {
            id: popupLoader
            anchors.fill: parent
            onLoaded: Qt.callLater(root.applyPendingGeometry)
        }
    }
}
