import QtQuick 2.15

Rectangle {
    id: root
    radius: 9
    clip: false
    property string title: "PANEL HEADER"

    Theme { id: theme }

    gradient: Gradient {
        GradientStop { position: 0.0; color: theme.panelTop }
        GradientStop { position: 0.16; color: "#282e2b" }
        GradientStop { position: 0.56; color: theme.panelMid }
        GradientStop { position: 1.0; color: theme.panelBottom }
    }
    border.color: "#52605a"
    border.width: 1

    Rectangle {
        z: -3
        anchors.fill: parent
        anchors.leftMargin: -3
        anchors.rightMargin: -3
        anchors.topMargin: 3
        anchors.bottomMargin: -6
        radius: root.radius + 2
        color: "#a8000000"
    }

    Rectangle {
        z: -2
        anchors.fill: parent
        anchors.margins: -1
        radius: root.radius + 1
        color: "transparent"
        border.color: "#26000000"
        border.width: 2
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: root.radius - 1
        color: "transparent"
        border.color: "#20ffffff"
        border.width: 1
        opacity: 0.75
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 2
        height: 1
        color: "#90a4a09b"
        opacity: 0.62
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 3
        anchors.rightMargin: 3
        height: 3
        radius: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#28000000" }
            GradientStop { position: 1.0; color: "#d0000000" }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 2
        anchors.leftMargin: 2
        width: parent.width * 0.42
        height: 1
        color: "#55ffffff"
        opacity: 0.42
    }

    Screw {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 6
        anchors.rightMargin: 7
        width: 6
        height: 6
        opacity: 0.7
        z: 20
    }
}
