import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property int cornerRadius: 10
    property int spacing: 8

    required property url pageUrl
    property bool canShare: false

    signal closeRequested()
    signal permissionsRequested()
    signal siteDataRequested()
    signal copyUrlRequested()
    signal shareUrlRequested()
    signal extensionsRequested()
    signal devToolsRequested()

    readonly property string scheme: String(pageUrl || "").split(":")[0].toLowerCase()
    readonly property string host: pageUrl && pageUrl.host ? pageUrl.host : ""

    readonly property string securityTitle: {
        if (scheme === "https") return "Connection is secure"
        if (scheme === "http") return "Connection is not secure"
        if (host && host.trim().length > 0) return "Site information"
        return "Page information"
    }

    readonly property string securityDetail: {
        if (scheme === "https") return "Your information is private when it is sent to this site."
        if (scheme === "http") return "This site does not use a secure connection."
        return ""
    }

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 320
    implicitHeight: column.implicitHeight + root.spacing * 2

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
                text: root.host && root.host.trim().length > 0 ? root.host : String(root.pageUrl || "")
                elide: Text.ElideRight
                font.bold: true
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

        Label {
            Layout.fillWidth: true
            text: root.securityTitle
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: root.securityDetail
            opacity: 0.75
            wrapMode: Text.Wrap
            visible: root.securityDetail && root.securityDetail.trim().length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Button {
                Layout.fillWidth: true
                text: "Permissions"
                onClicked: root.permissionsRequested()
            }

            Button {
                Layout.fillWidth: true
                text: "Site data"
                onClicked: root.siteDataRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Button {
                Layout.fillWidth: true
                visible: !root.canShare
                text: "Copy URL"
                onClicked: root.copyUrlRequested()
            }

            Button {
                Layout.fillWidth: true
                visible: root.canShare
                text: "Share URL"
                onClicked: root.shareUrlRequested()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.spacing

            Button {
                Layout.fillWidth: true
                text: "Extensions"
                onClicked: root.extensionsRequested()
            }

            Button {
                Layout.fillWidth: true
                text: "DevTools"
                onClicked: root.devToolsRequested()
            }
        }
    }
}
