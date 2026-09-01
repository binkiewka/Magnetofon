import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: 220
    height: 32
    property var spectrumData: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
    Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: "#05090c"
        border.color: "#164154"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 3
            color: "transparent"
            border.color: "#2016c8ff"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            spacing: 3

            Repeater {
                model: 16

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    property double level: index < root.spectrumData.length
                                                   ? Math.max(0, Math.min(1, root.spectrumData[index])) : 0

                    Column {
                        anchors.fill: parent
                        spacing: 1

                        Repeater {
                            model: 7
                            Rectangle {
                                width: parent.width
                                height: Math.max(1, (parent.height - 6) / 7)
                                radius: 0.7
                                property real threshold: (6 - index) / 7
                                color: index < 2 ? theme.red
                                                 : (index < 4 ? theme.cyanBright : theme.blue)
                                opacity: parent.parent.level >= threshold ? 0.96 : 0.09
                                Behavior on opacity { NumberAnimation { duration: 48 } }
                            }
                        }
                    }
                }
            }
        }
    }
}
