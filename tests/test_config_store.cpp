#include <QtTest>

#include "config_store.h"

#include <QFile>
#include <QTemporaryDir>

using namespace registro;

namespace {

ConfigStore makeStore(const QString &path)
{
    return ConfigStore(path, QStringLiteral("test"), QStringLiteral("# header line"),
                       {
                           // clang-format off
                             {"name",    SettingKind::String, QStringLiteral("default"), QString(), QStringLiteral("a name")},
                             {"port",    SettingKind::UShort, QStringLiteral("8080"), QStringLiteral("9090"), QStringLiteral("the port")},
                             {"enabled", SettingKind::Bool, QStringLiteral("false"), QString(), QStringLiteral("a flag")},
                           // clang-format on
                       });
}

} // namespace

class TestConfigStore : public QObject {
    Q_OBJECT
private slots:
    void missingFileCreatesDefaults();
    void garbageFileFallsBackToDefaults();
    void emptyFileKeepsDefaults();
    void typedAccessFallsBackOnInvalid();
    void roundTrip();
    void saveCreatesParentDirectories();
    void saveToDirectoryFails();
    void templateRendersCommentsAndFileDefaults();
};

void TestConfigStore::missingFileCreatesDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    ConfigStore store = makeStore(path);
    store.load();
    QVERIFY(store.fileRecreatedOnLoad());
    QVERIFY(store.lastError().isEmpty());
    QVERIFY(QFile::exists(path));
    QCOMPARE(store.stringValue("name"), QStringLiteral("default"));
    QCOMPARE(store.ushortValue("port"), 9090);
    QCOMPARE(store.boolValue("enabled"), false);
}

void TestConfigStore::garbageFileFallsBackToDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral("\x00\xFF\x9Cgarbage\x80"));
    file.close();

    ConfigStore store = makeStore(path);
    store.load();
    QVERIFY(!store.fileRecreatedOnLoad());
    QVERIFY(store.lastError().isEmpty());
    QCOMPARE(store.stringValue("name"), QStringLiteral("default"));
    QCOMPARE(store.ushortValue("port"), 8080);
    QCOMPARE(store.boolValue("enabled"), false);
}

void TestConfigStore::emptyFileKeepsDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    ConfigStore store = makeStore(path);
    store.load();
    QVERIFY(!store.fileRecreatedOnLoad());
    QCOMPARE(store.stringValue("name"), QStringLiteral("default"));
    QCOMPARE(store.ushortValue("port"), 8080);
    QCOMPARE(store.boolValue("enabled"), false);
}

void TestConfigStore::typedAccessFallsBackOnInvalid()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[test]\n"
               "name =   \n"
               "port = not-a-port\n"
               "enabled = 1\n");
    file.close();

    ConfigStore store = makeStore(path);
    store.load();
    QVERIFY(!store.fileRecreatedOnLoad());
    QCOMPARE(store.stringValue("name"), QStringLiteral("default"));
    QCOMPARE(store.ushortValue("port"), 8080);
    QCOMPARE(store.boolValue("enabled"), false);
}

void TestConfigStore::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    ConfigStore store = makeStore(path);
    store.load();
    store.setString("name", QStringLiteral("Иван Иванов"));
    store.setUShort("port", 0);
    store.setBool("enabled", true);
    QString error;
    QVERIFY2(store.save(&error), qPrintable(error));

    ConfigStore reloaded = makeStore(path);
    reloaded.load();
    QVERIFY(!reloaded.fileRecreatedOnLoad());
    QCOMPARE(reloaded.stringValue("name"), QStringLiteral("Иван Иванов"));
    QCOMPARE(reloaded.ushortValue("port"), 0);
    QCOMPARE(reloaded.boolValue("enabled"), true);

    QFile stored(path);
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(stored.readAll()).contains(QStringLiteral("enabled = true")));
}

void TestConfigStore::saveCreatesParentDirectories()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("a/b/x.ini"));
    ConfigStore store = makeStore(path);
    store.load();
    QString error;
    QVERIFY2(store.save(&error), qPrintable(error));
    QVERIFY(QFile::exists(path));
}

void TestConfigStore::saveToDirectoryFails()
{
    QTemporaryDir dir;
    ConfigStore store = makeStore(dir.path());
    store.load();
    QString error;
    QVERIFY(!store.save(&error));
    QVERIFY(!error.isEmpty());
}

void TestConfigStore::templateRendersCommentsAndFileDefaults()
{
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("x.ini"));
    ConfigStore store = makeStore(path);
    store.load();
    const QString out = dir.filePath(QStringLiteral("t.ini"));
    ConfigStore templater = makeStore(out);
    QString error;
    QVERIFY2(templater.writeTemplate(&error), qPrintable(error));
    QFile file(out);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("# header line")));
    QVERIFY(content.contains(QStringLiteral("# a name")));
    QVERIFY(content.contains(QStringLiteral("# the port")));
    QVERIFY(content.contains(QStringLiteral("port = 9090")));
    QVERIFY(content.contains(QStringLiteral("enabled = false")));
}

QTEST_MAIN(TestConfigStore)
#include "test_config_store.moc"
