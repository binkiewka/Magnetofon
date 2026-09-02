import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    z: 999
    color: "#d905080a" // Dark glass backdrop overlay

    signal closeRequested()

    property int targetFps: 60
    property string presetSource: "CURATED"

    Theme { id: theme }

    // Prevent click-through to underlying console controls
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    HifiPanel {
        id: container
        width: Math.min(840, parent.width - 40)
        height: Math.min(620, parent.height - 40)
        anchors.centerIn: parent
        title: "VISUALS & PERFORMANCE ENGINE"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            PanelHeader {
                Layout.fillWidth: true
                title: "VISUALS & PERFORMANCE ENGINE"
                iconText: "⛭"

                HifiButton {
                    text: "×"
                    isStop: true
                    isCompact: true
                    onClicked: root.closeRequested()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: container.width - 48
                    spacing: 12

                    // Card 1: GPU Performance & FPS Limiter
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: card1Col.implicitHeight + 20
                        radius: 7
                        color: "#070c0f"
                        border.color: "#1d2e35"
                        border.width: 1

                        ColumnLayout {
                            id: card1Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "GPU RENDER RATE & FPS LIMITER"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }

                            Text {
                                text: "Select target frame rate for low-spec GPUs, laptops, or power efficiency."
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 8
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                Repeater {
                                    model: [
                                        { label: "30 FPS (ECO)", value: 30 },
                                        { label: "60 FPS (DEFAULT)", value: 60 },
                                        { label: "120 FPS (HIGH HZ)", value: 120 },
                                        { label: "MAX / UNLIMITED", value: 0 }
                                    ]

                                    HifiButton {
                                        text: modelData.label
                                        isCompact: true
                                        isPrimary: visualizerLauncher.targetFps === modelData.value
                                        Layout.fillWidth: true
                                        onClicked: visualizerLauncher.targetFps = modelData.value
                                    }
                                }
                            }
                        }
                    }

                    // Card 2: Preset Rotation & Blend Timings
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: card2Col.implicitHeight + 20
                        radius: 7
                        color: "#070c0f"
                        border.color: "#1d2e35"
                        border.width: 1

                        ColumnLayout {
                            id: card2Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "PRESET ROTATION & BLEND TIMINGS"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Auto-Switch Interval"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(rotationSlider.value) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Slider {
                                id: rotationSlider
                                Layout.fillWidth: true
                                from: 2; to: 60; value: visualizerLauncher.presetDuration; stepSize: 1
                                onMoved: visualizerLauncher.presetDuration = Math.round(value)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Blend Transition Time"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: blendSlider.value.toFixed(1) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Slider {
                                id: blendSlider
                                Layout.fillWidth: true
                                from: 0.5; to: 10.0; value: visualizerLauncher.transitionDuration; stepSize: 0.5
                                onMoved: visualizerLauncher.transitionDuration = value
                            }
                        }
                    }

                    // Card 3: Beat Detector & Hard Cut Reactivity
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: card3Col.implicitHeight + 20
                        radius: 7
                        color: "#070c0f"
                        border.color: "#1d2e35"
                        border.width: 1

                        ColumnLayout {
                            id: card3Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "BEAT DETECTOR & HARD CUTS REACTIVITY"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 9
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                HifiButton {
                                    text: visualizerLauncher.hardCutsEnabled ? "ENABLED" : "DISABLED"
                                    isPowerOn: visualizerLauncher.hardCutsEnabled
                                    isCompact: true
                                    onClicked: visualizerLauncher.hardCutsEnabled = !visualizerLauncher.hardCutsEnabled
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                opacity: visualizerLauncher.hardCutsEnabled ? 1.0 : 0.4
                                Text {
                                    text: "Hard Cut Sensitivity"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: cutSensSlider.value.toFixed(1) + "x"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Slider {
                                id: cutSensSlider
                                Layout.fillWidth: true
                                enabled: visualizerLauncher.hardCutsEnabled
                                from: 0.1; to: 5.0; value: visualizerLauncher.hardCutSensitivity; stepSize: 0.1
                                onMoved: visualizerLauncher.hardCutSensitivity = value
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                opacity: visualizerLauncher.hardCutsEnabled ? 1.0 : 0.4
                                Text {
                                    text: "Minimum Time Between Hard Cuts"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(cutDurationSlider.value) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 8
                                    }
                                }
                            }

                            Slider {
                                id: cutDurationSlider
                                Layout.fillWidth: true
                                enabled: visualizerLauncher.hardCutsEnabled
                                from: 2; to: 60; value: visualizerLauncher.hardCutDuration; stepSize: 2
                                onMoved: visualizerLauncher.hardCutDuration = Math.round(value)
                            }
                        }
                    }

                    // Card 4: Preset Source Selector & Downloader
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: card4Col.implicitHeight + 20
                        radius: 7
                        color: "#070c0f"
                        border.color: "#1d2e35"
                        border.width: 1

                        ColumnLayout {
                            id: card4Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "VISUAL PRESET LIBRARY & DOWNLOADER"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 9
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                HifiButton {
                                    text: "CURATED PRESETS (50 PRESETS)"
                                    iconSymbol: "★"
                                    isCompact: true
                                    isPrimary: visualizerLauncher.presetSource === "CURATED"
                                    Layout.fillWidth: true
                                    onClicked: visualizerLauncher.presetSource = "CURATED"
                                }

                                HifiButton {
                                    text: "ALL 9,000+ PRESETS LIBRARY"
                                    iconSymbol: "🌐"
                                    isCompact: true
                                    isPrimary: visualizerLauncher.presetSource === "ALL"
                                    Layout.fillWidth: true
                                    onClicked: visualizerLauncher.presetSource = "ALL"
                                }
                            }


                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: presetPackDownloader.statusMessage
                                    color: theme.textMuted
                                    font.family: theme.technicalFont
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    implicitWidth: 94; implicitHeight: 18; radius: 3
                                    color: presetPackDownloader.isInstalled ? "#143a29" : "#3b2210"
                                    border.color: presetPackDownloader.isInstalled ? "#2ee59d" : "#f59e0b"

                                    Text {
                                        anchors.centerIn: parent
                                        text: presetPackDownloader.isInstalled ? "INSTALLED" : "NOT INSTALLED"
                                        color: presetPackDownloader.isInstalled ? "#2ee59d" : "#f59e0b"
                                        font.family: theme.technicalFont
                                        font.pixelSize: 7
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                visible: presetPackDownloader.isDownloading
                                value: presetPackDownloader.progress
                            }

                            HifiButton {
                                text: presetPackDownloader.isInstalled ? "REINSTALL 9,000+ PRESETS PACK (138 MB)" : "DOWNLOAD 9,000+ MILKDROP PRESETS PACK (138 MB)"
                                iconSymbol: "⬇"
                                isPrimary: !presetPackDownloader.isInstalled
                                isCompact: true
                                enabled: !presetPackDownloader.isDownloading
                                onClicked: presetPackDownloader.downloadPack()
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Card 5: Keyboard & Mouse Control Reference Guide
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: card5Col.implicitHeight + 20
                        radius: 7
                        color: "#070c0f"
                        border.color: "#1d2e35"
                        border.width: 1

                        ColumnLayout {
                            id: card5Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Text {
                                text: "KEYBOARD CONTROL SHORTCUTS REFERENCE"
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
                                columnSpacing: 12

                                Repeater {
                                    model: [
                                        { key: "SPACE", action: "Lock / Unlock active MilkDrop visual preset" },
                                        { key: "N", action: "Advance to Next visual preset" },
                                        { key: "P", action: "Return to Previous visual preset" },
                                        { key: "R", action: "Select Random visual preset" },
                                        { key: "F / F11", action: "Toggle Fullscreen visualizer mode" },
                                        { key: "CTRL + C", action: "Copy playing preset filename to clipboard" },
                                        { key: "L", action: "Like / Favorite playing preset" },
                                        { key: "ESC", action: "Exit Fullscreen / Close modal" }
                                    ]

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Rectangle {
                                            implicitWidth: 68
                                            implicitHeight: 20
                                            radius: 4
                                            color: "#121b1f"
                                            border.color: theme.cyan
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.key
                                                color: theme.cyanBright
                                                font.family: theme.technicalFont
                                                font.pixelSize: 8
                                                font.weight: Font.Bold
                                            }
                                        }

                                        Text {
                                            text: modelData.action
                                            color: theme.textSoft
                                            font.family: theme.uiFont
                                            font.pixelSize: 9
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

            // Footer Bar
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "MAGNETOFON ST-8000  •  PROJECTM / MILKDROP V4.2 NATIVE"
                    color: theme.textMuted
                    font.family: theme.technicalFont
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                }

                Item { Layout.fillWidth: true }

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
