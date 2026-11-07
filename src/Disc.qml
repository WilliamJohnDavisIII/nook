import QtQuick

// One disc, as you'd have pulled it from the case: black for the PlayStation,
// silver for the others, with the rainbow a pressed surface throws back.
Canvas {
    id: disc
    property string system: "ps1"
    property int hue: 200

    readonly property bool black: system === "ps1"

    onSystemChanged: requestPaint()
    onHueChanged: requestPaint()
    onWidthChanged: requestPaint()

    antialiasing: true

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const c = width / 2
        const outer = c - 1

        // Everything between two circles, leaving what's inside the smaller one alone.
        function ring(from, to, style) {
            ctx.beginPath()
            ctx.fillRule = Qt.OddEvenFill
            ctx.arc(c, c, from, 0, Math.PI * 2, false)
            ctx.closePath()
            ctx.moveTo(c + to, c)
            ctx.arc(c, c, to, 0, Math.PI * 2, false)
            ctx.closePath()
            ctx.fillStyle = style
            ctx.fill()
        }

        const hole = outer * 0.13
        const hub = outer * 0.3
        ring(outer, hole, black ? "#17171d" : "#c9ccd2")

        // The pressed surface: two flares opposite each other, as a disc does
        // under one lamp, running through the spectrum from this game's colour.
        const sheen = ctx.createConicalGradient(c, c, 0)
        const flares = [[0, 0.1], [0.05, 0.3], [0.09, 0.85], [0.12, 1], [0.15, 0.85], [0.19, 0.3], [0.26, 0.1]]
        for (const turn of [0, 0.5])
            for (const [at, strength] of flares) {
                const h = (hue / 360 + at * 3.4) % 1
                sheen.addColorStop(turn + at, Qt.hsla(h, 0.9, black ? 0.66 : 0.7, strength * (black ? 0.72 : 0.68)))
            }
        ring(outer, hub, sheen)

        // The clear inner ring, the hub, and the rim catching light.
        ring(hub, outer * 0.24, black ? "#1d1d24" : "#e6e8ec")
        ring(outer * 0.24, hole, black ? "#2b2b33" : "#f4f5f7")
        ring(outer, outer - Math.max(1, width * 0.006), black ? Qt.rgba(1, 1, 1, 0.4) : Qt.rgba(1, 1, 1, 0.8))
        // Its edge against the next disc back.
        ring(outer - width * 0.006, outer - width * 0.02, Qt.rgba(0, 0, 0, black ? 0.35 : 0.14))
    }
}
