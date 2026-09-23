#include "api_client.h"
#include "auth.h"
#include "log_categories.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <functional>

namespace registro {

namespace {

Client clientFromResponse(const QJsonObject &o)
{
    Client c;
    clientFromJson(o, &c);
    return c;
}

QJsonObject errorObject(QNetworkReply *reply)
{
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    return doc.object();
}

QUrl normalizedBaseUrl(const QUrl &url)
{
    QUrl u = url;
    const QString p = u.path();
    if (p.endsWith(QLatin1Char('/')) && p.size() > 1)
        u.setPath(p.chopped(1));
    return u;
}

} // namespace

ApiClient::ApiClient(const QUrl &baseUrl, QObject *parent)
    : QObject(parent), m_baseUrl(normalizedBaseUrl(baseUrl)), m_nam(new QNetworkAccessManager(this))
{
    qRegisterMetaType<Client>();
    qRegisterMetaType<QVector<Client>>();
}

bool ApiClient::isAdmin() const
{
    return m_userRole == QLatin1String(kRoleAdmin);
}

QNetworkReply *ApiClient::sendRequest(const QByteArray &method, const QString &path,
                                      const QJsonObject &body)
{
    QNetworkRequest req(m_baseUrl.resolved(QUrl(path)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (m_requestTimeout > 0)
        req.setTransferTimeout(m_requestTimeout);
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + m_token.toLatin1());

    const QByteArray payload =
        body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);
    if (method == "GET")
        return m_nam->get(req);
    if (method == "POST")
        return m_nam->post(req, payload);
    if (method == "PUT")
        return m_nam->put(req, payload);
    return m_nam->deleteResource(req);
}

void ApiClient::request(const QByteArray &method, const QString &path, const QJsonObject &body,
                        const std::function<void(const QJsonObject &)> &onSuccess)
{
    QNetworkReply *reply = sendRequest(method, path, body);
    const qint64 session = m_session;
    connect(
        reply, &QNetworkReply::finished, this, [this, reply, session, method, path, onSuccess]() {
            reply->deleteLater();
            if (session != m_session)
                return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString req = QStringLiteral("%1 %2").arg(QString::fromLatin1(method), path);
            if (status >= 200 && status < 300) {
                qCInfo(lcClient).noquote() << req << "->" << status;
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                onSuccess(doc.object());
                return;
            }
            if (m_loginInProgress) {
                m_loginInProgress = false;
                emit loginStateChanged();
            }
            if (status == 401) {
                qCWarning(lcClient).noquote() << req << "-> 401";
                emit authenticationNeeded();
                return;
            }
            const QJsonObject err = errorObject(reply);
            const QString message = err.value(QStringLiteral("error")).toString();
            if (status > 0) {
                const QString text =
                    message.isEmpty() ? QStringLiteral("HTTP %1").arg(status) : message;
                qCWarning(lcClient).noquote() << req << "->" << status << text;
                emit errorOccurred(text);
            } else {
                qCWarning(lcClient).noquote() << req << "-> network:" << reply->errorString();
                emit networkErrorOccurred(reply->errorString());
            }
            if (m_checking) {
                m_checking = false;
                m_loggedIn = false;
                emit sessionChanged();
            }
        });
}

void ApiClient::setRequestTimeout(int msecs)
{
    m_requestTimeout = msecs > 0 ? msecs : 0;
}

void ApiClient::initialize()
{
    request(QByteArrayLiteral("GET"), QStringLiteral("/api/auth/me"), {},
            [this](const QJsonObject &o) {
                m_loggedIn = true;
                m_checking = false;
                m_userName = o.value(QStringLiteral("username")).toString();
                m_userRole = o.value(QStringLiteral("role")).toString();
                emit sessionChanged();
            });
    connect(this, &ApiClient::authenticationNeeded, this, [this]() {
        if (m_checking) {
            m_checking = false;
            m_loggedIn = false;
            emit sessionChanged();
        }
    });
}

void ApiClient::restoreToken(const QString &token)
{
    m_token = token.trimmed();
}

void ApiClient::login(const QString &username, const QString &password)
{
    m_loginInProgress = true;
    emit loginStateChanged();
    request(
        QByteArrayLiteral("POST"), QStringLiteral("/api/auth/login"),
        QJsonObject{{QStringLiteral("username"), username}, {QStringLiteral("password"), password}},
        [this](const QJsonObject &o) {
            m_loginInProgress = false;
            emit loginStateChanged();
            m_token = o.value(QStringLiteral("token")).toString();
            const QJsonObject user = o.value(QStringLiteral("user")).toObject();
            m_loggedIn = true;
            m_checking = false;
            m_userName = user.value(QStringLiteral("username")).toString();
            m_userRole = user.value(QStringLiteral("role")).toString();
            emit sessionChanged();
        });
}

void ApiClient::logout()
{
    if (m_loginInProgress) {
        m_loginInProgress = false;
        emit loginStateChanged();
    }
    ++m_session;
    m_token.clear();
    m_checking = false;
    m_loggedIn = false;
    m_userName.clear();
    m_userRole.clear();
    emit sessionChanged();
}

void ApiClient::setBaseUrl(const QString &url)
{
    const QUrl normalized = normalizedBaseUrl(QUrl::fromUserInput(url.trimmed()));
    if (!normalized.isValid() || m_baseUrl == normalized)
        return;
    m_baseUrl = normalized;
    emit baseUrlChanged();
    ++m_session;
    m_token.clear();
    m_checking = false;
    m_loggedIn = false;
    if (m_loginInProgress) {
        m_loginInProgress = false;
        emit loginStateChanged();
    }
    m_userName.clear();
    m_userRole.clear();
    emit sessionChanged();
    emit authenticationNeeded();
}

void ApiClient::fetchClients(const QString &search, const QString &status, qint64 offset,
                             qint64 limit)
{
    doFetchClients(search, status, offset, limit);
}

void ApiClient::doFetchClients(const QString &search, const QString &status, qint64 offset,
                               qint64 limit)
{
    QUrlQuery q;
    if (!search.isEmpty())
        q.addQueryItem(QStringLiteral("search"), search);
    if (!status.isEmpty())
        q.addQueryItem(QStringLiteral("status"), status);
    q.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit));

    QString path = QStringLiteral("/api/clients");
    if (!q.isEmpty())
        path += QLatin1Char('?') + q.toString();

    request(QByteArrayLiteral("GET"), path, {}, [this](const QJsonObject &o) {
        const auto items = o.value(QStringLiteral("items")).toArray();
        QVector<Client> clients;
        clients.reserve(items.size());
        for (const QJsonValue &v : items)
            clients.append(clientFromResponse(v.toObject()));
        emit clientsLoaded(static_cast<qint64>(o.value(QStringLiteral("total")).toDouble()),
                           clients);
    });
}

