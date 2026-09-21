#include <QtTest>

#include "config.h"

#include <QFile>
#include <QTemporaryDir>

using namespace registro;

class TestConfig : public QObject {
    Q_OBJECT
private slots:
    void defaults();
    void roundTrip();
    void fileOverridesDefaults();
    void emptyAndInvalidValuesFallBackToDefaults();
    void nonEmptyValuesWin();
    void zeroPortAccepted();
    void defaultConfigPathIsNextToBinary();
    void loadConfigCreatesMissingFile();
    void loadConfigIgnoresGarbageFile();
    void loadConfigMergesValidFile();
    void loadConfigUnwritableFallsBackToDefaults();
    void loadConfigCreatesParentDirectories();
};

void TestConfig::defaults()
{
    const ServerConfig d = defaultConfig();
    QCOMPARE(d.host, QStringLiteral("127.0.0.1"));
    QCOMPARE(d.port, 9080);
    QVERIFY(d.logLevel.isEmpty());
    QVERIFY(d.logFile.isEmpty());
    QVERIFY(d.dbPath.isEmpty());
    QVERIFY(d.adminUser.isEmpty());
    QVERIFY(d.adminPassword.isEmpty());
    QVERIFY(d.demo);
}

void TestConfig::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("server.ini"));
    QString error;
    QVERIFY2(writeDefaultConfigFile(path, &error), qPrintable(error));

    ServerConfig cfg = defaultConfig();
    loadConfigFile(path, &cfg, &error);
    const ServerConfig d = defaultConfig();
    QCOMPARE(cfg.host, d.host);
    QCOMPARE(cfg.port, d.port);
    QCOMPARE(cfg.logLevel, QStringLiteral("info"));
    QCOMPARE(cfg.logFile, d.logFile);
    QCOMPARE(cfg.dbPath, d.dbPath);
    QCOMPARE(cfg.adminUser, d.adminUser);
    QCOMPARE(cfg.adminPassword, d.adminPassword);
    QCOMPARE(cfg.demo, true);
}

void TestConfig::fileOverridesDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("server.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[server]\n"
               "host = 0.0.0.0\n"
               "port = 1234\n"
               "log_level = warn\n"
               "log_file = C:/logs/server.log\n"
               "db = C:/data/reg.db\n"
               "admin_user = boss\n"
               "admin_password = secret8\n"
               "demo = true\n");
    file.close();

    ServerConfig cfg = defaultConfig();
    loadConfigFile(path, &cfg);
    QCOMPARE(cfg.host, QStringLiteral("0.0.0.0"));
    QCOMPARE(cfg.port, 1234);
    QCOMPARE(cfg.logLevel, QStringLiteral("warn"));
    QCOMPARE(cfg.logFile, QStringLiteral("C:/logs/server.log"));
    QCOMPARE(cfg.dbPath, QStringLiteral("C:/data/reg.db"));
    QCOMPARE(cfg.adminUser, QStringLiteral("boss"));
    QCOMPARE(cfg.adminPassword, QStringLiteral("secret8"));
    QCOMPARE(cfg.demo, true);
}

void TestConfig::emptyAndInvalidValuesFallBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("server.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[server]\n"
               "host =\n"
               "port = not-a-port\n"
               "log_level = \n"
               "db =\n"
               "demo = maybe\n");
    file.close();

    ServerConfig cfg = defaultConfig();
    loadConfigFile(path, &cfg);
    QCOMPARE(cfg.host, defaultConfig().host);
    QCOMPARE(cfg.port, 9080);
    QCOMPARE(cfg.dbPath, QString());
    QCOMPARE(cfg.logLevel, QString());
    QCOMPARE(cfg.demo, true);
}

void TestConfig::nonEmptyValuesWin()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("server.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[server]\nhost = 0.0.0.0\nlog_level = nonsense\n");
    file.close();

    ServerConfig cfg = defaultConfig();
    loadConfigFile(path, &cfg);
    QCOMPARE(cfg.host, QStringLiteral("0.0.0.0"));
    QCOMPARE(cfg.logLevel, QStringLiteral("nonsense"));
}

void TestConfig::zeroPortAccepted()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("server.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[server]\nport = 0\n");
    file.close();

    ServerConfig cfg = defaultConfig();
    loadConfigFile(path, &cfg);
    QCOMPARE(cfg.port, 0);
}

void TestConfig::defaultConfigPathIsNextToBinary()
{
    QCOMPARE(defaultConfigPath(QStringLiteral("C:/x")), QStringLiteral("C:/x/registro-server.ini"));
}

void TestConfig::loadConfigCreatesMissingFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("nope.ini"));
    ServerConfig cfg = defaultConfig();
    QString message;
    const ConfigFileInfo info = loadConfigFile(path, &cfg, &message);
    QVERIFY(info.recreated);
    QVERIFY(!message.isEmpty());
    QVERIFY(QFile::exists(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(file.readAll()).contains(QStringLiteral("[server]")));

    const ServerConfig d = defaultConfig();
    QCOMPARE(cfg.host, d.host);
    QCOMPARE(cfg.port, d.port);
    QCOMPARE(cfg.logLevel, QStringLiteral("info"));
    QVERIFY(cfg.demo);
}

void TestConfig::loadConfigIgnoresGarbageFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("bad.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("\x00\xFF\x9Cgarbage\x80\x81"));
    file.close();

    ServerConfig cfg = defaultConfig();
    QString message;
    const ConfigFileInfo info = loadConfigFile(path, &cfg, &message);
    QVERIFY(!info.recreated);
    QVERIFY(message.isEmpty());
    QCOMPARE(cfg.host, defaultConfig().host);
    QCOMPARE(cfg.port, defaultConfig().port);
    QVERIFY(cfg.demo);

    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QVERIFY(after.readAll() == QByteArrayLiteral("\x00\xFF\x9Cgarbage\x80\x81"));
}

void TestConfig::loadConfigMergesValidFile()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("ok.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[server]\nhost = 0.0.0.0\nport = 4321\n");
    file.close();
    QFile before(path);
    QVERIFY(before.open(QIODevice::ReadOnly));
    const QByteArray original = before.readAll();

    ServerConfig cfg = defaultConfig();
    QString message;
    const ConfigFileInfo info = loadConfigFile(path, &cfg, &message);
    QVERIFY(!info.recreated);
    QVERIFY(message.isEmpty());
    QCOMPARE(cfg.host, QStringLiteral("0.0.0.0"));
    QCOMPARE(cfg.port, 4321);

    QFile after(path);
    QVERIFY(after.open(QIODevice::ReadOnly));
    QCOMPARE(after.readAll(), original);
}

void TestConfig::loadConfigUnwritableFallsBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.path(); // a directory: cannot be read/written as a config file
    ServerConfig cfg = defaultConfig();
    QString message;
    const ConfigFileInfo info = loadConfigFile(path, &cfg, &message);
    QVERIFY(!info.recreated);
    QVERIFY(message.isEmpty());
    QCOMPARE(cfg.host, defaultConfig().host);
    QCOMPARE(cfg.port, defaultConfig().port);
}

void TestConfig::loadConfigCreatesParentDirectories()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a/b/c.ini"));
    ServerConfig cfg = defaultConfig();
    QString message;
    const ConfigFileInfo info = loadConfigFile(path, &cfg, &message);
    QVERIFY(info.recreated);
    QVERIFY(QFile::exists(path));
}

QTEST_MAIN(TestConfig)
#include "test_config.moc"