#include "log.h"

#include <QtTest>

using namespace registro;

class TestLog final : public QObject {
    Q_OBJECT

private slots:
    void parseLevels()
    {
        QCOMPARE(parseLogLevel(QStringLiteral("debug")), LogLevel::Debug);
        QCOMPARE(parseLogLevel(QStringLiteral("DEBUG")), LogLevel::Debug);
        QCOMPARE(parseLogLevel(QStringLiteral("  debug  ")), LogLevel::Debug);
        QCOMPARE(parseLogLevel(QStringLiteral("info")), LogLevel::Info);
        QCOMPARE(parseLogLevel(QStringLiteral("warn")), LogLevel::Warn);
        QCOMPARE(parseLogLevel(QStringLiteral("WARNING")), LogLevel::Warn);
        QCOMPARE(parseLogLevel(QStringLiteral("error")), LogLevel::Error);
        QCOMPARE(parseLogLevel(QStringLiteral("")), LogLevel::Info);
        QCOMPARE(parseLogLevel(QStringLiteral("verbose")), LogLevel::Info);
    }

    void formatLineBasics()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.test";
        const QString line = formatLogLine(QtInfoMsg, ctx, QStringLiteral("hello 42"));
        QVERIFY(line.contains(QStringLiteral("[i]")));
        QVERIFY(line.endsWith(QStringLiteral("hello 42")));
        QVERIFY(line.at(2) == QLatin1Char(':'));
        QVERIFY(line.at(5) == QLatin1Char(':'));
        QVERIFY(line.at(8) == QLatin1Char('.'));
        QVERIFY(!line.contains(QStringLiteral("registro")));
    }

    void formatLineLevelChars()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.test";
        QVERIFY(
            formatLogLine(QtDebugMsg, ctx, QStringLiteral("m")).contains(QStringLiteral("[d]")));
        QVERIFY(formatLogLine(QtInfoMsg, ctx, QStringLiteral("m")).contains(QStringLiteral("[i]")));
        QVERIFY(
            formatLogLine(QtWarningMsg, ctx, QStringLiteral("m")).contains(QStringLiteral("[w]")));
        QVERIFY(
            formatLogLine(QtCriticalMsg, ctx, QStringLiteral("m")).contains(QStringLiteral("[e]")));
        QVERIFY(
            formatLogLine(QtFatalMsg, ctx, QStringLiteral("m")).contains(QStringLiteral("[f]")));
    }

    void httpLinesGetTag()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.http";
        const QString line = formatLogLine(QtInfoMsg, ctx, QStringLiteral("Get /api/health 200"));
        QVERIFY(line.contains(QStringLiteral(" [http] ")));
    }

    void formatLineIncludesFileLine()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.test";
        ctx.file = "src/database.cpp";
        ctx.line = 20;

        const QString warn = formatLogLine(QtWarningMsg, ctx, QStringLiteral("boom"));
        QVERIFY(warn.contains(QStringLiteral("(database.cpp:20)")));

        const QString info = formatLogLine(QtInfoMsg, ctx, QStringLiteral("GET / 200"));
        QVERIFY(!info.contains(QStringLiteral("database.cpp")));

        const QString debug = formatLogLine(QtDebugMsg, ctx, QStringLiteral("m"));
        QVERIFY(!debug.contains(QStringLiteral("database.cpp")));
    }

    void formatLineNoContext()
    {
        QMessageLogContext ctx;
        ctx.category = nullptr;
        ctx.file = nullptr;
        ctx.line = 0;
        const QString line = formatLogLine(QtCriticalMsg, ctx, QStringLiteral("m"));
        QVERIFY(!line.contains(QStringLiteral("default")));
        QVERIFY(line.endsWith(QStringLiteral("m")));
    }

    void formatLineCollapsesNewlines()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.test";
        ctx.file = "database.cpp";
        ctx.line = 21;
        const QString line =
            formatLogLine(QtWarningMsg, ctx, QStringLiteral("line1\nline2\r\nline3"));
        QCOMPARE(line.count(QLatin1Char('\n')), 0);
        QCOMPARE(line.count(QLatin1Char('\r')), 0);
        QVERIFY(line.contains(QStringLiteral("line1\\nline2\\r\\nline3")));
    }

    void formatLineUtf8()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.test";
        const QString line =
            formatLogLine(QtWarningMsg, ctx, QStringLiteral("Клиент Иван — 100% «тест»"));
        QVERIFY(line.contains(QStringLiteral("Клиент Иван — 100% «тест»")));
    }

    void systemSummaryHasEnvironment()
    {
        const QString s = systemSummary();
        QVERIFY(!s.isEmpty());
        QVERIFY(s.contains(QStringLiteral("build")));
        QVERIFY(s.contains(QLatin1Char(',')));
#ifdef QT_DEBUG
        QVERIFY(s.contains(QStringLiteral("debug build")));
#endif
    }

    void formatLineFatalNotInfo()
    {
        QMessageLogContext ctx;
        ctx.category = "registro.http";
        ctx.file = "server/src/main.cpp";
        ctx.line = 30;
        const QString line = formatLogLine(QtFatalMsg, ctx, QStringLiteral("f"));
        QVERIFY(line.contains(QStringLiteral("[f]")));
        QVERIFY(line.contains(QStringLiteral("(main.cpp:30)")));
    }
};

QTEST_GUILESS_MAIN(TestLog)
#include "test_log.moc"
