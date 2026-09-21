#pragma once

#include "audit.h"
#include "client.h"

#include <QHash>
#include <QMutex>
#include <QSqlDatabase>
#include <QStringList>
#include <QThread>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>

namespace registro {

// Bearer token lifetime; tokens are stored as SHA-256 hashes with an expiry.
inline constexpr int kTokenTtlSeconds = 30 * 24 * 60 * 60;

struct UserRecord {
    qint64 id = 0;
    QString username;
    QString passwordHash;
    QString role;
};

struct AuditFilter {
    qint64 limit = 50;
    qint64 offset = 0;
};

struct AuditPage {
    qint64 total = 0;
    QVector<AuditEntry> items;
};

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const QString &filePath);
    QString lastError() const;

    qint64 addClient(const Client &c);
    bool getClient(qint64 id, Client *out);
    bool updateClient(qint64 id, const Client &c);
    bool deleteClient(qint64 id);
    ClientPage listClients(const ClientFilter &f);
    bool seedDemoClients(int count);

    qint64 addUser(const QString &username, const QString &passwordHash, const QString &role);
    bool getUserByUsername(const QString &username, UserRecord *out);
    bool getUserByTokenHash(const QString &tokenHash, UserRecord *out);
    bool storeToken(const QString &tokenHash, qint64 userId, int ttlSeconds = kTokenTtlSeconds);
    int deleteExpiredTokens();
    QVector<UserRecord> listUsers();
    qint64 userCount();

    qint64 addAudit(qint64 userId, const QString &action, const QString &entity, qint64 entityId);
    AuditPage listAudit(const AuditFilter &f);

private:
    void migrate();
    void backfillSearchNames();
    bool getClientLocked(qint64 id, Client *out);
    void clearConnections();
    QSqlDatabase threadDb();

    static Client rowToClient(const QVariantMap &row);
    static QVariantMap clientToRow(const Client &c);
    static UserRecord rowToUser(const QVariantMap &row);
    static AuditEntry rowToAudit(const QVariantMap &row);

    mutable QMutex m_mutex;
    QHash<Qt::HANDLE, QSqlDatabase *> m_threadConns;
    QString m_connection;
    QString m_path;
    QString m_lastError;
};

} // namespace registro
