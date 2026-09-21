#pragma once

#include "api_client.h"
#include "client_limits.h"
#include "client_model.h"
#include "config.h"

#include <QColor>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QtGlobal>
#include <QtQml/qqml.h>

namespace registro {

// QML-facing singletons. The real objects are constructed in main() and bound
// via qmlRegisterSingletonInstance(); create() only exists to satisfy the
// QML_SINGLETON metadata contract and must never be reached by the engine.
class ApiSingleton : public ApiClient {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Api)

public:
    explicit ApiSingleton(const QUrl &baseUrl, QObject *parent = nullptr)
        : ApiClient(baseUrl, parent)
    {
    }

    static ApiSingleton *create(QQmlEngine *, QJSEngine *)
    {
        qFatal("Api must be registered via qmlRegisterSingletonInstance()");
        Q_UNREACHABLE_RETURN(nullptr);
    }
};

class ClientModelSingleton : public ClientModel {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(ClientModel)

public:
    explicit ClientModelSingleton(ApiClient *api, QObject *parent = nullptr)
        : ClientModel(api, parent)
    {
    }

    static ClientModelSingleton *create(QQmlEngine *, QJSEngine *)
    {
        qFatal("ClientModel must be registered via qmlRegisterSingletonInstance()");
        Q_UNREACHABLE_RETURN(nullptr);
    }
};

class ConfigSingleton : public Config {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Config)

public:
    explicit ConfigSingleton(const QString &path, QObject *parent = nullptr) : Config(path, parent)
    {
    }

    static ConfigSingleton *create(QQmlEngine *, QJSEngine *)
    {
        qFatal("Config must be registered via qmlRegisterSingletonInstance()");
        Q_UNREACHABLE_RETURN(nullptr);
    }
};

class LimitsSingleton : public Limits {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(Limits)

public:
    static LimitsSingleton *create(QQmlEngine *, QJSEngine *)
    {
        qFatal("Limits must be registered via qmlRegisterSingletonInstance()");
        Q_UNREACHABLE_RETURN(nullptr);
    }
};

class AppInfo : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppInfo)

    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)

public:
    QString qtVersion() const { return QString::fromLatin1(qVersion()); }

    static AppInfo *create(QQmlEngine *, QJSEngine *)
    {
        qFatal("AppInfo must be registered via qmlRegisterSingletonInstance()");
        Q_UNREACHABLE_RETURN(nullptr);
    }
};

} // namespace registro
