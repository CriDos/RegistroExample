pragma ComponentBehavior: Bound
import QtQuick

FocusScope {
    id: root

    property alias text: input.text
    property alias validator: input.validator
    property alias inputMethodHints: input.inputMethodHints
    property alias selectByMouse: input.selectByMouse
    property string mask: ""
    property string placeholderText: ""
    property int maximumLength: 32767

    signal editingFinished
    signal textEdited

    implicitWidth: 200
    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: Theme.inputBg
        border.width: 1
        border.color: input.activeFocus ? Theme.primaryColor : Theme.tableBorder
    }

    Text {
        visible: root.mask.length === 0 && root.placeholderText.length > 0 && input.text.length === 0 && !input.activeFocus
        text: root.placeholderText
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeMd
        elide: Text.ElideRight
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: Text.AlignVCenter
    }

    TextInput {
        id: input
        focus: true
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        color: Theme.inputText
        selectionColor: Theme.primaryColor
        selectedTextColor: Theme.inputSelectionText
        inputMask: root.mask
        maximumLength: root.maximumLength
        font.pixelSize: Theme.fontSizeMd

        onEditingFinished: root.editingFinished()
        onTextEdited: root.textEdited()
    }
}
