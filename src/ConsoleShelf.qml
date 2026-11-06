import QtQuick
import QtQuick.Effects

// The empty drawer: the consoles themselves, standing on the shelf. With all
// four showing they're links to their shelves, so reaching for one lifts it off
// the line as its shadow softens. Alone on its own shelf, a console is only a
// picture: it's already where it would lead, so it doesn't respond at all.
Item {
    id: lineup
    property real unit: 1
    property bool reducedMotion: false
    // One system's id to show it alone, or empty for all four.
    property string only: ""
    signal chosen(string id)

    readonly property bool linked: only === ""

    // Each picture's width on screen, and its drawing's own proportions. The
    // drawings end exactly at the console's base, so it meets the line.
    readonly property var everyone: [
        { id: "snes", label: "SNES", wide: 300, ratio: 262 / 480 },
        { id: "ps1", label: "PS1", wide: 300, ratio: 262 / 480 },
        { id: "gc", label: "GameCube", wide: 215, ratio: 312 / 360 },
        { id: "ps2", label: "PS2", wide: 150, ratio: 396 / 260 }
    ]
    readonly property var shown: only === "" ? everyone : everyone.filter(c => c.id === only)
    readonly property real zoom: only === "" ? 1 : 1.45
    readonly property real standHeight: 250 * unit * zoom

    implicitHeight: standHeight + 54 * unit
    implicitWidth: row.implicitWidth

    Row {
        id: row
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 42 * lineup.unit

        Repeater {
            model: lineup.shown
            delegate: Item {
                id: stand
                required property var modelData
                width: modelData.wide * lineup.unit * lineup.zoom
                height: lineup.implicitHeight

                readonly property real pictureHeight: width * modelData.ratio
                readonly property bool hovered: lineup.linked && mouse.containsMouse

                // Everything about reaching for a console hangs off this one
                // number, so the lift, the shadow and the label move as one.
                property real reach: hovered ? 1 : 0
                Behavior on reach {
                    enabled: !lineup.reducedMotion
                    NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: lineup.standHeight - height / 2
                    width: parent.width * (0.96 - stand.reach * 0.14)
                    height: 14 * lineup.unit
                    radius: height / 2
                    color: "black"
                    // Lighter and tighter the further the console is off the shelf.
                    opacity: (theme.dark ? 0.55 : 0.24) * (1 - stand.reach * 0.45)
                    layer.enabled: true
                    layer.effect: MultiEffect { blurEnabled: true; blur: 0.7; blurMax: 20 }
                }

                Image {
                    source: "qrc:/consoles/" + stand.modelData.id + ".svg"
                    width: parent.width
                    height: stand.pictureHeight
                    y: lineup.standHeight - height - stand.reach * 14 * lineup.unit
                    sourceSize: Qt.size(Math.ceil(width * 2), Math.ceil(height * 2))
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    scale: 1 + stand.reach * 0.03
                    transformOrigin: Item.Bottom
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: lineup.standHeight + 20 * lineup.unit
                    text: stand.modelData.label
                    color: theme.foreground
                    opacity: 0.68 + stand.reach * 0.32
                    font.pixelSize: Math.round(17 * lineup.unit)
                }

                // Only the console and its name, and it stays still while the
                // picture lifts, so the hover can't flicker at the edge.
                MouseArea {
                    id: mouse
                    visible: lineup.linked
                    y: lineup.standHeight - stand.pictureHeight
                    width: parent.width
                    height: parent.height - y
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: lineup.chosen(stand.modelData.id)
                }
            }
        }
    }

    // The same line the games stand on, so an empty drawer is still the drawer.
    Rectangle {
        y: lineup.standHeight
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(row.width + 90 * lineup.unit, 500 * lineup.unit)
        height: Math.max(1, lineup.unit)
        color: theme.foreground
        opacity: 0.22
    }
}
