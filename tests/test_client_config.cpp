#include <QtTest>

#include "config.h"

#include <QFile>
#include <QTemporaryDir>

using namespace registro;

class TestClientConfig : public QObject {
    Q_OBJECT
private slots:
    void defaults();
    void roundTrip();
    void missingFileCreatesDefaultFile();
    void garbageFileFallsBackToDefaults();
    void emptyValuesKeepPrevious();
    void saveOverwrites();
    void clearTokenErasesStoredToken();
    void demoFromConfigFile();
    void defaultPathNonEmpty();
};

void TestClientConfig::defaults()
{
    QTemporaryDir dir;
    Config s(dir.filePath(QStringLiteral("config.ini")));
    QCOMPARE(s.serverUrl(), QStringLiteral("http://127.0.0.1:9080"));
    QVERIFY(s.token().isEmpty());
    QVERIFY(s.lastUser().isEmpty());
    QVERIFY(!s.remember());
    QVERIFY(s.demo());
}

void TestClientConfig::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    {
        Config s(path);
        s.setServerUrl(QStringLiteral("http://10.0.0.5:9123"));
        s.setToken(QStringLiteral("0123456789abcdef"));
        s.setLastUser(QStringLiteral("demo"));
        s.setRemember(false);
        s.save();
    }
    Config reloaded(path);
    QCOMPARE(reloaded.serverUrl(), QStringLiteral("http://10.0.0.5:9123"));
    QCOMPARE(reloaded.token(), QStringLiteral("0123456789abcdef"));
    QCOMPARE(reloaded.lastUser(), QStringLiteral("demo"));
    QCOMPARE(reloaded.remember(), false);

    // The token is persisted as-is (plaintext, cross-platform): the raw value
    // must be readable from the config file.
    QFile stored(path);
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(stored.readAll()).contains(QByteArrayLiteral("0123456789abcdef")));
}

void TestClientConfig::missingFileCreatesDefaultFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    Config s(path);
    QCOMPARE(s.serverUrl(), QStringLiteral("http://127.0.0.1:9080"));
    QVERIFY(!s.remember());
    QVERIFY(QFile::exists(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(file.readAll()).contains(QStringLiteral("[client]")));
}

void TestClientConfig::garbageFileFallsBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("\x00\xFF\x9Cgarbage\x80"));
    file.close();

    Config s(path);
    QCOMPARE(s.serverUrl(), QStringLiteral("http://127.0.0.1:9080"));
    QVERIFY(!s.remember());
    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QVERIFY(after.readAll() == QByteArrayLiteral("\x00\xFF\x9Cgarbage\x80"));
}

void TestClientConfig::emptyValuesKeepPrevious()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[client]\nserver_url =\n");
    file.close();

    Config s(path);
    QCOMPARE(s.serverUrl(), QStringLiteral("http://127.0.0.1:9080"));
}

void TestClientConfig::saveOverwrites()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    {
        Config s(path);
        s.setServerUrl(QStringLiteral("http://a:1"));
        s.save();
    }
    {
        Config s(path);
        s.setServerUrl(QStringLiteral("http://b:2"));
        s.save();
    }
    Config loaded(path);
    QCOMPARE(loaded.serverUrl(), QStringLiteral("http://b:2"));
}

void TestClientConfig::clearTokenErasesStoredToken()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    {
        Config s(path);
        s.setToken(QStringLiteral("deadbeef"));
        s.save();
    }
    {
        Config s(path);
        QCOMPARE(s.token(), QStringLiteral("deadbeef"));
        s.setToken(QString());
        s.save();
    }
    Config reloaded(path);
    QVERIFY(reloaded.token().isEmpty());
    QFile stored(path);
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QVERIFY(!QString::fromUtf8(stored.readAll()).contains(QByteArrayLiteral("deadbeef")));
}

void TestClientConfig::demoFromConfigFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("config.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[client]\ndemo = false\n");
    file.close();

    Config s(path);
    QVERIFY(!s.demo());
    QVERIFY(s.token().isEmpty());
}

void TestClientConfig::defaultPathNonEmpty()
{
    QVERIFY(!defaultClientConfigPath().isEmpty());
    QVERIFY(defaultClientConfigPath().endsWith(QStringLiteral("config.ini")));
}

QTEST_MAIN(TestClientConfig)
#include "test_client_config.moc"
