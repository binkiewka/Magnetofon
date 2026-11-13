import QtQuick 2.15
import QtTest 1.2
import "../../ui/qml"
Item {
    width: 640; height: 500
    Knob { id: volume; x: 10; y: 10; value: 0.5; onValueChangedByUser: function(v) { value = v } }
    EqSlider { id: band; x: 180; y: 20; value: 0; onValueChangedByUser: function(v) { value = v } }
    ProgramMonitorPanel { id: monitor; x: 10; y: 180; width: 600; height: 190; fallbackArtworkUrl: Qt.resolvedUrl("../../../resources/icon.png"); artworkUrl: Qt.resolvedUrl("../../../resources/icon.png"); duration: 120; position: 30; onSeekRequested: function(v) { position = v } }
    CassetteDeck { id: tape; x: 10; y: 380; width: 600; height: 100 }
    TestCase {
        name: "AccessibleSoundControls"
        when: windowShown
        function test_keyboardVolumeEqAndSeek() {
            volume.forceActiveFocus()
            keyClick(Qt.Key_Up)
            fuzzyCompare(volume.value, 0.52, 0.0001)
            keyClick(Qt.Key_Home)
            compare(volume.value, 0)
            band.forceActiveFocus()
            keyClick(Qt.Key_Right)
            compare(band.value, 0.5)
            keyClick(Qt.Key_Home)
            compare(band.value, 0)
            var seek = findChild(monitor, "seekBar")
            verify(seek)
            seek.forceActiveFocus()
            keyClick(Qt.Key_Right)
            compare(monitor.position, 35)
            keyClick(Qt.Key_Home)
            compare(monitor.position, 0)
        }
        function test_cassetteKeepsPhysicalAspectRatio() {
            var body = findChild(tape, "cassetteBody")
            verify(body)
            fuzzyCompare(body.width / body.height, 2.65, 0.001)
            verify(body.width <= tape.width && body.height <= tape.height)
        }
    }
}
