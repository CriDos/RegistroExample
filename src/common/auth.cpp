#include "auth.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace registro {

namespace {

QString requiredField(const QJsonObject &o, const QString &name, int maxLen, QString *error)
{
    const QString value = o.value(name).toString().trimmed();
    if (value.isEmpty()) {
        *error = QStringLiteral("field '%1' is required").arg(name);
        return {};
    }
    if (value.size() > maxLen) {
        *error = QStringLiteral("field '%1' is too long").arg(name);
        return {};
    }
    return value;
}

} // namespace

bool loginFromBody(const QByteArray &body, QString *username, QString *password, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("invalid JSON body");
        return false;
    }
    const QJsonObject o = doc.object();

    *username = requiredField(o, QStringLiteral("username"), 64, error);
    if (username->isEmpty())
        return false;
    *password = o.value(QStringLiteral("password")).toString();
    if (password->isEmpty()) {
        *error = QStringLiteral("field 'password' is required");
        return false;
    }
    return true;
}

bool userFromBody(const QByteArray &body, QString *username, QString *password, QString *role,
                  QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("invalid JSON body");
        return false;
    }
    const QJsonObject o = doc.object();

    *username = requiredField(o, QStringLiteral("username"), 64, error);
    if (username->isEmpty())
        return false;

    *password = o.value(QStringLiteral("password")).toString();
    if (password->size() < 8) {
        *error = QStringLiteral("field 'password' must be at least 8 characters");
        return false;
    }

    *role = o.value(QStringLiteral("role")).toString();
    if (role->isEmpty())
        *role = QString::fromLatin1(kRoleUser);
    if (*role != QLatin1String(kRoleUser) && *role != QLatin1String(kRoleAdmin)) {
        *error = QStringLiteral("field 'role' must be 'user' or 'admin'");
        return false;
    }
    return true;
}

QJsonObject userToJson(const User &u)
{
    return QJsonObject{{QStringLiteral("id"), static_cast<qint64>(u.id)},
                       {QStringLiteral("username"), u.username},
                       {QStringLiteral("role"), u.role}};
}

} // namespace registro