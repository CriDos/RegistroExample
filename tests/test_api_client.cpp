#include "api_client.h"
#include "api_server.h"
#include "client.h"

#include <QHostAddress>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

using namespace registro;

class TestApiClient final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void openModeFlow()
    {
        ApiServer server;
        QVERIFY(server.start(0, m_dir.filePath(QStringLiteral("client-open.db"))));
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())));

        QSignalSpy sessionSpy(&api, &ApiClient::sessionChanged);
        QSignalSpy authSpy(&api, &ApiClient::authenticationNeeded);
        api.initialize();
        QVERIFY(sessionSpy.wait(3000));
        QVERIFY(api.loggedIn());
        QVERIFY(!api.checking());
        QVERIFY(api.isAdmin());
        QCOMPARE(authSpy.count(), 0);

        QSignalSpy savedSpy(&api, &ApiClient::clientSaved);
        api.saveClient(0, QStringLiteral("Клиент Один"), QStringLiteral("ООО Тест"),
                       QStringLiteral("+7 900 000-00-01"), QStringLiteral("one@example.test"),
                       QStringLiteral("первая заметка"), QStringLiteral("active"));
        QVERIFY(savedSpy.wait(3000));
        const QList<QVariant> createArgs = savedSpy.takeFirst();
        const qint64 id = createArgs.at(0).toLongLong();
        QVERIFY(id > 0);
        QCOMPARE(createArgs.at(1).value<Client>().fullName, QStringLiteral("Клиент Один"));
        QCOMPARE(createArgs.at(1).value<Client>().notes, QStringLiteral("первая заметка"));

        QSignalSpy updateSpy(&api, &ApiClient::clientSaved);
        api.saveClient(id, QStringLiteral("Клиент Один Изменён"), QStringLiteral("ООО Тест"),
                       QString(), QStringLiteral("one@example.test"),
                       QStringLiteral("вторая заметка\n\nтретья"), QStringLiteral("archived"));
        QVERIFY(updateSpy.wait(3000));
        const QList<QVariant> updateArgs = updateSpy.takeFirst();
        QCOMPARE(updateArgs.at(0).toLongLong(), id);
        QCOMPARE(updateArgs.at(1).value<Client>().fullName, QStringLiteral("Клиент Один Изменён"));
        QCOMPARE(updateArgs.at(1).value<Client>().notes,
                 QStringLiteral("вторая заметка\n\nтретья"));

        QSignalSpy listSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QString(), QString(), 0);
        QVERIFY(listSpy.wait(3000));
        const QList<QVariant> args = listSpy.takeFirst();
        QCOMPARE(args.at(0).toLongLong(), 1);
        const QVector<Client> items = args.at(1).value<QVector<Client>>();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.at(0).fullName, QStringLiteral("Клиент Один Изменён"));
        QCOMPARE(items.at(0).status, QStringLiteral("archived"));
        QCOMPARE(items.at(0).notes, QStringLiteral("вторая заметка\n\nтретья"));
        QVERIFY(!items.at(0).updatedAt.isEmpty());

        QSignalSpy searchSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QStringLiteral("Один"), QString(), 0);
        QVERIFY(searchSpy.wait(3000));
        QCOMPARE(searchSpy.takeFirst().at(0).toLongLong(), 1);
        QSignalSpy noneSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QStringLiteral("несуществующий"), QString(), 0);
        QVERIFY(noneSpy.wait(3000));
        QCOMPARE(noneSpy.takeFirst().at(0).toLongLong(), 0);

        QSignalSpy errorSpy(&api, &ApiClient::errorOccurred);
        api.saveClient(0, QStringLiteral(" "), QString(), QString(), QString(), QString(),
                       QStringLiteral("active"));
        QVERIFY(errorSpy.wait(3000));
        QVERIFY(errorSpy.takeFirst().at(0).toString().contains(QStringLiteral("full_name")));

        api.deleteClient(999999);
        QVERIFY(errorSpy.wait(3000));
        QVERIFY(errorSpy.takeFirst().at(0).toString().contains(QStringLiteral("not found")));

        QSignalSpy deletedSpy(&api, &ApiClient::clientDeleted);
        api.deleteClient(id);
        QVERIFY(deletedSpy.wait(3000));
    }

    void trailingSlashBaseUrlNormalized()
    {
        ApiServer server;
        QVERIFY(server.start(0, m_dir.filePath(QStringLiteral("client-slash.db"))));
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(server.port())));

        QSignalSpy sessionSpy(&api, &ApiClient::sessionChanged);
        api.initialize();
        QVERIFY(sessionSpy.wait(3000));
        QVERIFY(api.loggedIn());

        QSignalSpy listSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QString(), QString(), 0);
        QVERIFY(listSpy.wait(3000));
        QCOMPARE(listSpy.takeFirst().at(0).toLongLong(), 0);
    }

    void authModeLogin()
    {
        ApiServer server;
        QVERIFY(server.start(0, m_dir.filePath(QStringLiteral("client-auth.db")),
                             QStringLiteral("admin"), QStringLiteral("secret123")));
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())));

        QSignalSpy authSpy(&api, &ApiClient::authenticationNeeded);
        api.initialize();
        QVERIFY(authSpy.wait(3000));
        QVERIFY(!api.loggedIn());

        QSignalSpy badLoginSpy(&api, &ApiClient::authenticationNeeded);
        api.login(QStringLiteral("admin"), QStringLiteral("wrong-password"));
        QVERIFY(api.loginInProgress());
        QVERIFY(badLoginSpy.wait(3000));
        QVERIFY(!api.loggedIn());
        QVERIFY(!api.loginInProgress());

        QSignalSpy sessionSpy(&api, &ApiClient::sessionChanged);
        api.login(QStringLiteral("admin"), QStringLiteral("secret123"));
        QVERIFY(api.loginInProgress());
        QVERIFY(sessionSpy.wait(3000));
        QVERIFY(api.loggedIn());
        QVERIFY(!api.loginInProgress());
        QCOMPARE(api.userName(), QStringLiteral("admin"));
        QCOMPARE(api.userRole(), QStringLiteral("admin"));
        QVERIFY(api.isAdmin());

        QSignalSpy listSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QString(), QString(), 0);
        QVERIFY(listSpy.wait(3000));
        QCOMPARE(listSpy.takeFirst().at(0).toLongLong(), 0);

        QSignalSpy loggedOutSpy(&api, &ApiClient::sessionChanged);
        api.logout();
        QVERIFY(loggedOutSpy.count() >= 1);
        QVERIFY(!api.loggedIn());
        QSignalSpy afterLogoutSpy(&api, &ApiClient::authenticationNeeded);
        api.fetchClients(QString(), QString(), 0);
        QVERIFY(afterLogoutSpy.wait(3000));
    }

    void demoModeLogin()
    {
        ApiServer server;
        QVERIFY(server.start(0, m_dir.filePath(QStringLiteral("client-demo.db")), {}, {}, true));
        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())));

        QSignalSpy authSpy(&api, &ApiClient::authenticationNeeded);
        api.initialize();
        QVERIFY(authSpy.wait(3000));
        QVERIFY(!api.loggedIn());

        QSignalSpy sessionSpy(&api, &ApiClient::sessionChanged);
        api.login(QStringLiteral("demo"), QStringLiteral("demo"));
        QVERIFY(sessionSpy.wait(3000));
        QVERIFY(api.loggedIn());
        QCOMPARE(api.userName(), QStringLiteral("demo"));
        QCOMPARE(api.userRole(), QStringLiteral("admin"));
        QVERIFY(api.isAdmin());
        QVERIFY(!api.token().isEmpty());

        QSignalSpy listSpy(&api, &ApiClient::clientsLoaded);
        api.fetchClients(QString(), QString(), 0);
        QVERIFY(listSpy.wait(3000));
    }

    void restoreTokenSkipsLogin()
    {
        ApiServer server;
        QVERIFY(server.start(0, m_dir.filePath(QStringLiteral("client-token.db")),
                             QStringLiteral("admin"), QStringLiteral("secret123")));

        ApiClient first(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())));
        QSignalSpy firstSession(&first, &ApiClient::sessionChanged);
        first.login(QStringLiteral("admin"), QStringLiteral("secret123"));
        QVERIFY(firstSession.wait(3000));
        QVERIFY(first.loggedIn());
        const QString token = first.token();
        QVERIFY(!token.isEmpty());

        // A fresh client with the restored token never needs the login page.
        ApiClient second(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.port())));
        QSignalSpy secondSession(&second, &ApiClient::sessionChanged);
        QSignalSpy authSpy(&second, &ApiClient::authenticationNeeded);
        second.restoreToken(token);
        second.initialize();
        QVERIFY(secondSession.wait(3000));
        QVERIFY(second.loggedIn());
        QCOMPARE(second.userName(), QStringLiteral("admin"));
        QCOMPARE(authSpy.count(), 0);
    }

    void networkErrorMapping()
    {
        QTcpServer dead;
        QVERIFY(dead.listen(QHostAddress::LocalHost, 0));
        const quint16 deadPort = dead.serverPort();
        dead.close();

        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(deadPort)));
        QSignalSpy netSpy(&api, &ApiClient::networkErrorOccurred);
        QSignalSpy sessionSpy(&api, &ApiClient::sessionChanged);
        api.initialize();
        QVERIFY(netSpy.wait(5000));
        QVERIFY(!netSpy.takeFirst().at(0).toString().isEmpty());
        QCOMPARE(sessionSpy.count(), 1);
        QVERIFY(!api.checking());
        QVERIFY(!api.loggedIn());
    }

    void hungServerTimesOut()
    {
        QTcpServer hang;
        QVERIFY(hang.listen(QHostAddress::LocalHost, 0));
        QVector<QTcpSocket *> accepted;
        QObject::connect(&hang, &QTcpServer::newConnection, &hang,
                         [&hang, &accepted]() { accepted.append(hang.nextPendingConnection()); });

        ApiClient api(QUrl(QStringLiteral("http://127.0.0.1:%1").arg(hang.serverPort())));
        api.setRequestTimeout(300);

        QSignalSpy netSpy(&api, &ApiClient::networkErrorOccurred);
        api.initialize();
        QVERIFY(netSpy.wait(5000));
        QVERIFY(!netSpy.takeFirst().at(0).toString().isEmpty());
        QVERIFY(!api.checking());

        qDeleteAll(accepted);
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestApiClient)
#include "test_api_client.moc"
