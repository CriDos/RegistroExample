#include "api_server.h"
#include "config.h"
#include "log_categories.h"
#include <log.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("RegistroServer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(REGISTRO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("RegistroExample REST server"));
    parser.addHelpOption();
    QCommandLineOption portOpt({QStringLiteral("p"), QStringLiteral("port")},
                               QStringLiteral("TCP port (0 = ephemeral)"), QStringLiteral("port"),
                               QStringLiteral("9080"));
    parser.addOption(portOpt);
    QCommandLineOption logLevelOpt(QStringLiteral("log-level"),
                                   QStringLiteral("Log verbosity: debug | info | warn | error"),
                                   QStringLiteral("level"));
    parser.addOption(logLevelOpt);
    QCommandLineOption logFileOpt(QStringLiteral("log-file"),
                                  QStringLiteral("Append log output to this file"),
                                  QStringLiteral("file"));
    parser.addOption(logFileOpt);
    QCommandLineOption dbOpt(QStringLiteral("db"),
                             QStringLiteral("SQLite database file (default: next to the binary)"),
                             QStringLiteral("path"));
    parser.addOption(dbOpt);
    QCommandLineOption adminUserOpt(
        QStringLiteral("admin-user"),
        QStringLiteral("Seed this admin user on first run (when the users table is empty)"),
        QStringLiteral("username"));
    parser.addOption(adminUserOpt);
    QCommandLineOption adminPasswordOpt(QStringLiteral("admin-password"),
                                        QStringLiteral("Password for the seeded admin (min 8)"),
                                        QStringLiteral("password"));
    parser.addOption(adminPasswordOpt);
    QCommandLineOption demoOpt(
        QStringLiteral("demo"),
        QStringLiteral("Demo mode (default): seed 'demo'/'demo' (admin) + 10000 clients on first "
                       "run and advertise demo via /api/health"));
    parser.addOption(demoOpt);
    QCommandLineOption noDemoOpt(
        QStringLiteral("no-demo"),
        QStringLiteral("Disable demo mode: use registro.db and seed no demo account/clients"));
    parser.addOption(noDemoOpt);
    QCommandLineOption configOpt(
        QStringLiteral("config"),
        QStringLiteral("Config file (INI; default: registro-server.ini next to the binary)"),
        QStringLiteral("path"));
    parser.addOption(configOpt);
    QCommandLineOption writeConfigOpt(QStringLiteral("write-config"),
                                      QStringLiteral("Write a default config file and exit"),
                                      QStringLiteral("path"));
    parser.addOption(writeConfigOpt);
    parser.process(app);

    const QString appDir = QCoreApplication::applicationDirPath();

    if (parser.isSet(writeConfigOpt)) {
        QString error;
        if (!registro::writeDefaultConfigFile(parser.value(writeConfigOpt), &error)) {
            std::fprintf(stderr, "failed to write config: %s\n", qPrintable(error));
            return 1;
        }
        std::printf("default config written to %s\n", qPrintable(parser.value(writeConfigOpt)));
        return 0;
    }

    registro::ServerConfig cfg = registro::defaultConfig();
    const QString configPath =
        parser.isSet(configOpt) ? parser.value(configOpt) : registro::defaultConfigPath(appDir);
    QString configNote;
    (void)registro::loadConfigFile(configPath, &cfg, &configNote);
    if (!configNote.isEmpty())
        std::fprintf(stderr, "%s\n", qPrintable(configNote));
    if (parser.isSet(portOpt)) {
        bool ok = false;
        const quint16 port = parser.value(portOpt).toUShort(&ok);
        if (!ok) {
            std::fprintf(stderr, "invalid --port value: %s\n", qPrintable(parser.value(portOpt)));
            return 1;
        }
        cfg.port = port;
    }
    if (parser.isSet(logLevelOpt))
        cfg.logLevel = parser.value(logLevelOpt);
    if (parser.isSet(logFileOpt))
        cfg.logFile = parser.value(logFileOpt);
    if (parser.isSet(dbOpt))
        cfg.dbPath = parser.value(dbOpt);
    if (parser.isSet(adminUserOpt))
        cfg.adminUser = parser.value(adminUserOpt);
    if (parser.isSet(adminPasswordOpt))
        cfg.adminPassword = parser.value(adminPasswordOpt);
    if (parser.isSet(demoOpt))
        cfg.demo = true;
    if (parser.isSet(noDemoOpt))
        cfg.demo = false;

    registro::LogConfig logCfg;
    if (cfg.logLevel.isEmpty()) {
#ifdef QT_DEBUG
        logCfg.level = registro::LogLevel::Debug;
#else
        logCfg.level = registro::LogLevel::Info;
#endif
    } else {
        logCfg.level = registro::parseLogLevel(cfg.logLevel);
    }
    logCfg.filePath = cfg.logFile;
    registro::installLogging(logCfg);
    qCInfo(lcServer).noquote() << QStringLiteral("RegistroServer %1 - Qt %2 - %3")
                                      .arg(QString::fromLatin1(REGISTRO_VERSION),
                                           QString::fromLatin1(qVersion()),
                                           registro::systemSummary());
    qCInfo(lcServer).noquote() << QStringLiteral("config: %1").arg(configPath);

    const QString dbPath = cfg.dbPath.isEmpty()
                               ? QDir(appDir).filePath(cfg.demo ? QStringLiteral("registro-demo.db")
                                                                : QStringLiteral("registro.db"))
                               : cfg.dbPath;

    QHostAddress host(cfg.host);
    if (host.isNull()) {
        qCWarning(lcServer) << "invalid host in config:" << cfg.host
                            << "- falling back to 127.0.0.1";
        host = QHostAddress::LocalHost;
    }

    registro::ApiServer server;
    if (!server.start(host, cfg.port, dbPath, cfg.adminUser, cfg.adminPassword, cfg.demo)) {
        std::fprintf(stderr, "failed to start server\n");
        return 1;
    }
    qCInfo(lcServer).noquote()
        << QStringLiteral("listening on %1:%2").arg(host.toString()).arg(server.port());

    return QCoreApplication::exec();
}
