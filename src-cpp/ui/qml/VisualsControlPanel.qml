import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    anchors.fill: parent
    z: 999
    color: "#eb040608" // Dark sapphire glass acrylic backdrop

    signal closeRequested()

    Theme { id: theme }

    // Prevent click-through to underlying panel controls
    MouseArea {
        anchors.fill: parent
        onClicked: {}
    }

    Rectangle {
        id: container
        width: Math.min(880, parent.width - 36)
        height: Math.min(650, parent.height - 36)
        anchors.centerIn: parent
        radius: 12
        color: "#0a0e10"
        border.color: theme.cyan
        border.width: 1.5

        // Top metallic bevel
        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: 10
            color: "transparent"
            border.color: "#2500c8ff"
            border.width: 1
        }

        Screw { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 7 }
        Screw { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 7 }
        Screw { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7 }
        Screw { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7 }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            spacing: 12

            // Header Title Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Rectangle {
                    width: 26; height: 26; radius: 6
                    color: "#122a36"
                    border.color: theme.cyan
                    Text {
                        anchors.centerIn: parent
                        text: "⛭"
                        color: theme.cyanBright
                        font.pixelSize: 14
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        text: "VISUALS CONTROL & AUDIO REACTIVITY ENGINE"
                        color: theme.text
                        font.family: theme.uiFont
                        font.weight: Font.Bold
                        font.pixelSize: 13
                        font.letterSpacing: 1.5
                    }
                    Text {
                        text: "NATIVE MILKDROP 4.2 PRESET ROTATION & BEAT ANALYSIS CONFIGURATION"
                        color: theme.textMuted
                        font.family: theme.technicalFont
                        font.pixelSize: 8
                        font.letterSpacing: 0.8
                    }
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
                color: "#1d2e36"
            }

            // Scrollable Content
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: container.width - 56
                    spacing: 14

                    // Section 1: Engine Status & Preset Control
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: sec1Col.implicitHeight + 24
                        radius: 8
                        color: "#0c1216"
                        border.color: "#1d2d35"
                        border.width: 1

                        ColumnLayout {
                            id: sec1Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "ENGINE STATUS & QUICK CONTROL"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }
                                Rectangle {
                                    implicitWidth: 74; implicitHeight: 18; radius: 3
                                    color: "#123724"; border.color: "#2ee59d"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "ACTIVE 60 FPS"
                                        color: "#2ee59d"
                                        font.family: theme.technicalFont
                                        font.pixelSize: 7
                                        font.weight: Font.Bold
                                    }
                                }
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
                                spacing: 8

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
                        implicitHeight: sec2Col.implicitHeight + 24
                        radius: 8
                        color: "#0c1216"
                        border.color: "#1d2d35"
                        border.width: 1

                        ColumnLayout {
                            id: sec2Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "PRESET ROTATION & BLEND TIMINGS"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 10
                                font.weight: Font.Bold
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
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(rotationSlider.value) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 9
                                    }
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
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: blendSlider.value.toFixed(1) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 9
                                    }
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
                        implicitHeight: sec3Col.implicitHeight + 24
                        radius: 8
                        color: "#0c1216"
                        border.color: "#1d2d35"
                        border.width: 1

                        ColumnLayout {
                            id: sec3Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "BEAT DETECTOR & HARD CUTS"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
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
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: cutSensSlider.value.toFixed(1) + "x"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 9
                                    }
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
                                Rectangle {
                                    implicitWidth: 40; implicitHeight: 18; radius: 3
                                    color: "#112630"; border.color: theme.cyan
                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(cutDurationSlider.value) + "s"
                                        color: theme.cyanBright
                                        font.family: theme.technicalFont
                                        font.pixelSize: 9
                                    }
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
                        implicitHeight: sec4Col.implicitHeight + 24
                        radius: 8
                        color: "#0c1216"
                        border.color: "#1d2d35"
                        border.width: 1

                        ColumnLayout {
                            id: sec4Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true

                                Text {
                                    text: "MILKDROP PRESET PACK INSTALLER"
                                    color: theme.cyanBright
                                    font.family: theme.technicalFont
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1.2
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    implicitWidth: 96; implicitHeight: 18; radius: 3
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
                        implicitHeight: sec5Col.implicitHeight + 24
                        radius: 8
                        color: "#0c1216"
                        border.color: "#1d2d35"
                        border.width: 1

                        ColumnLayout {
                            id: sec5Col
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Text {
                                text: "PRESET RATING & CURATION MANAGER"
                                color: theme.cyanBright
                                font.family: theme.technicalFont
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

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

            // Footer Bar
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "MAGNETOFON ST-8000 VISUALS ENGINE V4.2"
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
