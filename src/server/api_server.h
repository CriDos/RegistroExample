#pragma once

#include "database.h"

#include <QHash>
#include <QHostAddress>
#include <QHttpServer>
#include <QStringList>
#include <QTcpServer>

namespace registro {

class ApiServer {
public:
    bool start(const QHostAddress &address, quint16 port, const QString &dbPath,
               const QString &adminUser = {}, const QString &adminPassword = {}, bool demo = false);
    bool start(quint16 port, const QString &dbPath, const QString &adminUser = {},
               const QString &adminPassword = {}, bool demo = false);
    quint16 port() const { return m_port; }
    ~ApiServer();

private:
    void registerRoutes();
    void writeAudit(qint64 userId, const QString &action, const QString &entity, qint64 entityId);
    bool loginThrottled(const QString &peer) const;

    Database m_db;
    QTcpServer m_tcpServer;
    QHttpServer *m_http = nullptr;
    mutable QHash<QString, QList<qint64>> m_loginAttempts;
    quint16 m_port = 0;
    bool m_authEnabled = false;
    bool m_routesRegistered = false;
    bool m_demo = false;
    bool m_started = false;
};

} // namespace registro
