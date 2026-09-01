import QtQuick 2.15
import QtQuick.Layouts 1.15
import Magnetofon 1.0

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 166
    title: "TWIN SIGNAL LEVEL MONITOR"

    property double leftLevel: 0.0
    property double rightLevel: 0.0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        PanelHeader {
            Layout.fillWidth: true
            title: "TWIN SIGNAL LEVEL MONITOR"
            iconText: "⌁"
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#04080b"
            border.color: "#3b4d4e"
            border.width: 1

            VuMeterItem {
                anchors.fill: parent
                anchors.margins: 3
                leftLevel: root.leftLevel
                rightLevel: root.rightLevel
            }
        }
    }
}
