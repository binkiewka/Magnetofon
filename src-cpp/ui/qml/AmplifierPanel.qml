import QtQuick 2.15
import QtQuick.Layouts 1.15

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 176
    title: "POWER STAGE"

    property double volume: 0.8
    property string surroundMode: "AUTO"
    signal volumeChangedByUser(double val)
    signal modeSelected(string mode)

    Theme { id: theme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        PanelHeader {
            Layout.fillWidth: true
            title: "POWER STAGE"
            iconText: "∿"
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                Item {
                    Layout.preferredWidth: 170
                    Layout.fillHeight: true

                    Knob {
                        anchors.centerIn: parent
                        value: root.volume
                        min: 0.0
                        max: 1.0
                        size: 68
                        label: "MASTER VOLUME"
                        displayValue: root.volume === 0 ? "−∞ dB" : Math.round((root.volume - 1) * 60) + " dB"
                        onValueChangedByUser: function(val) { root.volumeChangedByUser(val) }
                    }
                }

                Rectangle {
                    Layout.fillHeight: true
                    Layout.topMargin: 7
                    Layout.bottomMargin: 7
                    width: 1
                    gradient: Gradient {
                        GradientStop { position: 0; color: "transparent" }
                        GradientStop { position: 0.25; color: "#45534d" }
                        GradientStop { position: 0.75; color: "#1e2925" }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 7

                    Item { Layout.fillHeight: true }

                    Text {
                        text: "PLAYBACK TOPOLOGY"
                        color: theme.textMuted
                        font.family: theme.technicalFont
                        font.pixelSize: 8
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.25
                        Layout.alignment: Qt.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                            Repeater {
                                model: ["AUTO", "STEREO", "SURROUND"]
                                HifiButton {
                                    text: modelData
                                    Layout.fillWidth: true
                                    isCompact: true
                                    isPrimary: root.surroundMode === modelData
                                    onClicked: root.modeSelected(modelData)
                                }
                            }

                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 6
                        Rectangle {
                            width: 5; height: 5; radius: 2.5
                            color: theme.cyan
                            opacity: 0.9
                        }
                        Text {
                            text: "24-BIT DIGITAL ATTENUATION"
                            color: theme.textMuted
                            font.family: theme.technicalFont
                            font.pixelSize: 7
                            font.letterSpacing: 0.8
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
