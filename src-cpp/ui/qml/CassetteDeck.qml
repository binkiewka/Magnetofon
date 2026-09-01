import QtQuick 2.15

Item {
    id: root
    implicitWidth: 480
    implicitHeight: 142

    property bool isPlaying: false
    property double progress: 0.0
    property double duration: 1.0
    property double fillPct: duration > 0 ? Math.min(1.0, Math.max(0.0, progress / duration)) : 0.0

    Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        radius: 7
        border.color: "#3c4844"
        border.width: 1
        gradient: Gradient {
            GradientStop { position: 0; color: "#070a09" }
            GradientStop { position: 0.55; color: "#111612" }
            GradientStop { position: 1; color: "#050706" }
        }

        Rectangle {
            id: cassShell
            anchors.fill: parent
            anchors.margins: 6
            radius: 11
            border.color: "#52605a"
            border.width: 1
            gradient: Gradient {
                GradientStop { position: 0; color: "#323733" }
                GradientStop { position: 0.24; color: "#1b201d" }
                GradientStop { position: 1; color: "#0a0d0b" }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 9
                color: "transparent"
                border.color: "#20ffffff"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 15
                width: 38
                height: parent.height * 0.54
                radius: 10
                color: "#10000000"
                border.color: "#30443e"
            }
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 15
                width: 38
                height: parent.height * 0.54
                radius: 10
                color: "#10000000"
                border.color: "#30443e"
            }

            Screw { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 8; width: 7; height: 7 }
            Screw { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8; width: 7; height: 7 }
            Screw { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8; width: 7; height: 7 }
            Screw { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 8; width: 7; height: 7 }

            Rectangle {
                id: topLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 5
                width: parent.width * 0.54
                height: 24
                radius: 3
                border.color: "#59615d"
                border.width: 1
                gradient: Gradient {
                    GradientStop { position: 0; color: "#b7b7aa" }
                    GradientStop { position: 0.52; color: "#deded0" }
                    GradientStop { position: 1; color: "#8b8d83" }
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 14
                    Text {
                        text: "MAGNETOFON"
                        color: "#1b1d1b"
                        font.family: theme.uiFont
                        font.pixelSize: 8
                        font.weight: Font.Black
                        font.letterSpacing: 1.1
                    }
                    Rectangle { width: 1; height: 10; color: "#777b74"; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: "TYPE I  /  NORMAL POSITION"
                        color: "#333733"
                        font.family: theme.technicalFont
                        font.pixelSize: 6
                        font.weight: Font.DemiBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 3
                    anchors.rightMargin: 3
                    height: 2
                    color: "#177fa2"
                    opacity: 0.72
                }
            }

            Rectangle {
                id: cassWindow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: parent.width * 0.075
                anchors.rightMargin: parent.width * 0.075
                anchors.topMargin: 30
                anchors.bottomMargin: 15
                radius: 9
                border.color: "#6a7671"
                border.width: 1.3
                gradient: Gradient {
                    GradientStop { position: 0; color: "#17201d" }
                    GradientStop { position: 0.16; color: "#0b100e" }
                    GradientStop { position: 0.7; color: "#050806" }
                    GradientStop { position: 1; color: "#101511" }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 3
                    radius: 6
                    color: "transparent"
                    border.color: "#1900c8ff"
                }

                Canvas {
                    id: tapePath
                    anchors.fill: parent
                    antialiasing: true
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        var leftX = width * 0.27
                        var rightX = width * 0.73
                        var centerY = height * 0.49

                        var well = ctx.createLinearGradient(0, height * 0.58, 0, height)
                        well.addColorStop(0, "rgba(44,51,47,0.18)")
                        well.addColorStop(1, "rgba(2,3,2,0.78)")
                        ctx.fillStyle = well
                        ctx.beginPath()
                        ctx.moveTo(width * 0.35, height * 0.61)
                        ctx.lineTo(width * 0.65, height * 0.61)
                        ctx.lineTo(width * 0.59, height * 0.96)
                        ctx.lineTo(width * 0.41, height * 0.96)
                        ctx.closePath()
                        ctx.fill()
                        ctx.strokeStyle = "rgba(132,145,138,0.36)"
                        ctx.lineWidth = 1
                        ctx.stroke()

                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"
                        ctx.strokeStyle = "#2d2117"
                        ctx.lineWidth = 4
                        ctx.beginPath()
                        ctx.moveTo(leftX + width * 0.055, centerY - height * 0.23)
                        ctx.lineTo(rightX - width * 0.055, centerY - height * 0.23)
                        ctx.stroke()
                        ctx.strokeStyle = "#8a7652"
                        ctx.lineWidth = 1.2
                        ctx.stroke()

                        ctx.strokeStyle = "#2a1d13"
                        ctx.lineWidth = 4
                        ctx.beginPath()
                        ctx.moveTo(leftX, centerY + height * 0.23)
                        ctx.lineTo(width * 0.38, height * 0.82)
                        ctx.lineTo(width * 0.62, height * 0.82)
                        ctx.lineTo(rightX, centerY + height * 0.23)
                        ctx.stroke()
                        ctx.strokeStyle = "#8e7750"
                        ctx.lineWidth = 1
                        ctx.stroke()

                        ctx.strokeStyle = "rgba(44,200,255,0.18)"
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(width * 0.32, height * 0.14)
                        ctx.lineTo(width * 0.68, height * 0.14)
                        ctx.stroke()
                    }
                }

                CassetteReel {
                    id: leftReel
                    anchors.left: parent.left
                    anchors.leftMargin: parent.width * 0.13
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(78, parent.height - 4)
                    height: width
                    tapeFill: 0.34 + root.fillPct * 0.38
                    running: root.isPlaying
                    reverse: false
                }

                CassetteReel {
                    anchors.right: parent.right
                    anchors.rightMargin: parent.width * 0.13
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(78, parent.height - 4)
                    height: width
                    tapeFill: 0.72 - root.fillPct * 0.38
                    running: root.isPlaying
                    reverse: false
                }

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Repeater {
                        model: 5
                        Rectangle {
                            width: 4; height: 6; radius: 1
                            color: root.isPlaying && index > 1 ? theme.cyan : "#27332f"
                            opacity: 0.85
                        }
                    }
                    Text { text: "L"; color: theme.textMuted; font.family: theme.technicalFont; font.pixelSize: 7 }
                }

                Column {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 3
                    Repeater {
                        model: 5
                        Rectangle {
                            width: 4; height: 6; radius: 1
                            color: root.isPlaying && index > 0 ? theme.cyan : "#27332f"
                            opacity: 0.85
                        }
                    }
                    Text { text: "R"; color: theme.textMuted; font.family: theme.technicalFont; font.pixelSize: 7 }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -3
                    spacing: 8
                    Rectangle {
                        width: 11; height: 11; radius: 5.5
                        color: "#161b18"; border.color: "#a4ada8"; border.width: 1.2
                        Rectangle { anchors.centerIn: parent; width: 4; height: 4; radius: 2; color: "#5f6964" }
                    }
                    Rectangle {
                        width: 17; height: 23; radius: 3
                        border.color: "#909791"
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#8a8f8b" }
                            GradientStop { position: 1; color: "#343936" }
                        }
                    }
                    Rectangle {
                        width: 11; height: 11; radius: 5.5
                        color: "#161b18"; border.color: "#a4ada8"; border.width: 1.2
                        Rectangle { anchors.centerIn: parent; width: 4; height: 4; radius: 2; color: "#5f6964" }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 4
                    height: parent.height * 0.27
                    radius: 4
                    color: "#0dffffff"
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    text: "A  •  90"
                    color: "#75827c"
                    font.family: theme.technicalFont
                    font.pixelSize: 6
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.1
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 3
                spacing: 20
                Text { text: "MX-90"; color: "#52615c"; font.family: theme.technicalFont; font.pixelSize: 7; font.weight: Font.Bold }
                Text { text: root.isPlaying ? "PLAY" : (playlistModel.count > 0 ? "CUED" : "EMPTY"); color: root.isPlaying ? theme.cyan : "#52615c"; font.family: theme.technicalFont; font.pixelSize: 7; font.weight: Font.Bold }
                Text { text: "DOLBY NR"; color: "#52615c"; font.family: theme.technicalFont; font.pixelSize: 7; font.weight: Font.Bold }
            }
        }
    }
}
