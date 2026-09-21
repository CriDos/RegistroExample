#pragma once

#include <QString>
#include <QtGlobal>

namespace registro {

QString hashPassword(const QString &password);
bool verifyPassword(const QString &password, const QString &stored);

QString generateToken();
QString hashToken(const QString &token);

} // namespace registro
