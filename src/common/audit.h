#pragma once

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

namespace registro {

struct AuditEntry {
    qint64 id = 0;
    qint64 userId = 0;
    QString action;
    QString entity;
    qint64 entityId = 0;
    QString createdAt;
};

QJsonObject auditToJson(const AuditEntry &a);

} // namespace registro
