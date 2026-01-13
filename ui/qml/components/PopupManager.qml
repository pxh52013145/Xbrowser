import QtQuick
import QtQuick.Controls

Item {
    id: root
    anchors.fill: parent
    z: 950

    function close() {
        popup.close()
        popupLoader.sourceComponent = null
    }

    function openAtItem(component, anchorItem) {
        if (!component || !anchorItem) {
            return
        }

        popupLoader.sourceComponent = component
        const pos = anchorItem.mapToItem(root, 0, anchorItem.height)
        popup.x = Math.max(8, Math.min(root.width - popup.implicitWidth - 8, Math.round(pos.x)))
        popup.y = Math.max(8, Math.min(root.height - popup.implicitHeight - 8, Math.round(pos.y)))
        popup.open()
    }

    function openAtPoint(component, x, y) {
        if (!component) {
            return
        }

        popupLoader.sourceComponent = component
        popup.x = Math.max(8, Math.min(root.width - popup.implicitWidth - 8, Math.round(x)))
        popup.y = Math.max(8, Math.min(root.height - popup.implicitHeight - 8, Math.round(y)))
        popup.open()
    }

    Popup {
        id: popup
        modal: false
        focus: true
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        onClosed: popupLoader.sourceComponent = null

        contentItem: Loader {
            id: popupLoader
        }
    }
}

