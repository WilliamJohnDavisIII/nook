import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtMultimedia

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 520
    title: "Nook"
    color: theme.background

    // Hidden while a game runs, so the emulator gets the whole machine.
    visibility: launcher.running ? Window.Hidden : startWindowed ? Window.Windowed : Window.FullScreen

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    readonly property real u: Math.max(0.5, Math.min(width / 1600, height / 1000))
    readonly property bool still: preferences.reducedMotion
    readonly property string serif: "Noto Serif"
    readonly property string mono: "JetBrainsMono Nerd Font"
    readonly property string confirmButton: gamepad.nintendoLayout ? "east" : "south"
    readonly property string backButton: gamepad.nintendoLayout ? "south" : "east"

    property string overlay: ""
    property string selectedPath: ""
    property bool launching: false
    // Choosing a disc: the case is open, and the left and right move between discs.
    property bool picking: false
    property int pickedDisc: 0
    property string toast: ""
    property string now: Qt.formatTime(new Date(), Qt.locale().timeFormat(Locale.ShortFormat))

    // Nothing shows until the window has reached its final size. The compositor
    // can hand us other sizes first, and everything here scales with the window,
    // so showing those would make the whole drawer jump.
    property bool settled: false
    readonly property bool atFinalSize: startWindowed || (width === Screen.width && height === Screen.height)
    onAtFinalSizeChanged: if (atFinalSize) settle.restart()
    Timer {
        id: settle
        interval: 40
        running: win.atFinalSize
        onTriggered: win.settled = true
    }
    // If the compositor never gives us the whole screen, still show up.
    Timer { interval: 1500; running: true; onTriggered: win.settled = true }

    readonly property var current: shelf.currentItem

    function playedText(ms) {
        if (!ms)
            return qsTr("Not played yet")
        const hours = Math.floor((Date.now() - ms) / 3600000)
        if (hours < 1)
            return qsTr("Played just now")
        if (hours < 24)
            return qsTr("Played %n hours ago", "", hours)
        return qsTr("Played %n days ago", "", Math.floor(hours / 24))
    }

    function move(delta) {
        if (library.count === 0)
            return
        const next = (shelf.currentIndex + delta + library.count) % library.count
        // Going off one end, the shelf drifts away and the other end drifts in.
        // Scrolling there would rush past every game in between.
        if (Math.abs(next - shelf.currentIndex) > 1 && !still) {
            wrapTarget = next
            wrapDirection = delta
            shelfSound.play()
            wrapAround.restart()
            return
        }
        shelf.currentIndex = next
        selectedPath = library.gameAt(shelf.currentIndex).path
        if (library.count > 1)
            playMove()
    }

    property int wrapTarget: 0
    property int wrapDirection: 1
    // The shelf is on its way out, and what's on it is about to change.
    property bool wrapLeaving: false
    // Jumping there, not travelling.
    property bool wrapJumping: false

    SequentialAnimation {
        id: wrapAround
        PropertyAction { target: win; property: "wrapLeaving"; value: true }
        ParallelAnimation {
            NumberAnimation { target: shelf; property: "opacity"; to: 0; duration: 170; easing.type: Easing.InCubic }
            NumberAnimation { target: shelfDrift; property: "x"; to: -win.wrapDirection * 70 * win.u; duration: 170; easing.type: Easing.InCubic }
        }
        ScriptAction {
            script: {
                if (win.wrapTarget < library.count) {
                    win.wrapJumping = true
                    shelf.currentIndex = win.wrapTarget
                    shelf.positionViewAtIndex(win.wrapTarget, ListView.Center)
                    win.wrapJumping = false
                    win.selectedPath = library.gameAt(win.wrapTarget).path
                }
                shelfDrift.x = win.wrapDirection * 70 * win.u
                win.wrapLeaving = false
            }
        }
        ParallelAnimation {
            NumberAnimation { target: shelf; property: "opacity"; to: 1; duration: 340; easing.type: Easing.OutCubic }
            NumberAnimation { target: shelfDrift; property: "x"; to: 0; duration: 340; easing.type: Easing.OutCubic }
        }
    }

    function openCase() {
        if (library.count === 0)
            return
        const game = library.gameAt(shelf.currentIndex)
        if (game.discs < 2)
            return
        pickedDisc = game.lastDisc
        picking = true
        shelfSound.play()
    }

    function restoreSelection() {
        if (library.count === 0)
            return
        const found = library.indexOfPath(selectedPath)
        shelf.currentIndex = found >= 0 ? found : 0
        selectedPath = library.gameAt(shelf.currentIndex).path
        shelf.positionViewAtIndex(shelf.currentIndex, ListView.Center)
    }

    function play() {
        if (library.count === 0 || launching)
            return
        launching = true
        launchDelay.restart()
    }

    function say(text) {
        toast = text
        toastTimer.restart()
    }

    function toggleMusic() {
        if (preferences.musicUrl.toString() === "") {
            say(qsTr("Add a music file to %1 to hear it here").arg(preferences.musicFolder))
            return
        }
        preferences.musicEnabled = !preferences.musicEnabled
        say(preferences.musicEnabled ? qsTr("Music on") : qsTr("Music off"))
    }

    function act(name) {
        if (launcher.running || launching || wrapLeaving)
            return
        if (overlay === "quit") {
            if (name === "play")
                Qt.quit()
            else if (name === "back")
                overlay = ""
            return
        }
        if (overlay !== "") {
            if (name === "play" || name === "back" || name === "help") {
                overlay = ""
                launcher.clearError()
            }
            return
        }
        if (picking) {
            const discs = library.gameAt(shelf.currentIndex).discs
            switch (name) {
            case "left": pickedDisc = (pickedDisc - 1 + discs) % discs; playMove(); break
            case "right": pickedDisc = (pickedDisc + 1) % discs; playMove(); break
            case "play": play(); break
            case "back":
            case "discs": picking = false; shelfSound.play(); break
            case "music": toggleMusic(); break
            }
            return
        }
        switch (name) {
        case "discs": openCase(); break
        case "left": move(-1); break
        case "right": move(1); break
        case "shelfPrevious": library.stepShelf(-1); shelfSound.play(); break
        case "shelfNext": library.stepShelf(1); shelfSound.play(); break
        case "play": play(); break
        case "back": overlay = "quit"; break
        case "music": toggleMusic(); break
        case "help": overlay = "help"; break
        }
    }

    Timer {
        id: launchDelay
        // Long enough for the drawer to fade, and the music with it.
        interval: win.still ? 0 : 480
        onTriggered: {
            const game = library.gameAt(shelf.currentIndex)
            const disc = win.picking ? win.pickedDisc : -1
            const file = library.launchPathFor(game.path, disc)
            if (disc >= 0 && game.system === "ps1")
                launcher.startOnDisc(file, disc, game.discFiles[disc])
            if (launcher.launch(game.system, file))
                library.markPlayed(game.path, disc)
            else
                win.launching = false
            win.picking = false
        }
    }

    Timer { id: toastTimer; interval: 3200; onTriggered: win.toast = "" }

    Timer {
        interval: 20000
        running: !launcher.running
        repeat: true
        triggeredOnStart: true
        onTriggered: win.now = Qt.formatTime(new Date(), Qt.locale().timeFormat(Locale.ShortFormat))
    }

    Connections {
        target: library
        function onModelReset() {
            win.picking = false
            wrapAround.stop()
            win.wrapLeaving = false
            shelf.opacity = 1
            shelfDrift.x = 0
            Qt.callLater(win.restoreSelection)
        }
    }

    Connections {
        target: launcher
        function onRunningChanged() {
            if (!launcher.running) {
                win.launching = false
                win.requestActivate()
                Qt.callLater(win.restoreSelection)
            }
        }
        function onLastErrorChanged() {
            if (launcher.lastError !== "")
                win.overlay = "error"
        }
    }

    Connections {
        target: gamepad
        function onAction(name) {
            if (name === "exitGame")
                launcher.stop()
            else
                win.act(name)
        }
    }

    Component.onCompleted: Qt.callLater(restoreSelection)

    Shortcut { sequences: ["Left", "H"]; context: Qt.ApplicationShortcut; onActivated: win.act("left") }
    Shortcut { sequences: ["Right", "L"]; context: Qt.ApplicationShortcut; onActivated: win.act("right") }
    Shortcut { sequences: ["Up", "Shift+Tab"]; context: Qt.ApplicationShortcut; onActivated: win.act("shelfPrevious") }
    Shortcut { sequences: ["Down", "Tab"]; context: Qt.ApplicationShortcut; onActivated: win.act("shelfNext") }
    Shortcut { sequences: ["Return", "Enter", "Space"]; context: Qt.ApplicationShortcut; onActivated: win.act("play") }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: win.act("back") }
    Shortcut { sequence: "D"; context: Qt.ApplicationShortcut; onActivated: win.act("discs") }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: win.act("music") }
    Shortcut { sequences: ["?", "Shift+/", "F1"]; context: Qt.ApplicationShortcut; onActivated: win.act("help") }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }

    // A soft ring between games, and a deeper one between shelves. They ring for
    // longer than it takes to scroll, so each new one takes the next voice rather
    // than cutting the last one off.
    property int nextVoice: 0
    function playMove() {
        moveVoices[nextVoice].play()
        nextVoice = (nextVoice + 1) % moveVoices.length
    }
    readonly property var moveVoices: [moveOne, moveTwo, moveThree, moveFour]
    SoundEffect { id: moveOne; source: "qrc:/sounds/move.wav" }
    SoundEffect { id: moveTwo; source: "qrc:/sounds/move.wav" }
    SoundEffect { id: moveThree; source: "qrc:/sounds/move.wav" }
    SoundEffect { id: moveFour; source: "qrc:/sounds/move.wav" }
    SoundEffect { id: shelfSound; source: "qrc:/sounds/shelf.wav" }

    // Off by default. Fades out rather than cutting when a game opens.
    MediaPlayer {
        id: music
        source: preferences.musicUrl
        loops: MediaPlayer.Infinite
        audioOutput: AudioOutput {
            id: speakers
            volume: music.wanted ? 0.42 : 0
            Behavior on volume { NumberAnimation { duration: 450 } }
        }
        readonly property bool wanted: preferences.musicEnabled && !launcher.running && !win.launching
        onWantedChanged: {
            if (wanted) {
                musicPause.stop()
                play()
            } else {
                musicPause.restart()
            }
        }
        Component.onCompleted: if (wanted) play()
    }
    Timer { id: musicPause; interval: 500; onTriggered: music.pause() }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        opacity: win.settled ? 1 : 0
        Behavior on opacity { enabled: !win.still; NumberAnimation { duration: 260; easing.type: Easing.OutQuad } }
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Backdrop {
            anchors.fill: parent
            art: win.current ? win.current.art : ""
            hue: win.current ? win.current.hue : 220
            hasGame: library.count > 0
            reducedMotion: win.still
            paused: launcher.running
            unit: win.u
        }

        // The drawer itself, which fades as a game opens.
        Item {
            id: drawer
            anchors.fill: parent
            opacity: win.launching ? 0 : 1
            Behavior on opacity { enabled: !win.still; NumberAnimation { duration: 420; easing.type: Easing.InOutQuad } }

            Text {
                id: wordmark
                x: 64 * win.u
                y: 42 * win.u
                text: "nook"
                color: theme.foreground
                font.family: win.serif
                font.italic: true
                font.pixelSize: Math.round(34 * win.u)
            }

            ShelfTabs {
                anchors.left: wordmark.right
                anchors.leftMargin: 58 * win.u
                anchors.verticalCenter: wordmark.verticalCenter
                unit: win.u
                reducedMotion: win.still
                onChosen: id => library.shelf = id
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 64 * win.u
                anchors.verticalCenter: wordmark.verticalCenter
                spacing: 24 * win.u

                Text {
                    visible: preferences.musicEnabled
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Music")
                    color: theme.foreground
                    opacity: 0.6
                    font.pixelSize: Math.round(15 * win.u)
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8 * win.u
                    height: width
                    radius: width / 2
                    color: gamepad.connected ? theme.foreground : "transparent"
                    border.color: theme.muted
                    border.width: Math.max(1, win.u)
                    opacity: 0.7
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: gamepad.connected ? gamepad.name : qsTr("No controller")
                    color: theme.foreground
                    opacity: 0.6
                    font.pixelSize: Math.round(15 * win.u)
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: win.now
                    color: theme.foreground
                    opacity: 0.8
                    font.family: win.mono
                    font.pixelSize: Math.round(17 * win.u)
                }
            }

            // The shelf. Games stand on the line below them.
            ListView {
                id: shelf
                readonly property real cell: 400 * win.u
                y: 128 * win.u
                width: parent.width
                height: 560 * win.u
                orientation: ListView.Horizontal
                model: library
                visible: library.count > 0
                clip: false
                cacheBuffer: cell * 3
                reuseItems: true
                highlightRangeMode: ListView.StrictlyEnforceRange
                preferredHighlightBegin: (width - cell) / 2
                preferredHighlightEnd: (width + cell) / 2
                highlightMoveDuration: win.still || win.wrapJumping ? 0 : 340
                transform: Translate { id: shelfDrift }
                highlightFollowsCurrentItem: true
                boundsBehavior: Flickable.StopAtBounds
                onMovementEnded: win.selectedPath = library.gameAt(currentIndex).path

                delegate: GameCard {
                    width: shelf.cell
                    height: shelf.height
                    unit: win.u
                    reducedMotion: win.still
                    pickedDisc: win.picking && index === shelf.currentIndex ? win.pickedDisc : -1
                    receded: win.picking && index !== shelf.currentIndex
                    onActivated: win.play()
                    onPicked: {
                        win.picking = false
                        shelf.currentIndex = index
                        win.selectedPath = path
                    }
                }
            }

            Rectangle {
                id: shelfLine
                visible: library.count > 0
                y: shelf.y + shelf.height + 1
                x: 64 * win.u
                width: parent.width - 128 * win.u
                height: Math.max(1, win.u)
                color: theme.foreground
                opacity: 0.2
            }

            // What you're on.
            Column {
                visible: library.count > 0
                x: 64 * win.u
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 56 * win.u
                width: parent.width * 0.58
                spacing: 8 * win.u

                Text {
                    width: parent.width
                    text: win.current ? win.current.title : ""
                    color: theme.foreground
                    elide: Text.ElideRight
                    font.family: win.serif
                    font.pixelSize: Math.round(50 * win.u)
                }
                Text {
                    text: !win.current ? ""
                          : win.picking ? qsTr("Disc %1 of %2").arg(win.pickedDisc + 1).arg(win.current.discs)
                          : [win.current.systemLabel].concat(win.current.discs > 1 ? [qsTr("%n discs", "", win.current.discs)] : [],
                                                             [win.playedText(win.current.lastPlayed)]).join("  ·  ")
                    color: theme.foreground
                    opacity: 0.62
                    font.pixelSize: Math.round(18 * win.u)
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 64 * win.u
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 66 * win.u
                spacing: 30 * win.u

                PadHint {
                    visible: library.count > 0
                    position: win.confirmButton
                    keyLabel: "Enter"
                    label: win.picking ? qsTr("Play disc %1").arg(win.pickedDisc + 1) : qsTr("Play")
                    unit: win.u
                }
                PadHint {
                    visible: !win.picking && !!win.current && win.current.discs > 1
                    position: "west"
                    keyLabel: "D"
                    label: qsTr("Discs")
                    unit: win.u
                }
                PadHint { visible: !win.picking; position: "shoulder"; keyLabel: "Tab"; label: qsTr("Shelf"); unit: win.u }
                PadHint { visible: !win.picking; position: "north"; keyLabel: "M"; label: qsTr("Music"); unit: win.u }
                PadHint { position: win.backButton; keyLabel: "Esc"; label: win.picking ? qsTr("Back") : qsTr("Close"); unit: win.u }
            }

            // Nothing on this shelf yet.
            Column {
                visible: library.count === 0
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -24 * win.u
                spacing: 18 * win.u
                width: Math.min(parent.width * 0.86, 1260 * win.u)

                ConsoleShelf {
                    anchors.horizontalCenter: parent.horizontalCenter
                    only: library.shelf
                    unit: win.u
                    reducedMotion: win.still
                    onChosen: id => library.shelf = id
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: library.shelf === "" ? qsTr("The drawer is empty")
                                               : qsTr("Nothing on this shelf yet")
                    color: theme.foreground
                    font.family: win.serif
                    font.pixelSize: Math.round(44 * win.u)
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: library.shelf === ""
                          ? qsTr("Put your games in %1, in the snes, ps1, gc and ps2 folders. They'll appear here on their own.").arg(library.romsPath)
                          : qsTr("Put games in %1 and they'll appear here on their own.").arg(library.romsPath + "/" + library.shelf)
                    color: theme.foreground
                    opacity: 0.66
                    font.pixelSize: Math.round(19 * win.u)
                    lineHeight: 1.3
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18 * win.u
                text: win.toast
                visible: win.toast !== ""
                color: theme.foreground
                opacity: 0.8
                font.pixelSize: Math.round(16 * win.u)
            }
        }

        NoteOverlay {
            anchors.fill: parent
            unit: win.u
            reducedMotion: win.still
            shown: win.overlay === "help"

            Column {
                spacing: 12 * win.u
                Text {
                    text: qsTr("Finding your way around")
                    color: theme.foreground
                    font.family: win.serif
                    font.pixelSize: Math.round(30 * win.u)
                    bottomPadding: 10 * win.u
                }
                Repeater {
                    model: [
                        [qsTr("Choose a game"), qsTr("D-pad or stick"), "← →"],
                        [qsTr("Play"), gamepad.nintendoLayout ? qsTr("A button") : qsTr("Bottom button"), "Enter"],
                        [qsTr("Choose a disc"), qsTr("Left button"), "D"],
                        [qsTr("Change shelf"), qsTr("Shoulders or up/down"), "Tab"],
                        [qsTr("Music"), qsTr("Top button"), "M"],
                        [qsTr("This help"), qsTr("Start"), "F1"],
                        [qsTr("Close the drawer"), gamepad.nintendoLayout ? qsTr("B button") : qsTr("Right button"), "Q"],
                        [qsTr("Leave a game"), qsTr("Hold Select + Start"), ""],
                        [qsTr("Game menu, in RetroArch"), qsTr("Home button"), ""]
                    ]
                    delegate: Row {
                        required property var modelData
                        spacing: 28 * win.u
                        Text { width: 270 * win.u; text: modelData[0]; color: theme.foreground; font.pixelSize: Math.round(18 * win.u) }
                        Text { width: 240 * win.u; text: modelData[1]; color: theme.foreground; opacity: 0.66; font.pixelSize: Math.round(18 * win.u) }
                        Text { width: 80 * win.u; text: modelData[2]; color: theme.foreground; opacity: 0.66; font.family: win.mono; font.pixelSize: Math.round(16 * win.u) }
                    }
                }
            }
        }

        NoteOverlay {
            anchors.fill: parent
            unit: win.u
            reducedMotion: win.still
            shown: win.overlay === "quit"

            Column {
                spacing: 22 * win.u
                Text {
                    text: qsTr("Close the drawer?")
                    color: theme.foreground
                    font.family: win.serif
                    font.pixelSize: Math.round(30 * win.u)
                }
                Row {
                    spacing: 30 * win.u
                    PadHint { position: win.confirmButton; keyLabel: "Enter"; label: qsTr("Close"); unit: win.u }
                    PadHint { position: win.backButton; keyLabel: "Esc"; label: qsTr("Stay"); unit: win.u }
                }
            }
        }

        NoteOverlay {
            anchors.fill: parent
            unit: win.u
            reducedMotion: win.still
            shown: win.overlay === "error"

            Column {
                spacing: 18 * win.u
                Text {
                    text: qsTr("That game didn't start")
                    color: theme.foreground
                    font.family: win.serif
                    font.pixelSize: Math.round(30 * win.u)
                }
                Text {
                    width: Math.min(760 * win.u, win.width * 0.65)
                    text: launcher.lastError
                    wrapMode: Text.Wrap
                    color: theme.foreground
                    opacity: 0.78
                    font.pixelSize: Math.round(18 * win.u)
                    lineHeight: 1.3
                }
                PadHint { position: win.confirmButton; keyLabel: "Enter"; label: qsTr("Okay"); unit: win.u }
            }
        }
    }
}
