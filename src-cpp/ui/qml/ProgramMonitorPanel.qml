import QtQuick 2.15
import QtQuick.Layouts 1.15

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 166
    title: "INPUT & ACTIVE TRACK DECK"

    property string trackTitle: "READY FOR INPUT"
    property string artist: "UNKNOWN ARTIST"
    property string album: "MAGNETOFON SYSTEM"
    property string albumArtist: ""
    property string artworkUrl: ""
    property string genre: ""
    property string year: ""
    property string trackNumber: ""
    property string codec: ""
    property string formatLabel: "PCM STEREO"
    property int sampleRate: 0
    property int channels: 0
    property int bitDepth: 0
    property double bitrate: 0
    property double position: 0.0
    property double duration: 0.0
    signal seekRequested(double position)

    property int progressIndex: duration > 0 ? Math.floor((position / duration) * 48) : 0
    Theme { id: theme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        PanelHeader {
            Layout.fillWidth: true
            title: "INPUT & ACTIVE TRACK DECK"
            iconText: "●"

            Rectangle {
                implicitWidth: 98
                implicitHeight: 22
                radius: 4
                color: "#0a1a21"
                border.color: theme.cyan
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: root.formatLabel
                    color: theme.cyanBright
                    font.family: theme.technicalFont
                    font.pixelSize: 8
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.8
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#03080b"
            border.color: "#168bc2"
            border.width: 1.4

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                radius: 4
                color: "transparent"
                border.color: "#2416c8ff"
            }

            Repeater {
                model: 7
                Rectangle {
                    x: 4
                    y: 7 + index * ((parent.height - 14) / 7)
                    width: parent.width - 8
                    height: 1
                    color: "#071d27"
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 11

                Rectangle {
                    id: artworkFrame
                    width: 55
                    height: 55
                    radius: 5
                    color: "#07131a"
                    border.color: "#148fc8"
                    border.width: 1.2

                    clip: true

                    Image {
                        id: coverArtwork
                        anchors.fill: parent
                        anchors.margins: 2
                        source: root.artworkUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        cache: true
                        visible: status === Image.Ready
                    }

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: "qrc:/resources/icon.png"
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        mipmap: true
                        visible: coverArtwork.status !== Image.Ready
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 3
                        height: 1
                        color: "#5fffffff"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: root.trackTitle
                        color: theme.cyanBright
                        font.family: theme.uiFont
                        font.weight: Font.Bold
                        font.pixelSize: 13
                        font.letterSpacing: 0.45
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.artist + "  •  " + root.album
                        color: theme.textMuted
                        font.family: theme.technicalFont
                        font.pixelSize: 8
                        font.letterSpacing: 0.45
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: root.metadataLine()
                        visible: text.length > 0
                        color: "#52717e"
                        font.family: theme.technicalFont
                        font.pixelSize: 7
                        font.letterSpacing: 0.35
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    width: 1
                    height: 47
                    color: "#173441"
                }

                ColumnLayout {
                    spacing: 1
                    Text {
                        text: formatTime(root.position)
                        color: theme.amber
                        font.family: theme.technicalFont
                        font.weight: Font.Bold
                        font.pixelSize: 15
                        font.letterSpacing: 0.5
                        Layout.alignment: Qt.AlignRight
                    }
                    Text {
                        text: "/ " + formatTime(root.duration)
                        color: "#65747b"
                        font.family: theme.technicalFont
                        font.pixelSize: 9
                        Layout.alignment: Qt.AlignRight
                    }
                }

                Rectangle { width: 1; height: 47; color: "#173441" }

                ColumnLayout {
                    spacing: 1
                    Text { text: root.sampleRateText(); color: theme.cyan; font.family: theme.technicalFont; font.pixelSize: 8; font.weight: Font.Bold; Layout.alignment: Qt.AlignRight }
                    Text { text: root.bitDepth > 0 ? root.bitDepth + "-BIT" : (root.channels > 0 ? root.channels + " CH" : "—"); color: theme.textMuted; font.family: theme.technicalFont; font.pixelSize: 8; Layout.alignment: Qt.AlignRight }
                    Text { text: root.bitrate > 0 ? Math.round(root.bitrate / 1000) + " kbps" : (root.codec.length > 0 ? root.codec : "—"); color: theme.amber; font.family: theme.technicalFont; font.pixelSize: 8; Layout.alignment: Qt.AlignRight }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: 3
                height: 1
                color: "#28ffffff"
            }
        }

        Rectangle {
            id: progressBar
            Layout.fillWidth: true
            height: 10
            radius: 3
            color: "#030608"
            border.color: "#203541"

            Row {
                anchors.fill: parent
                anchors.margins: 2
                spacing: 1
                Repeater {
                    model: 48
                    Rectangle {
                        width: (progressBar.width - 4 - 47) / 48
                        height: progressBar.height - 4
                        radius: 0.5
                        color: index < root.progressIndex ? theme.cyan : "#0d1820"
                        opacity: index < root.progressIndex ? 0.9 : 1.0
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: function(mouse) {
                    if (root.duration > 0) root.seekRequested((mouse.x / width) * root.duration)
                }
            }
        }
    }

    function formatTime(sec) {
        var safe = isNaN(sec) ? 0 : Math.max(0, sec)
        var s = Math.floor(safe % 60)
        var m = Math.floor(safe / 60)
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    function sampleRateText() {
        if (sampleRate <= 0) return "—"
        var khz = sampleRate / 1000
        return (sampleRate % 1000 === 0 ? khz.toFixed(0) : khz.toFixed(1)) + " kHz"
    }

    function metadataLine() {
        var values = []
        if (trackNumber.length > 0) values.push("TRACK " + trackNumber)
        if (year.length > 0) values.push(year)
        if (genre.length > 0) values.push(genre.toUpperCase())
        if (albumArtist.length > 0 && albumArtist.toLowerCase() !== artist.toLowerCase())
            values.push("ALBUM ARTIST: " + albumArtist.toUpperCase())
        return values.join("  •  ")
    }
}
