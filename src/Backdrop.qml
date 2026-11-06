import QtQuick
import QtQuick.Effects

// The room behind the shelf: the current game's art, blurred until it's only a
// mood, with two slow drifts of colour and the theme's own background over it.
Item {
    id: room
    property string art: ""
    property int hue: 220
    property bool hasGame: false
    property bool reducedMotion: false
    property bool paused: false
    property real unit: 1

    property string shown: ""
    property bool useFirst: true

    onArtChanged: {
        if (useFirst)
            second.source = art === "" ? "" : "file://" + art
        else
            first.source = art === "" ? "" : "file://" + art
        useFirst = !useFirst
    }

    Rectangle { anchors.fill: parent; color: theme.background }

    Item {
        id: mood
        anchors.fill: parent
        layer.enabled: true
        // Small on purpose: blurring a tiny picture is cheap, and softer for it.
        layer.textureSize: Qt.size(Math.max(1, width / 14), Math.max(1, height / 14))
        layer.smooth: true
        layer.effect: MultiEffect {
            autoPaddingEnabled: false
            blurEnabled: true
            blur: 1.0
            blurMax: 64
            // Past recognising anything: only the colours should survive.
            blurMultiplier: 3
        }

        Rectangle {
            anchors.fill: parent
            color: room.hasGame ? Qt.hsla(room.hue / 360, 0.3, theme.dark ? 0.2 : 0.8, 1) : theme.surface
            Behavior on color { enabled: !room.reducedMotion; ColorAnimation { duration: 900 } }
        }

        Image {
            id: first
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(20, 20)
            asynchronous: true
            opacity: room.useFirst && status === Image.Ready ? 1 : 0
            Behavior on opacity { enabled: !room.reducedMotion; NumberAnimation { duration: 900 } }
        }
        Image {
            id: second
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(20, 20)
            asynchronous: true
            opacity: !room.useFirst && status === Image.Ready ? 1 : 0
            Behavior on opacity { enabled: !room.reducedMotion; NumberAnimation { duration: 900 } }
        }

        Repeater {
            model: 2
            delegate: Rectangle {
                id: drift
                required property int index
                readonly property real size: room.width * (index === 0 ? 0.5 : 0.38)
                width: size
                height: size
                radius: size / 2
                color: Qt.hsla(((room.hue + (index === 0 ? 30 : -40)) % 360 + 360) % 360 / 360, 0.34, theme.dark ? 0.34 : 0.72, 1)
                opacity: 0.55
                x: room.width * (index === 0 ? 0.62 : -0.06) + wander * room.width * 0.12
                y: room.height * (index === 0 ? -0.2 : 0.46) + wander * room.height * (index === 0 ? 0.12 : -0.1)
                Behavior on color { enabled: !room.reducedMotion; ColorAnimation { duration: 1200 } }

                property real wander: 0
                SequentialAnimation on wander {
                    running: !room.reducedMotion && !room.paused
                    loops: Animation.Infinite
                    NumberAnimation { to: 1; duration: drift.index === 0 ? 23000 : 31000; easing.type: Easing.InOutSine }
                    NumberAnimation { to: -1; duration: drift.index === 0 ? 29000 : 37000; easing.type: Easing.InOutSine }
                }
            }
        }
    }

    // The theme stays in charge: its background washes over the mood, more
    // strongly at the bottom where the words sit.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.alpha(theme.background, theme.dark ? 0.66 : 0.68) }
            GradientStop { position: 0.62; color: Qt.alpha(theme.background, theme.dark ? 0.52 : 0.56) }
            GradientStop { position: 1.0; color: Qt.alpha(theme.background, theme.dark ? 0.9 : 0.9) }
        }
    }
}
