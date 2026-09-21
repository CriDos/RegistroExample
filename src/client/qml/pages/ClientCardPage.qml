pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "format.js" as Format

Page {
    id: root

    property int clientId: 0
    property var initialData: ({})
    property StackView stackView

    RegularExpressionValidator {
        id: emailCharFilter
        regularExpression: /^[\w.!#$%&'*+\-/=?^_`{|}~@а-яА-ЯёЁ]*$/
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: saveButtonTop.enabled
        onActivated: root.saveClientAndNotes()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: root.stackView.pop(StackView.Immediate)
    }

    header: ToolBar {
        width: root.width

        background: Rectangle {
            color: palette.window
            border.color: Theme.tableBorder
            border.width: 1
        }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Button {
                text: qsTr("← Назад")
                implicitHeight: 32
                font.pixelSize: Theme.fontSizeSm
                onClicked: root.stackView.pop(StackView.Immediate)
            }

            Rectangle {
                implicitWidth: 1
                implicitHeight: 20
                color: Theme.tableBorder
            }

            ColumnLayout {
                spacing: 2

                RowLayout {
                    spacing: 8

                    Text {
                        text: root.clientId > 0 ? qsTr("Карточка клиента #%1").arg(root.clientId) : qsTr("Новый клиент")
                        font.pixelSize: Theme.fontSizeXl
                        font.bold: true
                        color: palette.windowText
                    }

                    StatusBadge {
                        visible: root.clientId > 0
                        status: statusBox.currentValue
                        pixelSize: Theme.fontSizeSm
                    }
                }

                Text {
                    text: root.clientId > 0 ? qsTr("Создан: %1   •   Обновлен: %2").arg(Format.formatDate(root.initialData.createdAt)).arg(Format.formatDate(root.initialData.updatedAt)) : qsTr("Заполните реквизиты для добавления нового клиента")
                    font.pixelSize: Theme.fontSizeXs
                    color: Theme.textSecondary
                }
            }

            Item {
                Layout.fillWidth: true
            }

            BusyIndicator {
                id: savingIndicator
                running: false
                visible: running
                implicitWidth: 24
                implicitHeight: 24
            }

            Button {
                text: qsTr("Удалить")
                visible: root.clientId > 0 && Api.isAdmin
                enabled: !savingIndicator.running
                implicitHeight: 32
                font.pixelSize: Theme.fontSizeSm
                onClicked: confirmDelete.open()
            }

            Button {
                id: saveButtonTop
                text: qsTr("Сохранить (Ctrl+S)")
                highlighted: true
                enabled: fullNameField.text.trim().length > 0 && !savingIndicator.running && notesArea.text.length <= Limits.noteMax
                implicitHeight: 32
                font.bold: true
                font.pixelSize: Theme.fontSizeSm
                onClicked: root.saveClientAndNotes()
            }
        }
    }

    Dialog {
        id: confirmDelete
        title: qsTr("Удалить клиента?")
        modal: true
        standardButtons: Dialog.Yes | Dialog.No
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        onAccepted: Api.deleteClient(root.clientId)
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 16
        radius: 6
        color: palette.base
        border.color: Theme.tableBorder
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 18

            ColumnLayout {
                Layout.fillHeight: true
                Layout.preferredWidth: Math.min(480, Math.max(360, parent.width * 0.40))
                Layout.maximumWidth: 520
                spacing: 12

                Label {
                    text: qsTr("Основные сведения")
                    font.pixelSize: Theme.fontSizeLg
                    font.bold: true
                    color: palette.windowText
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.tableBorder
                }

                FormField {
                    label: qsTr("ФИО")
                    counterText: qsTr("%1 / %2").arg(fullNameField.text.length).arg(Limits.fullNameMax)

                    TextField {
                        id: fullNameField
                        text: root.initialData.fullName ?? ""
                        placeholderText: qsTr("Иванов Иван Иванович")
                        maximumLength: Limits.fullNameMax
                        Layout.fillWidth: true
                        selectByMouse: true
                        implicitHeight: 34
                        font.pixelSize: Theme.fontSizeMd
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FormField {
                        label: qsTr("Телефон")
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        MaskedTextField {
                            id: phoneField
                            text: root.initialData.phone ?? ""
                            mask: "+7 (000) 000-00-00;_"
                            maximumLength: Limits.phoneMax
                            Layout.fillWidth: true
                            onEditingFinished: phoneField.text = Format.normalizePhone(phoneField.text)
                        }
                    }

                    FormField {
                        label: qsTr("Email")
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        MaskedTextField {
                            id: emailField
                            text: root.initialData.email ?? ""
                            placeholderText: "client@example.com"
                            validator: emailCharFilter
                            maximumLength: Limits.emailMax
                            Layout.fillWidth: true
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FormField {
                        label: qsTr("Организация")
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1.3

                        TextField {
                            id: orgField
                            text: root.initialData.org ?? ""
                            placeholderText: qsTr("ООО «Компания»")
                            maximumLength: Limits.orgMax
                            Layout.fillWidth: true
                            selectByMouse: true
                            implicitHeight: 34
                            font.pixelSize: Theme.fontSizeMd
                        }
                    }

                    FormField {
                        label: qsTr("Статус")
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1

                        ComboBox {
                            id: statusBox
                            Layout.fillWidth: true
                            implicitHeight: 34
                            model: [
                                {
                                    value: Limits.statusActive,
                                    label: qsTr("Активен")
                                },
                                {
                                    value: Limits.statusArchived,
                                    label: qsTr("В архиве")
                                }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: (root.initialData.status ?? Limits.statusActive) === Limits.statusArchived ? 1 : 0
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 1
                color: Theme.tableBorder
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Заметки о клиенте")
                        font.pixelSize: Theme.fontSizeLg
                        font.bold: true
                        color: palette.windowText
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("%1 / %2 симв.").arg(notesArea.text.length).arg(Limits.noteMax)
                        font.pixelSize: Theme.fontSizeXs
                        font.bold: notesArea.text.length > Limits.noteMax
                        color: notesArea.text.length > Limits.noteMax ? Theme.errorColor : Theme.textSecondary
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.tableBorder
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: palette.base
                    border.color: notesArea.activeFocus ? Theme.primaryColor : Theme.tableBorder
                    border.width: 1
                    clip: true

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 2
                        clip: true

                        TextArea {
                            id: notesArea
                            text: root.initialData.notes ?? ""
                            placeholderText: qsTr("Введите служебные заметки и историю взаимодействия с клиентом…")
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            verticalAlignment: TextEdit.AlignTop
                            font.pixelSize: Theme.fontSizeMd
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 10
                            bottomPadding: 10
                            background: null
                        }
                    }
                }

                Label {
                    text: qsTr("Заметки сохраняются автоматически при сохранении карточки клиента")
                    font.pixelSize: Theme.fontSizeXs
                    color: Theme.textSecondary
                }
            }
        }
    }

    function saveClientAndNotes() {
        if (fullNameField.text.trim().length === 0)
            return;

        phoneField.text = Format.normalizePhone(phoneField.text);

        savingIndicator.running = true;
        Api.saveClient(root.clientId, fullNameField.text.trim(), orgField.text.trim(), phoneField.text.trim(), emailField.text.trim(), notesArea.text.trim(), statusBox.currentValue);
    }

    Connections {
        target: Api
        function onClientSaved(savedId, client) {
            root.stackView.pop(StackView.Immediate);
        }
        function onClientDeleted(id) {
            root.stackView.pop(StackView.Immediate);
        }
        function onErrorOccurred(message) {
            savingIndicator.running = false;
        }
        function onNetworkErrorOccurred(message) {
            savingIndicator.running = false;
        }
    }
}
