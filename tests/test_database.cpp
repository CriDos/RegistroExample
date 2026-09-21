#include "database.h"

#include <QDateTime>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QtTest>

using namespace registro;

class TestDatabase final : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString dbPath(const QString &name) const
    {
        return m_dir.filePath(name + QStringLiteral(".db"));
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void openAppliesMigrations()
    {
        Database db;
        QVERIFY(db.open(dbPath("open")));
        QVERIFY(db.lastError().isEmpty());
    }

    void demoSeedPopulatesClients()
    {
        Database db;
        QVERIFY(db.open(dbPath("seeddemo")));

        QVERIFY(db.seedDemoClients(25));
        ClientPage page = db.listClients(ClientFilter{});
        QCOMPARE(page.total, 25);
        QCOMPARE(page.items.size(), 25);

        for (const Client &c : page.items) {
            QVERIFY(!c.fullName.isEmpty());
            QVERIFY(c.fullName.count(QLatin1Char(' ')) >= 2);
            QVERIFY(c.phone.startsWith(QStringLiteral("+7 9")));
            QVERIFY(c.email.contains(QLatin1Char('@')));
            QVERIFY(c.status == QLatin1String(kStatusActive) ||
                    c.status == QLatin1String(kStatusArchived));
            QVERIFY(!c.createdAt.isEmpty());
            QVERIFY(!c.updatedAt.isEmpty());
        }

        QVERIFY(db.seedDemoClients(10));
        QCOMPARE(db.listClients(ClientFilter{}).total, 35);

        QVERIFY(db.seedDemoClients(0));
        QVERIFY(db.seedDemoClients(-5));
        QCOMPARE(db.listClients(ClientFilter{}).total, 35);
    }

    void migrationsAreIdempotent()
    {
        Database db;
        QVERIFY(db.open(dbPath("idempotent")));
        ClientPage page = db.listClients({});
        QCOMPARE(page.total, 0);
    }

    void createAndGetClient()
    {
        Database db;
        QVERIFY(db.open(dbPath("crud")));

        Client c;
        c.fullName = QStringLiteral("Иванов Иван");
        c.org = QStringLiteral("ООО Ромашка");
        c.phone = QStringLiteral("+7 900 000-00-00");
        c.email = QStringLiteral("ivanov@example.com");
        c.status = QStringLiteral("active");

        const qint64 id = db.addClient(c);
        QVERIFY(id > 0);

        Client loaded;
        QVERIFY(db.getClient(id, &loaded));
        QCOMPARE(loaded.id, id);
        QCOMPARE(loaded.fullName, c.fullName);
        QCOMPARE(loaded.org, c.org);
        QCOMPARE(loaded.status, c.status);
        QVERIFY(!loaded.createdAt.isEmpty());
        QVERIFY(!loaded.updatedAt.isEmpty());
    }

    void timestampsAreUtcIso()
    {
        Database db;
        QVERIFY(db.open(dbPath("utc")));

        Client c;
        c.fullName = QStringLiteral("UTC Check");
        const qint64 id = db.addClient(c);
        QVERIFY(id > 0);

        const auto isUtcIso = [](const QString &s) {
            if (!s.endsWith(QChar::fromLatin1('Z')))
                return false;
            const QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
            return dt.isValid() && dt.timeSpec() == Qt::UTC;
        };

        Client loaded;
        QVERIFY(db.getClient(id, &loaded));
        QVERIFY(isUtcIso(loaded.createdAt));
        QVERIFY(isUtcIso(loaded.updatedAt));

        c.fullName = QStringLiteral("UTC Check Updated");
        QVERIFY(db.updateClient(id, c));
        QVERIFY(db.getClient(id, &loaded));
        QVERIFY(isUtcIso(loaded.updatedAt));

        QVERIFY(db.deleteClient(id));
    }

    void updateClientTouchesTimestamps()
    {
        Database db;
        QVERIFY(db.open(dbPath("update")));

        Client c;
        c.fullName = QStringLiteral("Петров Пётр");
        const qint64 id = db.addClient(c);
        QVERIFY(id > 0);

        Client loaded;
        QVERIFY(db.getClient(id, &loaded));
        const QString before = loaded.updatedAt;

        QTest::qWait(5);

        c.fullName = QStringLiteral("Петров Пётр Иванович");
        QVERIFY(db.updateClient(id, c));

        QVERIFY(db.getClient(id, &loaded));
        QCOMPARE(loaded.fullName, QStringLiteral("Петров Пётр Иванович"));
        QVERIFY(loaded.updatedAt >= before);
    }

    void updateMissingClientFails()
    {
        Database db;
        QVERIFY(db.open(dbPath("update_missing")));

        Client c;
        c.fullName = QStringLiteral("Nobody");
        QVERIFY(!db.updateClient(999999, c));
    }

    void deleteIsSoft()
    {
        Database db;
        QVERIFY(db.open(dbPath("delete")));

        Client c;
        c.fullName = QStringLiteral("Сидоров Сидор");
        const qint64 id = db.addClient(c);

        QVERIFY(db.deleteClient(id));
        Client loaded;
        QVERIFY(!db.getClient(id, &loaded));
        QVERIFY(!db.deleteClient(id));
    }

    void listSearchFilterPagination()
    {
        Database db;
        QVERIFY(db.open(dbPath("list")));

        const auto add = [&db](const QString &name, const QString &org, const QString &status) {
            Client c;
            c.fullName = name;
            c.org = org;
            c.status = status;
            return db.addClient(c);
        };
        add(QStringLiteral("Alpha Corp"), QStringLiteral("Acme"), QStringLiteral("active"));
        add(QStringLiteral("Beta LLC"), QStringLiteral("Acme"), QStringLiteral("archived"));
        add(QStringLiteral("Gamma OOO"), QStringLiteral("Beta"), QStringLiteral("active"));
        add(QStringLiteral("Deleted Inc"), QStringLiteral("Beta"), QStringLiteral("active"));
        QVERIFY(db.deleteClient(4));

        ClientFilter f;
        f.search = QStringLiteral("alph");
        ClientPage page = db.listClients(f);
        QCOMPARE(page.total, 1);
        QCOMPARE(page.items.first().fullName, QStringLiteral("Alpha Corp"));

        f.search = QStringLiteral("acme");
        page = db.listClients(f);
        QCOMPARE(page.total, 2);

        f = ClientFilter{};
        f.status = QStringLiteral("archived");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);
        QCOMPARE(page.items.first().fullName, QStringLiteral("Beta LLC"));

        f = ClientFilter{};
        f.limit = 2;
        f.offset = 1;
        page = db.listClients(f);
        QCOMPARE(page.total, 3);
        QCOMPARE(page.items.size(), 2);

        f = ClientFilter{};
        page = db.listClients(f);
        QCOMPARE(page.total, 3);
        QCOMPARE(page.items.first().fullName, QStringLiteral("Gamma OOO"));
        QCOMPARE(page.items.at(1).fullName, QStringLiteral("Beta LLC"));
        QCOMPARE(page.items.at(2).fullName, QStringLiteral("Alpha Corp"));

        f = ClientFilter{};
        f.search = QStringLiteral("beta");
        f.status = QStringLiteral("active");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);
        QCOMPARE(page.items.first().fullName, QStringLiteral("Gamma OOO"));

        Client wild;
        wild.fullName = QStringLiteral("100% client");
        QVERIFY(db.addClient(wild) > 0);
        f = ClientFilter{};
        f.search = QStringLiteral("100%");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);
        QCOMPARE(page.items.first().fullName, QStringLiteral("100% client"));

        f = ClientFilter{};
        f.search = QStringLiteral("100_");
        page = db.listClients(f);
        QCOMPARE(page.total, 0);

        f = ClientFilter{};
        f.search = QStringLiteral("_");
        page = db.listClients(f);
        QCOMPARE(page.total, 0);
    }

    void searchIsCaseInsensitiveForCyrillic()
    {
        Database db;
        QVERIFY(db.open(dbPath("search-cyr")));

        Client c;
        c.fullName = QStringLiteral("Иванов Иван Иванович");
        c.org = QStringLiteral("ООО «Волга»");
        QVERIFY(db.addClient(c) > 0);
        c = Client{};
        c.fullName = QStringLiteral("Сидорова Анна Петровна");
        QVERIFY(db.addClient(c) > 0);

        ClientFilter f;
        f.search = QStringLiteral("иванов");
        ClientPage page = db.listClients(f);
        QCOMPARE(page.total, 1);
        QCOMPARE(page.items.first().fullName, QStringLiteral("Иванов Иван Иванович"));

        f.search = QStringLiteral("ИВАНОВ");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);

        f.search = QStringLiteral("иванович");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);

        f.search = QStringLiteral("волга");
        page = db.listClients(f);
        QCOMPARE(page.total, 1);

        c = page.items.first();
        c.fullName = QStringLiteral("Петров Пётр Петрович");
        QVERIFY(db.updateClient(c.id, c));
        f.search = QStringLiteral("иванов");
        QCOMPARE(db.listClients(f).total, 0);
        f.search = QStringLiteral("пётр петрович");
        QCOMPARE(db.listClients(f).total, 1);
    }

    void searchPhoneByDigits()
    {
        Database db;
        QVERIFY(db.open(dbPath("search-phone")));

        Client c;
        c.fullName = QStringLiteral("Телефонный Клиент");
        c.phone = QStringLiteral("+7 (912) 345-67-89");
        QVERIFY(db.addClient(c) > 0);

        ClientFilter f;
        f.search = QStringLiteral("9123456789");
        QCOMPARE(db.listClients(f).total, 1);

        f.search = QStringLiteral("912 345");
        QCOMPARE(db.listClients(f).total, 1);

        f.search = QStringLiteral("79123456789");
        QCOMPARE(db.listClients(f).total, 1);

        f.search = QStringLiteral("912 999");
        QCOMPARE(db.listClients(f).total, 0);
    }

    void schemaV1UpgradeBackfillsSearchNames()
    {
        // Craft a v1 database by hand (the pre-search_name schema) and verify
        // that opening it migrates to v2 and backfills the searchable column.
        const QString path = dbPath(QStringLiteral("v1upgrade"));
        const auto conn = [&path] {
            static int n = 0;
            return QStringLiteral("v1craft_%1_%2").arg(path.size()).arg(++n);
        }();
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            raw.setDatabaseName(path);
            QVERIFY2(raw.open(), qPrintable(raw.lastError().text()));
            QSqlQuery ddl(raw);
            QVERIFY(ddl.exec(QStringLiteral("CREATE TABLE clients ("
                                            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                            " full_name TEXT NOT NULL,"
                                            " org TEXT NOT NULL DEFAULT '',"
                                            " phone TEXT NOT NULL DEFAULT '',"
                                            " email TEXT NOT NULL DEFAULT '',"
                                            " notes TEXT NOT NULL DEFAULT '',"
                                            " status TEXT NOT NULL DEFAULT 'active',"
                                            " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                                            " updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
                                            " deleted INTEGER NOT NULL DEFAULT 0)")));
            QSqlQuery ins(raw);
            ins.prepare(QStringLiteral("INSERT INTO clients (full_name, org, phone) VALUES"
                                       " (:f, :o, :p)"));
            ins.bindValue(QStringLiteral(":f"), QStringLiteral("Мигрант Мирослав"));
            ins.bindValue(QStringLiteral(":o"), QStringLiteral("Старая База"));
            ins.bindValue(QStringLiteral(":p"), QStringLiteral("+7 111 222-33-44"));
            QVERIFY(ins.exec());
            QSqlQuery ver(raw);
            QVERIFY(ver.exec(QStringLiteral("PRAGMA user_version = 1")));
            raw.close();
        }
        QSqlDatabase::removeDatabase(conn);

        Database db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));

        ClientFilter f;
        f.search = QStringLiteral("мигрант");
        QCOMPARE(db.listClients(f).total, 1);
        f.search = QStringLiteral("МИГРАНТ");
        QCOMPARE(db.listClients(f).total, 1);
        f.search = QStringLiteral("1112223344");
        QCOMPARE(db.listClients(f).total, 1);

        Client c;
        c.fullName = QStringLiteral("Новый Клиент");
        const qint64 id = db.addClient(c);
        f.search = QStringLiteral("новый");
        QCOMPARE(db.listClients(f).total, 1);
        QCOMPARE(db.listClients(f).items.first().id, id);
    }

    void schemaV1FullBaselineUpgrade()
    {
        // A real v1 database (created by an older build) has the full 8-entry
        // baseline applied: clients + users + tokens + audit, a demo user and
        // clients data. Opening it must run only the entries appended since
        // v1 (the search_name ALTER) — re-running the baseline would fail with
        // "table users already exists".
        const QString path = dbPath(QStringLiteral("v1full"));
        const auto conn = [&path] {
            static int n = 0;
            return QStringLiteral("v1fullcraft_%1_%2").arg(path.size()).arg(++n);
        }();
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            raw.setDatabaseName(path);
            QVERIFY2(raw.open(), qPrintable(raw.lastError().text()));
            const QStringList baseline = {
                QStringLiteral("CREATE TABLE clients ("
                               " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               " full_name TEXT NOT NULL,"
                               " org TEXT NOT NULL DEFAULT '',"
                               " phone TEXT NOT NULL DEFAULT '',"
                               " email TEXT NOT NULL DEFAULT '',"
                               " notes TEXT NOT NULL DEFAULT '',"
                               " status TEXT NOT NULL DEFAULT 'active',"
                               " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                               " updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
                               " deleted INTEGER NOT NULL DEFAULT 0)"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clients_status "
                               "ON clients(status) WHERE deleted = 0"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clients_name ON clients(full_name)"),
                QStringLiteral("CREATE TABLE users ("
                               " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               " username TEXT NOT NULL UNIQUE,"
                               " password_hash TEXT NOT NULL,"
                               " role TEXT NOT NULL DEFAULT 'user',"
                               " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                               " updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
                               " deleted INTEGER NOT NULL DEFAULT 0)"),
                QStringLiteral("CREATE TABLE tokens ("
                               " token_hash TEXT PRIMARY KEY,"
                               " user_id INTEGER NOT NULL REFERENCES users(id),"
                               " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                               " expires_at TEXT)"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_tokens_user ON tokens(user_id)"),
                QStringLiteral("CREATE TABLE audit ("
                               " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                               " user_id INTEGER,"
                               " action TEXT NOT NULL,"
                               " entity TEXT NOT NULL,"
                               " entity_id INTEGER NOT NULL,"
                               " created_at TEXT NOT NULL DEFAULT (datetime('now')))"),
                QStringLiteral("CREATE INDEX IF NOT EXISTS idx_audit_created ON audit(created_at)"),
            };
            for (const QString &ddl : baseline) {
                QSqlQuery q(raw);
                QVERIFY2(q.exec(ddl), qPrintable(q.lastError().text()));
            }
            QSqlQuery ins(raw);
            ins.prepare(QStringLiteral("INSERT INTO users (username, password_hash) VALUES"
                                       " (:u, :h)"));
            ins.bindValue(QStringLiteral(":u"), QStringLiteral("legacy"));
            ins.bindValue(QStringLiteral(":h"), QStringLiteral("pbkdf2$legacy"));
            QVERIFY(ins.exec());
            QSqlQuery cli(raw);
            cli.prepare(QStringLiteral("INSERT INTO clients (full_name, org) VALUES (:f, :o)"));
            cli.bindValue(QStringLiteral(":f"), QStringLiteral("Базовый Клиент"));
            cli.bindValue(QStringLiteral(":o"), QStringLiteral("Старая База"));
            QVERIFY(cli.exec());
            QSqlQuery ver(raw);
            QVERIFY(ver.exec(QStringLiteral("PRAGMA user_version = 1")));
            raw.close();
        }
        QSqlDatabase::removeDatabase(conn);

        Database db;
        QVERIFY2(db.open(path), qPrintable(db.lastError()));

        QCOMPARE(db.userCount(), 1);
        ClientFilter f;
        f.search = QStringLiteral("базовый");
        QCOMPARE(db.listClients(f).total, 1);
    }

    void listLimitIsCapped()
    {
        Database db;
        QVERIFY(db.open(dbPath("cap")));

        for (int i = 1; i <= kMaxPageSize + 5; ++i) {
            Client c;
            c.fullName = QStringLiteral("Cap %1").arg(i);
            QVERIFY(db.addClient(c) > 0);
        }

        ClientFilter f;
        f.limit = kMaxPageSize + 100;
        ClientPage page = db.listClients(f);
        QCOMPARE(page.total, kMaxPageSize + 5);
        QCOMPARE(page.items.size(), kMaxPageSize);

        f.limit = -1;
        page = db.listClients(f);
        QCOMPARE(page.items.size(), kMaxPageSize);
    }

    void userAndTokenCrud()
    {
        Database db;
        QVERIFY(db.open(dbPath("users")));
        QCOMPARE(db.userCount(), 0);

        const qint64 alice =
            db.addUser(QStringLiteral("alice"), QStringLiteral("hash1"), QStringLiteral("user"));
        QVERIFY(alice > 0);
        QCOMPARE(
            db.addUser(QStringLiteral("alice"), QStringLiteral("hash2"), QStringLiteral("user")),
            0);

        UserRecord u;
        QVERIFY(db.getUserByUsername(QStringLiteral("alice"), &u));
        QCOMPARE(u.id, alice);
        QCOMPARE(u.username, QStringLiteral("alice"));
        QCOMPARE(u.passwordHash, QStringLiteral("hash1"));
        QCOMPARE(u.role, QStringLiteral("user"));
        QVERIFY(!db.getUserByUsername(QStringLiteral("nobody"), &u));

        QVERIFY(db.storeToken(QStringLiteral("tokh1"), alice));
        QVERIFY(db.getUserByTokenHash(QStringLiteral("tokh1"), &u));
        QCOMPARE(u.id, alice);
        QVERIFY(!db.getUserByTokenHash(QStringLiteral("missing"), &u));

        QCOMPARE(db.userCount(), 1);
        QCOMPARE(db.listUsers().size(), 1);
    }

    void tokenExpiry()
    {
        Database db;
        QVERIFY(db.open(dbPath("token-ttl")));

        const qint64 alice =
            db.addUser(QStringLiteral("alice"), QStringLiteral("hash1"), QStringLiteral("user"));
        QVERIFY(alice > 0);

        QVERIFY(db.storeToken(QStringLiteral("expired"), alice, -1));
        QVERIFY(db.storeToken(QStringLiteral("valid"), alice, 3600));

        UserRecord u;
        QVERIFY(!db.getUserByTokenHash(QStringLiteral("expired"), &u));
        QVERIFY(db.getUserByTokenHash(QStringLiteral("valid"), &u));

        QCOMPARE(db.deleteExpiredTokens(), 1);
        QVERIFY(db.getUserByTokenHash(QStringLiteral("valid"), &u));
        QVERIFY(!db.getUserByTokenHash(QStringLiteral("expired"), &u));
    }

    void concurrentAccessIsSafe()
    {
        Database db;
        QVERIFY(db.open(dbPath("concurrent")));

        constexpr int kSeed = 20;
        for (int i = 0; i < kSeed; ++i) {
            Client c;
            c.fullName = QStringLiteral("Seed %1").arg(i);
            QVERIFY(db.addClient(c) > 0);
        }

        constexpr int kThreads = 8;
        constexpr int kIters = 25;
        constexpr int kAddsPerThread = 9;
        QVector<int> failures(kThreads, 0);

        QVector<QThread *> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.append(QThread::create([&db, &failures, t] {
                for (int i = 0; i < kIters; ++i) {
                    if (db.listClients({}).total < kSeed) {
                        ++failures[t];
                        continue;
                    }
                    if (i % 3 == 0) {
                        Client c;
                        c.fullName = QStringLiteral("Add %1-%2").arg(t).arg(i);
                        if (db.addClient(c) <= 0)
                            ++failures[t];
                    }
                    if (i % 5 == 0) {
                        Client c;
                        if (!db.getClient(1, &c))
                            ++failures[t];
                    }
                    if (i % 7 == 0) {
                        Client c;
                        c.fullName = QStringLiteral("Update %1").arg(i);
                        if (!db.updateClient(1, c))
                            ++failures[t];
                    }
                }
            }));
            threads.last()->start();
        }
        for (QThread *th : threads) {
            th->wait();
            delete th;
        }
        for (int f : failures)
            QCOMPARE(f, 0);

        ClientFilter f;
        f.limit = kMaxPageSize;
        const ClientPage page = db.listClients(f);
        QCOMPARE(page.total, qint64(kSeed + kThreads * kAddsPerThread));
        QCOMPARE(page.items.size(), page.total);
    }

    void notesAreStoredOnClient()
    {
        Database db;
        QVERIFY(db.open(dbPath("notesfield")));

        Client c;
        c.fullName = QStringLiteral("Note Owner");
        c.notes = QStringLiteral("первая заметка");
        const qint64 clientId = db.addClient(c);
        QVERIFY(clientId > 0);

        Client loaded;
        QVERIFY(db.getClient(clientId, &loaded));
        QCOMPARE(loaded.notes, QStringLiteral("первая заметка"));

        Client updated = loaded;
        updated.notes = QStringLiteral("изменённые заметки");
        QVERIFY(db.updateClient(clientId, updated));
        QVERIFY(db.getClient(clientId, &loaded));
        QCOMPARE(loaded.notes, QStringLiteral("изменённые заметки"));

        ClientPage page = db.listClients(ClientFilter{});
        QCOMPARE(page.items.size(), 1);
        QCOMPARE(page.items.at(0).notes, QStringLiteral("изменённые заметки"));
    }

    void rejectsUnsupportedSchema()
    {
        // Pre-1.0 databases are deliberately unsupported (no migration path):
        // opening an incompatible file fails with a clear error instead of
        // silently upgrading or corrupting it.
        const QString path = dbPath("legacynotes");
        {
            {
                QSqlDatabase raw =
                    QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "legacy_notes");
                raw.setDatabaseName(path);
                QVERIFY(raw.open());
                QSqlQuery q(raw);
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE clients (id INTEGER PRIMARY KEY, full_name TEXT NOT NULL)")));
                QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 42")));
            }
            QSqlDatabase::removeDatabase(QStringLiteral("legacy_notes"));
        }

        Database db;
        QVERIFY(!db.open(path));
        QVERIFY(db.lastError().contains(QStringLiteral("schema version")));

        QSqlDatabase check =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "legacy_notes_check");
        check.setDatabaseName(path);
        QVERIFY(check.open());
        QSqlQuery q(check);
        QVERIFY(q.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("clients"));
        QSqlDatabase::removeDatabase(QStringLiteral("legacy_notes_check"));
    }

    void freshSchemaIsV2()
    {
        Database db;
        QVERIFY(db.open(dbPath("freshv2")));

        QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "fresh_v2_check");
        raw.setDatabaseName(dbPath("freshv2"));
        QVERIFY(raw.open());

        QSqlQuery uv(raw);
        QVERIFY(uv.exec(QStringLiteral("PRAGMA user_version")));
        QVERIFY(uv.next());
        QCOMPARE(uv.value(0).toInt(), 2);

        QSqlQuery cols(raw);
        QVERIFY(cols.exec(QStringLiteral("PRAGMA table_info(clients)")));
        QStringList names;
        while (cols.next())
            names << cols.value(1).toString();
        QVERIFY(names.contains(QStringLiteral("notes")));
        QVERIFY(names.contains(QStringLiteral("search_name")));

        QSqlQuery tabs(raw);
        QVERIFY(tabs.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table'")));
        QStringList tables;
        while (tabs.next())
            tables << tabs.value(0).toString();
        QVERIFY(!tables.contains(QStringLiteral("notes")));
        QSqlDatabase::removeDatabase(QStringLiteral("fresh_v2_check"));
    }

    void auditLog()
    {
        Database db;
        QVERIFY(db.open(dbPath("audit")));

        QVERIFY(db.addAudit(0, QStringLiteral("create"), QStringLiteral("client"), 7) > 0);
        QVERIFY(db.addAudit(3, QStringLiteral("update"), QStringLiteral("client"), 7) > 0);
        QVERIFY(db.addAudit(3, QStringLiteral("delete"), QStringLiteral("note"), 9) > 0);

        AuditPage page = db.listAudit({});
        QCOMPARE(page.total, 3);
        QCOMPARE(page.items.size(), 3);
        QCOMPARE(page.items.at(0).id, 3);
        QCOMPARE(page.items.at(0).action, QStringLiteral("delete"));
        QCOMPARE(page.items.at(0).entityId, 9);
        QCOMPARE(page.items.at(2).userId, 0);
        QVERIFY(!page.items.at(0).createdAt.isEmpty());

        AuditFilter f;
        f.limit = 1;
        page = db.listAudit(f);
        QCOMPARE(page.total, 3);
        QCOMPARE(page.items.size(), 1);
        QCOMPARE(page.items.at(0).action, QStringLiteral("delete"));

        f.limit = 2;
        f.offset = 1;
        page = db.listAudit(f);
        QCOMPARE(page.items.size(), 2);
        QCOMPARE(page.items.at(0).action, QStringLiteral("update"));
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
