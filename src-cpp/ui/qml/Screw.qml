import QtQuick 2.15

Item {
    id: root
    width: 9
    height: 9

    Canvas {
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var g = ctx.createRadialGradient(width * 0.34, height * 0.28, 0.4,
                                             width * 0.5, height * 0.5, width * 0.52)
            g.addColorStop(0, "#8a918c")
            g.addColorStop(0.35, "#4a504c")
            g.addColorStop(1, "#121514")
            ctx.fillStyle = g
            ctx.beginPath()
            ctx.arc(width / 2, height / 2, width * 0.46, 0, Math.PI * 2)
            ctx.fill()
            ctx.strokeStyle = "#090b0a"
            ctx.lineWidth = 1
            ctx.stroke()
            ctx.save()
            ctx.translate(width / 2, height / 2)
            ctx.rotate(-Math.PI / 4)
            ctx.strokeStyle = "#161918"
            ctx.lineWidth = 1.2
            ctx.beginPath()
            ctx.moveTo(-width * 0.25, 0)
            ctx.lineTo(width * 0.25, 0)
            ctx.stroke()
            ctx.restore()
        }
    }
}
