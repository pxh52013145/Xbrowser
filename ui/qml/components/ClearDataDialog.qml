import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    anchors.fill: parent

    required property var view
    required property var history
    required property var downloads
    required property var sitePermissions

    signal closeRequested()

    property bool clearing: false
    property int timeRangeIndex: 3 // 0=1h, 1=24h, 2=7d, 3=all

    property bool clearHistory: true
    property bool clearCookies: true
    property bool clearCache: true
    property bool clearDownloads: false
    property bool clearPermissions: false

    property int pendingWebKinds: 0

    readonly property bool hasSelection: clearHistory || clearCookies || clearCache || clearDownloads || clearPermissions

    function timeRangeMs() {
        if (timeRangeIndex === 0) {
            return 60 * 60 * 1000
        }
        if (timeRangeIndex === 1) {
            return 24 * 60 * 60 * 1000
        }
        if (timeRangeIndex === 2) {
            return 7 * 24 * 60 * 60 * 1000
        }
        return 0
    }

    function computeRange() {
        const span = timeRangeMs()
        if (span <= 0) {
            return { fromMs: 0, toMs: 0 }
        }
        const toMs = Date.now()
        return { fromMs: Math.max(1, toMs - span), toMs: toMs }
    }

    function computeWebKinds() {
        let kinds = 0
        if (clearHistory) {
            kinds |= 0x1000 // COREWEBVIEW2_BROWSING_DATA_KINDS_BROWSING_HISTORY
        }
        if (clearCookies) {
            kinds |= 0x40 // COREWEBVIEW2_BROWSING_DATA_KINDS_COOKIES
        }
        if (clearCache) {
            kinds |= 0x100 // COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE
        }
        if (clearDownloads) {
            kinds |= 0x200 // COREWEBVIEW2_BROWSING_DATA_KINDS_DOWNLOAD_HISTORY
        }
        return kinds
    }

    function clearNow() {
        if (clearing || !hasSelection) {
            return
        }

        clearing = true
        const range = computeRange()
        const fromMs = range.fromMs
        const toMs = range.toMs

        if (history) {
            if (clearHistory) {
                if (fromMs > 0 && toMs > fromMs && history.clearRange) {
                    history.clearRange(fromMs, toMs)
                } else if (history.clearAll) {
                    history.clearAll()
                }
            }
        }

        if (downloads) {
            if (clearDownloads) {
                if (fromMs > 0 && toMs > fromMs && downloads.clearRange) {
                    downloads.clearRange(fromMs, toMs)
                } else if (downloads.clearAll) {
                    downloads.clearAll()
                } else if (downloads.clearFinished) {
                    downloads.clearFinished()
                }
            }
        }

        if (sitePermissions && clearPermissions && sitePermissions.clearAll) {
            sitePermissions.clearAll()
        }

        const kinds = computeWebKinds()
        pendingWebKinds = kinds
        if (kinds === 0) {
            clearing = false
            toast.showToast("Cleared browsing data")
            root.closeRequested()
            return
        }

        if (!view || !view.clearBrowsingData) {
            clearing = false
            toast.showToast("Web engine unavailable (could not clear cookies/cache)")
            root.closeRequested()
            return
        }

        view.clearBrowsingData(kinds, fromMs, toMs)
    }

    Connections {
        target: root.view
        function onBrowsingDataCleared(dataKinds, success, error) {
            if (!root.clearing) {
                return
            }
            if (dataKinds !== root.pendingWebKinds) {
                return
            }

            root.clearing = false

            if (success) {
                toast.showToast("Cleared browsing data")
            } else {
                toast.showToast(error && error.length > 0 ? ("Clear failed: " + error) : "Clear failed")
            }
            root.closeRequested()
        }
    }

    Rectangle {
        id: panel
        width: Math.min(560, parent.width - 80)
        height: Math.min(520, parent.height - 80)
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        radius: theme.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.98)
        border.color: Qt.rgba(0, 0, 0, 0.12)
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: theme.spacing
            spacing: theme.spacing

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: "Clear browsing data"
                    font.bold: true
                    font.pixelSize: 14
                }

                ToolButton {
                    enabled: !root.clearing
                    text: "×"
                    onClicked: root.closeRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label { text: "Time range"; opacity: 0.85 }

                ComboBox {
                    Layout.fillWidth: true
                    enabled: !root.clearing
                    model: ["Last hour", "Last 24 hours", "Last 7 days", "All time"]
                    currentIndex: root.timeRangeIndex
                    onActivated: root.timeRangeIndex = currentIndex
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Math.max(6, Math.round(theme.spacing / 2))

                    CheckBox { text: "Browsing history"; enabled: !root.clearing; checked: root.clearHistory; onToggled: root.clearHistory = checked }
                    CheckBox { text: "Cookies"; enabled: !root.clearing; checked: root.clearCookies; onToggled: root.clearCookies = checked }
                    CheckBox { text: "Cached images and files"; enabled: !root.clearing; checked: root.clearCache; onToggled: root.clearCache = checked }
                    CheckBox { text: "Download history"; enabled: !root.clearing; checked: root.clearDownloads; onToggled: root.clearDownloads = checked }
                    CheckBox { text: "Site permissions"; enabled: !root.clearing; checked: root.clearPermissions; onToggled: root.clearPermissions = checked }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: theme.spacing

                        BusyIndicator {
                            running: root.clearing
                            visible: root.clearing
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.clearing ? "Clearing…" : ""
                            opacity: 0.75
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    enabled: !root.clearing
                    text: "Cancel"
                    onClicked: root.closeRequested()
                }

                Item { Layout.fillWidth: true }

                Button {
                    enabled: root.hasSelection && !root.clearing
                    text: "Clear"
                    onClicked: root.clearNow()
                }
            }
        }
    }

    Keys.onEscapePressed: if (!root.clearing) root.closeRequested()
}

