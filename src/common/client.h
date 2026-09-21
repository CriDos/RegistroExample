#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QVector>
#include <QtGlobal>

class QUrlQuery;

namespace registro {

inline constexpr qint64 kMaxPageSize = 1000;

inline constexpr int kFullNameMax = 200;
inline constexpr int kOrgMax = 200;
inline constexpr int kPhoneMax = 50;
inline constexpr int kEmailMax = 200;
inline constexpr int kNotesMax = 4000;

inline constexpr char kStatusActive[] = "active";
inline constexpr char kStatusArchived[] = "archived";

struct Client {
    qint64 id = 0;
    QString fullName;
    QString org;
    QString phone;
    QString email;
    QString notes;
    QString status = QLatin1String(kStatusActive);
    QString createdAt;
    QString updatedAt;
};

struct ClientFilter {
    QString search;
    QString status;
    qint64 limit = 50;
    qint64 offset = 0;
};

struct ClientPage {
    qint64 total = 0;
    QVector<Client> items;
};

bool isValidStatus(const QString &status);

QJsonObject clientToJson(const Client &c);

bool clientFromBody(const QByteArray &body, Client *out, QString *error);

ClientFilter filterFromQuery(const QUrlQuery &q);

void clientFromJson(const QJsonObject &o, Client *c);

} // namespace registro

Q_DECLARE_METATYPE(registro::Client)
Q_DECLARE_METATYPE(QVector<registro::Client>)
