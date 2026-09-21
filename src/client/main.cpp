#include "config.h"
#include "log.h"
#include "log_categories.h"
#include "qml_singletons.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include <QUrl>
#include <QtQml/qqml.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("RegistroClient"));
    QGuiApplication::setOrganizationName(QStringLiteral("RegistroExample"));
    QGuiApplication::setApplicationVersion(QStringLiteral(REGISTRO_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/Registro/Client/registro-client.svg")));

#ifdef QT_DEBUG
    registro::LogConfig logCfg;
    logCfg.level = registro::LogLevel::Debug;
#else
    registro::LogConfig logCfg;
    logCfg.level = registro::LogLevel::Info;
#endif
    registro::installLogging(logCfg);

    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);

    registro::Config config(registro::defaultClientConfigPath());
    const QString baseUrl = argc > 1 ? QString::fromLocal8Bit(argv[1]) : config.serverUrl();
    config.setServerUrl(baseUrl);
    qCInfo(lcClient).noquote() << QStringLiteral("RegistroClient %1 - Qt %2 - %3, server: %4")
                                      .arg(QString::fromLatin1(REGISTRO_VERSION),
                                           QString::fromLatin1(qVersion()),
                                           registro::systemSummary(), baseUrl);

    registro::ApiSingleton api(QUrl::fromUserInput(baseUrl));
    if (!config.token().isEmpty())
        api.restoreToken(config.token());
    registro::ClientModelSingleton clientModel(&api);
    registro::LimitsSingleton limits;
    registro::AppInfo appInfo;

    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("Registro.Client", 1, 0, "Api", &api);
    qmlRegisterSingletonInstance("Registro.Client", 1, 0, "Config", &config);
    qmlRegisterSingletonInstance("Registro.Client", 1, 0, "Limits", &limits);
    qmlRegisterSingletonInstance("Registro.Client", 1, 0, "AppInfo", &appInfo);
    qmlRegisterSingletonInstance("Registro.Client", 1, 0, "ClientModel", &clientModel);

    engine.loadFromModule(QStringLiteral("Registro.Client"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return -1;

    api.initialize();

    return app.exec();
}
