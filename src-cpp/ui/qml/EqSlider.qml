import QtQuick 2.15

Item {
    id: root
    width: 24
    height: 76

    property double value: 0.0
    property bool isSecondary: false
    signal valueChangedByUser(double val)

    property double normValue: Math.max(0.0, Math.min(1.0, (value + 12.0) / 24.0))
    Theme { id: theme }

    Repeater {
        model: 5
        Rectangle {
            x: index % 2 ? 3 : 1
            y: 4 + index * ((root.height - 10) / 4)
            width: index % 2 ? 4 : 6
            height: 1
            color: "#53605c"
            opacity: 0.46
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        width: 7
        radius: 3
        color: "#050807"
        border.color: "#3d4844"
        border.width: 1

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 1
            width: 3
            height: Math.max(2, (parent.height - 2) * root.normValue)
            radius: 1.5
            color: root.isSecondary ? theme.amber : theme.cyan
            opacity: 0.82
        }
    }

    Rectangle {
        id: thumbShadow
        anchors.horizontalCenter: parent.horizontalCenter
        y: thumb.y + 2
        width: 20
        height: 12
        radius: 3
        color: "#a0000000"
    }

    Rectangle {
        id: thumb
        anchors.horizontalCenter: parent.horizontalCenter
        y: (1 - root.normValue) * (root.height - height)
        width: 20
        height: 11
        radius: 3
        border.color: hover.hovered ? "#b7cbc3" : "#68736e"
        border.width: 1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#69716d" }
            GradientStop { position: 0.35; color: "#343a37" }
            GradientStop { position: 1.0; color: "#141816" }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 11
            height: 2
            radius: 1
            color: root.isSecondary ? theme.amber : theme.cyanBright
        }
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        onPressed: function(mouse) { updateVal(mouse.y) }
        onPositionChanged: function(mouse) { if (pressed) updateVal(mouse.y) }
        onWheel: function(wheel) {
            var delta = wheel.angleDelta.y > 0 ? 0.5 : -0.5
            root.valueChangedByUser(Math.max(-12, Math.min(12, root.value + delta)))
        }

        function updateVal(mouseY) {
            var y = Math.max(0, Math.min(root.height, mouseY))
            root.valueChangedByUser((1.0 - y / root.height) * 24.0 - 12.0)
        }
    }
}
