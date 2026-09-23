#include "log.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStringConverter>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>

namespace registro {

namespace {

QMutex g_mutex;
QFile g_file;

char levelChar(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return 'd';
    case QtInfoMsg:
        return 'i';
    case QtWarningMsg:
        return 'w';
    case QtCriticalMsg:
        return 'e';
    case QtFatalMsg:
        return 'f';
    }
    return '?';
}

QString sourceLocation(const QMessageLogContext &ctx)
{
    if (!ctx.file || ctx.line <= 0)
        return QString();
    QString file = QString::fromUtf8(ctx.file);
    const int slash = file.lastIndexOf(QLatin1Char('/'));
    const int backslash = file.lastIndexOf(QLatin1Char('\\'));
    file.remove(0, qMax(slash, backslash) + 1);
    return QStringLiteral(" (%1:%2)").arg(file).arg(ctx.line);
}

} // namespace

LogLevel parseLogLevel(const QString &level)
{
    const QString lvl = level.trimmed().toLower();
    if (lvl == QLatin1String("debug"))
        return LogLevel::Debug;
    if (lvl == QLatin1String("warn") || lvl == QLatin1String("warning"))
        return LogLevel::Warn;
    if (lvl == QLatin1String("error"))
        return LogLevel::Error;
    return LogLevel::Info;
}

QString formatLogLine(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const QString httpTag =
        QByteArrayView(ctx.category).startsWith(QByteArrayLiteral("registro.http"))
            ? QStringLiteral(" [http]")
            : QString();
    const bool withSource = type != QtDebugMsg && type != QtInfoMsg;
    const QString src = withSource ? sourceLocation(ctx) : QString();
    const QString level = QStringLiteral("[%1]").arg(QChar::fromLatin1(levelChar(type)));
    QString text = msg;
    text.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    text.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    return QStringLiteral("%1 %2%3%4 %5")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")), level, httpTag,
             src, text);
}

void installLogging(const LogConfig &cfg)
{
    if (!qEnvironmentVariableIsSet("QT_LOGGING_RULES")) {
        const char *suffix = nullptr;
        switch (cfg.level) {
        case LogLevel::Debug:
            suffix = "debug";
            break;
        case LogLevel::Warn:
            suffix = "warning";
            break;
        case LogLevel::Error:
            suffix = "critical";
            break;
        case LogLevel::Info:
            suffix = "info";
            break;
        }
        QLoggingCategory::setFilterRules(
            QStringLiteral("registro.*.%1=true").arg(QLatin1String(suffix)));
    }

    if (!cfg.filePath.isEmpty()) {
        if (g_file.isOpen())
            g_file.close();
        g_file.setFileName(cfg.filePath);
        g_file.open(QIODevice::Append | QIODevice::Text);
    }

    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
        const QString line = formatLogLine(type, ctx, msg);
        QMutexLocker lock(&g_mutex);
        QTextStream err(stderr);
        err.setEncoding(QStringConverter::Utf8);
        err << line << '\n';
        err.flush();
        if (g_file.isOpen()) {
            QTextStream f(&g_file);
            f.setEncoding(QStringConverter::Utf8);
            f << line << '\n';
            f.flush();
        }
    });
}

QString systemSummary()
{
    const char *buildTag = "release build";
#ifdef QT_DEBUG
    buildTag = "debug build";
#endif
    return QStringLiteral("%1, %2, %3")
        .arg(QSysInfo::prettyProductName())
        .arg(QSysInfo::currentCpuArchitecture())
        .arg(QString::fromLatin1(buildTag));
}

} // namespace registro
