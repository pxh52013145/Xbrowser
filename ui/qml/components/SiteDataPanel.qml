import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import XBrowser 1.0

Rectangle {
    id: root

    property int cornerRadius: 10
    property int spacing: 8

    required property var view
    required property url pageUrl
    required property string host

    signal closeRequested()

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 420
    implicitHeight: Math.min(520, column.implicitHeight + root.spacing * 2)

    WebView2CookieModel {
        id: cookies
        view: root.view
        url: root.pageUrl
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: root.spacing
        spacing: root.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Label {
                Layout.fillWidth: true
                text: "Site data"
                font.bold: true
                elide: Text.ElideRight
            }

            ToolButton {
                text: "×"
                onClicked: root.closeRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.host && root.host.trim().length > 0 ? root.host : String(root.pageUrl || "")
            wrapMode: Text.Wrap
            opacity: 0.75
            visible: (root.host && root.host.trim().length > 0) || String(root.pageUrl || "").length > 0
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(0, 0, 0, 0.08)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Button {
                text: cookies.loading ? "Loading..." : "Refresh"
                enabled: !cookies.loading
                onClicked: cookies.refresh()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Clear cookies"
                enabled: !cookies.loading && cookies.count > 0
                onClicked: cookies.clearAll()
            }
        }

        Label {
            Layout.fillWidth: true
            text: cookies.lastError
            color: "#B00020"
            wrapMode: Text.Wrap
            visible: cookies.lastError && cookies.lastError.trim().length > 0
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: cookies.loading
            visible: cookies.loading
        }

        Label {
            Layout.fillWidth: true
            text: "No cookies found."
            opacity: 0.65
            visible: !cookies.loading && (!cookies.lastError || cookies.lastError.trim().length === 0) && cookies.count === 0
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(360, contentHeight)
            clip: true
            model: cookies
            spacing: root.spacing
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                width: ListView.view.width
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.03)
                border.color: Qt.rgba(0, 0, 0, 0.06)
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: name
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        ToolButton {
                            text: "Delete"
                            onClicked: cookies.deleteAt(index)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: domain + path
                        opacity: 0.75
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        opacity: 0.65
                        wrapMode: Text.Wrap
                        text: {
                            const tags = []
                            if (secure) tags.push("Secure")
                            if (httpOnly) tags.push("HttpOnly")
                            if (session) tags.push("Session")
                            if (!session && expiresMs > 0) tags.push("Expires: " + (new Date(expiresMs)).toLocaleString())
                            return tags.join(" · ")
                        }
                        visible: secure || httpOnly || session || expiresMs > 0
                    }
                }
            }
        }
    }
}

