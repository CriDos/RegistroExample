#include "api_server.h"

#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtTest>

using namespace registro;

namespace {

struct HttpResult {
    int status = 0;
    QJsonObject json;
    bool ok = false;
};

QString g_token;

void attachAuth(QNetworkRequest &req)
{
    if (!g_token.isEmpty())
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + g_token.toLatin1());
}

HttpResult httpCall(QNetworkAccessManager *nam, const QString &method, const QUrl &url,
                    const QJsonObject &body = {})
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    attachAuth(req);

    HttpResult r;

    QNetworkReply *reply = nullptr;
    if (method == QStringLiteral("GET"))
        reply = nam->get(req);
    else if (method == QStringLiteral("POST"))
        reply = nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    else if (method == QStringLiteral("PUT"))
        reply = nam->put(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    else if (method == QStringLiteral("DELETE"))
        reply = nam->deleteResource(req);
    else {
        r.status = -1;
        r.json = QJsonObject{
            {QStringLiteral("error"), QStringLiteral("unsupported method in test helper")}};
        return r;
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    r.ok = r.status > 0;
    if (!data.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        r.json = doc.object();
    }
    return r;
}

HttpResult httpCallRaw(QNetworkAccessManager *nam, const QString &method, const QUrl &url,
                       const QByteArray &body, const QString &contentType)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    attachAuth(req);

    QNetworkReply *reply = nullptr;
    if (method == QStringLiteral("POST"))
        reply = nam->post(req, body);
    else if (method == QStringLiteral("PUT"))
        reply = nam->put(req, body);
    else
        return {};

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    HttpResult r;
    r.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    if (!data.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        r.json = doc.object();
    }
    return r;
}

} // namespace

