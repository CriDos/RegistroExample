#pragma once

#include <QObject>
#include <QString>

namespace registro {

class ConfigStore;

QString defaultClientConfigPath();

// QML-facing config singleton (Registro.Client "Config"). Backed by the shared
// ConfigStore ([client] section). The remembered login token is persisted
// as-is (plaintext, cross-platform); with remember=false it is never written.
class Config : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    Q_PROPERTY(QString token READ token WRITE setToken NOTIFY tokenChanged)
    Q_PROPERTY(QString lastUser READ lastUser WRITE setLastUser NOTIFY lastUserChanged)
    Q_PROPERTY(bool remember READ remember WRITE setRemember NOTIFY rememberChanged)
    Q_PROPERTY(bool demo READ demo NOTIFY demoChanged)

public:
    explicit Config(const QString &path, QObject *parent = nullptr);
    ~Config() override;

    QString serverUrl() const;
    void setServerUrl(const QString &url);

    QString token() const;
    void setToken(const QString &token);

    QString lastUser() const;
    void setLastUser(const QString &user);

    bool remember() const;
    void setRemember(bool remember);

    // Demo mode comes from the config file only (never probed from the server):
    // it drives the login-form prefill and the «(демо-режим)» title suffix.
    bool demo() const;

    Q_INVOKABLE void save();

signals:
    void serverUrlChanged();
    void tokenChanged();
    void lastUserChanged();
    void rememberChanged();
    void demoChanged();

private:
    ConfigStore *m_store;
};

} // namespace registro