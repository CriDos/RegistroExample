#include "config_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>

namespace registro {

namespace {

bool writeFileAtomically(const QString &path, const QByteArray &content, QString *error)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(content);
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString normalizeCommentLine(const QString &line)
{
    return line.trimmed().isEmpty() ? QString() : (QStringLiteral("# ") + line);
}

void readSection(QSettings *settings, const QString &section, const QList<SettingDef> &defs,
                 QHash<QString, QString> *values)
{
    settings->beginGroup(section);
    for (const SettingDef &def : defs)
        (*values)[QString::fromLatin1(def.name)] =
            settings->value(QString::fromLatin1(def.name)).toString();
    settings->endGroup();
}

} // namespace

ConfigStore::ConfigStore(const QString &path, const QString &section, const QString &header,
                         std::initializer_list<SettingDef> definitions)
    : m_path(path), m_section(section), m_header(header), m_defs(definitions)
{
}

void ConfigStore::load()
{
    m_recreated = false;
    m_lastError.clear();
    for (const SettingDef &def : m_defs)
        m_values.insert(QString::fromLatin1(def.name), def.defaultValue);

    const auto readAll = [this] {
        QSettings settings(m_path, QSettings::IniFormat);
        readSection(&settings, m_section, m_defs, &m_values);
    };

    // The file is read as-is; keys that are missing or unreadable simply keep
    // their defaults (no strict validation - this is a demo project).
    if (QFile::exists(m_path)) {
        readAll();
        return;
    }

    QString error;
    if (writeTemplate(&error)) {
        m_recreated = true;
        readAll();
    } else {
        m_lastError = error;
    }
}

const SettingDef *ConfigStore::findDef(const char *key) const
{
    const QString name = QString::fromLatin1(key);
    for (const SettingDef &def : m_defs) {
        if (name == QLatin1String(def.name))
            return &def;
    }
    return nullptr;
}

// Value rendered in the default template; falls back to the runtime default.
QString ConfigStore::templateValueOf(const SettingDef &def) const
{
    return !def.fileDefault.isEmpty() ? def.fileDefault : def.defaultValue;
}

// Empty values mean "not set": the key's default applies. Non-empty values win.
QString ConfigStore::stringValue(const char *key) const
{
    const SettingDef *def = findDef(key);
    if (!def)
        return {};
    const QString raw = m_values.value(QString::fromLatin1(key)).trimmed();
    return raw.isEmpty() ? def->defaultValue : raw;
}

quint16 ConfigStore::ushortValue(const char *key) const
{
    const SettingDef *def = findDef(key);
    if (!def)
        return 0;
    bool ok = false;
    const quint16 value = m_values.value(QString::fromLatin1(key)).trimmed().toUShort(&ok);
    if (ok)
        return value;
    return static_cast<quint16>(def->defaultValue.toUShort());
}

bool ConfigStore::boolValue(const char *key) const
{
    const SettingDef *def = findDef(key);
    if (!def)
        return false;
    const QString raw = m_values.value(QString::fromLatin1(key)).trimmed().toLower();
    if (raw == QLatin1String("true"))
        return true;
    if (raw == QLatin1String("false"))
        return false;
    return def->defaultValue.toLower() == QLatin1String("true");
}

void ConfigStore::setString(const char *key, const QString &value)
{
    if (findDef(key))
        m_values[QString::fromLatin1(key)] = value;
}

void ConfigStore::setUShort(const char *key, quint16 value)
{
    if (findDef(key))
        m_values[QString::fromLatin1(key)] = QString::number(value);
}

void ConfigStore::setBool(const char *key, bool value)
{
    if (findDef(key))
        m_values[QString::fromLatin1(key)] =
            value ? QStringLiteral("true") : QStringLiteral("false");
}

QString ConfigStore::renderIni(bool defaultsOnly) const
{
    QString text;
    text += m_header;
    if (text.isEmpty() || !text.endsWith(QLatin1Char('\n')))
        text += QLatin1Char('\n');
    text += QLatin1Char('\n');
    text += QLatin1Char('[');
    text += m_section;
    text += QStringLiteral("]\n");

    for (const SettingDef &def : m_defs) {
        if (!def.comment.isEmpty()) {
            const QStringList lines = def.comment.split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const QString comment = normalizeCommentLine(line);
                if (!comment.isEmpty())
                    text += comment + QLatin1Char('\n');
            }
        }
        QString value =
            defaultsOnly ? templateValueOf(def) : m_values.value(QString::fromLatin1(def.name));
        if (value.isEmpty() && def.kind == SettingKind::Bool)
            value = QStringLiteral("false");
        text += QString::fromLatin1(def.name);
        text += QStringLiteral(" = ");
        text += value;
        text += QLatin1Char('\n');
    }
    return text;
}

bool ConfigStore::writeFile(const QString &path, const QString &content, QString *error) const
{
    return writeFileAtomically(path, content.toUtf8(), error);
}

bool ConfigStore::save(QString *error)
{
    return writeFile(m_path, renderIni(false), error);
}

bool ConfigStore::writeTemplate(QString *error) const
{
    return writeFile(m_path, renderIni(true), error);
}

} // namespace registro
