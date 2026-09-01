import QtQuick 2.15

Item {
    id: root
    width: 74
    height: 74

    property real tapeFill: 0.55
    property bool running: false
    property bool reverse: false

    Canvas {
        id: tapeCanvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx = width / 2
            var cy = height / 2
            var outer = Math.max(width * 0.35, width * (0.33 + root.tapeFill * 0.15))

            ctx.fillStyle = "rgba(0,0,0,0.48)"
            ctx.beginPath()
            ctx.ellipse(cx + 1.5, cy + 3, outer + 2, outer + 1, 0, 0, Math.PI * 2)
            ctx.fill()

            var tape = ctx.createRadialGradient(cx - outer * 0.24, cy - outer * 0.28, 1,
                                                cx, cy, outer)
            tape.addColorStop(0, "#b5a47e")
            tape.addColorStop(0.28, "#716047")
            tape.addColorStop(0.58, "#493a29")
            tape.addColorStop(0.82, "#261d15")
            tape.addColorStop(1, "#0e0c09")
            ctx.fillStyle = tape
            ctx.beginPath()
            ctx.arc(cx, cy, outer, 0, Math.PI * 2)
            ctx.fill()

            ctx.strokeStyle = "rgba(219,197,143,0.22)"
            ctx.lineWidth = 0.7
            for (var ring = outer - 2; ring > width * 0.25; ring -= 2.2) {
                ctx.beginPath()
                ctx.arc(cx, cy, ring, 0, Math.PI * 2)
                ctx.stroke()
            }

            ctx.fillStyle = "#080a09"
            ctx.beginPath()
            ctx.arc(cx, cy, width * 0.225, 0, Math.PI * 2)
            ctx.fill()

            ctx.strokeStyle = "rgba(235,224,196,0.34)"
            ctx.lineWidth = 1
            ctx.beginPath()
            ctx.arc(cx, cy, outer - 0.8, 0, Math.PI * 2)
            ctx.stroke()
        }
        Connections {
            target: root
            function onTapeFillChanged() { tapeCanvas.requestPaint() }
        }
    }

    Item {
        id: rotor
        anchors.centerIn: parent
        width: parent.width * 0.56
        height: width

        RotationAnimation on rotation {
            from: root.reverse ? 360 : 0
            to: root.reverse ? 0 : 360
            duration: 1120
            loops: Animation.Infinite
            running: root.running
        }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            border.color: "#c6cbc7"
            border.width: 1.4
            gradient: Gradient {
                GradientStop { position: 0; color: "#8e9792" }
                GradientStop { position: 0.34; color: "#4a514e" }
                GradientStop { position: 1; color: "#181d1b" }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 7
            height: width
            radius: width / 2
            color: "#111614"
            border.color: "#727d78"
            border.width: 1
        }

        Repeater {
            model: 6
            Rectangle {
                anchors.centerIn: parent
                width: 4
                height: rotor.height * 0.79
                radius: 2
                gradient: Gradient {
                    GradientStop { position: 0; color: "#d1d5d2" }
                    GradientStop { position: 1; color: "#6d7772" }
                }
                transform: Rotation {
                    origin.x: 2
                    origin.y: rotor.height * 0.395
                    angle: index * 60
                }
            }
        }

        Repeater {
            model: 6
            Rectangle {
                width: 5
                height: 5
                radius: 2.5
                x: rotor.width / 2 + Math.cos(index * Math.PI / 3) * rotor.width * 0.31 - width / 2
                y: rotor.height / 2 + Math.sin(index * Math.PI / 3) * rotor.height * 0.31 - height / 2
                color: "#0b0e0d"
                border.color: "#69736f"
                border.width: 0.7
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 17
            height: 17
            radius: 8.5
            color: "#0a0d0c"
            border.color: "#d9dedb"
            border.width: 1.3
            Rectangle {
                anchors.centerIn: parent
                width: 6
                height: 6
                radius: 3
                color: "#7e8984"
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 3
            height: 1
            color: "#65ffffff"
        }
    }
}
