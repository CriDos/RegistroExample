#pragma once

#include <QString>

namespace registro {

struct ServerConfig {
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 9080;
    QString logLevel;
    QString logFile;
    QString dbPath;
    QString adminUser;
    QString adminPassword;
    bool demo = true;
};

ServerConfig defaultConfig();

QString defaultConfigPath(const QString &appDir);

struct ConfigFileInfo {
    bool recreated = false; // file was missing and has been (re)written with defaults
};

ConfigFileInfo loadConfigFile(const QString &path, ServerConfig *cfg, QString *message = nullptr);

bool writeDefaultConfigFile(const QString &path, QString *error);

} // namespace registro