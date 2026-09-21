#pragma once

#include "client.h"
#include "log.h"
#include "log_categories.h"

#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <QtGlobal>

class QNetworkAccessManager;
class QNetworkReply;

namespace registro {

class ApiClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY sessionChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY sessionChanged)
    Q_PROPERTY(QString userRole READ userRole NOTIFY sessionChanged)
    Q_PROPERTY(bool isAdmin READ isAdmin NOTIFY sessionChanged)
    Q_PROPERTY(bool loginInProgress READ loginInProgress NOTIFY loginStateChanged)
    Q_PROPERTY(QString token READ token NOTIFY sessionChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)

public:
    explicit ApiClient(const QUrl &baseUrl, QObject *parent = nullptr);

    bool checking() const { return m_checking; }
    bool loggedIn() const { return m_loggedIn; }
    QString userName() const { return m_userName; }
    QString userRole() const { return m_userRole; }
    bool isAdmin() const;
    bool loginInProgress() const { return m_loginInProgress; }
    QString token() const { return m_token; }
    QString baseUrl() const { return m_baseUrl.toString(); }

    Q_INVOKABLE void initialize();
    Q_INVOKABLE void restoreToken(const QString &token);
    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void setBaseUrl(const QString &url);

    Q_INVOKABLE void fetchClients(const QString &search, const QString &status, qint64 offset,
                                  qint64 limit = kMaxPageSize);
    Q_INVOKABLE void saveClient(qint64 id, const QString &fullName, const QString &org,
                                const QString &phone, const QString &email, const QString &notes,
                                const QString &status);
    Q_INVOKABLE void deleteClient(qint64 id);

    // Per-request transfer timeout in milliseconds (0 = no timeout). The
    // default keeps the UI from spinning forever on a hung server; tests use
    // a smaller value.
    void setRequestTimeout(int msecs);
    int requestTimeout() const { return m_requestTimeout; }

signals:
    void sessionChanged();
    void authenticationNeeded();
    void baseUrlChanged();
    void loginStateChanged();

    void errorOccurred(const QString &message);
    void networkErrorOccurred(const QString &message);

    void clientsLoaded(qint64 total, const QVector<Client> &items);
    void clientSaved(qint64 id, const Client &client);
    void clientCreated(qint64 id);
    void clientDeleted(qint64 id);

protected:
    virtual void doFetchClients(const QString &search, const QString &status, qint64 offset,
                                qint64 limit);
    virtual void doSaveClient(qint64 id, const Client &c);
    virtual void doDeleteClient(qint64 id);

private:
    QNetworkReply *sendRequest(const QByteArray &method, const QString &path,
                               const QJsonObject &body);
    void request(const QByteArray &method, const QString &path, const QJsonObject &body,
                 const std::function<void(const QJsonObject &)> &onSuccess);

    QUrl m_baseUrl;
    QNetworkAccessManager *m_nam = nullptr;
    QString m_token;
    qint64 m_session = 0;
    int m_requestTimeout = 15000;
    bool m_checking = true;
    bool m_loggedIn = false;
    bool m_loginInProgress = false;
    QString m_userName;
    QString m_userRole;
};

} // namespace registro
