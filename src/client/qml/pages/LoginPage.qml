pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    objectName: "login"

    property bool attempted: false
    property bool switchingServer: false
    // Injected by Main.qml (stack.replace properties): the shared top error
    // banner, so validation and login errors use the same strip as other screens.
    property ErrorBanner banner: null

    Rectangle {
        anchors.fill: parent
        color: palette.window
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(380, parent.width - 32)
        implicitHeight: contentCol.implicitHeight + 48
        radius: 10
        color: palette.base
        border.color: Theme.tableBorder
        border.width: 1

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                Text {
                    text: qsTr("Registro")
                    font.pixelSize: Theme.fontSizeHero
                    font.bold: true
                    color: palette.windowText
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: qsTr("Вход в справочник клиентов")
                    font.pixelSize: Theme.fontSizeMd
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Theme.tableBorder
                Layout.topMargin: 4
                Layout.bottomMargin: 4
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Сервер:")
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }

                TextField {
                    id: serverField
                    text: Config.serverUrl
                    placeholderText: "http://127.0.0.1:9080"
                    Layout.fillWidth: true
                    implicitHeight: 40
                    horizontalAlignment: TextInput.AlignHCenter
                    selectByMouse: true
                    onAccepted: userField.forceActiveFocus()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Логин:")
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }

                TextField {
                    id: userField
                    placeholderText: qsTr("Имя пользователя")
                    Layout.fillWidth: true
                    implicitHeight: 40
                    horizontalAlignment: TextInput.AlignHCenter
                    selectByMouse: true
                    onAccepted: passField.forceActiveFocus()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: qsTr("Пароль:")
                    font.pixelSize: Theme.fontSizeSm
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }

                TextField {
                    id: passField
                    placeholderText: qsTr("Пароль")
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                    implicitHeight: 40
                    horizontalAlignment: TextInput.AlignHCenter
                    selectByMouse: true
                    onAccepted: root.submit()
                }
            }

            CheckBox {
                id: rememberCheck
                text: qsTr("Запомнить меня")
                checked: Config.remember
                Layout.alignment: Qt.AlignHCenter
            }

            Button {
                id: submitButton
                highlighted: true
                enabled: userField.text.length > 0 && passField.text.length > 0 && !Api.loginInProgress
                implicitWidth: 160
                implicitHeight: 38
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                onClicked: root.submit()

                contentItem: Item {
                    opacity: submitButton.enabled ? 1 : 0.35

                    Text {
                        anchors.centerIn: parent
                        visible: !Api.loginInProgress
                        text: qsTr("Войти")
                        font.pixelSize: Theme.fontSizeLg
                        font.bold: true
                        color: submitButton.palette.buttonText
                    }

                    BusyIndicator {
                        anchors.centerIn: parent
                        implicitWidth: 16
                        implicitHeight: 16
                        visible: Api.loginInProgress
                        running: Api.loginInProgress
                    }
                }
            }
        }
    }

    function submit() {
        if (Api.loginInProgress)
            return;
        if (userField.text.trim().length === 0 || passField.text.length === 0) {
            if (banner)
                banner.showError(qsTr("Введите логин и пароль."));
            return;
        }
        root.attempted = true;
        if (serverField.text.trim().length > 0) {
            Config.serverUrl = serverField.text;
            Config.save();
            root.switchingServer = true;
            Api.setBaseUrl(Config.serverUrl);
            root.switchingServer = false;
        }
        Api.login(userField.text, passField.text);
    }

    Component.onCompleted: {
        userField.text = Config.lastUser;
        if (Config.demo && !root.attempted) {
            userField.text = "demo";
            passField.text = "demo";
        }
    }

    Connections {
        target: Api
        function onAuthenticationNeeded() {
            if (root.attempted && !root.switchingServer && root.banner)
                root.banner.showError(qsTr("Неверный логин или пароль."));
        }
        function onSessionChanged() {
            if (!Api.loggedIn || !root.attempted)
                return;
            Config.lastUser = Api.userName;
            Config.remember = rememberCheck.checked;
            Config.token = rememberCheck.checked ? Api.token : "";
            Config.save();
        }
    }
}

