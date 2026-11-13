import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import Magnetofon 1.0

ApplicationWindow {
    id: window
    objectName: "mainConsole"
    width: Math.max(640, Math.min(1060, Screen.desktopAvailableWidth - 24))
    height: Math.max(480, Math.min(800, Screen.desktopAvailableHeight - 24))
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: "MAGNETOFON — HI-FI STEREO CONSOLE / MODEL ST-8000"
    color: "#080a08"
    flags: Qt.FramelessWindowHint | Qt.Window

    // The video surface is a separate top-level window, so Qt cannot infer that
    // closing this console means the whole application should exit.
    onClosing: Qt.quit()

    Theme { id: theme }
    Shortcut { sequence: "Space"; enabled: !visualsModal.visible && !speakersDialog.visible && !playlistPanel.searchHasFocus; onActivated: audioPlayer.togglePlayPause() }
    Shortcut { sequence: "Media Play"; onActivated: audioPlayer.play() }
    Shortcut { sequence: "Media Pause"; onActivated: audioPlayer.pause() }
    Shortcut { sequence: "Media Toggle Play/Pause"; onActivated: audioPlayer.togglePlayPause() }
    Shortcut { sequence: "Media Next"; onActivated: audioPlayer.next() }
    Shortcut { sequence: "Media Previous"; onActivated: audioPlayer.previous() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: audioPlayer.next() }
    Shortcut { sequence: "Ctrl+Left"; onActivated: audioPlayer.previous() }
    Shortcut { sequence: "Ctrl+F"; onActivated: playlistPanel.focusSearch() }
    Shortcut { sequence: "Ctrl+Up"; enabled: !playlistPanel.listHasFocus; onActivated: audioPlayer.volume = Math.min(1, audioPlayer.volume + 0.02) }
    Shortcut { sequence: "Ctrl+Down"; enabled: !playlistPanel.listHasFocus; onActivated: audioPlayer.volume = Math.max(0, audioPlayer.volume - 0.02) }


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
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: window.width >= 900 ? parent.horizontalCenter : undefined
                        anchors.left: window.width < 900 ? parent.left : undefined
                        spacing: 0

                        Text {
                            text: "MAGNETOFON"
                            color: theme.text
                            font.family: theme.uiFont
                            font.weight: Font.Black
                            font.pixelSize: window.width < 900 ? 18 : 23
                            font.letterSpacing: window.width < 900 ? 2.5 : 7.2
                            Layout.alignment: Qt.AlignHCenter
                        }

                        RowLayout {
                            visible: window.width >= 900
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
                        spacing: 5

                        HifiButton {
                            text: "VIDEO"
                            iconSymbol: "▣"
                            isCompact: true
                            enabled: audioPlayer.hasVideo
                            isPrimary: audioPlayer.videoVisible
                            onClicked: audioPlayer.toggleVideo()
                        }
                        HifiButton {
                            text: "VISUALS"
                            enabled: visualizerLauncher.runtimeAvailable || visualizerLauncher.isRunning
                            iconSymbol: "◇"
                            isCompact: true
                            isPrimary: visualizerLauncher.isRunning
                            onClicked: visualizerLauncher.toggleVisuals()
                        }
                        HifiButton {
                            text: "VIS CTRL"
                            iconSymbol: "⛭"
                            isCompact: true
                            isPrimary: visualsModal.visible
                            onClicked: visualsModal.visible = !visualsModal.visible
                        }
                        HifiButton {
                            text: "—"
                            isCompact: true
                            implicitWidth: 26
                            onClicked: window.showMinimized()
                        }
                        HifiButton {
                            text: "×"
                            isCompact: true
                            implicitWidth: 26
                            isStop: true
                            onClicked: window.close()
                        }
                    }

                }


                RowLayout {
                    Layout.fillWidth: true
                    visible: audioPlayer.playbackError.length > 0 || audioPlayer.loading
                    Text {
                        Layout.fillWidth: true
                        text: audioPlayer.playbackError.length > 0 ? audioPlayer.playbackError : "Loading media…"
                        color: audioPlayer.playbackError.length > 0 ? theme.red : theme.cyan
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        Accessible.role: Accessible.StaticText
                        Accessible.name: text
                    }
                    HifiButton { text: "RETRY"; visible: audioPlayer.playbackError.length > 0; onClicked: audioPlayer.retry() }
                    HifiButton { text: "SKIP"; visible: audioPlayer.playbackError.length > 0; onClicked: audioPlayer.next() }
                }

                Flickable {
                    id: rackViewport
                    objectName: "rackViewport"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: width
                    contentHeight: moduleGrid.height
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: rackViewport.contentHeight > rackViewport.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                    GridLayout {
                    id: moduleGrid
                    width: rackViewport.width
                    height: Math.max(implicitHeight, rackViewport.height)
                    columns: width < 850 ? 1 : 2
                    rowSpacing: 14
                    columnSpacing: 14

                        AmplifierPanel {
                            Layout.minimumWidth: 0
                            Layout.column: moduleGrid.columns === 1 ? 0 : 0
                            Layout.row: moduleGrid.columns === 1 ? 3 : 0
                            onSpeakerSettingsRequested: speakersDialog.visible = true
                            Layout.fillWidth: true
                            Layout.preferredHeight: playlistPanel.isExpanded ? 0 : (moduleGrid.columns === 1 ? 190 : 225)
                            visible: !playlistPanel.isExpanded
                            volume: audioPlayer.volume
                            surroundMode: audioPlayer.surroundMode
                            onVolumeChangedByUser: function(val) { audioPlayer.setVolume(val) }
                            onModeSelected: function(mode) { audioPlayer.setSurroundMode(mode) }
                        }

                        VuMeterPanel {
                            Layout.minimumWidth: 0
                            Layout.column: moduleGrid.columns === 1 ? 0 : 0
                            Layout.row: moduleGrid.columns === 1 ? 4 : 1
                            Layout.fillWidth: true
                            Layout.preferredHeight: playlistPanel.isExpanded ? 0 : (moduleGrid.columns === 1 ? 166 : 190)
                            visible: !playlistPanel.isExpanded
                            analysisAvailable: audioPlayer.analysisAvailable
                            playing: audioPlayer.isPlaying
                            leftLevel: audioPlayer.leftMeter
                            rightLevel: audioPlayer.rightMeter
                        }

                        PlaylistPanel {
                            Layout.minimumWidth: 0
                            Layout.column: 0
                            Layout.row: moduleGrid.columns === 1 ? 2 : (isExpanded ? 0 : 2)
                            Layout.rowSpan: moduleGrid.columns > 1 && isExpanded ? 3 : 1
                            id: playlistPanel
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 190
                            onFileOpenRequested: playlistModel.openFileDialog()
                            onFolderOpenRequested: playlistModel.openFolderDialog()
                            onPlaylistLoadRequested: playlistModel.loadPlaylistDialog()
                            onPlaylistSaveRequested: playlistModel.savePlaylistDialog()
                            onClearRequested: playlistModel.clear()
                        }

                        EqualizerPanel {
                            Layout.minimumWidth: 0
                            Layout.column: moduleGrid.columns === 1 ? 0 : 1
                            Layout.row: moduleGrid.columns === 1 ? 5 : 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: 225
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
                            Layout.minimumWidth: 0
                            Layout.column: moduleGrid.columns === 1 ? 0 : 1
                            Layout.row: moduleGrid.columns === 1 ? 0 : 1
                            id: programMonitorPanel
                            readonly property var currentTrack: playlistModel.currentTrack
                            Layout.fillWidth: true
                            Layout.preferredHeight: 190
                            trackTitle: playlistModel.currentIndex >= 0 ? currentTrack.title : "READY FOR INPUT"
                            artist: playlistModel.currentIndex >= 0 ? currentTrack.artist : "UNKNOWN ARTIST"
                            album: playlistModel.currentIndex >= 0 ? currentTrack.album : "MAGNETOFON SYSTEM"
                            albumArtist: playlistModel.currentIndex >= 0 ? currentTrack.albumArtist : ""
                            artworkUrl: playlistModel.currentIndex >= 0 ? currentTrack.artworkUrl : ""
                            genre: playlistModel.currentIndex >= 0 ? currentTrack.genre : ""
                            year: playlistModel.currentIndex >= 0 ? currentTrack.year : ""
                            trackNumber: playlistModel.currentIndex >= 0 ? currentTrack.trackNumber : ""
                            codec: playlistModel.currentIndex >= 0 ? currentTrack.codec : ""
                            formatLabel: playlistModel.currentIndex >= 0 ? currentTrack.formatLabel : "NO INPUT"
                            sampleRate: playlistModel.currentIndex >= 0 ? currentTrack.sampleRate : 0
                            channels: playlistModel.currentIndex >= 0 ? currentTrack.channels : 0
                            bitDepth: playlistModel.currentIndex >= 0 ? currentTrack.bitDepth : 0
                            bitrate: playlistModel.currentIndex >= 0 ? currentTrack.bitrate : 0
                            audioTracks: audioPlayer.audioTracks
                            selectedAudioTrackId: audioPlayer.selectedAudioTrackId
                            decodedAudioLabel: audioPlayer.decodedAudioLabel
                            sourceAudioLabel: audioPlayer.sourceAudioLabel
                            position: audioPlayer.position
                            duration: audioPlayer.duration > 0 ? audioPlayer.duration
                                                               : (playlistModel.currentIndex >= 0 ? currentTrack.duration : 0)
                            onSeekRequested: function(position) { audioPlayer.seek(position) }
                            onAudioTrackSelected: function(trackId) { audioPlayer.selectAudioTrack(trackId) }
                        }

                        CassettePanel {
                            Layout.minimumWidth: 0
                            Layout.column: moduleGrid.columns === 1 ? 0 : 1
                            Layout.row: moduleGrid.columns === 1 ? 1 : 2
                            gaplessEnabled: audioPlayer.gaplessEnabled
                            onGaplessToggled: function(enabled) { audioPlayer.gaplessEnabled = enabled }
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 260
                            isPlaying: audioPlayer.isPlaying
                            progress: audioPlayer.position
                            duration: audioPlayer.duration
                            onEjectRequested: playlistModel.openFileDialog()
                            onPreviousRequested: audioPlayer.previous()
                            onPlayPauseRequested: audioPlayer.togglePlayPause()
                            onStopRequested: audioPlayer.stop()
                            onNextRequested: audioPlayer.next()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: moduleGrid.columns === 1 || window.height < 780
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: playlistModel.currentIndex >= 0 ? playlistModel.currentTrack.title : "READY FOR INPUT"
                        color: theme.textSoft
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    HifiButton { text: "|◀"; Accessible.name: "Previous track"; onClicked: audioPlayer.previous() }
                    HifiButton { text: audioPlayer.isPlaying ? "Ⅱ" : "▶"; Accessible.name: audioPlayer.isPlaying ? "Pause" : "Play"; isPrimary: audioPlayer.isPlaying; onClicked: audioPlayer.togglePlayPause() }
                    HifiButton { text: "■"; Accessible.name: "Stop"; isStop: true; onClicked: audioPlayer.stop() }
                    HifiButton { text: "▶|"; Accessible.name: "Next track"; onClicked: audioPlayer.next() }
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

        SpeakerControlPanel {
            id: speakersDialog
            visible: false
            onCloseRequested: visible = false
        }
        VisualsControlPanel {
            id: visualsModal
            visible: false
            onCloseRequested: visible = false
        }
    }
}