class TestApi final : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QNetworkAccessManager *m_nam = nullptr;
    quint16 m_port = 0;
    registro::ApiServer m_server;
    QString m_adminToken;

    QUrl url(const QString &path) const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_port).arg(path));
    }

    QString login(const QString &username, const QString &password)
    {
        return httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/auth/login")),
                        QJsonObject{{QStringLiteral("username"), username},
                                    {QStringLiteral("password"), password}})
            .json.value(QStringLiteral("token"))
            .toString();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_nam = new QNetworkAccessManager(this);

        QVERIFY(m_server.start(0, m_dir.filePath(QStringLiteral("api.db")), QStringLiteral("admin"),
                               QStringLiteral("secret123")));
        m_port = m_server.port();
        QVERIFY(m_port > 0);

        m_adminToken = login(QStringLiteral("admin"), QStringLiteral("secret123"));
        QVERIFY(!m_adminToken.isEmpty());
        g_token = m_adminToken;
    }

    void healthEndpoint()
    {
        const HttpResult r =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/health")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    }

    void openModeWithoutAuth()
    {
        registro::ApiServer openServer;
        QVERIFY(openServer.start(0, m_dir.filePath(QStringLiteral("open.db"))));

        const QUrl base = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(openServer.port()));
        g_token.clear();

        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), base.resolved(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Open Client")}});
        QCOMPARE(r.status, 201);
        const qint64 id = r.json.value(QStringLiteral("id")).toInteger();
        QVERIFY(id > 0);

        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), 1);

        r = httpCall(m_nam, QStringLiteral("DELETE"),
                     base.resolved(QStringLiteral("/api/client?id=%1").arg(id)));
        QCOMPARE(r.status, 200);

        r = httpCall(m_nam, QStringLiteral("GET"),
                     base.resolved(QStringLiteral("/api/client?id=%1").arg(id)));
        QCOMPARE(r.status, 404);

        g_token = m_adminToken;
    }

    void openModeBootstrapLocksAuthAfterFirstUser()
    {
        registro::ApiServer openServer;
        QVERIFY(openServer.start(0, m_dir.filePath(QStringLiteral("open-bootstrap.db"))));
        const QUrl base = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(openServer.port()));
        g_token.clear();

        // Open mode: anyone may create the first user; it is forced to admin
        // (bootstrap), regardless of the requested role.
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), base.resolved(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("root")},
                                 {QStringLiteral("password"), QStringLiteral("rootpass12")},
                                 {QStringLiteral("role"), QStringLiteral("user")}});
        QCOMPARE(r.status, 201);
        QCOMPARE(r.json.value(QStringLiteral("role")).toString(), QStringLiteral("admin"));

        // Auth flips on immediately: unauthenticated access is rejected
        // without a server restart.
        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 401);
        r = httpCall(m_nam, QStringLiteral("POST"), base.resolved(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("x")},
                                 {QStringLiteral("password"), QStringLiteral("xpass1234")}});
        QCOMPARE(r.status, 401);

        r = httpCall(m_nam, QStringLiteral("POST"),
                     base.resolved(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("root")},
                                 {QStringLiteral("password"), QStringLiteral("rootpass12")}});
        QCOMPARE(r.status, 200);
        const QString token = r.json.value(QStringLiteral("token")).toString();
        QVERIFY(!token.isEmpty());
        QCOMPARE(r.json.value(QStringLiteral("user"))
                     .toObject()
                     .value(QStringLiteral("role"))
                     .toString(),
                 QStringLiteral("admin"));

        g_token = token;
        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        g_token = m_adminToken;
    }

    void demoModeSeedsAccountAndAdvertises()
    {
        const QString db = m_dir.filePath(QStringLiteral("demo.db"));

        // First run in demo mode: the demo/demo admin account is seeded.
        registro::ApiServer demoServer;
        QVERIFY(demoServer.start(0, db, {}, {}, true));
        const QUrl base = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(demoServer.port()));

        HttpResult health =
            httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/health")));
        QCOMPARE(health.status, 200);
        QCOMPARE(health.json.value(QStringLiteral("demo")).toBool(), true);

        HttpResult r = httpCall(m_nam, QStringLiteral("POST"),
                                base.resolved(QStringLiteral("/api/auth/login")),
                                QJsonObject{{QStringLiteral("username"), QStringLiteral("demo")},
                                            {QStringLiteral("password"), QStringLiteral("demo")}});
        QCOMPARE(r.status, 200);
        const QString token = r.json.value(QStringLiteral("token")).toString();
        QVERIFY(!token.isEmpty());
        const QJsonObject user = r.json.value(QStringLiteral("user")).toObject();
        QCOMPARE(user.value(QStringLiteral("username")).toString(), QStringLiteral("demo"));
        QCOMPARE(user.value(QStringLiteral("role")).toString(), QStringLiteral("admin"));

        g_token = token;
        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/auth/me")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("username")).toString(), QStringLiteral("demo"));

        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/users")));
        QCOMPARE(r.status, 200);
        const QJsonArray users = r.json.value(QStringLiteral("items")).toArray();
        QCOMPARE(users.size(), 1);
        QCOMPARE(users.at(0).toObject().value(QStringLiteral("username")).toString(),
                 QStringLiteral("demo"));

        // The demo database was populated with demo clients on first run.
        r = httpCall(m_nam, QStringLiteral("GET"), base.resolved(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), 10000);
        const QJsonArray clients = r.json.value(QStringLiteral("items")).toArray();
        QCOMPARE(clients.size(), 50);
        const QJsonObject first = clients.at(0).toObject();
        QVERIFY(!first.value(QStringLiteral("full_name")).toString().isEmpty());
        QVERIFY(first.value(QStringLiteral("phone")).toString().startsWith(QStringLiteral("+7")));
        QVERIFY(first.value(QStringLiteral("email")).toString().contains(QLatin1Char('@')));

        r = httpCall(m_nam, QStringLiteral("POST"), base.resolved(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Demo Client")}});
        QCOMPARE(r.status, 201);

        // Restart on the same database must not duplicate the demo account
        // or re-seed clients.
        registro::ApiServer again;
        QVERIFY(again.start(0, db, {}, {}, true));
        const QUrl againBase = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(again.port()));
        const QString againToken =
            httpCall(m_nam, QStringLiteral("POST"),
                     againBase.resolved(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("demo")},
                                 {QStringLiteral("password"), QStringLiteral("demo")}})
                .json.value(QStringLiteral("token"))
                .toString();
        QVERIFY(!againToken.isEmpty());
        g_token = againToken;
        r = httpCall(m_nam, QStringLiteral("GET"),
                     againBase.resolved(QStringLiteral("/api/users")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), 1);
        r = httpCall(m_nam, QStringLiteral("GET"),
                     againBase.resolved(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), 10001);

        g_token = m_adminToken;
    }

    void fullCrudFlow()
    {
        QJsonObject body{
            {QStringLiteral("full_name"), QStringLiteral("ООО Ромашка")},
            {QStringLiteral("org"), QStringLiteral("Ромашка Group")},
            {QStringLiteral("phone"), QStringLiteral("+7 900 111-22-33")},
            {QStringLiteral("email"), QStringLiteral("info@romashka.ru")},
        };
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")), body);
        QCOMPARE(r.status, 201);
        const qint64 id = r.json.value(QStringLiteral("id")).toInteger();
        QVERIFY(id > 0);
        QCOMPARE(r.json.value(QStringLiteral("full_name")).toString(),
                 QStringLiteral("ООО Ромашка"));

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), 1);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), 1);

        r = httpCall(m_nam, QStringLiteral("GET"),
                     url(QStringLiteral("/api/client?id=%1").arg(id)));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("email")).toString(),
                 QStringLiteral("info@romashka.ru"));

        body.insert(QStringLiteral("full_name"), QStringLiteral("ООО Ромашка Плюс"));
        r = httpCall(m_nam, QStringLiteral("PUT"), url(QStringLiteral("/api/client?id=%1").arg(id)),
                     body);
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("full_name")).toString(),
                 QStringLiteral("ООО Ромашка Плюс"));

        r = httpCall(m_nam, QStringLiteral("DELETE"),
                     url(QStringLiteral("/api/client?id=%1").arg(id)));
        QCOMPARE(r.status, 200);

        r = httpCall(m_nam, QStringLiteral("GET"),
                     url(QStringLiteral("/api/client?id=%1").arg(id)));
        QCOMPARE(r.status, 404);
    }

    void missingFieldRejected()
    {
        const HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("org"), QStringLiteral("no name")}});
        QCOMPARE(r.status, 400);
        QVERIFY(r.json.contains(QStringLiteral("error")));
    }

    void oversizedFieldsRejected()
    {
        const auto longText = [](int n) { return QString(n, QLatin1Char('x')); };

        QJsonObject body{{QStringLiteral("full_name"), longText(201)}};
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")), body);
        QCOMPARE(r.status, 400);
        QVERIFY(
            r.json.value(QStringLiteral("error")).toString().contains(QStringLiteral("full_name")));

        body = QJsonObject{{QStringLiteral("full_name"), QStringLiteral("ok")},
                           {QStringLiteral("phone"), longText(51)}};
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")), body);
        QCOMPARE(r.status, 400);
        QVERIFY(r.json.value(QStringLiteral("error")).toString().contains(QStringLiteral("phone")));

        body = QJsonObject{{QStringLiteral("full_name"), QStringLiteral("ok")},
                           {QStringLiteral("org"), longText(200)}};
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")), body);
        QCOMPARE(r.status, 201);
    }

    void invalidJsonRejected()
    {
        QNetworkRequest req(url(QStringLiteral("/api/clients")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        attachAuth(req);

        QNetworkReply *reply = m_nam->post(req, QByteArrayLiteral("{not json"));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        QCOMPARE(status, 400);
    }

    void notFoundPaths()
    {
        HttpResult r =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/client?id=424242")));
        QCOMPARE(r.status, 404);

        r = httpCall(m_nam, QStringLiteral("PUT"), url(QStringLiteral("/api/client?id=424242")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Ghost")}});
        QCOMPARE(r.status, 404);

        r = httpCall(m_nam, QStringLiteral("DELETE"), url(QStringLiteral("/api/client?id=424242")));
        QCOMPARE(r.status, 404);
    }

    void repeatedDeleteReturns404()
    {
        const qint64 id =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Gone Person")}})
                .json.value(QStringLiteral("id"))
                .toInteger();
        QVERIFY(id > 0);

        QCOMPARE(httpCall(m_nam, QStringLiteral("DELETE"),
                          url(QStringLiteral("/api/client?id=%1").arg(id)))
                     .status,
                 200);
        QCOMPARE(httpCall(m_nam, QStringLiteral("DELETE"),
                          url(QStringLiteral("/api/client?id=%1").arg(id)))
                     .status,
                 404);
        QCOMPARE(
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/client?id=%1").arg(id)))
                .status,
            404);
    }

    void limitDefaultsAndClamps()
    {
        const auto create = [this](const QString &name) {
            return httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                            QJsonObject{{QStringLiteral("full_name"), name}})
                .json.value(QStringLiteral("id"))
                .toInteger();
        };
        create(QStringLiteral("Clamp One"));
        create(QStringLiteral("Clamp Two"));

        const qint64 base =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients")))
                .json.value(QStringLiteral("total"))
                .toInteger();
        QVERIFY(base > 0);
        const QJsonArray newestFirst =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients")))
                .json.value(QStringLiteral("items"))
                .toArray();
        QCOMPARE(newestFirst.at(0).toObject().value(QStringLiteral("full_name")).toString(),
                 QStringLiteral("Clamp Two"));
        QCOMPARE(newestFirst.at(1).toObject().value(QStringLiteral("full_name")).toString(),
                 QStringLiteral("Clamp One"));

        HttpResult r =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients?limit=50000")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), base);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), base);

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients?limit=-5")));
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), base);
        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients?limit=abc")));
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), base);

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients?limit=0")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), base);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), 0);
    }

    void unknownRouteReturns404()
    {
        const HttpResult r = httpCall(m_nam, QStringLiteral("GET"),
                                      url(QStringLiteral("/api/definitely-not-a-route")));
        QCOMPARE(r.status, 404);
    }

    void nonJsonContentTypeRejected()
    {
        const QByteArray body =
            QJsonDocument(QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Text Plain")}})
                .toJson(QJsonDocument::Compact);

        HttpResult r =
            httpCallRaw(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")), body,
                        QStringLiteral("text/plain"));
        QCOMPARE(r.status, 415);

        r = httpCallRaw(m_nam, QStringLiteral("PUT"), url(QStringLiteral("/api/client?id=1")), body,
                        QStringLiteral("text/plain"));
        QCOMPARE(r.status, 415);
    }

    void invalidStatusRejected()
    {
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Bad Status")},
                                 {QStringLiteral("status"), QStringLiteral("banned")}});
        QCOMPARE(r.status, 400);
        QVERIFY(
            r.json.value(QStringLiteral("error")).toString().contains(QStringLiteral("status")));

        const qint64 id =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Good Status")}})
                .json.value(QStringLiteral("id"))
                .toInteger();
        QVERIFY(id > 0);

        r = httpCall(m_nam, QStringLiteral("PUT"), url(QStringLiteral("/api/client?id=%1").arg(id)),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Good Status")},
                                 {QStringLiteral("status"), QStringLiteral("fresh")}});
        QCOMPARE(r.status, 400);
    }

    void loginFailures()
    {
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("ghost")},
                                 {QStringLiteral("password"), QStringLiteral("xpass1234")}});
        QCOMPARE(r.status, 401);
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("admin")},
                                 {QStringLiteral("password"), QStringLiteral("wrong-password")}});
        QCOMPARE(r.status, 401);

        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("admin")}});
        QCOMPARE(r.status, 400);
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/auth/login")),
                     QJsonObject{{QStringLiteral("password"), QStringLiteral("xpass1234")}});
        QCOMPARE(r.status, 400);
    }

    void meEndpoint()
    {
        const HttpResult r =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/auth/me")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("username")).toString(), QStringLiteral("admin"));
        QCOMPARE(r.json.value(QStringLiteral("role")).toString(), QStringLiteral("admin"));
        QVERIFY(r.json.value(QStringLiteral("id")).toInteger() > 0);
    }

    void unauthenticatedRejected()
    {
        g_token.clear();
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients"))).status,
                 401);
        QCOMPARE(
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/client?id=1"))).status,
            401);
        QCOMPARE(httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                          QJsonObject{{QStringLiteral("full_name"), QStringLiteral("X")}})
                     .status,
                 401);
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/auth/me"))).status,
                 401);
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/users"))).status,
                 401);

        g_token = QStringLiteral("deadbeef");
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients"))).status,
                 401);

        g_token.clear();
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/health"))).status,
                 200);
        g_token = m_adminToken;
    }

    void adminCreatesAndListsUsers()
    {
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("bob")},
                                 {QStringLiteral("password"), QStringLiteral("bobpass123")},
                                 {QStringLiteral("role"), QStringLiteral("user")}});
        QCOMPARE(r.status, 201);
        QCOMPARE(r.json.value(QStringLiteral("username")).toString(), QStringLiteral("bob"));
        QCOMPARE(r.json.value(QStringLiteral("role")).toString(), QStringLiteral("user"));
        QVERIFY(!r.json.contains(QStringLiteral("password")));

        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("bob")},
                                 {QStringLiteral("password"), QStringLiteral("bobpass123")}});
        QCOMPARE(r.status, 409);

        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("short")},
                                 {QStringLiteral("password"), QStringLiteral("tiny")}});
        QCOMPARE(r.status, 400);
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("rooty")},
                                 {QStringLiteral("password"), QStringLiteral("rootpass12")},
                                 {QStringLiteral("role"), QStringLiteral("superuser")}});
        QCOMPARE(r.status, 400);

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/users")));
        QCOMPARE(r.status, 200);
        bool hasBob = false;
        const QJsonArray items = r.json.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &v : items) {
            if (v.toObject().value(QStringLiteral("username")).toString() ==
                QStringLiteral("bob")) {
                hasBob = true;
            }
        }
        QVERIFY(hasBob);
    }

    void nonAdminForbidden()
    {
        g_token = m_adminToken;
        const HttpResult created =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("carol")},
                                 {QStringLiteral("password"), QStringLiteral("carolpass1")}});
        QCOMPARE(created.status, 201);

        const QString carolToken = login(QStringLiteral("carol"), QStringLiteral("carolpass1"));
        QVERIFY(!carolToken.isEmpty());
        g_token = carolToken;

        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients"))).status,
                 200);
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/users"))).status,
                 403);
        QCOMPARE(httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                          QJsonObject{{QStringLiteral("username"), QStringLiteral("x")},
                                      {QStringLiteral("password"), QStringLiteral("xpass1234")}})
                     .status,
                 403);
        QCOMPARE(httpCall(m_nam, QStringLiteral("DELETE"), url(QStringLiteral("/api/client?id=1")))
                     .status,
                 403);

        g_token = m_adminToken;
    }

    void clientsCarryNotes()
    {
        HttpResult r =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Note Client")},
                                 {QStringLiteral("notes"), QStringLiteral("первая заметка")}});
        QCOMPARE(r.status, 201);
        const qint64 clientId = r.json.value(QStringLiteral("id")).toInteger();
        QVERIFY(clientId > 0);
        QCOMPARE(r.json.value(QStringLiteral("notes")).toString(),
                 QStringLiteral("первая заметка"));

        r = httpCall(
            m_nam, QStringLiteral("PUT"), url(QStringLiteral("/api/client?id=%1").arg(clientId)),
            QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Note Client")},
                        {QStringLiteral("notes"), QStringLiteral("вторая заметка\n\nтретья")}});
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("notes")).toString(),
                 QStringLiteral("вторая заметка\n\nтретья"));

        r = httpCall(m_nam, QStringLiteral("GET"),
                     url(QStringLiteral("/api/client?id=%1").arg(clientId)));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("notes")).toString(),
                 QStringLiteral("вторая заметка\n\nтретья"));

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients")));
        QCOMPARE(r.status, 200);
        bool found = false;
        const QJsonArray items = r.json.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &v : items) {
            if (v.toObject().value(QStringLiteral("id")).toInteger() == clientId) {
                QCOMPARE(v.toObject().value(QStringLiteral("notes")).toString(),
                         QStringLiteral("вторая заметка\n\nтретья"));
                found = true;
            }
        }
        QVERIFY(found);

        const QString tooLongNotes(4001, QLatin1Char('x'));
        r = httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Long Notes")},
                                 {QStringLiteral("notes"), tooLongNotes}});
        QCOMPARE(r.status, 400);
        QVERIFY(r.json.value(QStringLiteral("error")).toString().contains(QStringLiteral("notes")));

        r = httpCall(m_nam, QStringLiteral("PUT"),
                     url(QStringLiteral("/api/client?id=%1").arg(clientId)),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Note Client")},
                                 {QStringLiteral("notes"), tooLongNotes}});
        QCOMPARE(r.status, 400);
    }

    void auditEndpoint()
    {
        const qint64 before =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/audit")))
                .json.value(QStringLiteral("total"))
                .toInteger();
        QVERIFY(before > 0);

        const qint64 clientId =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                     QJsonObject{{QStringLiteral("full_name"), QStringLiteral("Audit Target")}})
                .json.value(QStringLiteral("id"))
                .toInteger();

        HttpResult r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/audit")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), before + 1);

        const QJsonArray items = r.json.value(QStringLiteral("items")).toArray();
        QCOMPARE(items.at(0).toObject().value(QStringLiteral("entity")).toString(),
                 QStringLiteral("client"));
        QCOMPARE(items.at(0).toObject().value(QStringLiteral("entity_id")).toInteger(), clientId);
        QCOMPARE(items.at(0).toObject().value(QStringLiteral("action")).toString(),
                 QStringLiteral("create"));
        QVERIFY(items.at(0).toObject().value(QStringLiteral("user_id")).toInteger() > 0);

        r = httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/audit?limit=1")));
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), before + 1);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), 1);

        g_token = m_adminToken;
        const HttpResult created =
            httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/users")),
                     QJsonObject{{QStringLiteral("username"), QStringLiteral("dave")},
                                 {QStringLiteral("password"), QStringLiteral("davepass12")}});
        QCOMPARE(created.status, 201);
        const QString daveToken = login(QStringLiteral("dave"), QStringLiteral("davepass12"));
        QVERIFY(!daveToken.isEmpty());
        g_token = daveToken;
        QCOMPARE(httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/audit"))).status,
                 403);
        g_token = m_adminToken;
    }

    void searchAndPaginationOverHttp()
    {
        const auto create = [this](const QString &name) {
            return httpCall(m_nam, QStringLiteral("POST"), url(QStringLiteral("/api/clients")),
                            QJsonObject{{QStringLiteral("full_name"), name}})
                .json.value(QStringLiteral("id"))
                .toInteger();
        };

        const qint64 beforeTotal =
            httpCall(m_nam, QStringLiteral("GET"), url(QStringLiteral("/api/clients")))
                .json.value(QStringLiteral("total"))
                .toInteger();
        const qint64 beforeSearch = httpCall(m_nam, QStringLiteral("GET"),
                                             url(QStringLiteral("/api/clients?search=Search")))
                                        .json.value(QStringLiteral("total"))
                                        .toInteger();

        create(QStringLiteral("Alice Search"));
        create(QStringLiteral("Bob Search"));
        create(QStringLiteral("Carol Other"));

        HttpResult r = httpCall(m_nam, QStringLiteral("GET"),
                                url(QStringLiteral("/api/clients?search=Search")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), beforeSearch + 2);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), beforeSearch + 2);

        r = httpCall(m_nam, QStringLiteral("GET"),
                     url(QStringLiteral("/api/clients?limit=2&offset=1")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), beforeTotal + 3);
        QCOMPARE(r.json.value(QStringLiteral("items")).toArray().size(), 2);

        r = httpCall(m_nam, QStringLiteral("GET"),
                     url(QStringLiteral("/api/clients?status=archived")));
        QCOMPARE(r.status, 200);
        QCOMPARE(r.json.value(QStringLiteral("total")).toInteger(), 0);
    }
};

QTEST_GUILESS_MAIN(TestApi)
#include "test_api.moc"
