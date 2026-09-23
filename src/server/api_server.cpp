#include "api_server.h"
#include "audit.h"
#include "auth.h"
#include "client.h"
#include "log_categories.h"
#include "password.h"
#include <log.h>

#include <QDateTime>
#include <QElapsedTimer>
#include <QHttpHeaders>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QTcpServer>
#include <QUrlQuery>
#include <optional>

namespace registro {

ApiServer::~ApiServer() {}

namespace {

// Login brute-force throttle: per peer address, at most this many failed
// attempts within the window; further attempts get HTTP 429.
constexpr int kLoginWindowMs = 60 * 1000;
constexpr int kMaxLoginAttempts = 10;

struct AuthUser {
    qint64 id = 0;
    QString username;
    QString role;
};

bool bodyIsJson(const QHttpServerRequest &req)
{
    const QByteArray ct =
        req.headers().value(QHttpHeaders::WellKnownHeader::ContentType).toByteArray().toLower();
    return ct.startsWith(QByteArrayLiteral("application/json"));
}

QHttpServerResponse jsonError(const QString &message, QHttpServerResponse::StatusCode status)
{
    return QHttpServerResponse(QJsonObject{{QStringLiteral("error"), message}}, status);
}

QHttpServerResponse unauthorized()
{
    return jsonError(QStringLiteral("unauthorized"), QHttpServerResponse::StatusCode::Unauthorized);
}

QHttpServerResponse forbidden()
{
    return jsonError(QStringLiteral("forbidden"), QHttpServerResponse::StatusCode::Forbidden);
}
std::optional<AuthUser> authenticate(const QHttpServerRequest &req, Database &db, bool authEnabled)
{
    if (!authEnabled)
        return AuthUser{0, QString(), QString::fromLatin1(kRoleAdmin)};

    const QByteArrayView header = req.headers().value(QHttpHeaders::WellKnownHeader::Authorization);
    if (!header.startsWith(QByteArrayView(QByteArrayLiteral("Bearer "))))
        return std::nullopt;
    const QByteArray token = header.mid(7).trimmed().toByteArray();
    if (token.isEmpty())
        return std::nullopt;

    UserRecord u;
    if (!db.getUserByTokenHash(hashToken(QString::fromLatin1(token)), &u))
        return std::nullopt;
    return AuthUser{u.id, u.username, u.role};
}

std::optional<qint64> idFromQuery(const QHttpServerRequest &req)
{
    const qint64 id = req.query().queryItemValue(QStringLiteral("id")).toLongLong();
    return id > 0 ? std::optional<qint64>(id) : std::nullopt;
}

} // namespace

constexpr int kDemoClientCount = 10000;

bool ApiServer::start(const QHostAddress &address, quint16 port, const QString &dbPath,
                      const QString &adminUser, const QString &adminPassword, bool demo)
{
    if (m_started)
        return false;

    if (!m_db.open(dbPath)) {
        qCCritical(lcServer).noquote() << "database open failed:" << m_db.lastError();
        return false;
    }

    m_demo = demo;

    qint64 nUsers = m_db.userCount();
    if (nUsers < 0) {
        qCCritical(lcServer).noquote() << "user count query failed:" << m_db.lastError();
        return false;
    }

    if (!adminUser.isEmpty()) {
        if (adminPassword.size() < 8) {
            qCCritical(lcServer) << "--admin-password must be at least 8 characters";
            return false;
        }
        if (nUsers == 0) {
            const qint64 id = m_db.addUser(adminUser, hashPassword(adminPassword),
                                           QString::fromLatin1(kRoleAdmin));
            if (id == 0) {
                qCCritical(lcServer).noquote() << "admin seed failed:" << m_db.lastError();
                return false;
            }
            nUsers = 1;
            qCInfo(lcServer).noquote() << "seeded admin user" << adminUser;
        } else {
            qCInfo(lcServer) << "users exist, admin seed skipped";
        }
    }

    if (demo) {
        UserRecord demoUser;
        if (!m_db.getUserByUsername(QStringLiteral("demo"), &demoUser)) {
            const qint64 id =
                m_db.addUser(QStringLiteral("demo"), hashPassword(QStringLiteral("demo")),
                             QString::fromLatin1(kRoleAdmin));
            if (id == 0) {
                qCCritical(lcServer).noquote() << "demo user seed failed:" << m_db.lastError();
                return false;
            }
            nUsers = m_db.userCount();
            qCInfo(lcServer).noquote()
                << "demo mode: seeded user 'demo'/'demo' (admin), API requires auth as usual";
        } else {
            qCInfo(lcServer) << "demo mode: demo user already present";
        }

        if (m_db.clientCount() == 0) {
            QElapsedTimer timer;
            timer.start();
            if (!m_db.seedDemoClients(kDemoClientCount)) {
                qCCritical(lcServer).noquote() << "demo client seed failed:" << m_db.lastError();
                return false;
            }
            qCInfo(lcServer).noquote()
                << QStringLiteral("demo mode: seeded %1 demo clients in %2 ms")
                       .arg(kDemoClientCount)
                       .arg(timer.elapsed());
        } else {
            qCInfo(lcServer) << "demo mode: database already has clients, seed skipped";
        }
    }

    m_authEnabled = nUsers > 0;
    if (!m_authEnabled)
        qCInfo(lcServer) << "no users yet: API is open (no auth required)";

    if (!m_tcpServer.listen(address, port)) {
        qCCritical(lcServer).noquote() << "listen failed:" << m_tcpServer.errorString();
        return false;
    }
    m_port = m_tcpServer.serverPort();
    // Routes are registered only after a successful listen (and only once):
    // a failed start must not leave duplicate handlers for a retry.
    m_http = new QHttpServer;
    registerRoutes();
    m_http->bind(&m_tcpServer);
    m_started = true;
    return true;
}

bool ApiServer::start(quint16 port, const QString &dbPath, const QString &adminUser,
                      const QString &adminPassword, bool demo)
{
    return start(QHostAddress::LocalHost, port, dbPath, adminUser, adminPassword, demo);
}

void ApiServer::writeAudit(qint64 userId, const QString &action, const QString &entity,
                           qint64 entityId)
{
    if (m_db.addAudit(userId, action, entity, entityId) == 0)
        qCWarning(lcServer).noquote() << "audit write failed:" << m_db.lastError();
}

bool ApiServer::loginThrottled(const QString &peer) const
{
    const qint64 windowStart = QDateTime::currentMSecsSinceEpoch() - kLoginWindowMs;
    for (auto it = m_loginAttempts.begin(); it != m_loginAttempts.end();) {
        it.value().removeIf([windowStart](qint64 attempt) { return attempt < windowStart; });
        if (it.value().isEmpty())
            it = m_loginAttempts.erase(it);
        else
            ++it;
    }
    return m_loginAttempts.value(peer).size() >= kMaxLoginAttempts;
}

void ApiServer::registerRoutes()
{
    if (m_routesRegistered)
        return;
    m_routesRegistered = true;

    m_http->addAfterRequestHandler(
        m_http, [](const QHttpServerRequest &req, QHttpServerResponse &resp) {
            const char *method = QMetaEnum::fromType<QHttpServerRequest::Method>().valueToKey(
                static_cast<int>(req.method()));
            qCInfo(lcHttp).noquote()
                << QStringLiteral("%1 %2 %3")
                       .arg(method ? QLatin1String(method) : QStringLiteral("?"), req.url().path(),
                            QString::number(static_cast<int>(resp.statusCode())));
        });

    m_http->route(QStringLiteral("/api/health"), QHttpServerRequest::Method::Get, [this] {
        QJsonObject response{{QStringLiteral("status"), QStringLiteral("ok")}};
        if (m_demo)
            response.insert(QStringLiteral("demo"), true);
        return QHttpServerResponse(response);
    });

    m_http->route(
        QStringLiteral("/api/auth/login"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &req) {
            if (!bodyIsJson(req))
                return jsonError(QStringLiteral("Content-Type must be application/json"),
                                 QHttpServerResponse::StatusCode::UnsupportedMediaType);

            const QString peer = req.remoteAddress().toString();
            if (loginThrottled(peer))
                return jsonError(QStringLiteral("too many login attempts, try again later"),
                                 QHttpServerResponse::StatusCode::TooManyRequests);

            QString username;
            QString password;
            QString error;
            if (!loginFromBody(req.body(), &username, &password, &error))
                return jsonError(error, QHttpServerResponse::StatusCode::BadRequest);

            UserRecord u;
            if (!m_db.getUserByUsername(username, &u) ||
                !verifyPassword(password, u.passwordHash)) {
                m_loginAttempts[peer].append(QDateTime::currentMSecsSinceEpoch());
                return unauthorized();
            }
            m_loginAttempts.remove(peer);

            m_db.deleteExpiredTokens();
            const QString token = generateToken();
            if (!m_db.storeToken(hashToken(token), u.id))
                return jsonError(m_db.lastError(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
            return QHttpServerResponse(
                QJsonObject{{QStringLiteral("token"), token},
                            {QStringLiteral("user"), userToJson(User{u.id, u.username, u.role})}});
        });

    m_http->route(QStringLiteral("/api/auth/me"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) {
                      const auto me = authenticate(req, m_db, m_authEnabled);
                      if (!me)
                          return unauthorized();
                      return QHttpServerResponse(userToJson(User{me->id, me->username, me->role}));
                  });

    m_http->route(QStringLiteral("/api/users"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) {
                      const auto caller = authenticate(req, m_db, m_authEnabled);
                      if (!caller)
                          return unauthorized();
                      if (caller->role != QLatin1String(kRoleAdmin))
                          return forbidden();

                      const QVector<UserRecord> users = m_db.listUsers();
                      QJsonArray items;
                      for (const UserRecord &u : users)
                          items.append(userToJson(User{u.id, u.username, u.role}));
                      return QHttpServerResponse(QJsonObject{{QStringLiteral("items"), items}});
                  });

    m_http->route(
        QStringLiteral("/api/users"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &req) {
            const auto caller = authenticate(req, m_db, m_authEnabled);
            if (!caller)
                return unauthorized();
            if (caller->role != QLatin1String(kRoleAdmin))
                return forbidden();
            if (!bodyIsJson(req))
                return jsonError(QStringLiteral("Content-Type must be application/json"),
                                 QHttpServerResponse::StatusCode::UnsupportedMediaType);

            QString username;
            QString password;
            QString role;
            QString error;
            if (!userFromBody(req.body(), &username, &password, &role, &error))
                return jsonError(error, QHttpServerResponse::StatusCode::BadRequest);
            // While the API is open (no users yet), any caller is treated as
            // admin and the bootstrap user is always admin — creating a plain
            // 'user' from an unauthenticated request makes no sense.
            const QString effectiveRole = m_authEnabled ? role : QString::fromLatin1(kRoleAdmin);
            const qint64 id = m_db.addUser(username, hashPassword(password), effectiveRole);
            if (id == 0) {
                if (m_db.lastError().contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive))
                    return jsonError(QStringLiteral("username already exists"),
                                     QHttpServerResponse::StatusCode::Conflict);
                return jsonError(m_db.lastError(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
            }
            // The first user locks auth on: every subsequent request must
            // present a token (no restart required).
            m_authEnabled = true;
            writeAudit(caller->id, QStringLiteral("create"), QStringLiteral("user"), id);
            return QHttpServerResponse(userToJson(User{id, username, effectiveRole}),
                                       QHttpServerResponse::StatusCode::Created);
        });

    m_http->route(QStringLiteral("/api/clients"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) {
                      if (!authenticate(req, m_db, m_authEnabled))
                          return unauthorized();
                      const ClientPage page = m_db.listClients(filterFromQuery(req.query()));
                      QJsonArray items;
                      for (const Client &c : page.items)
                          items.append(clientToJson(c));
                      return QHttpServerResponse(QJsonObject{
                          {QStringLiteral("total"), page.total},
                          {QStringLiteral("items"), items},
                      });
                  });

    m_http->route(
        QStringLiteral("/api/clients"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &req) {
            const auto caller = authenticate(req, m_db, m_authEnabled);
            if (!caller)
                return unauthorized();
            if (!bodyIsJson(req))
                return jsonError(QStringLiteral("Content-Type must be application/json"),
                                 QHttpServerResponse::StatusCode::UnsupportedMediaType);

            Client c;
            QString error;
            if (!clientFromBody(req.body(), &c, &error))
                return jsonError(error, QHttpServerResponse::StatusCode::BadRequest);

            const qint64 id = m_db.addClient(c);
            if (id == 0)
                return jsonError(m_db.lastError(),
                                 QHttpServerResponse::StatusCode::InternalServerError);
            writeAudit(caller->id, QStringLiteral("create"), QStringLiteral("client"), id);
            c.id = id;
            m_db.getClient(id, &c);
            return QHttpServerResponse(clientToJson(c), QHttpServerResponse::StatusCode::Created);
        });

    m_http->route(QStringLiteral("/api/client"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) {
                      if (!authenticate(req, m_db, m_authEnabled))
                          return unauthorized();
                      const auto id = idFromQuery(req);
                      Client c;
                      if (!id || !m_db.getClient(*id, &c))
                          return jsonError(QStringLiteral("not found"),
                                           QHttpServerResponse::StatusCode::NotFound);
                      return QHttpServerResponse(clientToJson(c));
                  });

    m_http->route(QStringLiteral("/api/client"), QHttpServerRequest::Method::Put,
                  [this](const QHttpServerRequest &req) {
                      const auto caller = authenticate(req, m_db, m_authEnabled);
                      if (!caller)
                          return unauthorized();
                      if (!bodyIsJson(req))
                          return jsonError(QStringLiteral("Content-Type must be application/json"),
                                           QHttpServerResponse::StatusCode::UnsupportedMediaType);

                      const auto id = idFromQuery(req);
                      Client c;
                      Client updated;
                      QString error;
                      if (!clientFromBody(req.body(), &c, &error))
                          return jsonError(error, QHttpServerResponse::StatusCode::BadRequest);
                      if (!id || !m_db.updateClient(*id, c, &updated))
                          return jsonError(QStringLiteral("not found"),
                                           QHttpServerResponse::StatusCode::NotFound);
                      writeAudit(caller->id, QStringLiteral("update"), QStringLiteral("client"),
                                 *id);
                      return QHttpServerResponse(clientToJson(updated));
                  });

    m_http->route(QStringLiteral("/api/client"), QHttpServerRequest::Method::Delete,
                  [this](const QHttpServerRequest &req) {
                      const auto caller = authenticate(req, m_db, m_authEnabled);
                      if (!caller)
                          return unauthorized();
                      if (caller->role != QLatin1String(kRoleAdmin))
                          return forbidden();

                      const auto id = idFromQuery(req);
                      if (!id || !m_db.deleteClient(*id))
                          return jsonError(QStringLiteral("not found"),
                                           QHttpServerResponse::StatusCode::NotFound);
                      writeAudit(caller->id, QStringLiteral("delete"), QStringLiteral("client"),
                                 *id);
                      return QHttpServerResponse(QJsonObject{{QStringLiteral("deleted"), true}});
                  });

    m_http->route(QStringLiteral("/api/audit"), QHttpServerRequest::Method::Get,
                  [this](const QHttpServerRequest &req) {
                      const auto caller = authenticate(req, m_db, m_authEnabled);
                      if (!caller)
                          return unauthorized();
                      if (caller->role != QLatin1String(kRoleAdmin))
                          return forbidden();

                      const ClientFilter f = filterFromQuery(req.query());
                      const AuditPage page = m_db.listAudit(AuditFilter{f.limit, f.offset});
                      QJsonArray items;
                      for (const AuditEntry &a : page.items)
                          items.append(auditToJson(a));
                      return QHttpServerResponse(QJsonObject{
                          {QStringLiteral("total"), page.total},
                          {QStringLiteral("items"), items},
                      });
                  });
}

} // namespace registro
