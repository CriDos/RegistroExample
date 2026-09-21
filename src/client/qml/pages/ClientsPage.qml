pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "format.js" as Format

Page {
    id: root

    property StackView stackView

    Component {
        id: cardPage
        ClientCardPage {}
    }

    QtObject {
        id: tableMetrics
        readonly property real contentW: list.width
        readonly property real idW: 48
        readonly property real phoneW: 120
        readonly property real statusW: 90
        readonly property real dateW: 105
        readonly property real actionW: 34
        readonly property real spacing: 8
        readonly property real marginLeft: 10
        readonly property real marginRight: 26
        readonly property real fixedW: idW + phoneW + statusW + dateW + actionW + 7 * spacing + marginLeft + marginRight
        readonly property real flexW: contentW - fixedW
        readonly property real nameW: Math.max(240, flexW * 0.44)
        readonly property real orgW: Math.max(140, flexW * 0.28)
        readonly property real emailW: Math.max(150, flexW * 0.28)
    }

    function pushCard(clientId, data) {
        stackView.push(cardPage, {
            clientId: clientId,
            initialData: data,
            stackView: stackView
        }, StackView.Immediate);
    }

    AboutDialog {
        id: aboutDialog
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: searchField
                placeholderText: qsTr("Поиск по ФИО, организации, телефону, email…")
                Layout.fillWidth: true
                implicitHeight: 34
                selectByMouse: true
                font.pixelSize: Theme.fontSizeMd
                onTextEdited: ClientModel.search = text

                Button {
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 22
                    height: 22
                    flat: true
                    visible: searchField.text.length > 0
                    text: "✕"
                    font.pixelSize: Theme.fontSizeXs
                    onClicked: {
                        searchField.text = "";
                        ClientModel.search = "";
                    }
                }
            }

            ComboBox {
                id: statusBox
                implicitHeight: 34
                implicitWidth: 130
                model: [
                    {
                        value: "",
                        label: qsTr("Все статусы")
                    },
                    {
                        value: Limits.statusActive,
                        label: qsTr("Активные")
                    },
                    {
                        value: Limits.statusArchived,
                        label: qsTr("В архиве")
                    }
                ]
                textRole: "label"
                valueRole: "value"
                onActivated: ClientModel.statusFilter = currentValue
            }

            Button {
                id: newClientBtn
                text: qsTr("+ Новый клиент")
                highlighted: true
                implicitHeight: 34
                font.bold: true
                font.pixelSize: Theme.fontSizeSm
                onClicked: root.pushCard(0, {})
            }

            Button {
                id: refreshButton
                text: qsTr("Обновить")
                implicitHeight: 34
                font.pixelSize: Theme.fontSizeSm
                onClicked: ClientModel.reload()
            }

            Button {
                id: aboutButton
                text: qsTr("О программе")
                implicitHeight: 34
                font.pixelSize: Theme.fontSizeSm
                onClicked: aboutDialog.open()
            }

            Button {
                id: logoutButton
                text: qsTr("Выход")
                implicitHeight: 34
                font.pixelSize: Theme.fontSizeSm
                onClicked: Api.logout()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: ClientModel.search.length > 0 || ClientModel.statusFilter.length > 0 ? qsTr("Найдено: %1").arg(ClientModel.total) : qsTr("Всего клиентов: %1").arg(ClientModel.total)
                font.pixelSize: Theme.fontSizeSm
                font.bold: true
                color: Theme.textSecondary
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                visible: ClientModel.total > 0
                text: qsTr("Двойной клик для просмотра карточки")
                font.pixelSize: Theme.fontSizeXs
                color: Theme.textSecondary
            }
        }

        Rectangle {
            id: tableHeader
            Layout.fillWidth: true
            implicitHeight: 34
            color: Theme.tableHeaderBg
            border.color: Theme.tableBorder
            border.width: 1
            radius: 4

            Row {
                anchors.fill: parent
                anchors.leftMargin: tableMetrics.marginLeft
                anchors.rightMargin: tableMetrics.marginRight
                spacing: tableMetrics.spacing

                Label {
                    width: tableMetrics.idW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("№")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                }

                Label {
                    width: tableMetrics.nameW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("ФИО")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    width: tableMetrics.orgW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Организация")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    width: tableMetrics.phoneW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Телефон")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                }

                Label {
                    width: tableMetrics.emailW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Email")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }

                Label {
                    width: tableMetrics.statusW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Статус")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                }

                Label {
                    width: tableMetrics.dateW
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    text: qsTr("Изменён")
                    font.bold: true
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                }

                Item {
                    width: tableMetrics.actionW
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: palette.base
            border.color: Theme.tableBorder
            border.width: 1
            radius: 4
            clip: true

            ListView {
                id: list
                anchors.fill: parent
                model: ClientModel
                spacing: 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 600
                reuseItems: true

                ScrollBar.vertical: ScrollBar {
                    id: verticalScrollBar
                    width: 14
                    policy: ScrollBar.AlwaysOn

                    contentItem: Rectangle {
                        implicitWidth: 14
                        implicitHeight: 24
                        radius: 3
                        color: verticalScrollBar.pressed ? Theme.scrollBarPressed : verticalScrollBar.hovered ? Theme.scrollBarHover : Theme.scrollBarIdle
                    }

                    background: Rectangle {
                        color: Theme.scrollBarTrack
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: ClientModel.busy && ClientModel.total === 0
                    visible: running
                }

                delegate: Rectangle {
                    id: rowItem
                    required property var model
                    required property int index
                    width: list.width
                    height: 36
                    color: list.currentIndex === rowItem.index ? Theme.tableRowSelected : (rowHover.hovered ? Theme.tableRowHover : (rowItem.index % 2 === 1 ? Theme.tableRowAlt : palette.base))

                    border.color: Theme.tableBorder
                    border.width: 1

                    HoverHandler {
                        id: rowHover
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: tableMetrics.marginLeft
                        anchors.rightMargin: tableMetrics.marginRight
                        spacing: tableMetrics.spacing

                        Text {
                            width: tableMetrics.idW
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.model.id
                            font.pixelSize: Theme.fontSizeSm
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Text {
                            width: tableMetrics.nameW
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.model.fullName
                            font.pixelSize: Theme.fontSizeMd
                            font.bold: true
                            color: palette.windowText
                            elide: Text.ElideRight
                        }

                        Text {
                            width: tableMetrics.orgW
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.model.org && rowItem.model.org.length > 0 ? rowItem.model.org : "—"
                            font.pixelSize: Theme.fontSizeSm
                            color: rowItem.model.org && rowItem.model.org.length > 0 ? palette.windowText : Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Text {
                            width: tableMetrics.phoneW
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.model.phone && rowItem.model.phone.length > 0 ? rowItem.model.phone : "—"
                            font.pixelSize: Theme.fontSizeSm
                            color: rowItem.model.phone && rowItem.model.phone.length > 0 ? palette.windowText : Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Text {
                            width: tableMetrics.emailW
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.model.email && rowItem.model.email.length > 0 ? rowItem.model.email : "—"
                            font.pixelSize: Theme.fontSizeSm
                            color: rowItem.model.email && rowItem.model.email.length > 0 ? palette.windowText : Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Item {
                            width: tableMetrics.statusW
                            height: parent.height
                            anchors.verticalCenter: parent.verticalCenter

                            StatusBadge {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                status: rowItem.model.status
                            }
                        }

                        Text {
                            width: tableMetrics.dateW
                            anchors.verticalCenter: parent.verticalCenter
                            text: Format.formatDate(rowItem.model.updatedAt || rowItem.model.createdAt)
                            font.pixelSize: Theme.fontSizeXs
                            color: Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        Button {
                            width: tableMetrics.actionW
                            anchors.verticalCenter: parent.verticalCenter
                            implicitHeight: 24
                            text: "✎"
                            font.pixelSize: Theme.fontSizeSm
                            flat: true
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Открыть карточку")
                            onClicked: {
                                list.currentIndex = rowItem.index;
                                root.pushCard(rowItem.model.id, ClientModel.clientAt(rowItem.index));
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: -1
                        onClicked: {
                            list.currentIndex = rowItem.index;
                        }
                        onDoubleClicked: {
                            list.currentIndex = rowItem.index;
                            root.pushCard(rowItem.model.id, ClientModel.clientAt(rowItem.index));
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    visible: !ClientModel.busy && ClientModel.total === 0
                    spacing: 8

                    Text {
                        text: qsTr("Клиенты не найдены")
                        font.pixelSize: Theme.fontSizeXxl
                        font.bold: true
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: searchField.text.length > 0 ? qsTr("Попробуйте изменить поисковый запрос или сбросить фильтр.") : qsTr("Нажмите «+ Новый клиент», чтобы добавить первую запись.")
                        font.pixelSize: Theme.fontSizeSm
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: ClientModel.pageCount > 0
            spacing: 6

            Label {
                text: qsTr("Страница")
                font.pixelSize: Theme.fontSizeSm
                color: Theme.textSecondary
            }

            Button {
                text: "⟪"
                implicitHeight: 30
                implicitWidth: 34
                font.pixelSize: Theme.fontSizeSm
                enabled: ClientModel.currentPage > 1 && !ClientModel.busy
                onClicked: ClientModel.firstPage()
            }

            Button {
                text: "◀"
                implicitHeight: 30
                implicitWidth: 34
                font.pixelSize: Theme.fontSizeSm
                enabled: ClientModel.currentPage > 1 && !ClientModel.busy
                onClicked: ClientModel.prevPage()
            }

            TextField {
                id: pageField
                implicitWidth: 55
                implicitHeight: 30
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator {
                    bottom: 1
                    top: Math.max(1, ClientModel.pageCount)
                }
                onAccepted: {
                    const p = parseInt(text, 10);
                    if (!Number.isNaN(p))
                        ClientModel.goToPage(p);
                }

                Binding {
                    target: pageField
                    property: "text"
                    value: ClientModel.currentPage
                    when: !pageField.activeFocus
                }
            }

            Label {
                text: qsTr("из %1").arg(ClientModel.pageCount)
                font.pixelSize: Theme.fontSizeSm
                color: Theme.textSecondary
            }

            Button {
                text: "▶"
                implicitHeight: 30
                implicitWidth: 34
                font.pixelSize: Theme.fontSizeSm
                enabled: ClientModel.currentPage < ClientModel.pageCount && !ClientModel.busy
                onClicked: ClientModel.nextPage()
            }

            Button {
                text: "⟫"
                implicitHeight: 30
                implicitWidth: 34
                font.pixelSize: Theme.fontSizeSm
                enabled: ClientModel.currentPage < ClientModel.pageCount && !ClientModel.busy
                onClicked: ClientModel.lastPage()
            }
        }
    }

    Connections {
        target: Api
        function onClientCreated(id) {
            ClientModel.reloadFromFirstPage();
        }
        function onClientSaved(id, client) {
            ClientModel.reload();
        }
        function onClientDeleted(id) {
            ClientModel.reload();
        }
    }

    Component.onCompleted: ClientModel.reload()
}
