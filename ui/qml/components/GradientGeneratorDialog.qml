import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

Rectangle {
    id: root

    required property var workspaces
    required property int workspaceIndex

    signal closeRequested()

    color: Qt.rgba(1, 1, 1, 0.98)
    radius: theme.cornerRadius
    border.color: Qt.rgba(0, 0, 0, 0.08)
    border.width: 1

    implicitWidth: 420
    implicitHeight: layout.implicitHeight + theme.spacing * 2

    property color baseFrom: Qt.rgba(1, 1, 1, 1)
    property color baseMid: Qt.rgba(1, 1, 1, 1)
    property color baseTo: Qt.rgba(1, 1, 1, 1)
    property bool useThirdColor: false
    property int angle: 0
    property int strength: 20

    function mixWithWhite(c, factor) {
        const f = Math.max(0, Math.min(1, Number(factor || 0)))
        return Qt.rgba(
            c.r * (1.0 - f) + 1.0 * f,
            c.g * (1.0 - f) + 1.0 * f,
            c.b * (1.0 - f) + 1.0 * f,
            1.0
        )
    }

    function applyStrength(c) {
        const s = Math.max(0, Math.min(100, Number(root.strength || 0)))
        const normalized = 1.0 - (s / 100.0)
        return root.mixWithWhite(c, normalized * 0.92)
    }

    function toHex(c) {
        function ch(v) {
            const n = Math.max(0, Math.min(255, Math.round(Number(v || 0) * 255)))
            const s = n.toString(16)
            return s.length === 1 ? ("0" + s) : s
        }
        return "#" + ch(c.r) + ch(c.g) + ch(c.b)
    }

    readonly property color effectiveFrom: applyStrength(baseFrom)
    readonly property color effectiveMid: applyStrength(baseMid)
    readonly property color effectiveTo: applyStrength(baseTo)

    readonly property string cssGradient: {
        const a = Math.max(0, Math.min(359, Number(root.angle || 0)))
        const c1 = root.toHex(root.effectiveFrom)
        if (root.useThirdColor) {
            const c2 = root.toHex(root.effectiveMid)
            const c3 = root.toHex(root.effectiveTo)
            return "linear-gradient(" + a + "deg, " + c1 + ", " + c2 + ", " + c3 + ")"
        }
        const c2 = root.toHex(root.effectiveTo)
        return "linear-gradient(" + a + "deg, " + c1 + ", " + c2 + ")"
    }

    Component.onCompleted: {
        if (!root.workspaces || root.workspaceIndex < 0) {
            root.baseFrom = theme.accentColor
            root.baseTo = Qt.darker(theme.accentColor, 1.3)
            root.baseMid = Qt.darker(theme.accentColor, 1.1)
            return
        }

        if (root.workspaces.hasCustomBackgroundAt && root.workspaces.hasCustomBackgroundAt(root.workspaceIndex)) {
            root.baseFrom = root.workspaces.backgroundFromAt(root.workspaceIndex)
            root.baseTo = root.workspaces.backgroundToAt(root.workspaceIndex)
            root.useThirdColor = root.workspaces.hasCustomBackgroundMidAt
                                 ? root.workspaces.hasCustomBackgroundMidAt(root.workspaceIndex)
                                 : false
            root.baseMid = root.useThirdColor ? root.workspaces.backgroundMidAt(root.workspaceIndex) : Qt.darker(root.baseFrom, 1.05)
            root.angle = root.workspaces.backgroundAngleAt ? root.workspaces.backgroundAngleAt(root.workspaceIndex) : 0
            root.strength = root.workspaces.backgroundStrengthAt ? root.workspaces.backgroundStrengthAt(root.workspaceIndex) : 20
            return
        }

        root.baseFrom = theme.accentColor
        root.baseTo = Qt.darker(theme.accentColor, 1.3)
        root.baseMid = Qt.darker(theme.accentColor, 1.1)
        root.useThirdColor = false
        root.angle = 0
        root.strength = 20
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: "Background Gradient"
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    text: root.workspaces && root.workspaceIndex >= 0 ? ("Workspace: " + root.workspaces.nameAt(root.workspaceIndex)) : ""
                    opacity: 0.75
                    visible: text.length > 0
                    font.pixelSize: 12
                }
            }

            ToolButton {
                text: "×"
                Accessible.name: "Close"
                onClicked: root.closeRequested()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            radius: 10
            border.color: Qt.rgba(0, 0, 0, 0.12)
            border.width: 1
            clip: true

            Rectangle {
                anchors.centerIn: parent
                width: Math.ceil(Math.sqrt(parent.width * parent.width + parent.height * parent.height))
                height: width
                rotation: root.angle
                gradient: Gradient {
                    GradientStop { position: 0.0; color: root.effectiveFrom }
                    GradientStop { position: 0.5; color: root.useThirdColor ? root.effectiveMid : Qt.rgba(
                                       (root.effectiveFrom.r + root.effectiveTo.r) / 2,
                                       (root.effectiveFrom.g + root.effectiveTo.g) / 2,
                                       (root.effectiveFrom.b + root.effectiveTo.b) / 2,
                                       1.0) }
                    GradientStop { position: 1.0; color: root.effectiveTo }
                }
            }
        }

        CheckBox {
            Layout.fillWidth: true
            text: "Use 3 colors"
            checked: root.useThirdColor
            onToggled: root.useThirdColor = checked
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: theme.spacing
            rowSpacing: Math.max(4, Math.round(theme.spacing / 2))

            Label { text: "Color 1"; opacity: 0.85 }
            Rectangle { width: 26; height: 18; radius: 4; color: root.baseFrom; border.color: Qt.rgba(0, 0, 0, 0.18); border.width: 1 }
            Button { text: "Pick"; onClicked: color1Dialog.open() }

            Label { text: root.useThirdColor ? "Color 2" : "Color 2"; opacity: 0.85 }
            Rectangle { width: 26; height: 18; radius: 4; color: root.useThirdColor ? root.baseMid : root.baseTo; border.color: Qt.rgba(0, 0, 0, 0.18); border.width: 1 }
            Button { text: "Pick"; onClicked: (root.useThirdColor ? color2Dialog : color3Dialog).open() }

            Label { text: "Color 3"; opacity: 0.85; visible: root.useThirdColor }
            Rectangle { width: 26; height: 18; radius: 4; color: root.baseTo; border.color: Qt.rgba(0, 0, 0, 0.18); border.width: 1; visible: root.useThirdColor }
            Button { text: "Pick"; visible: root.useThirdColor; onClicked: color3Dialog.open() }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Math.max(4, Math.round(theme.spacing / 2))

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Angle"; opacity: 0.85 }
                Item { Layout.fillWidth: true }
                Label { text: String(root.angle) + "°"; opacity: 0.75 }
            }

            Slider {
                Layout.fillWidth: true
                from: 0
                to: 359
                stepSize: 1
                value: root.angle
                onMoved: root.angle = Math.round(value)
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Strength"; opacity: 0.85 }
                Item { Layout.fillWidth: true }
                Label { text: String(root.strength) + "%"; opacity: 0.75 }
            }

            Slider {
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: root.strength
                onMoved: root.strength = Math.round(value)
            }
        }

        TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            readOnly: true
            wrapMode: Text.Wrap
            selectByMouse: true
            text: root.cssGradient
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: theme.spacing

            Button {
                text: "Reset"
                enabled: root.workspaces && root.workspaceIndex >= 0 && root.workspaces.clearBackgroundGradientAt
                onClicked: {
                    if (!root.workspaces || root.workspaceIndex < 0) return
                    root.workspaces.clearBackgroundGradientAt(root.workspaceIndex)
                    root.closeRequested()
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Cancel"
                onClicked: root.closeRequested()
            }

            Button {
                text: "Apply"
                enabled: root.workspaces && root.workspaceIndex >= 0 && root.workspaces.setBackgroundGradient2At
                onClicked: {
                    if (!root.workspaces || root.workspaceIndex < 0) return

                    if (root.useThirdColor && root.workspaces.setBackgroundGradient3At) {
                        root.workspaces.setBackgroundGradient3At(root.workspaceIndex, root.baseFrom, root.baseMid, root.baseTo, root.angle, root.strength)
                    } else {
                        root.workspaces.setBackgroundGradient2At(root.workspaceIndex, root.baseFrom, root.baseTo, root.angle, root.strength)
                    }
                    root.closeRequested()
                }
            }
        }
    }

    Platform.ColorDialog {
        id: color1Dialog
        title: "Pick Color 1"
        currentColor: root.baseFrom
        onAccepted: root.baseFrom = color
    }

    Platform.ColorDialog {
        id: color2Dialog
        title: "Pick Color 2"
        currentColor: root.baseMid
        onAccepted: root.baseMid = color
    }

    Platform.ColorDialog {
        id: color3Dialog
        title: "Pick Color 3"
        currentColor: root.baseTo
        onAccepted: root.baseTo = color
    }
}
