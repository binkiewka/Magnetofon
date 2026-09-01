import QtQuick 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    implicitHeight: 28
    property string title: "PANEL"
    property string iconText: "◉"
    default property alias actions: actionRow.data

    Theme { id: theme }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 23
        spacing: 8

        Item {
            implicitWidth: 17
            implicitHeight: 17

            Rectangle {
                anchors.centerIn: parent
                width: 15
                height: 15
                radius: 7.5
                color: "#08131a"
                border.color: theme.cyan
                border.width: 1.2
            }
            Text {
                anchors.centerIn: parent
                text: root.iconText
                color: theme.cyanBright
                font.family: theme.technicalFont
                font.pixelSize: 8
                font.bold: true
            }
        }

        Text {
            text: root.title
            color: theme.textSoft
            font.family: theme.technicalFont
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 1.45
            Layout.fillWidth: true
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        RowLayout {
            id: actionRow
            spacing: 5
            Layout.alignment: Qt.AlignVCenter
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 2
        color: "#060807"

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: "#47514d"
            opacity: 0.65
        }
    }
}
