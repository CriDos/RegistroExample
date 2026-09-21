pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: banner
    height: 0
    visible: height > 0
    color: Theme.errorColor
    z: 100
    clip: true

    property alias text: label.text

    function showError(message) {
        label.text = message;
        height = 44;
        timer.restart();
    }

    Text {
        id: label
        anchors.centerIn: parent
        color: Theme.onError
        font.pixelSize: Theme.fontSizeLg
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    Timer {
        id: timer
        interval: 5000
        onTriggered: banner.height = 0
    }

    Behavior on height {
        NumberAnimation {
            duration: 180
        }
    }
}