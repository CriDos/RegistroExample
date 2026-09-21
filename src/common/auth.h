#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace registro {

struct User {
    qint64 id = 0;
    QString username;
    QString role;
};

inline constexpr char kRoleUser[] = "user";
inline constexpr char kRoleAdmin[] = "admin";

bool loginFromBody(const QByteArray &body, QString *username, QString *password, QString *error);

bool userFromBody(const QByteArray &body, QString *username, QString *password, QString *role,
                  QString *error);

QJsonObject userToJson(const User &u);

} // namespace registro
