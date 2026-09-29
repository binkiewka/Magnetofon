import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    z: 1000
    color: "#e6080b0d"
    signal closeRequested()
    property bool editingSurround: false
    onVisibleChanged: if (visible) {
        editingSurround = audioPlayer.surroundMode === "SURROUND"
        forceActiveFocus()
    }
    Keys.onEscapePressed: root.closeRequested()

    Theme { id: theme }

    MouseArea {
        anchors.fill: parent
        onClicked: {}
        onWheel: function(wheel) { wheel.accepted = true }
    }

    HifiPanel {
        width: Math.min(800, parent.width - 28)
        height: Math.min(root.editingSurround ? 650 : 470, parent.height - 28)
        anchors.centerIn: parent
        title: "SPEAKER TUNING"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            PanelHeader {
                Layout.fillWidth: true
                title: "SPEAKERS & SUBWOOFER · CROSSOVER CONTROL"
                iconText: "∿"
                HifiButton {
                    text: "×"
                    isStop: true
                    isCompact: true
                    implicitWidth: 28
                    onClicked: root.closeRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                HifiButton {
                    objectName: "stereoProfileTab"
                    text: "2.1 STEREO"
                    isPrimary: !root.editingSurround
                    onClicked: root.editingSurround = false
                }
                HifiButton {
                    objectName: "surroundProfileTab"
                    text: "5.1 SURROUND"
                    isPrimary: root.editingSurround
                    onClicked: root.editingSurround = true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: "PLAYBACK · " + audioPlayer.surroundMode
                    color: theme.textMuted
                    font.family: theme.technicalFont
                    font.pixelSize: 8
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 64
                radius: 6
                color: "#091014"
                border.color: "#285d50"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 11
                    Rectangle {
                        width: 10; height: 10; radius: 5
                        color: theme.cyan
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: root.editingSurround ? "5.1 · INDIVIDUAL SPEAKER CROSSOVERS" : "2.1 · STEREO & SUBWOOFER"
                            color: theme.cyanBright
                            font.family: theme.technicalFont
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 1
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.editingSurround
                                ? "Tune native 5.1 and stereo upmixing. Bass below each speaker’s cutoff is sent to the subwoofer."
                                : "Tune stereo enhancement independently of Surround. Auto always bypasses speaker tuning."
                            color: theme.textSoft
                            font.family: theme.uiFont
                            font.pixelSize: 9
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 7
                color: "#080e11"
                border.color: "#1d353e"
                ScrollView {
                    id: settingsScroll
                    anchors.fill: parent
                    anchors.margins: 11
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: settingsColumn.implicitHeight
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ColumnLayout {
                        id: settingsColumn
                        width: settingsScroll.availableWidth - 8
                        spacing: 5
                        Repeater {
                            model: root.editingSurround
                                ? ["Front left · FL", "Front right · FR", "Center · C", "Surround left · SL", "Surround right · SR"]
                                : ["Front speakers · L / R"]
                            VisualSettingSlider {
                                implicitHeight: root.editingSurround ? 46 : 54
                                objectName: "speakerCutoff" + index
                                Layout.fillWidth: true
                                label: modelData
                                description: "High-pass cutoff · 0 Hz keeps this speaker full range."
                                from: 0; to: 200; stepSize: 5; decimals: 0; suffix: " Hz"
                                value: root.editingSurround ? audioPlayer.surroundCutoffs[index] : audioPlayer.speakerCutoff
                                onValueCommitted: function(value) {
                                    if (root.editingSurround) audioPlayer.setSurroundCutoff(index, value)
                                    else audioPlayer.speakerCutoff = value
                                }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; height: 1; color: "#1d353e" }
                        VisualSettingSlider {
                            implicitHeight: root.editingSurround ? 46 : 54
                            objectName: "subwooferCutoff"
                            Layout.fillWidth: true
                            label: "Subwoofer · LFE"
                            description: root.editingSurround ? "Low-pass for original LFE + redirected speaker bass." : "Upper frequency limit for the generated bass channel."
                            from: 40; to: 200; stepSize: 5; decimals: 0; suffix: " Hz"
                            value: root.editingSurround ? audioPlayer.surroundSubwooferCutoff : audioPlayer.subwooferCutoff
                            onValueCommitted: function(value) {
                                if (root.editingSurround) audioPlayer.surroundSubwooferCutoff = value
                                else audioPlayer.subwooferCutoff = value
                            }
                        }
                        VisualSettingSlider {
                            implicitHeight: root.editingSurround ? 46 : 54
                            objectName: "subwooferGain"
                            Layout.fillWidth: true
                            label: "Subwoofer level"
                            description: "Balance the subwoofer against your main speakers."
                            from: -12; to: 6; stepSize: 0.5; decimals: 1; suffix: " dB"
                            value: root.editingSurround ? audioPlayer.surroundSubwooferGain : audioPlayer.subwooferGain
                            onValueCommitted: function(value) {
                                if (root.editingSurround) audioPlayer.surroundSubwooferGain = value
                                else audioPlayer.subwooferGain = value
                            }
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.editingSurround
                    ? "12 dB/octave · Supports rear or side 5.1 surrounds. Other multichannel layouts pass through unchanged."
                    : "12 dB/octave · Requires an output with a dedicated subwoofer / LFE channel."
                color: theme.textMuted
                font.family: theme.uiFont
                font.pixelSize: 8
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "SEPARATE PROFILES · AUTO-SAVED"
                    color: theme.textMuted
                    font.family: theme.technicalFont
                    font.pixelSize: 7
                    font.letterSpacing: 0.8
                }
                Item { Layout.fillWidth: true }
                HifiButton {
                    objectName: "resetProfile"
                    text: root.editingSurround ? "RESET 5.1" : "RESET 2.1"
                    isCompact: true
                    onClicked: audioPlayer.resetSpeakerProfile(root.editingSurround)
                }
                HifiButton {
                    text: "DONE"
                    isCompact: true
                    isPrimary: true
                    onClicked: root.closeRequested()
                }
            }
        }
    }
}
