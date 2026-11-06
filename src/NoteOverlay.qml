import QtQuick

// A quiet card over a dimmed drawer, for help, errors and the goodbye question.
Item {
    id: note
    property real unit: 1
    property bool reducedMotion: false
    property bool shown: false
    default property alias content: holder.data

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { enabled: !note.reducedMotion; NumberAnimation { duration: 180 } }

    Rectangle {
        anchors.fill: parent
        color: Qt.alpha(theme.background, 0.74)
    }

    // Swallows clicks so the shelf behind can't be touched.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, holder.childrenRect.width + 96 * note.unit)
        height: holder.childrenRect.height + 84 * note.unit
        color: theme.surface
        radius: theme.radius
        border.color: Qt.alpha(theme.foreground, 0.14)
        border.width: 1

        Item {
            id: holder
            anchors.fill: parent
            anchors.leftMargin: 48 * note.unit
            anchors.rightMargin: 48 * note.unit
            anchors.topMargin: 42 * note.unit
            anchors.bottomMargin: 42 * note.unit
        }
    }
}
