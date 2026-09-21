#include "config.h"
#include "config_store.h"

#include <QDir>

namespace registro {

namespace {

QString configHeader()
{
    return QStringLiteral(
        "# RegistroServer configuration (QSettings INI).\n"
        "# All keys are optional; missing keys fall back to built-in defaults.\n"
        "# Resolution order: defaults < this file < CLI flags (--port, --db, ...).\n"
        "# Default location (auto-discovered when --config is absent):\n"
        "#   same directory as the server binary.");
}

ConfigStore makeStore(const QString &path)
{
    return ConfigStore(
        path, QStringLiteral("server"), configHeader(),
        {
            // clang-format off
            {"host",           SettingKind::String, QStringLiteral("127.0.0.1"), QString(),
             QStringLiteral("host: bind address (IP or name); default 127.0.0.1.")},
            {"port",           SettingKind::UShort, QStringLiteral("9080"), QStringLiteral("9080"),
             QStringLiteral("port: 0 = ephemeral free port (handy for tests).")},
            {"log_level",      SettingKind::String, QString(),
             QStringLiteral("info"),
             QStringLiteral("log_level: debug | info | warn | error; empty = build default.")},
            {"log_file",       SettingKind::String, QString(), QString(),
             QStringLiteral("log_file: append logs here; empty = stderr.")},
            {"db",             SettingKind::String, QString(), QString(),
             QStringLiteral("db: SQLite file; empty = registro.db next to the binary.")},
            {"admin_user",     SettingKind::String, QString(), QString(),
             QStringLiteral("admin_user/admin_password: seed admin on first run only.")},
            {"admin_password", SettingKind::String, QString(), QString(), QString()},
            {"demo",           SettingKind::Bool, QStringLiteral("true"), QStringLiteral("true"),
             QStringLiteral("demo: demo mode (default on - this is a demo project). Uses a\n"
                            "      separate registro-demo.db, seeds 'demo'/'demo' (admin) and 10000\n"
                            "      random clients on first run, and marks /api/health with demo:true\n"
                            "      as an informational flag. Set to false for a plain server\n"
                            "      (registro.db). The fixed demo account is exempt from the\n"
                            "      min-8-char password rule.")},
            // clang-format on
        });
}

} // namespace

ServerConfig defaultConfig()
{
    return ServerConfig{};
}

QString defaultConfigPath(const QString &appDir)
{
    return QDir(appDir).filePath(QStringLiteral("registro-server.ini"));
}

ConfigFileInfo loadConfigFile(const QString &path, ServerConfig *cfg, QString *message)
{
    ConfigStore store = makeStore(path);
    store.load();

    ConfigFileInfo info;
    info.recreated = store.fileRecreatedOnLoad();
    if (info.recreated) {
        if (message)
            *message = QStringLiteral("config file missing: created default at %1").arg(path);
    } else if (!store.lastError().isEmpty() && message) {
        *message = QStringLiteral("config file missing at %1; cannot write defaults: "
                                  "%2; using built-in defaults")
                       .arg(path, store.lastError());
    }

    // String keys: only non-empty values override the caller's defaults
    // (empty/invalid values resolve to the key's default inside the store).
    const QString host = store.stringValue("host");
    if (!host.isEmpty())
        cfg->host = host;
    cfg->port = store.ushortValue("port");
    const QString logLevel = store.stringValue("log_level");
    if (!logLevel.isEmpty())
        cfg->logLevel = logLevel;
    const QString logFile = store.stringValue("log_file");
    if (!logFile.isEmpty())
        cfg->logFile = logFile;
    const QString dbPath = store.stringValue("db");
    if (!dbPath.isEmpty())
        cfg->dbPath = dbPath;
    const QString adminUser = store.stringValue("admin_user");
    if (!adminUser.isEmpty())
        cfg->adminUser = adminUser;
    const QString adminPassword = store.stringValue("admin_password");
    if (!adminPassword.isEmpty())
        cfg->adminPassword = adminPassword;
    cfg->demo = store.boolValue("demo");
    return info;
}

bool writeDefaultConfigFile(const QString &path, QString *error)
{
    ConfigStore store = makeStore(path);
    return store.writeTemplate(error);
}

} // namespace registro
