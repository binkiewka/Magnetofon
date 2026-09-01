import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control
    implicitHeight: isRoundPlay ? 42 : (isCompact ? 22 : 28)
    implicitWidth: isRoundPlay ? 42 : (isCompact ? Math.max(40, textMeasurer.implicitWidth + 18) : Math.max(50, textMeasurer.implicitWidth + 24))
    padding: 0
    hoverEnabled: true

    property bool isPrimary: false
    property bool isPowerOn: false
    property bool isStop: false
    property bool isRoundPlay: false
    property bool isCompact: false
    property string iconSymbol: ""

    Theme { id: theme }

    Text {
        id: textMeasurer
        visible: false
        text: (control.iconSymbol !== "" ? control.iconSymbol + "  " : "") + control.text
        font.family: theme.technicalFont
        font.pixelSize: isCompact ? 8 : 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.8
    }

    contentItem: Text {
        text: (control.iconSymbol !== "" ? control.iconSymbol + "  " : "") + control.text
        font.family: control.isRoundPlay ? theme.uiFont : theme.technicalFont
        font.pixelSize: control.isRoundPlay ? 16 : (control.isCompact ? 8 : 9)
        font.weight: Font.DemiBold
        font.letterSpacing: control.isRoundPlay ? 0 : 0.8
        color: control.isStop ? theme.red
                              : ((control.isPrimary || control.isPowerOn || control.hovered)
                                 ? theme.text : theme.textSoft)

        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1.0 : 0.4
        y: control.pressed ? 1 : 0
    }

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 3
            radius: control.isRoundPlay ? width / 2 : 5
            color: "#b0000000"
        }

        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: control.pressed ? 0 : 2
            anchors.topMargin: control.pressed ? 2 : 0
            radius: control.isRoundPlay ? width / 2 : 5
            border.color: control.isPrimary || control.isPowerOn
                          ? theme.cyanBright
                          : (control.isStop && control.hovered ? "#8fff4f70"
                                                              : (control.hovered ? "#829aa4a0" : "#4b5c625f"))
            border.width: control.isPrimary || control.isPowerOn ? 1.4 : 1

            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: control.isPrimary || control.isPowerOn
                           ? (control.pressed ? "#0872bf" : "#139ee9")
                           : (control.pressed ? "#252a28" : (control.hovered ? "#58615d" : "#444a47"))
                }
                GradientStop {
                    position: 0.5
                    color: control.isPrimary || control.isPowerOn
                           ? "#0874d8"
                           : (control.pressed ? "#171b19" : "#2a2f2d")
                }
                GradientStop {
                    position: 1.0
                    color: control.isPrimary || control.isPowerOn ? "#063f91" : "#141816"
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 3
                anchors.rightMargin: 3
                height: 1
                radius: 1
                color: control.isPrimary || control.isPowerOn ? "#b5e9fbff" : "#6fffffff"
                opacity: control.pressed ? 0.18 : 0.72
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: parent.radius + 3
                color: "transparent"
                border.color: theme.cyan
                border.width: 1
                visible: control.isPrimary || control.isPowerOn
                opacity: 0.34
            }
        }
    }

    Behavior on scale { NumberAnimation { duration: 90 } }
    onPressedChanged: scale = pressed ? 0.975 : 1.0
}
