import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

HifiPanel {
    id: root
    implicitWidth: 480
    implicitHeight: 215
    title: "PROGRAM MEMORY"

    property bool isExpanded: false
    signal fileOpenRequested()
    signal clearRequested()
    Theme { id: theme }


    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        PanelHeader {
            Layout.fillWidth: true
            title: "PROGRAM MEMORY"
            iconText: "▥"

            HifiButton { text: "ADD"; iconSymbol: "+"; isCompact: true; onClicked: root.fileOpenRequested() }
            HifiButton { text: "LOAD"; isCompact: true; onClicked: root.fileOpenRequested() }
            HifiButton { text: "SAVE"; isCompact: true }
            HifiButton { text: "CLEAR"; isStop: true; isCompact: true; onClicked: root.clearRequested() }
        }


        Rectangle {
            id: playlistBay
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: "#05090b"
            border.color: "#35443f"
            border.width: 1

            DropArea {
                id: fileDropArea
                anchors.fill: parent
                z: 40

                onEntered: function(drag) {
                    drag.accepted = drag.hasUrls
                }
                onDropped: function(drop) {
                    if (!drop.hasUrls) return
                    playlistModel.addFiles(drop.urls)
                    drop.acceptProposedAction()
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                z: 41
                visible: fileDropArea.containsDrag
                radius: 5
                color: "#d9142630"
                border.color: theme.cyan
                border.width: 1.5

                Column {
                    anchors.centerIn: parent
                    spacing: 7
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "+"
                        color: theme.cyanBright
                        font.family: theme.uiFont
                        font.pixelSize: 24
                        font.weight: Font.Light
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "DROP MUSIC OR VIDEO FILES"
                        color: theme.cyanBright
                        font.family: theme.technicalFont
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.5
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: 5
                color: "transparent"
                border.color: "#1600c8ff"
            }

            Item {
                anchors.fill: parent
                visible: playlistModel.count === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 9

                    Item {
                        width: 44
                        height: 31
                        anchors.horizontalCenter: parent.horizontalCenter

                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            width: 19; height: 19; radius: 9.5
                            color: "transparent"; border.color: "#304d58"; border.width: 2
                            Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; color: "#304d58" }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 19; height: 19; radius: 9.5
                            color: "transparent"; border.color: "#304d58"; border.width: 2
                            Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; color: "#304d58" }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 24; height: 2; color: "#304d58"; rotation: -28
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "PROGRAM BUFFER EMPTY"
                        color: "#3c5660"
                        font.family: theme.technicalFont
                        font.pixelSize: 9
                        font.letterSpacing: 1.8
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "ADD MEDIA TO BEGIN PLAYBACK"
                        color: "#293c43"
                        font.family: theme.technicalFont
                        font.pixelSize: 7
                        font.letterSpacing: 0.7
                    }
                }
            }

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                spacing: 2
                model: playlistModel
                visible: playlistModel.count > 0

                delegate: Rectangle {
                    width: listView.width
                    height: 40
                    radius: 4
                    color: index === playlistModel.currentIndex ? "#122f3a"
                                                                   : (delegateHover.hovered ? "#17201f" : "#0b1011")
                    border.color: index === playlistModel.currentIndex ? "#237da3" : "#1a2926"
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        onClicked: playlistModel.currentIndex = index
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 9
                        anchors.rightMargin: 7
                        spacing: 9

                        Rectangle {
                            width: 24; height: 24; radius: 4
                            color: index === playlistModel.currentIndex ? "#0c4e6b" : "#121b1d"
                            border.color: index === playlistModel.currentIndex ? theme.cyan : "#293b3a"
                            Text {
                                anchors.centerIn: parent
                                text: (index + 1 < 10 ? "0" : "") + (index + 1)
                                color: index === playlistModel.currentIndex ? theme.cyanBright : theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 8
                                font.weight: Font.Bold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                Layout.fillWidth: true
                                text: model.title
                                color: index === playlistModel.currentIndex ? theme.text : theme.textSoft
                                font.family: theme.uiFont
                                font.weight: index === playlistModel.currentIndex ? Font.DemiBold : Font.Normal
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: model.artist + (model.album.length > 0 && model.album !== "Unknown Album" ? "  •  " + model.album : "")
                                color: theme.textMuted
                                font.family: theme.technicalFont
                                font.pixelSize: 7
                                elide: Text.ElideRight
                            }
                        }

                        Button {
                            implicitWidth: 25
                            implicitHeight: 25
                            text: "×"
                            hoverEnabled: true
                            onClicked: playlistModel.removeTrack(index)
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? theme.red : theme.textMuted
                                font.family: theme.uiFont
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 4
                                color: parent.hovered ? "#2b1820" : "transparent"
                                border.color: parent.hovered ? "#6b2d3d" : "transparent"
                            }
                        }
                    }

                    HoverHandler { id: delegateHover }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "COUNT  " + playlistModel.count.toString().padStart(2, "0") + "  TRACKS"
                color: theme.textMuted
                font.family: theme.technicalFont
                font.pixelSize: 8
                font.letterSpacing: 0.8
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "44.1 kHz  •  STEREO"
                color: "#49656c"
                font.family: theme.technicalFont
                font.pixelSize: 7
                font.letterSpacing: 0.7
            }
            HifiButton {
                text: root.isExpanded ? "RESTORE" : "EXPAND"
                iconSymbol: root.isExpanded ? "↙" : "↗"
                isCompact: true
                isPrimary: root.isExpanded
                Layout.leftMargin: 8
                onClicked: root.isExpanded = !root.isExpanded
            }
        }
    }
}

