import QtQuick

// A drawn cover for a game with no box art yet: the rough shape of what you'd
// have held for that system, in a colour that belongs to the title.
Item {
    id: cover
    property string title: ""
    property string system: "snes"
    property int hue: 200
    property real unit: 1

    readonly property real aspect: system === "snes" ? 1.36 : system === "ps1" ? 1.0 : 0.72
    readonly property real paintedWidth: Math.min(width, height * aspect)
    readonly property real paintedHeight: paintedWidth / aspect
    readonly property color body: Qt.hsla(hue / 360, 0.26, theme.dark ? 0.27 : 0.42, 1)
    readonly property color edge: Qt.darker(body, 1.28)
    readonly property color paper: Qt.hsla(hue / 360, 0.3, 0.9, 1)

    Rectangle {
        id: box
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: cover.paintedWidth
        height: cover.paintedHeight
        radius: Math.min(6, theme.radius)
        color: cover.body

        // Cartridge: ridges along the top, and a paper label.
        Column {
            visible: cover.system === "snes"
            anchors.top: parent.top
            anchors.topMargin: parent.height * 0.05
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: parent.height * 0.022
            Repeater {
                model: 3
                delegate: Rectangle { width: box.width * 0.84; height: box.height * 0.018; color: cover.edge; opacity: 0.8 }
            }
        }

        // Jewel case: the hinge down the left side.
        Rectangle {
            visible: cover.system === "ps1"
            width: parent.width * 0.075
            height: parent.height
            color: cover.edge
        }

        // Tall case: a darker band across the top.
        Rectangle {
            visible: cover.system === "ps2"
            width: parent.width
            height: parent.height * 0.1
            color: cover.edge
        }

        // Small disc: the ring you'd see through the case.
        Rectangle {
            visible: cover.system === "gc"
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.1
            width: parent.width * 0.62
            height: width
            radius: width / 2
            color: "transparent"
            border.color: cover.edge
            border.width: parent.width * 0.05
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.22
                height: width
                radius: width / 2
                color: cover.edge
            }
        }

        Rectangle {
            id: label
            readonly property real side: box.width * 0.07
            x: cover.system === "ps1" ? box.width * 0.13 : side
            width: box.width - x - side
            anchors.bottom: parent.bottom
            anchors.bottomMargin: side
            height: cover.system === "snes" ? box.height * 0.6 : box.height * 0.34
            color: cover.paper
            radius: Math.min(3, theme.radius)

            Text {
                anchors.fill: parent
                anchors.margins: parent.width * 0.07
                text: cover.title
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: 4
                verticalAlignment: Text.AlignBottom
                color: Qt.hsla(cover.hue / 360, 0.34, 0.2, 1)
                font.family: "Noto Serif"
                font.pixelSize: Math.max(10, Math.round(box.width * 0.078))
            }
        }
    }
}
