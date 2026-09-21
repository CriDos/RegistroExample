#pragma once

#include <QLoggingCategory>
#include <QMessageLogContext>
#include <QString>

namespace registro {

enum class LogLevel { Debug, Info, Warn, Error };

struct LogConfig {
    LogLevel level = LogLevel::Info;
    QString filePath;
};

LogLevel parseLogLevel(const QString &level);

QString formatLogLine(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);

void installLogging(const LogConfig &cfg);

QString systemSummary();

} // namespace registro
