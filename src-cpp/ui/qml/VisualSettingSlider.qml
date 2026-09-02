import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    implicitHeight: 54

    property string label: "SETTING"
    property string description: ""
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.1
    property int decimals: 1
    property string suffix: ""
    property bool dimmed: false

    signal valueCommitted(real value)

    Theme { id: theme }

    RowLayout {
        anchors.fill: parent
        spacing: 14
        opacity: root.dimmed ? 0.38 : 1.0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 260
            spacing: 2

            Text {
                text: root.label
                color: theme.text
                font.family: theme.uiFont
                font.pixelSize: 10
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Text {
                text: root.description
                color: theme.textMuted
                font.family: theme.uiFont
                font.pixelSize: 8
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        Slider {
            id: slider
            Layout.preferredWidth: 260
            Layout.alignment: Qt.AlignVCenter
            from: root.from
            to: root.to
            stepSize: root.stepSize
            value: root.value
            enabled: root.enabled

            onPressedChanged: {
                if (!pressed)
                    root.valueCommitted(value)
            }

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 5
                width: slider.availableWidth
                height: implicitHeight
                radius: 2.5
                color: "#172126"
                border.color: "#526168"

                Rectangle {
                    width: slider.visualPosition * parent.width
                    height: parent.height
                    radius: parent.radius
                    color: theme.cyan
                    opacity: 0.86
                }
            }

            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                implicitWidth: 16
                implicitHeight: 16
                radius: 8
                color: slider.pressed ? theme.cyanBright : theme.cyan
                border.color: "#b8ecfb"
                border.width: 1

                Rectangle {
                    anchors.centerIn: parent
                    width: 4
                    height: 4
                    radius: 2
                    color: "#09222c"
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 24
            Layout.alignment: Qt.AlignVCenter
            radius: 4
            color: "#0b171d"
            border.color: "#247491"

            Text {
                anchors.centerIn: parent
                text: Number(slider.value).toFixed(root.decimals) + root.suffix
                color: theme.cyanBright
                font.family: theme.technicalFont
                font.pixelSize: 9
                font.weight: Font.Bold
            }
        }
    }
}
