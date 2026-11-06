import QtQuick

// One line of the hint bar. Shows which button by its position rather than its
// letter, because this pad's letters don't match what the emulators call them.
Row {
    id: hint
    property string position: "south"
    property string keyLabel: ""
    property string label: ""
    property real unit: 1

    spacing: 9 * unit

    Item {
        id: glyph
        anchors.verticalCenter: parent.verticalCenter
        width: gamepad.connected ? 22 * hint.unit : keyText.implicitWidth + 14 * hint.unit
        height: 22 * hint.unit

        Repeater {
            model: gamepad.connected && hint.position !== "shoulder" ? ["north", "east", "south", "west"] : []
            delegate: Rectangle {
                required property string modelData
                readonly property real dot: 7 * hint.unit
                width: dot
                height: dot
                radius: dot / 2
                x: modelData === "west" ? 0 : modelData === "east" ? glyph.width - dot : (glyph.width - dot) / 2
                y: modelData === "north" ? 0 : modelData === "south" ? glyph.height - dot : (glyph.height - dot) / 2
                color: modelData === hint.position ? theme.foreground : "transparent"
                border.color: theme.muted
                border.width: Math.max(1, hint.unit)
            }
        }

        Text {
            anchors.centerIn: parent
            visible: gamepad.connected && hint.position === "shoulder"
            text: "LB RB"
            color: theme.foreground
            font.pixelSize: Math.round(11 * hint.unit)
            font.family: "JetBrainsMono Nerd Font"
        }

        Rectangle {
            anchors.fill: parent
            visible: !gamepad.connected
            color: "transparent"
            border.color: theme.muted
            border.width: Math.max(1, hint.unit)
            radius: Math.min(4, theme.radius)

            Text {
                id: keyText
                anchors.centerIn: parent
                text: hint.keyLabel
                color: theme.foreground
                font.pixelSize: Math.round(12 * hint.unit)
                font.family: "JetBrainsMono Nerd Font"
            }
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: hint.label
        color: theme.foreground
        opacity: 0.72
        font.pixelSize: Math.round(15 * hint.unit)
    }
}
