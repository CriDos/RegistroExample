#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace registro {

enum class SettingKind { String, UShort, Bool };

struct SettingDef {
    const char *name = "";
    SettingKind kind = SettingKind::String;
    QString defaultValue; // runtime fallback when the file lacks/invalidates the key
    QString fileDefault;  // value rendered in the regenerated template (empty = defaultValue)
    QString comment;      // comment lines for the template (may contain '\n')
};

// Unified INI config storage shared by the server config and the client config
// (RegistroCommon, Qt::Core only - no platform-specific code here).
//
// Policy: a missing file is (re)created with the commented default template.
// An existing file is read as-is without validation - unreadable or garbage
// content simply leaves every key at its default (demo project tradeoff).
// Never fails: on write failure the in-memory default of every key is used and
// the reason is available via lastError(). Typed reads follow the "non-empty /
// valid wins, otherwise default" rule.
class ConfigStore {
public:
    ConfigStore(const QString &path, const QString &section, const QString &header,
                std::initializer_list<SettingDef> definitions);

    void load();

    bool fileRecreatedOnLoad() const { return m_recreated; }
    const QString &lastError() const { return m_lastError; }
    const QString &filePath() const { return m_path; }

    QString stringValue(const char *key) const;
    quint16 ushortValue(const char *key) const;
    bool boolValue(const char *key) const;

    void setString(const char *key, const QString &value);
    void setUShort(const char *key, quint16 value);
    void setBool(const char *key, bool value);

    bool save(QString *error = nullptr);

    bool writeTemplate(QString *error = nullptr) const;

private:
    const SettingDef *findDef(const char *key) const;
    QString templateValueOf(const SettingDef &def) const;
    QString renderIni(bool defaultsOnly) const;
    bool writeFile(const QString &path, const QString &content, QString *error) const;

    QString m_path;
    QString m_section;
    QString m_header;
    QList<SettingDef> m_defs;
    QHash<QString, QString> m_values; // raw string values, defaults on load
    bool m_recreated = false;
    QString m_lastError;
};

} // namespace registro
