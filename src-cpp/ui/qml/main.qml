import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Magnetofon 1.0

ApplicationWindow {
    id: window
    width: 1060
    height: 730
    minimumWidth: 960
    minimumHeight: 660
    visible: true
    title: "MAGNETOFON — HI-FI STEREO CONSOLE / MODEL ST-8000"
    color: "#080a08"
    flags: Qt.FramelessWindowHint | Qt.Window

    Theme { id: theme }

    Item {
        anchors.fill: parent

        Rectangle {
            id: chassis
            anchors.fill: parent
            radius: 14
            border.color: "#7b8b8580"
            border.width: 1

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#353b37" }
                GradientStop { position: 0.12; color: "#2b302d" }
                GradientStop { position: 0.42; color: "#1a1e1b" }
                GradientStop { position: 0.78; color: "#101310" }
                GradientStop { position: 1.0; color: "#080a08" }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 12
                color: "transparent"
                border.color: "#2effffff"
                border.width: 1
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 3
                height: 2
                radius: 1
                color: "#85b4beb8"
                opacity: 0.5
            }

            Repeater {
                model: 46
                Rectangle {
                    x: 20
                    y: 10 + index * ((chassis.height - 20) / 46)
                    width: chassis.width - 40
                    height: 1
                    color: index % 3 === 0 ? "#0bffffff" : "#08000000"
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 19
                radius: 13
                border.color: "#44ffffff"
                border.width: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#0c0f0d" }
                    GradientStop { position: 0.26; color: "#343b37" }
                    GradientStop { position: 0.52; color: "#59625d" }
                    GradientStop { position: 0.73; color: "#252b28" }
                    GradientStop { position: 1.0; color: "#090b0a" }
                }
            }

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 19
                radius: 13
                border.color: "#44ffffff"
                border.width: 1
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#090b0a" }
                    GradientStop { position: 0.28; color: "#252b28" }
                    GradientStop { position: 0.5; color: "#59625d" }
                    GradientStop { position: 0.76; color: "#343b37" }
                    GradientStop { position: 1.0; color: "#0c0f0d" }
                }
            }

            Screw { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 6 }
            Screw { anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 6 }
            Screw { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 6 }
            Screw { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 6 }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 29
                anchors.rightMargin: 29
                anchors.topMargin: 12
                anchors.bottomMargin: 16
                spacing: 12

                Item {
                    Layout.fillWidth: true
                    implicitHeight: 46

                    MouseArea {
                        anchors.fill: parent
                        onPressed: window.startSystemMove()
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 0

                        Text {
                            text: "MAGNETOFON"
                            color: theme.text
                            font.family: theme.uiFont
                            font.weight: Font.Black
                            font.pixelSize: 23
                            font.letterSpacing: 7.2
                            Layout.alignment: Qt.AlignHCenter
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8
                            Rectangle { width: 24; height: 1; color: theme.cyan; opacity: 0.55 }
                            Text {
                                text: "HI-FI STEREO CONSOLE  /  MODEL ST-8000"
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 8
                                font.letterSpacing: 1.45
                            }
                            Rectangle { width: 24; height: 1; color: theme.cyan; opacity: 0.55 }
                        }
                    }

                    RowLayout {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        HifiButton { text: "VISUALS"; iconSymbol: "◇" }
                        HifiButton {
                            text: "—"
                            implicitWidth: 30
                            onClicked: window.showMinimized()
                        }
                        HifiButton {
                            text: "×"
                            implicitWidth: 30
                            isStop: true
                            onClicked: window.close()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        AmplifierPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 176
                            volume: audioPlayer.volume
                            surroundMode: audioPlayer.surroundMode
                            onVolumeChangedByUser: function(val) { audioPlayer.setVolume(val) }
                            onModeSelected: function(mode) { audioPlayer.setSurroundMode(mode) }
                        }

                        VuMeterPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 166
                            leftLevel: audioPlayer.leftMeter
                            rightLevel: audioPlayer.rightMeter
                        }

                        PlaylistPanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 190
                            onFileOpenRequested: playlistModel.openFileDialog()
                            onClearRequested: playlistModel.clear()
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        EqualizerPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 176
                            eqEnabled: audioPlayer.eqEnabled
                            preamp: audioPlayer.preamp
                            eqBands: audioPlayer.eqBands
                            surroundMode: audioPlayer.surroundMode
                            onUserEqToggled: function(enabled) { audioPlayer.setEqEnabled(enabled) }
                            onUserPreampChanged: function(value) { audioPlayer.setPreamp(value) }
                            onUserBandChanged: function(index, value) {
                                var newBands = audioPlayer.eqBands
                                newBands[index] = value
                                audioPlayer.setEqBands(newBands)
                            }
                            onUserSurroundModeSelected: function(mode) { audioPlayer.setSurroundMode(mode) }
                        }

                        ProgramMonitorPanel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 166
                            trackTitle: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).title : "READY FOR INPUT"
                            artist: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).artist : "UNKNOWN ARTIST"
                            album: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).album : "MAGNETOFON SYSTEM"
                            albumArtist: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).albumArtist : ""
                            artworkUrl: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).artworkUrl : ""
                            genre: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).genre : ""
                            year: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).year : ""
                            trackNumber: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).trackNumber : ""
                            codec: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).codec : ""
                            formatLabel: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).formatLabel : "NO INPUT"
                            sampleRate: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).sampleRate : 0
                            channels: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).channels : 0
                            bitDepth: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).bitDepth : 0
                            bitrate: playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).bitrate : 0
                            position: audioPlayer.position
                            duration: audioPlayer.duration > 0 ? audioPlayer.duration
                                                               : (playlistModel.currentIndex >= 0 ? playlistModel.getTrack(playlistModel.currentIndex).duration : 0)
                            onSeekRequested: function(position) { audioPlayer.seek(position) }
                        }

                        CassettePanel {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 190
                            isPlaying: audioPlayer.isPlaying
                            progress: audioPlayer.position
                            duration: audioPlayer.duration
                            onEjectRequested: playlistModel.openFileDialog()
                            onPreviousRequested: playlistModel.previousTrack()
                            onPlayPauseRequested: audioPlayer.togglePlayPause()
                            onStopRequested: audioPlayer.stop()
                            onNextRequested: playlistModel.nextTrack()
                        }
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -8
                anchors.left: parent.left
                anchors.leftMargin: 44
                width: 64
                height: 11
                radius: 4
                border.color: "#5c6964"
                gradient: Gradient {
                    GradientStop { position: 0; color: "#505652" }
                    GradientStop { position: 0.45; color: "#252927" }
                    GradientStop { position: 1; color: "#080a09" }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: -8
                anchors.right: parent.right
                anchors.rightMargin: 44
                width: 64
                height: 11
                radius: 4
                border.color: "#5c6964"
                gradient: Gradient {
                    GradientStop { position: 0; color: "#505652" }
                    GradientStop { position: 0.45; color: "#252927" }
                    GradientStop { position: 1; color: "#080a09" }
                }
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            cursorShape: Qt.SizeHorCursor
            onPressed: window.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            cursorShape: Qt.SizeHorCursor
            onPressed: window.startSystemResize(Qt.RightEdge)
        }
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 6
            cursorShape: Qt.SizeVerCursor
            onPressed: window.startSystemResize(Qt.TopEdge)
        }
        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 6
            cursorShape: Qt.SizeVerCursor
            onPressed: window.startSystemResize(Qt.BottomEdge)
        }
    }
}
