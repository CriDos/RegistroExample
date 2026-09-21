pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("О программе")
    modal: true
    standardButtons: Dialog.Close
    width: 440
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    ColumnLayout {
        spacing: 10
        width: parent.width

        RowLayout {
            spacing: 14
            Layout.fillWidth: true

            Image {
                source: "registro-client.svg"
                sourceSize.width: 52
                sourceSize.height: 52
                Layout.preferredWidth: 52
                Layout.preferredHeight: 52
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true

                Text {
                    text: qsTr("Registro — справочник клиентов")
                    font.pixelSize: Theme.fontSizeTitle
                    font.bold: true
                }

                Text {
                    text: qsTr("Клиентское приложение для ведения каталога")
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textSecondary
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.tableBorder
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 6

            Label {
                text: qsTr("Версия приложения:")
                color: Theme.textSecondary
            }
            Label {
                text: Qt.application.version
                font.bold: true
            }

            Label {
                text: qsTr("Версия Qt:")
                color: Theme.textSecondary
            }
            Label {
                text: AppInfo.qtVersion
                font.bold: true
            }

            Label {
                text: qsTr("Адрес сервера:")
                color: Theme.textSecondary
            }
            Label {
                text: Api.baseUrl
                font.bold: true
            }
        }
    }
}
