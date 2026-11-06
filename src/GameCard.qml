import QtQuick
import QtQuick.Effects

// One game, as the box it came in: a front and a spine meeting at a corner,
// turned a little away from you, standing on the shelf. The selected game turns
// toward you and sways slightly, but it stays standing: a box on a shelf doesn't
// float. Everything about its look comes from its own artwork.
Item {
    id: card

    required property int index
    required property string title
    required property string system
    required property string systemLabel
    required property string path
    required property string art
    required property var lastPlayed
    required property int hue

    property real unit: 1
    property bool reducedMotion: false
    readonly property bool isCurrent: ListView.isCurrentItem
    readonly property bool hasArt: art !== "" && artImage.status === Image.Ready

    signal activated()
    signal picked()

    // How thick each kind of box is, against its width: a deep cardboard box
    // for SNES, a slim jewel case for PS1, keep cases for the disc systems.
    readonly property real thickness: system === "snes" ? 0.17 : system === "ps1" ? 0.07 : 0.1
    // Cardboard takes the light softly. Plastic shines.
    readonly property real gloss: system === "snes" ? 0.05 : 0.11

    readonly property real aspect: hasArt && artImage.implicitHeight > 0
                                   ? artImage.implicitWidth / artImage.implicitHeight : drawn.aspect
    readonly property real fitHeight: Math.min(height * 0.97, width * 0.82 / aspect)
    readonly property real boxWidth: fitHeight * aspect
    readonly property real boxHeight: fitHeight
    readonly property real depth: boxWidth * thickness

    // Turned away by this many degrees; the selected one faces you more.
    property real turn: isCurrent ? 15 : 27
    Behavior on turn { enabled: !card.reducedMotion; NumberAnimation { duration: 380; easing.type: Easing.OutCubic } }

    property real sway: 0
    SequentialAnimation {
        running: card.isCurrent && !card.reducedMotion && !launcher.running
        loops: Animation.Infinite
        NumberAnimation { target: card; property: "sway"; to: -2.2; duration: 3600; easing.type: Easing.InOutSine }
        NumberAnimation { target: card; property: "sway"; to: 2.2; duration: 7200; easing.type: Easing.InOutSine }
        NumberAnimation { target: card; property: "sway"; to: 0; duration: 3600; easing.type: Easing.InOutSine }
        onStopped: card.sway = 0
    }

    // How far away the eye is. Qt's own 3D rotation fixes this at a distance
    // that makes a box this size look like a toy, so the projection is built here.
    readonly property real eye: 2200 * unit
    readonly property real radians: (turn + sway) * Math.PI / 180
    readonly property real spineShown: depth * Math.sin(radians)
    readonly property real frontShown: boxWidth * Math.cos(radians) / (1 + boxWidth * Math.sin(radians) / eye)

    // Turns a flat face about the vertical line at originX, keeping the bottom
    // edge at originY level so the box stays standing on the shelf.
    function turned(angle, originX, originY) {
        const cosine = Math.cos(angle), sine = Math.sin(angle)
        const toOrigin = Qt.matrix4x4(1, 0, 0, -originX, 0, 1, 0, -originY, 0, 0, 1, 0, 0, 0, 0, 1)
        const project = Qt.matrix4x4(cosine, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, sine / eye, 0, 0, 1)
        const back = Qt.matrix4x4(1, 0, 0, originX, 0, 1, 0, originY, 0, 0, 1, 0, 0, 0, 0, 1)
        return back.times(project).times(toOrigin)
    }

    Item {
        id: lift
        width: parent.width
        height: parent.height
        scale: card.isCurrent ? 1.0 : 0.76
        opacity: card.isCurrent ? 1.0 : 0.74
        transformOrigin: Item.Bottom

        Behavior on scale { enabled: !card.reducedMotion; NumberAnimation { duration: 340; easing.type: Easing.OutCubic } }
        Behavior on opacity { enabled: !card.reducedMotion; NumberAnimation { duration: 280 } }

        // Where it touches the shelf: darkest right under the box, fading to
        // nothing, so it never reads as a ledge.
        Rectangle {
            readonly property real inset: 6 * card.unit
            x: (card.width - card.frontShown - card.spineShown) / 2 + inset
            y: lift.height - height / 2
            width: card.frontShown + card.spineShown - inset * 2
            height: (card.isCurrent ? 26 : 14) * card.unit
            radius: height / 2
            opacity: theme.dark ? 0.7 : 0.4
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0) }
                GradientStop { position: 0.5; color: Qt.rgba(0, 0, 0, 0.8) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0) }
            }
            layer.enabled: true
            layer.effect: MultiEffect { blurEnabled: true; blur: 0.7; blurMax: 20; autoPaddingEnabled: true }
        }

        // The box. Its corner, where spine meets front, is what's nearest you.
        Item {
            id: box
            // Always the full cell, whatever the box's size. Art arrives after
            // the card is made and changes the box's shape; a layer that resized
            // with it would keep drawing at the old size and lift the box off
            // the shelf.
            anchors.fill: parent
            readonly property real cornerX: (card.width - card.frontShown + card.spineShown) / 2
            readonly property real topY: card.height - card.boxHeight

            property real shadowStrength: card.isCurrent ? 1 : 0
            Behavior on shadowStrength { enabled: !card.reducedMotion; NumberAnimation { duration: 300 } }

            layer.enabled: true
            layer.samples: 2
            layer.effect: MultiEffect {
                autoPaddingEnabled: false
                shadowEnabled: box.shadowStrength > 0.01
                shadowBlur: 1.0
                shadowOpacity: (theme.dark ? 0.55 : 0.26) * box.shadowStrength
                shadowHorizontalOffset: 12 * card.unit
                shadowVerticalOffset: 6 * card.unit
                shadowColor: "black"
            }

            Item {
                id: spine
                x: box.cornerX - card.depth
                y: box.topY
                width: card.depth
                height: card.boxHeight
                // Flat first, then turned, so what's on it stays crisp.
                layer.enabled: true
                layer.smooth: true
                transform: Matrix4x4 { matrix: card.turned(card.radians - Math.PI / 2, card.depth, card.boxHeight) }

                Image {
                    // A sliver of the cover's own edge, so the spine always
                    // belongs to the box it's on.
                    anchors.fill: parent
                    visible: card.hasArt
                    source: card.hasArt ? artImage.source : ""
                    asynchronous: true
                    sourceSize.height: 760
                    sourceClipRect: Qt.rect(0, 0, Math.max(4, artImage.implicitWidth * 0.045), artImage.implicitHeight)
                    fillMode: Image.Stretch
                    smooth: true
                }
                Rectangle {
                    anchors.fill: parent
                    visible: !card.hasArt
                    color: drawn.edge
                }
                // Turned from the light, darkening toward the back.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.62) }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.3) }
                    }
                }
            }

            Item {
                id: front
                x: box.cornerX
                y: box.topY
                width: card.boxWidth
                height: card.boxHeight
                layer.enabled: true
                layer.smooth: true
                layer.mipmap: true
                transform: Matrix4x4 { matrix: card.turned(card.radians, 0, card.boxHeight) }

                Image {
                    id: artImage
                    anchors.fill: parent
                    source: card.art === "" ? "" : "file://" + card.art
                    asynchronous: true
                    fillMode: Image.Stretch
                    sourceSize.height: 760
                    mipmap: true
                    smooth: true
                    visible: card.hasArt
                }

                GameCover {
                    id: drawn
                    anchors.fill: parent
                    visible: !card.hasArt
                    title: card.title
                    system: card.system
                    hue: card.hue
                    unit: card.unit
                }

                // Light falling across the front: brighter at the near corner,
                // dimmer as the box turns away.
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.07) }
                        GradientStop { position: 0.45; color: Qt.rgba(0, 0, 0, 0) }
                        GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.16) }
                    }
                }

                // A soft band of reflected light across the case.
                Item {
                    anchors.fill: parent
                    clip: true
                    Rectangle {
                        width: parent.width * 0.5
                        height: parent.height * 1.5
                        x: parent.width * 0.12
                        y: -parent.height * 0.25
                        rotation: 16
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0) }
                            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, card.gloss) }
                            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
                        }
                    }
                }

                // The corner catches the light, and the top edge a little less.
                Rectangle {
                    width: Math.max(1, 1.5 * card.unit)
                    height: parent.height
                    color: Qt.rgba(1, 1, 1, 0.42)
                }
                Rectangle {
                    width: parent.width
                    height: Math.max(1, card.unit)
                    color: Qt.rgba(1, 1, 1, 0.24)
                }
            }
        }
    }

    MouseArea {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: card.frontShown + card.spineShown
        height: card.boxHeight
        cursorShape: Qt.PointingHandCursor
        onClicked: card.isCurrent ? card.activated() : card.picked()
    }
}
