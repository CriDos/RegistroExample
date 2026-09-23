#include "config.h"
#include "config_store.h"
#include "log_categories.h"

#include <QDir>
#include <QStandardPaths>

namespace registro {

namespace {

QString configHeader()
{
    return QStringLiteral("; RegistroClient configuration (QSettings INI).\n"
                          "; Missing files are recreated with these defaults at startup.");
}

} // namespace

QString defaultClientConfigPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("config.ini"));
}

Config::Config(const QString &path, QObject *parent)
    : QObject(parent),
      m_store(new ConfigStore(
          path, QStringLiteral("client"), configHeader(),
          {
              // clang-format off
              {"server_url", SettingKind::String,
               QStringLiteral("http://127.0.0.1:9080"), QString(),
               QStringLiteral("server_url: server address used by the login form.")},
              {"token", SettingKind::String, QString(), QString(),
               QStringLiteral("token: last login session; written as-is when remember is on\n"
                              "      (no encryption - plaintext, cross-platform).")},
              {"last_user", SettingKind::String, QString(), QString(),
               QStringLiteral("last_user: last login name for the form.")},
              {"remember", SettingKind::Bool, QStringLiteral("false"), QString(),
               QStringLiteral("remember: persist the token after login.")},
              {"demo", SettingKind::Bool, QStringLiteral("true"), QStringLiteral("true"),
               QStringLiteral("demo: demo mode (default on - this is a demo project). Prefills\n"
                              "      the login form with demo/demo and adds a (demo mode) title\n"
                              "      suffix. Set to false for a plain server.")},
              // clang-format on
          }))
{
    m_store->load();
    if (m_store->fileRecreatedOnLoad()) {
        qCWarning(lcClient).noquote()
            << QStringLiteral("config file missing: created default at %1")
                   .arg(m_store->filePath());
    } else if (!m_store->lastError().isEmpty()) {
        qCWarning(lcClient).noquote()
            << QStringLiteral("config file missing at %1; cannot write defaults: "
                              "%2; using built-in defaults")
                   .arg(m_store->filePath(), m_store->lastError());
    }
}

Config::~Config()
{
    delete m_store;
}

QString Config::serverUrl() const
{
    return m_store->stringValue("server_url");
}

void Config::setServerUrl(const QString &url)
{
    const QString normalized = url.trimmed();
    if (normalized == serverUrl())
        return;
    m_store->setString("server_url", normalized);
    emit serverUrlChanged();
}

QString Config::token() const
{
    return m_store->stringValue("token");
}

void Config::setToken(const QString &token)
{
    const QString normalized = token.trimmed();
    if (normalized == m_store->stringValue("token"))
        return;
    m_store->setString("token", normalized);
    emit tokenChanged();
}

QString Config::lastUser() const
{
    return m_store->stringValue("last_user");
}

void Config::setLastUser(const QString &user)
{
    const QString normalized = user.trimmed();
    if (normalized == lastUser())
        return;
    m_store->setString("last_user", normalized);
    emit lastUserChanged();
}

bool Config::remember() const
{
    return m_store->boolValue("remember");
}

void Config::setRemember(bool remember)
{
    if (remember == Config::remember())
        return;
    m_store->setBool("remember", remember);
    emit rememberChanged();
}

bool Config::demo() const
{
    return m_store->boolValue("demo");
}

void Config::save()
{
    QString error;
    if (!m_store->save(&error))
        qCWarning(lcClient).noquote() << error;
}

} // namespace registro