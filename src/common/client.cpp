#include "client.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrlQuery>

namespace registro {

namespace {

bool tooLong(const QString &field, int max, QString *error)
{
    *error = QStringLiteral("field '%1' is too long (max %2)").arg(field).arg(max);
    return false;
}

} // namespace

bool isValidStatus(const QString &status)
{
    return status == QLatin1String(kStatusActive) || status == QLatin1String(kStatusArchived);
}

QJsonObject clientToJson(const Client &c)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), static_cast<qint64>(c.id));
    o.insert(QStringLiteral("full_name"), c.fullName);
    o.insert(QStringLiteral("org"), c.org);
    o.insert(QStringLiteral("phone"), c.phone);
    o.insert(QStringLiteral("email"), c.email);
    o.insert(QStringLiteral("notes"), c.notes);
    o.insert(QStringLiteral("status"), c.status);
    o.insert(QStringLiteral("created_at"), c.createdAt);
    o.insert(QStringLiteral("updated_at"), c.updatedAt);
    return o;
}

bool clientFromBody(const QByteArray &body, Client *out, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = QStringLiteral("invalid JSON body");
        return false;
    }
    const QJsonObject o = doc.object();

    const QString fullName = o.value(QStringLiteral("full_name")).toString().trimmed();
    if (fullName.isEmpty()) {
        *error = QStringLiteral("field 'full_name' is required");
        return false;
    }
    if (fullName.size() > kFullNameMax)
        return tooLong(QStringLiteral("full_name"), kFullNameMax, error);

    out->fullName = fullName;
    out->org = o.value(QStringLiteral("org")).toString().trimmed();
    out->phone = o.value(QStringLiteral("phone")).toString().trimmed();
    out->email = o.value(QStringLiteral("email")).toString().trimmed();
    out->notes = o.value(QStringLiteral("notes")).toString().trimmed();
    if (out->org.size() > kOrgMax)
        return tooLong(QStringLiteral("org"), kOrgMax, error);
    if (out->phone.size() > kPhoneMax)
        return tooLong(QStringLiteral("phone"), kPhoneMax, error);
    if (out->email.size() > kEmailMax)
        return tooLong(QStringLiteral("email"), kEmailMax, error);
    if (out->notes.size() > kNotesMax)
        return tooLong(QStringLiteral("notes"), kNotesMax, error);

    const QString status = o.value(QStringLiteral("status")).toString();
    if (!status.isEmpty() && !isValidStatus(status)) {
        *error = QStringLiteral("field 'status' must be 'active' or 'archived'");
        return false;
    }
    out->status = status.isEmpty() ? QString::fromLatin1(kStatusActive) : status;
    return true;
}

ClientFilter filterFromQuery(const QUrlQuery &q)
{
    ClientFilter f;
    f.search = q.queryItemValue(QStringLiteral("search"));
    f.status = q.queryItemValue(QStringLiteral("status"));

    const QString limit = q.queryItemValue(QStringLiteral("limit"));
    const QString offset = q.queryItemValue(QStringLiteral("offset"));
    bool ok = false;
    const qlonglong l = limit.toLongLong(&ok);
    if (ok && l >= 0 && l <= kMaxPageSize)
        f.limit = l;
    const qlonglong off = offset.toLongLong(&ok);
    if (ok && off >= 0)
        f.offset = off;
    return f;
}

void clientFromJson(const QJsonObject &o, Client *c)
{
    if (!c)
        return;
    c->id = static_cast<qint64>(o.value(QStringLiteral("id")).toDouble());
    c->fullName = o.value(QStringLiteral("full_name")).toString();
    c->org = o.value(QStringLiteral("org")).toString();
    c->phone = o.value(QStringLiteral("phone")).toString();
    c->email = o.value(QStringLiteral("email")).toString();
    c->notes = o.value(QStringLiteral("notes")).toString();
    c->status = o.value(QStringLiteral("status")).toString();
    c->createdAt = o.value(QStringLiteral("created_at")).toString();
    c->updatedAt = o.value(QStringLiteral("updated_at")).toString();
}

} // namespace registro
