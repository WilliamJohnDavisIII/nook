import QtQuick

// All, SNES, PS1 and so on. A soft row of words, with a line under the one
// you're on, and a count so an empty shelf explains itself.
Row {
    id: tabs
    property real unit: 1
    property bool reducedMotion: false
    signal chosen(string id)

    spacing: 34 * unit

    Repeater {
        model: library.shelves
        delegate: Item {
            id: tab
            required property var modelData
            readonly property bool active: library.shelf === modelData.id
            width: labelRow.implicitWidth
            height: 44 * tabs.unit

            Row {
                id: labelRow
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7 * tabs.unit

                Text {
                    text: tab.modelData.label
                    color: theme.foreground
                    opacity: tab.active ? 1 : tab.modelData.count > 0 ? 0.62 : 0.34
                    font.pixelSize: Math.round(18 * tabs.unit)
                    font.weight: tab.active ? Font.Medium : Font.Normal
                    Behavior on opacity { enabled: !tabs.reducedMotion; NumberAnimation { duration: 180 } }
                }
                Text {
                    anchors.baseline: parent.children[0].baseline
                    text: tab.modelData.count
                    color: theme.foreground
                    opacity: tab.active ? 0.6 : 0.3
                    font.pixelSize: Math.round(12 * tabs.unit)
                    font.family: "JetBrainsMono Nerd Font"
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                height: Math.max(2, 2 * tabs.unit)
                width: tab.active ? parent.width : 0
                color: theme.foreground
                Behavior on width { enabled: !tabs.reducedMotion; NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: tabs.chosen(tab.modelData.id)
            }
        }
    }
}
