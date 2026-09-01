import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 176
    title: "10 BAND FREQUENCY PROCESSOR"

    property bool eqEnabled: false
    property double preamp: -3.0
    property var eqBands: [0,0,0,0,0,0,0,0,0,0]
    property string surroundMode: "AUTO"

    signal userEqToggled(bool enabled)
    signal userPreampChanged(double value)
    signal userBandChanged(int index, double value)
    signal userSurroundModeSelected(string mode)

    property var presets: ({
        "FLAT": [0,0,0,0,0,0,0,0,0,0],
        "ROCK": [4,3,2,0,-1,-1,0,2,3,4],
        "POP": [-1,0,2,3,4,3,2,0,-1,-1],
        "CLASSICAL": [4,3,2,2,0,0,0,2,3,3],
        "JAZZ": [3,2,1,2,-1,-1,0,1,2,3],
        "BASS BOOST": [6,5,4,2,0,0,0,0,0,0]
    })

    Theme { id: theme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        PanelHeader {
            Layout.fillWidth: true
            title: root.width < 460 ? "10 BAND FREQUENCY EQ" : "10 BAND FREQUENCY PROCESSOR"
            iconText: "≋"

            ComboBox {
                id: presetCombo
                implicitWidth: 124
                implicitHeight: 27
                model: ["PRESET: CUSTOM", "FLAT", "ROCK", "POP", "CLASSICAL", "JAZZ", "BASS BOOST"]
                font.family: theme.technicalFont
                font.pixelSize: 8

                contentItem: Text {
                    leftPadding: 9
                    rightPadding: 22
                    text: presetCombo.displayText
                    color: theme.textSoft
                    font: presetCombo.font
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                indicator: Text {
                    x: presetCombo.width - width - 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "⌄"
                    color: theme.textMuted
                    font.family: theme.uiFont
                    font.pixelSize: 13
                }
                background: Rectangle {
                    radius: 5
                    border.color: presetCombo.hovered ? "#71817b" : "#46534f"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#3b423f" }
                        GradientStop { position: 1; color: "#151917" }
                    }
                }

                onActivated: {
                    if (currentIndex <= 0) return
                    var vals = root.presets[currentText]
                    if (!vals) return
                    for (var i = 0; i < 10; ++i) root.userBandChanged(i, vals[i])
                }
            }

            HifiButton {
                text: root.eqEnabled ? "ACTIVE" : "BYPASS"
                iconSymbol: "◉"
                isPowerOn: root.eqEnabled
                onClicked: root.userEqToggled(!root.eqEnabled)
            }
        }

        SpectrumAnalyzer {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            spectrumData: audioPlayer.spectrum
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 5
            color: "#080c0e"
            border.color: root.eqEnabled ? "#29566a" : "#303a37"
            border.width: 1
            opacity: root.eqEnabled ? 1.0 : 0.48

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 3

                ColumnLayout {
                    Layout.fillHeight: true
                    spacing: 1
                    EqSlider {
                        Layout.fillHeight: true
                        value: root.preamp
                        isSecondary: true
                        onValueChangedByUser: function(val) {
                            if (root.eqEnabled) root.userPreampChanged(val)
                        }
                    }
                    Text {
                        text: "GAIN"
                        color: theme.amber
                        font.family: theme.technicalFont
                        font.pixelSize: 7
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }
                }

                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 4
                    color: "#34423d"
                }

                Repeater {
                    model: ["31", "62", "125", "250", "500", "1K", "2K", "4K", "8K", "16K"]
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 1
                        EqSlider {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            value: index < root.eqBands.length ? root.eqBands[index] : 0.0
                            onValueChangedByUser: function(val) {
                                if (root.eqEnabled) root.userBandChanged(index, val)
                            }
                        }
                        Text {
                            text: modelData
                            color: theme.textMuted
                            font.family: theme.technicalFont
                            font.pixelSize: 7
                            font.weight: Font.DemiBold
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
