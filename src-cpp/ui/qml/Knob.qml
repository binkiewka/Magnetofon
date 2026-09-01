import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    implicitWidth: size + 48
    implicitHeight: size + 50

    property double value: 0.8
    property double min: 0.0
    property double max: 1.0
    property int size: 70
    property string label: "MASTER VOLUME"
    property string displayValue: "0 dB"

    signal valueChangedByUser(double val)

    property double normValue: Math.max(0.0, Math.min(1.0, (value - min) / (max - min)))
    property double angle: -135 + normValue * 270

    Theme { id: theme }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 1

        Item {
            id: dialStage
            width: root.size + 34
            height: root.size + 30
            Layout.alignment: Qt.AlignHCenter

            Canvas {
                id: knobCanvas
                anchors.fill: parent
                antialiasing: true

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var cx = width / 2
                    var cy = height / 2 + 1
                    var r = root.size / 2

                    ctx.strokeStyle = "rgba(164,185,180,0.26)"
                    ctx.lineWidth = 1.1
                    for (var i = 0; i < 21; ++i) {
                        var a = (-135 + i * 13.5) * Math.PI / 180
                        var active = i / 20 <= root.normValue
                        var r1 = r + (i % 5 === 0 ? 8 : 10)
                        var r2 = r + 14
                        ctx.strokeStyle = active ? "#20caff" : "rgba(191,207,201,0.24)"
                        ctx.lineWidth = i % 5 === 0 ? 1.8 : 1
                        ctx.beginPath()
                        ctx.moveTo(cx + Math.sin(a) * r1, cy - Math.cos(a) * r1)
                        ctx.lineTo(cx + Math.sin(a) * r2, cy - Math.cos(a) * r2)
                        ctx.stroke()
                    }

                    ctx.fillStyle = "rgba(0,0,0,0.62)"
                    ctx.beginPath()
                    ctx.arc(cx + 2, cy + 4, r + 2, 0, Math.PI * 2)
                    ctx.fill()

                    var rim = ctx.createRadialGradient(cx - r * 0.3, cy - r * 0.35, 2, cx, cy, r + 2)
                    rim.addColorStop(0, "#a5ada8")
                    rim.addColorStop(0.42, "#4e5652")
                    rim.addColorStop(0.74, "#202523")
                    rim.addColorStop(1, "#080a09")
                    ctx.fillStyle = rim
                    ctx.beginPath()
                    ctx.arc(cx, cy, r + 1, 0, Math.PI * 2)
                    ctx.fill()

                    ctx.strokeStyle = "rgba(255,255,255,0.38)"
                    ctx.lineWidth = 1
                    ctx.stroke()

                    for (var k = 0; k < 36; ++k) {
                        var ka = k * Math.PI * 2 / 36
                        ctx.strokeStyle = k % 2 ? "rgba(0,0,0,0.45)" : "rgba(255,255,255,0.18)"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(cx + Math.cos(ka) * (r - 4), cy + Math.sin(ka) * (r - 4))
                        ctx.lineTo(cx + Math.cos(ka) * r, cy + Math.sin(ka) * r)
                        ctx.stroke()
                    }

                    var cap = ctx.createRadialGradient(cx - r * 0.28, cy - r * 0.32, 1, cx, cy, r - 7)
                    cap.addColorStop(0, "#49514d")
                    cap.addColorStop(0.44, "#252b28")
                    cap.addColorStop(1, "#101311")
                    ctx.fillStyle = cap
                    ctx.beginPath()
                    ctx.arc(cx, cy, r - 7, 0, Math.PI * 2)
                    ctx.fill()

                    var pa = root.angle * Math.PI / 180
                    ctx.strokeStyle = "rgba(0,0,0,0.7)"
                    ctx.lineWidth = 4.5
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.sin(pa) * 7 + 1, cy - Math.cos(pa) * 7 + 2)
                    ctx.lineTo(cx + Math.sin(pa) * (r - 12) + 1, cy - Math.cos(pa) * (r - 12) + 2)
                    ctx.stroke()
                    ctx.strokeStyle = "#3bd7ff"
                    ctx.lineWidth = 3
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.moveTo(cx + Math.sin(pa) * 7, cy - Math.cos(pa) * 7)
                    ctx.lineTo(cx + Math.sin(pa) * (r - 12), cy - Math.cos(pa) * (r - 12))
                    ctx.stroke()
                }

                Connections {
                    target: root
                    function onAngleChanged() { knobCanvas.requestPaint() }
                    function onNormValueChanged() { knobCanvas.requestPaint() }
                    function onSizeChanged() { knobCanvas.requestPaint() }
                }
            }

            MouseArea {
                anchors.fill: parent
                property real lastY: 0
                onPressed: function(mouse) { lastY = mouse.y }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    var dy = lastY - mouse.y
                    lastY = mouse.y
                    var newVal = Math.max(root.min, Math.min(root.max,
                                      root.value + (dy / 120.0) * (root.max - root.min)))
                    root.valueChangedByUser(newVal)
                }
                onWheel: function(wheel) {
                    var delta = wheel.angleDelta.y > 0 ? 0.04 : -0.04
                    root.valueChangedByUser(Math.max(root.min, Math.min(root.max, root.value + delta)))
                }
            }
        }

        Text {
            text: root.label
            color: theme.textMuted
            font.family: theme.technicalFont
            font.pixelSize: 8
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: root.displayValue
            color: theme.cyanBright
            font.family: theme.technicalFont
            font.pixelSize: 10
            font.weight: Font.Bold
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
