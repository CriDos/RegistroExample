pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    width: 960
    height: 600
    minimumWidth: 1060
    minimumHeight: 480
    visible: true
    title: Config.demo ? qsTr("Registro %1 — справочник клиентов (демо-режим)").arg(Qt.application.version) : qsTr("Registro %1 — справочник клиентов").arg(Qt.application.version)

    Rectangle {
        id: background
        anchors.fill: parent
        color: palette.window
    }

    ErrorBanner {
        id: banner
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: splash

        pushEnter: Transition {}
        pushExit: Transition {}
        popEnter: Transition {}
        popExit: Transition {}
        replaceEnter: Transition {}
        replaceExit: Transition {}
    }

    Component {
        id: splash
        Item {
            BusyIndicator {
                anchors.centerIn: parent
                running: true
            }
        }
    }

    Component {
        id: loginPage
        LoginPage {}
    }

    Component {
        id: clientsPage
        ClientsPage {}
    }

    function isLoginPage() {
        return stack.currentItem && stack.currentItem.objectName === "login";
    }

    Connections {
        target: Api
        function onErrorOccurred(message) {
            banner.showError(message);
        }
        function onNetworkErrorOccurred(message) {
            banner.showError(qsTr("Нет связи с сервером: %1").arg(message));
        }
        function onSessionChanged() {
            if (Api.checking)
                return;
            if (Api.loggedIn) {
                stack.replace(clientsPage, {
                    stackView: stack
                });
            } else if (!root.isLoginPage())
                stack.replace(loginPage, {
                    banner: banner
                });
        }
        function onAuthenticationNeeded() {
            Api.logout();
            if (!root.isLoginPage())
                stack.replace(loginPage, {
                    banner: banner
                });
        }
    }
}