void ApiClient::saveClient(qint64 id, const QString &fullName, const QString &org,
                           const QString &phone, const QString &email, const QString &notes,
                           const QString &status)
{
    Client c;
    c.id = id;
    c.fullName = fullName;
    c.org = org;
    c.phone = phone;
    c.email = email;
    c.notes = notes;
    c.status = status.isEmpty() ? QString::fromLatin1(kStatusActive) : status;
    doSaveClient(id, c);
}

void ApiClient::doSaveClient(qint64 id, const Client &c)
{
    const QJsonObject body = clientToJson(c);
    if (id > 0) {
        request(QByteArrayLiteral("PUT"), QStringLiteral("/api/client?id=%1").arg(id), body,
                [this, id](const QJsonObject &o) { emit clientSaved(id, clientFromResponse(o)); });
    } else {
        request(QByteArrayLiteral("POST"), QStringLiteral("/api/clients"), body,
                [this](const QJsonObject &o) {
                    const Client saved = clientFromResponse(o);
                    emit clientSaved(saved.id, saved);
                    emit clientCreated(saved.id);
                });
    }
}

void ApiClient::deleteClient(qint64 id)
{
    doDeleteClient(id);
}

void ApiClient::doDeleteClient(qint64 id)
{
    request(QByteArrayLiteral("DELETE"), QStringLiteral("/api/client?id=%1").arg(id), {},
            [this, id](const QJsonObject &) { emit clientDeleted(id); });
}

} // namespace registro
