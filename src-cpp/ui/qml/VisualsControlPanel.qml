import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    z: 999
    color: "#e6080b0d"

    signal closeRequested()

    Theme { id: theme }

    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    HifiPanel {
        id: container
        width: Math.min(930, parent.width - 28)
        height: Math.min(680, parent.height - 28)
        anchors.centerIn: parent
        title: "VISUALS CONTROL"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            PanelHeader {
                Layout.fillWidth: true
                title: "VISUALS CONTROL & AUDIO REACTIVITY ENGINE"
                iconText: "◇"

                HifiButton {
                    text: "×"
                    isStop: true
                    isCompact: true
                    implicitWidth: 28
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 62
                radius: 6
                color: "#091014"
                border.color: visualizerLauncher.isRunning ? "#285d50" : "#3b2c31"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 11
                    spacing: 11

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: visualizerLauncher.isRunning ? "#2ee59d" : theme.red

                        SequentialAnimation on opacity {
                            running: visualizerLauncher.isRunning
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.42; duration: 850 }
                            NumberAnimation { to: 1.0; duration: 850 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: visualizerLauncher.isRunning ? "NATIVE PROJECTM · RUNNING" : "NATIVE PROJECTM · STOPPED"
                            color: visualizerLauncher.isRunning ? "#67f3b9" : theme.red
                            font.family: theme.technicalFont
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            font.letterSpacing: 1.0
                        }

                        Text {
                            text: visualizerLauncher.currentPresetName !== ""
                                  ? "NOW PLAYING  ·  " + visualizerLauncher.currentPresetName
                                  : visualizerLauncher.statusMessage
                            color: theme.textSoft
                            font.family: theme.uiFont
                            font.pixelSize: 9
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: visualizerLauncher.isRunning ? visualizerLauncher.statusMessage : "Settings are saved automatically"
                            color: theme.textMuted
                            font.family: theme.technicalFont
                            font.pixelSize: 7
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    HifiButton {
                        text: visualizerLauncher.isRunning ? "STOP VISUALS" : "LAUNCH VISUALS"
                        iconSymbol: visualizerLauncher.isRunning ? "■" : "▶"
                        isPrimary: !visualizerLauncher.isRunning
                        isStop: visualizerLauncher.isRunning
                        onClicked: visualizerLauncher.toggleVisuals()
                    }
                }
            }

            ScrollView {
                id: scroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    width: scroll.availableWidth - 8
                    spacing: 11

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: libraryColumn.implicitHeight + 22
                        radius: 7
                        color: "#080e11"
                        border.color: "#1d353e"

                        ColumnLayout {
                            id: libraryColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "▣"
                                    color: theme.cyanBright
                                    font.pixelSize: 13
                                }
                                Text {
                                    text: "PRESET LIBRARY"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: sourceText.implicitWidth + 16
                                    implicitHeight: 20
                                    radius: 3
                                    color: "#10242c"
                                    border.color: "#235b70"
                                    Text {
                                        id: sourceText
                                        anchors.centerIn: parent
                                        text: visualizerLauncher.presetCount + " ACTIVE"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 7
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            Text {
                                text: "Choose the folder scanned by projectM. Curated is the bundled, quality-checked set; Full uses the downloaded 9k+ library."
                                color: theme.textMuted
                                font.family: theme.uiFont
                                font.pixelSize: 8
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                HifiButton {
                                    text: "CURATED · " + visualizerLauncher.curatedPresetCount + " PRESETS"
                                    iconSymbol: "★"
                                    isPrimary: visualizerLauncher.presetSource === "CURATED"
                                    enabled: visualizerLauncher.curatedPresetCount > 0
                                    Layout.fillWidth: true
                                    onClicked: visualizerLauncher.presetSource = "CURATED"
                                }

                                HifiButton {
                                    text: visualizerLauncher.fullLibraryAvailable
                                          ? "FULL LIBRARY · " + visualizerLauncher.fullPresetCount + " PRESETS"
                                          : "FULL LIBRARY · NOT INSTALLED"
                                    iconSymbol: "◎"
                                    isPrimary: visualizerLauncher.presetSource === "ALL"
                                    enabled: visualizerLauncher.fullLibraryAvailable
                                    Layout.fillWidth: true
                                    onClicked: visualizerLauncher.presetSource = "ALL"
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: presetPackDownloader.statusMessage
                                        color: presetPackDownloader.isInstalled ? "#55dca8" : theme.textSoft
                                        font.family: theme.uiFont
                                        font.pixelSize: 8
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text: visualizerLauncher.activePresetDirectory
                                        color: theme.textMuted
                                        font.family: theme.technicalFont
                                        font.pixelSize: 7
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }
                                }

                                HifiButton {
                                    text: presetPackDownloader.isInstalled ? "REINSTALL FULL PACK" : "DOWNLOAD FULL PACK · 138 MB"
                                    iconSymbol: "↓"
                                    isCompact: true
                                    enabled: !presetPackDownloader.isDownloading
                                    onClicked: presetPackDownloader.downloadPack()
                                }
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                visible: presetPackDownloader.isDownloading
                                value: presetPackDownloader.progress
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: timingColumn.implicitHeight + 22
                        radius: 7
                        color: "#080e11"
                        border.color: "#1d353e"

                        ColumnLayout {
                            id: timingColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "◷  DISPLAY TIMING & ROTATION"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                HifiButton {
                                    text: visualizerLauncher.shuffleEnabled ? "SHUFFLE ON" : "SEQUENTIAL"
                                    iconSymbol: visualizerLauncher.shuffleEnabled ? "↝" : "→"
                                    isPowerOn: visualizerLauncher.shuffleEnabled
                                    isCompact: true
                                    onClicked: visualizerLauncher.shuffleEnabled = !visualizerLauncher.shuffleEnabled
                                }
                            }

                            Text {
                                text: "Rotation advances automatically. Shuffle changes only the order. Hard cuts can switch earlier than the normal preset timer."
                                color: theme.textMuted
                                font.family: theme.uiFont
                                font.pixelSize: 8
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            VisualSettingSlider {
                                Layout.fillWidth: true
                                label: "Preset duration"
                                description: "Target time before a normal switch (projectM may vary by about one second). Hard cuts can end it earlier."
                                from: 3
                                to: 120
                                stepSize: 1
                                decimals: 0
                                suffix: " s"
                                value: visualizerLauncher.presetDuration
                                onValueCommitted: function(newValue) { visualizerLauncher.presetDuration = Math.round(newValue) }
                            }

                            VisualSettingSlider {
                                Layout.fillWidth: true
                                label: "Transition duration"
                                description: "Crossfade time for normal changes. Hard cuts skip it; 0.5 s is the safe minimum."
                                from: 0.5
                                to: Math.min(10, visualizerLauncher.presetDuration - 0.5)
                                stepSize: 0.5
                                decimals: 1
                                suffix: " s"
                                value: visualizerLauncher.transitionDuration
                                onValueCommitted: function(newValue) { visualizerLauncher.transitionDuration = newValue }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: "Visualizer window"
                                        color: theme.text
                                        font.family: theme.uiFont
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: "Borderless removes the title bar and frame. Use F or F11 inside the visualizer for fullscreen."
                                        color: theme.textMuted
                                        font.family: theme.uiFont
                                        font.pixelSize: 8
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                }

                                HifiButton {
                                    text: visualizerLauncher.borderlessWindow ? "BORDERLESS" : "FRAMED"
                                    iconSymbol: visualizerLauncher.borderlessWindow ? "□" : "▣"
                                    isPowerOn: visualizerLauncher.borderlessWindow
                                    isCompact: true
                                    onClicked: visualizerLauncher.borderlessWindow = !visualizerLauncher.borderlessWindow
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        text: "Target frame rate"
                                        color: theme.text
                                        font.family: theme.uiFont
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        text: "30 saves power · 60 is recommended · Unlimited follows the renderer/v-sync limit."
                                        color: theme.textMuted
                                        font.family: theme.uiFont
                                        font.pixelSize: 8
                                    }
                                }

                                Repeater {
                                    model: [
                                        { label: "30", value: 30 },
                                        { label: "60", value: 60 },
                                        { label: "120", value: 120 },
                                        { label: "MAX", value: 0 }
                                    ]
                                    HifiButton {
                                        text: modelData.label
                                        isCompact: true
                                        isPrimary: visualizerLauncher.targetFps === modelData.value
                                        onClicked: visualizerLauncher.targetFps = modelData.value
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: reactivityColumn.implicitHeight + 22
                        radius: 7
                        color: "#080e11"
                        border.color: "#1d353e"

                        ColumnLayout {
                            id: reactivityColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 7

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "⌁  MUSIC REACTIVITY & HARD CUTS"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                HifiButton {
                                    text: visualizerLauncher.hardCutsEnabled ? "HARD CUTS ON" : "HARD CUTS OFF"
                                    isPowerOn: visualizerLauncher.hardCutsEnabled
                                    isCompact: true
                                    onClicked: visualizerLauncher.hardCutsEnabled = !visualizerLauncher.hardCutsEnabled
                                }
                            }

                            Text {
                                text: "Audio source  ·  " + (visualizerLauncher.audioSourceName !== ""
                                      ? visualizerLauncher.audioSourceName
                                      : "system output monitor (resolved when visuals launch)")
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 7
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            VisualSettingSlider {
                                Layout.fillWidth: true
                                label: "Beat sensitivity"
                                description: "Scales bass, mid and treble energy sent to presets. Higher values make audio-driven motion react more strongly."
                                from: 0
                                to: 2
                                stepSize: 0.1
                                decimals: 1
                                suffix: "×"
                                value: visualizerLauncher.beatSensitivity
                                onValueCommitted: function(newValue) { visualizerLauncher.beatSensitivity = newValue }
                            }

                            VisualSettingSlider {
                                Layout.fillWidth: true
                                enabled: visualizerLauncher.hardCutsEnabled
                                dimmed: !visualizerLauncher.hardCutsEnabled
                                label: "Hard-cut threshold"
                                description: "Volume jump required for an instant scene cut. Lower values cut more often; higher values need a stronger hit."
                                from: 0.1
                                to: 5
                                stepSize: 0.1
                                decimals: 1
                                suffix: "×"
                                value: visualizerLauncher.hardCutSensitivity
                                onValueCommitted: function(newValue) { visualizerLauncher.hardCutSensitivity = newValue }
                            }

                            VisualSettingSlider {
                                Layout.fillWidth: true
                                enabled: visualizerLauncher.hardCutsEnabled
                                dimmed: !visualizerLauncher.hardCutsEnabled
                                label: "Hard-cut cooldown"
                                description: "Minimum age of the current preset before a strong beat may trigger a cut. Normal timed rotation is separate."
                                from: 2
                                to: Math.max(2, visualizerLauncher.presetDuration - 1)
                                stepSize: 1
                                decimals: 0
                                suffix: " s"
                                value: visualizerLauncher.hardCutDuration
                                onValueCommitted: function(newValue) { visualizerLauncher.hardCutDuration = Math.round(newValue) }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 30
                                radius: 4
                                color: "#0e1c21"
                                border.color: "#214653"

                                Text {
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    text: "Hard cuts can replace the normal timer: after the cooldown, a strong enough peak switches immediately. Turn them off for duration-only rotation."
                                    color: theme.textSoft
                                    font.family: theme.uiFont
                                    font.pixelSize: 8
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: shortcutsColumn.implicitHeight + 22
                        radius: 7
                        color: "#080e11"
                        border.color: "#1d353e"

                        ColumnLayout {
                            id: shortcutsColumn
                            anchors.fill: parent
                            anchors.margins: 11
                            spacing: 8

                            Text {
                                text: "⌨  LIVE VISUALIZER SHORTCUTS"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                rowSpacing: 6
                                columnSpacing: 10

                                Repeater {
                                    model: [
                                        { key: "N / P", action: "Next / previous preset" },
                                        { key: "R", action: "Jump to a random preset" },
                                        { key: "SPACE", action: "Lock / unlock automatic changes" },
                                        { key: "F / F11", action: "Toggle fullscreen" },
                                        { key: "CTRL + C", action: "Copy current preset path" },
                                        { key: "ESC", action: "Leave fullscreen" }
                                    ]

                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: 29
                                        radius: 4
                                        color: "#0b1418"
                                        border.color: "#193441"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            spacing: 8
                                            Rectangle {
                                                implicitWidth: Math.max(58, keyLabel.implicitWidth + 14)
                                                implicitHeight: 19
                                                radius: 3
                                                color: "#10232b"
                                                border.color: "#22728e"
                                                Text {
                                                    id: keyLabel
                                                    anchors.centerIn: parent
                                                    text: modelData.key
                                                    color: theme.cyanBright
                                                    font.family: theme.technicalFont
                                                    font.pixelSize: 7
                                                    font.weight: Font.Bold
                                                }
                                            }
                                            Text {
                                                text: modelData.action
                                                color: theme.textSoft
                                                font.family: theme.uiFont
                                                font.pixelSize: 8
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: visualizerLauncher.isRunning
                          ? "Changes restart the visualizer once after the control is released"
                          : "Settings saved · launch visuals when ready"
                    color: theme.textMuted
                    font.family: theme.technicalFont
                    font.pixelSize: 7
                    Layout.fillWidth: true
                }

                HifiButton {
                    text: "DONE / CLOSE"
                    isPrimary: true
                    isCompact: true
                    onClicked: root.closeRequested()
                }
            }
        }
    }
}
