pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


ColumnLayout {
    id: root

    property string label: ""
    property string counterText: ""
    property bool counterCritical: false

    default property alias content: field.data

    spacing: 3
    Layout.fillWidth: true

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: root.label
            font.pixelSize: Theme.fontSizeSm
            font.bold: true
            color: Theme.textSecondary
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            visible: root.counterText.length > 0
            text: root.counterText
            font.pixelSize: Theme.fontSizeXs
            font.bold: root.counterCritical
            color: root.counterCritical ? Theme.errorColor : Theme.textSecondary
        }
    }

    ColumnLayout {
        id: field
        Layout.fillWidth: true
        spacing: 0
    }
}
