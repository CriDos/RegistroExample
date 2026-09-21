#include "audit.h"

namespace registro {

QJsonObject auditToJson(const AuditEntry &a)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), static_cast<qint64>(a.id));
    o.insert(QStringLiteral("user_id"),
             a.userId > 0 ? QJsonValue(static_cast<qint64>(a.userId)) : QJsonValue());
    o.insert(QStringLiteral("action"), a.action);
    o.insert(QStringLiteral("entity"), a.entity);
    o.insert(QStringLiteral("entity_id"), static_cast<qint64>(a.entityId));
    o.insert(QStringLiteral("created_at"), a.createdAt);
    return o;
}

} // namespace registro