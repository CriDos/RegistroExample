pragma ComponentBehavior: Bound
import QtQuick

Rectangle {
    id: root

    required property string status
    property int pixelSize: Theme.fontSizeXs

    readonly property bool active: root.status === Limits.statusActive

    radius: 4
    implicitWidth: statusText.implicitWidth + 12
    implicitHeight: 20
    color: root.active ? Theme.statusActiveBg : Theme.statusArchivedBg

    Text {
        id: statusText
        anchors.centerIn: parent
        text: root.active ? qsTr("Активен") : qsTr("В архиве")
        color: root.active ? Theme.statusActiveFg : Theme.statusArchivedFg
        font.pixelSize: root.pixelSize
        font.bold: true
    }
}
