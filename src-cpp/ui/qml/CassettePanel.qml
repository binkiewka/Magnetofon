import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 220
    title: "TAPE PLAYBACK ENGINE"

    property bool gaplessEnabled: false
    signal gaplessToggled(bool enabled)
    property bool isPlaying: false
    property double progress: 0.0
    property double duration: 1.0

    signal ejectRequested()
    signal previousRequested()
    signal playPauseRequested()
    signal stopRequested()
    signal nextRequested()

    Theme { id: theme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        PanelHeader {
            Layout.fillWidth: true
            title: "TAPE PLAYBACK ENGINE"
            iconText: "◉"
            HifiButton {
                objectName: "repeatButton"
                isCompact: true
                text: "REPEAT " + audioPlayer.repeatMode
                isPrimary: audioPlayer.repeatMode !== "OFF"
                onClicked: audioPlayer.repeatMode = audioPlayer.repeatMode === "OFF" ? "ALL" : (audioPlayer.repeatMode === "ALL" ? "ONE" : "OFF")
                ToolTip.visible: hovered
                ToolTip.text: "Repeat Off / All / One; independent of gapless playback"
            }
            Text {
                text: playlistModel.count === 0 ? "READY"
                                               : (root.isPlaying ? "PLAYING" : "PAUSED")
                color: playlistModel.count > 0 && root.isPlaying ? theme.cyan : theme.textMuted
                font.family: theme.technicalFont
                font.pixelSize: 10
                font.letterSpacing: 0.8
            }
        }

        CassetteDeck {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 92
            isPlaying: root.isPlaying && playlistModel.count > 0
            hasMedia: playlistModel.count > 0
            progress: root.progress
            duration: root.duration
        }

        Item {
            Layout.fillWidth: true
            height: 35

            Rectangle {
                anchors.fill: transportFace
                anchors.topMargin: 3
                radius: 6
                color: "#9a000000"
            }

            Rectangle {
                id: transportFace
                anchors.fill: parent
                anchors.bottomMargin: 3
                radius: 6
                border.color: "#5e6b65"
                border.width: 1
                gradient: Gradient {
                    GradientStop { position: 0; color: "#3d4541" }
                    GradientStop { position: 0.18; color: "#303733" }
                    GradientStop { position: 0.58; color: "#222824" }
                    GradientStop { position: 1; color: "#171c19" }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 3
                    anchors.rightMargin: 3
                    height: 1
                    color: "#78ffffff"
                    opacity: 0.58
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.bottomMargin: 3
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                HifiButton {
                    text: "EJECT"
                    iconSymbol: "△"
                    implicitWidth: 58
                    implicitHeight: 22
                    onClicked: root.ejectRequested()
                }

                Text {
                    text: "LOGIC CONTROL"
                    color: "#41524d"
                    font.family: theme.technicalFont
                    font.pixelSize: 10
                    font.letterSpacing: 0.9
                    Layout.leftMargin: 4
                }

                HifiButton {
                    objectName: "gaplessButton"
                    text: "GAPLESS"
                    isCompact: true
                    isPrimary: root.gaplessEnabled
                    onClicked: root.gaplessToggled(!root.gaplessEnabled)
                    ToolTip.visible: hovered
                    ToolTip.text: "Continuous album playback · no added pauses or crossfade"
                }

                Item { Layout.fillWidth: true }

                HifiButton {
                    Accessible.name: "Previous track"
                    text: "|◀"
                    implicitWidth: 30
                    implicitHeight: 22
                    onClicked: root.previousRequested()
                }
                HifiButton {
                    Accessible.name: root.isPlaying ? "Pause" : "Play"
                    text: root.isPlaying && playlistModel.count > 0 ? "Ⅱ" : "▶"
                    implicitWidth: 30
                    implicitHeight: 22
                    onClicked: root.playPauseRequested()
                }
                HifiButton {
                    Accessible.name: "Stop"
                    text: "■"
                    isStop: true
                    implicitWidth: 30
                    implicitHeight: 22
                    onClicked: root.stopRequested()
                }
                HifiButton {
                    Accessible.name: "Next track"
                    text: "▶|"
                    implicitWidth: 30
                    implicitHeight: 22
                    onClicked: root.nextRequested()
                }
            }
        }
    }
}
