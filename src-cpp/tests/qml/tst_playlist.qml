import QtQuick 2.15
import QtTest 1.2
import "../../ui/qml"
Item {
    width: 480; height: 300
    ListModel {
        id: playlistModel
        property int currentIndex: 0
        property int moveCount: 0
        function moveTrack(from, to) {
            move(from, to, 1)
            moveCount++
            if (currentIndex === from) currentIndex = to
            else if (from < currentIndex && to >= currentIndex) currentIndex--
            else if (from > currentIndex && to <= currentIndex) currentIndex++
        }
        function removeTrack(index) { remove(index) }
    }
    PlaylistPanel { id: panel; anchors.fill: parent }
    TestCase {
        name: "PlaylistInteractions"
        when: windowShown
        function test_scrollAndReorder() {
            for (var i = 0; i < 30; i++)
                playlistModel.append({title: "Track " + i, artist: "Artist", album: "Continuous Album"})
            verify(waitForRendering(panel))
            var view = findChild(panel, "playlistView")
            mouseWheel(view, 120, 80, 0, -120)
            tryVerify(function() { return view.contentY > 0 })
            compare(playlistModel.moveCount, 0)
            view.contentY = 0
            mouseDrag(view, 120, 120, 0, -90)
            tryVerify(function() { return view.contentY > 0 })
            compare(playlistModel.moveCount, 0)
            view.cancelFlick()
            view.contentY = 0
            wait(60)
            mouseClick(view, 100, 62)
            compare(view.currentIndex, 1)
            compare(playlistModel.currentIndex, 0)
            keyClick(Qt.Key_Down)
            compare(view.currentIndex, 2)
            compare(playlistModel.get(2).title, "Track 1")
            compare(playlistModel.currentIndex, 0)
            keyClick(Qt.Key_Up)
            compare(view.currentIndex, 1)
            compare(playlistModel.get(1).title, "Track 1")
            mouseClick(findChild(panel, "moveDown1"))
            compare(view.currentIndex, 2)
            compare(playlistModel.get(2).title, "Track 1")
            compare(playlistModel.currentIndex, 0)
            keyClick(Qt.Key_Return)
            compare(playlistModel.currentIndex, 2)
        }
    }
}
