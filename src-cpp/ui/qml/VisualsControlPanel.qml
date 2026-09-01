import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    z: 999
    color: "#e605080a" // Dark glass acrylic backdrop

    signal closeRequested()

    Theme { id: theme }

    // Prevent click-through to underlying panel controls
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    Rectangle {
        id: container
        width: Math.min(840, parent.width - 40)
        height: Math.min(620, parent.height - 40)
        anchors.centerIn: parent
        radius: 12
        color: "#0c1114"
        border.color: theme.cyan
        border.width: 1.5

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 10
            color: "transparent"
            border.color: "#1d00c8ff"
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // Header Title Bar
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "⛭"
                    color: theme.cyanBright
                    font.pixelSize: 16
                }

                Text {
                    text: "VISUALS CONTROL & AUDIO REACTIVITY ENGINE"
                    color: theme.text
                    font.family: theme.uiFont
                    font.weight: Font.Bold
                    font.pixelSize: 13
                    font.letterSpacing: 1.5
                    Layout.fillWidth: true
                }

                HifiButton {
                    text: "×"
                    isStop: true
                    isCompact: true
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#1c2e35"
            }

            // Scrollable Content
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: container.width - 48
                    spacing: 16

                    // Section 1: Engine Status & Preset Control
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 110
                        radius: 8
                        color: "#080e11"
                        border.color: "#19282f"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8

                            Text {
                                text: "ENGINE STATUS & QUICK CONTROL"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "ACTIVE PRESET:"
                                    color: theme.textMuted
                                    font.family: theme.technicalFont
                                    font.pixelSize: 9
                                }

                                Text {
                                    text: "Isosceles - Cosmic Ray Spectrum (MilkDrop v2)"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                HifiButton { text: "PREV PRESET"; iconSymbol: "◀"; isCompact: true }
                                HifiButton { text: "NEXT PRESET"; iconSymbol: "▶"; isCompact: true }
                                HifiButton { text: "LOCK PRESET"; iconSymbol: "🔒"; isCompact: true }
                                Item { Layout.fillWidth: true }
                                HifiButton { text: "RANDOM PRESET"; iconSymbol: "🔀"; isCompact: true }
                            }
                        }
                    }

                    // Section 2: Preset Rotation & Blend Timings
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 135
                        radius: 8
                        color: "#080e11"
                        border.color: "#19282f"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            Text {
                                text: "PRESET ROTATION & BLEND TIMINGS"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Auto-Switch Interval"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: Math.round(rotationSlider.value) + "s"
                                    color: theme.cyan
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                }
                            }
                            Slider {
                                id: rotationSlider
                                Layout.fillWidth: true
                                from: 2; to: 60; value: 15; stepSize: 1
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "Blend Transition Time"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: blendSlider.value.toFixed(1) + "s"
                                    color: theme.cyan
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                }
                            }
                            Slider {
                                id: blendSlider
                                Layout.fillWidth: true
                                from: 0.5; to: 10.0; value: 2.5; stepSize: 0.5
                            }
                        }
                    }

                    // Section 3: Hard Cut Peak Reactivity
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 145
                        radius: 8
                        color: "#080e11"
                        border.color: "#19282f"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "BEAT DETECTOR & HARD CUTS"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                HifiButton {
                                    text: hardCutsToggle.checked ? "ENABLED" : "DISABLED"
                                    isPowerOn: hardCutsToggle.checked
                                    isCompact: true
                                    onClicked: hardCutsToggle.checked = !hardCutsToggle.checked
                                }
                                Switch {
                                    id: hardCutsToggle
                                    checked: true
                                    visible: false
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                opacity: hardCutsToggle.checked ? 1.0 : 0.4
                                Text {
                                    text: "Hard Cut Sensitivity"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: cutSensSlider.value.toFixed(1) + "x"
                                    color: theme.cyan
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                }
                            }
                            Slider {
                                id: cutSensSlider
                                Layout.fillWidth: true
                                enabled: hardCutsToggle.checked
                                from: 0.1; to: 5.0; value: 1.0; stepSize: 0.1
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                opacity: hardCutsToggle.checked ? 1.0 : 0.4
                                Text {
                                    text: "Minimum Time Between Hard Cuts"
                                    color: theme.text
                                    font.family: theme.uiFont
                                    font.pixelSize: 10
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: Math.round(cutDurationSlider.value) + "s"
                                    color: theme.cyan
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                }
                            }
                            Slider {
                                id: cutDurationSlider
                                Layout.fillWidth: true
                                enabled: hardCutsToggle.checked
                                from: 2; to: 60; value: 10; stepSize: 2
                            }
                        }
                    }

                    // Section 4: 9,000+ Presets Pack Manager & Downloader
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: presetPackDownloader.isDownloading ? 160 : 130
                        radius: 8
                        color: "#080e11"
                        border.color: "#19282f"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "MILKDROP PRESET PACK INSTALLER"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    implicitWidth: 100
                                    implicitHeight: 20
                                    radius: 4
                                    color: presetPackDownloader.isInstalled ? "#143a29" : "#3b2210"
                                    border.color: presetPackDownloader.isInstalled ? "#2ee59d" : "#f59e0b"

                                    Text {
                                        anchors.centerIn: parent
                                        text: presetPackDownloader.isInstalled ? "INSTALLED" : "NOT INSTALLED"
                                        color: presetPackDownloader.isInstalled ? "#2ee59d" : "#f59e0b"
                                        font.family: theme.technicalFont
                                        font.pixelSize: 8
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            Text {
                                text: presetPackDownloader.statusMessage
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 9
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                visible: presetPackDownloader.isDownloading
                                value: presetPackDownloader.progress
                            }

                            HifiButton {
                                text: presetPackDownloader.isInstalled ? "REINSTALL PRESETS PACK (138 MB)" : "DOWNLOAD 9,000+ MILKDROP PRESETS PACK (138 MB)"
                                iconSymbol: "⬇"
                                isPrimary: !presetPackDownloader.isInstalled
                                isCompact: true
                                enabled: !presetPackDownloader.isDownloading
                                onClicked: presetPackDownloader.downloadPack()
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Section 5: Preset Rating & Curation Manager
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 130
                        radius: 8
                        color: "#080e11"
                        border.color: "#19282f"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10

                            Text {
                                text: "PRESET RATING & CURATION MANAGER"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                HifiButton {
                                    text: "LIKE FROM CLIPBOARD (Ctrl+C)"
                                    iconSymbol: "📋"
                                    isCompact: true
                                    Layout.fillWidth: true
                                }
                                HifiButton {
                                    text: "MOVE LIKED TO CURATED FOLDER"
                                    iconSymbol: "♥"
                                    isPrimary: true
                                    isCompact: true
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: "Presets library containing 9,000+ MilkDrop presets pre-configured."
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 8
                            }
                        }
                    }
                }
            }


            // Footer
            RowLayout {
                Layout.fillWidth: true

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
