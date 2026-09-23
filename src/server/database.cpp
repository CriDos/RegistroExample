#include "database.h"
#include "log_categories.h"
#include <log.h>

#include <QDate>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTime>
#include <QTimeZone>
#include <QUuid>
#include <QVariant>

namespace registro {

namespace {

constexpr int kBusyTimeoutMs = 5000;

void applyPragmas(QSqlDatabase &db)
{
    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout = %1").arg(kBusyTimeoutMs));
    pragma.exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    // Keep the main .db file roughly in sync with the WAL: with the default
    // 1000-page threshold the demo seed (10k rows ~ 3 MB) stays entirely in
    // the -wal file, which looks like an empty database and breaks
    // plain-file backups. 512 pages ~ 2 MB, still cheap per checkpoint.
    pragma.exec(QStringLiteral("PRAGMA wal_autocheckpoint = 512"));
}

bool checkpoint(QSqlDatabase &db)
{
    QSqlQuery q(db);
    return q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
}

bool exec(QSqlQuery &q, QString *err)
{
    if (!q.exec()) {
        const QString text = q.lastError().text();
        qCWarning(lcDb).noquote() << text;
        if (err)
            *err = text;
        return false;
    }
    return true;
}

const QStringList &clientColumns()
{
    static const QStringList cols = {
        QStringLiteral("id"),     QStringLiteral("full_name"),  QStringLiteral("org"),
        QStringLiteral("phone"),  QStringLiteral("email"),      QStringLiteral("notes"),
        QStringLiteral("status"), QStringLiteral("created_at"), QStringLiteral("updated_at"),
    };
    return cols;
}

const QStringList &userColumns()
{
    static const QStringList cols = {QStringLiteral("id"), QStringLiteral("username"),
                                     QStringLiteral("password_hash"), QStringLiteral("role")};
    return cols;
}

const QStringList &auditColumns()
{
    static const QStringList cols = {
        QStringLiteral("id"),     QStringLiteral("user_id"),   QStringLiteral("action"),
        QStringLiteral("entity"), QStringLiteral("entity_id"), QStringLiteral("created_at"),
    };
    return cols;
}

QString utcIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

// Lowercase (Unicode-aware) searchable copy of a client row, so LIKE works for
// Cyrillic too. The phone contributes digit-only text: users type numbers the
// same way regardless of the display mask ("9123456789" finds "+7 (912) 345-67-89").
QString searchNameFor(const Client &c)
{
    QString phoneDigits;
    phoneDigits.reserve(c.phone.size());
    for (const QChar ch : c.phone) {
        if (ch.isDigit())
            phoneDigits.append(ch);
    }
    return QStringLiteral("%1\n%2\n%3\n%4")
        .arg(c.fullName.toLower(), c.org.toLower(), phoneDigits, c.email.toLower());
}

// Schema versions stored in PRAGMA user_version:
// 1 - the initial baseline (tables/indexes in baselineStatements()).
// 2 - clients.search_name: lowercase (Cyrillic-aware) copy of the searchable
//     fields, so LIKE is case-insensitive for Russian names too.
//
// Version 1 is applied when the database file is created (user_version 0).
// Every later change is a SchemaMigration keyed by the version that added it:
// migrate() executes each entry with version > stored version, exactly once.
// Adding a schema change = bump kSchemaVersion + append one record.
inline constexpr int kSchemaVersion = 2;

struct SchemaMigration {
    int version;
    QStringList statements;
};

// The version-1 baseline. Never edited after release - schema changes go into
// new SchemaMigration records (the search_name column below is version 2).
QStringList baselineStatements()
{
    return {
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
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_clients_status ON clients(status) WHERE deleted = 0"),
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
}

const QList<SchemaMigration> &schemaMigrations()
{
    static const QList<SchemaMigration> migrations = {
        // 2: search_name so LIKE works case-insensitively for Cyrillic (SQLite
        // LIKE only folds ASCII). Values are maintained in C++ where
        // QString::toLower() is Unicode-aware; rows created before this
        // migration are backfilled by Database::backfillSearchNames().
        {2,
         {QStringLiteral("ALTER TABLE clients ADD COLUMN search_name TEXT NOT NULL DEFAULT ''")}},
    };
    return migrations;
}

} // namespace

Database::~Database()
{
    QMutexLocker locker(&m_mutex);
    clearConnections();
}

void Database::clearConnections()
{
    for (auto it = m_threadConns.constBegin(); it != m_threadConns.constEnd(); ++it) {
        const QString name = it.value()->connectionName();
        delete it.value();
        if (QSqlDatabase::contains(name))
            QSqlDatabase::removeDatabase(name);
    }
    m_threadConns.clear();
    if (!m_connection.isEmpty() && QSqlDatabase::contains(m_connection))
        QSqlDatabase::removeDatabase(m_connection);
    m_connection.clear();
}

bool Database::open(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    clearConnections();
    m_connection =
        QStringLiteral("registro_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_path = filePath;

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    applyPragmas(db);

    migrate();
    if (m_lastError.isEmpty())
        backfillSearchNames();

    return m_lastError.isEmpty();
}

QString Database::lastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

QSqlDatabase Database::threadDb()
{
    const Qt::HANDLE tid = QThread::currentThreadId();
    QSqlDatabase *cached = m_threadConns.value(tid);
    if (cached)
        return *cached;

    const QString name = QStringLiteral("%1_%2").arg(m_connection).arg(quintptr(tid), 0, 16);
    auto *db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name));
    db->setDatabaseName(m_path);
    if (!db->open()) {
        m_lastError = db->lastError().text();
        // Do not cache the broken connection: the next call retries from
        // scratch instead of reusing a permanently dead handle.
        delete db;
        if (QSqlDatabase::contains(name))
            QSqlDatabase::removeDatabase(name);
        return QSqlDatabase();
    }
    applyPragmas(*db);
    m_threadConns.insert(tid, db);
    return *db;
}

void Database::migrate()
{
    QSqlDatabase db = threadDb();

    int current = 0;
    {
        QSqlQuery q(db);
        q.exec(QStringLiteral("PRAGMA user_version"));
        if (q.next())
            current = q.value(0).toInt();
    }

    if (current == kSchemaVersion)
        return;
    if (current > kSchemaVersion) {
        m_lastError =
            QStringLiteral("database schema version %1 is not supported by this build (expected"
                           " %2); delete the database file and restart")
                .arg(current)
                .arg(kSchemaVersion);
        return;
    }

    // A fresh file (0) gets the baseline first; anything stored below
    // kSchemaVersion then receives the migration records added since.
    if (current == 0) {
        for (const QString &statement : baselineStatements()) {
            QSqlQuery q(db);
            if (!q.exec(statement)) {
                m_lastError =
                    QStringLiteral("baseline creation failed: %1").arg(q.lastError().text());
                return;
            }
        }
        current = 1; // the baseline IS version 1
    }

    for (const SchemaMigration &migration : schemaMigrations()) {
        if (migration.version <= current)
            continue;
        for (const QString &statement : migration.statements) {
            QSqlQuery q(db);
            if (!q.exec(statement)) {
                m_lastError = QStringLiteral("migration %1 failed: %2")
                                  .arg(migration.version)
                                  .arg(q.lastError().text());
                return;
            }
        }
    }

    QSqlQuery uv(db);
    uv.prepare(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion));
    uv.exec();
}

// Rows created before the search_name column existed (v1 databases upgraded
// in place) get their searchable text filled in now, in C++, where
// QString::toLower() folds Cyrillic correctly.
void Database::backfillSearchNames()
{
    // Called from open(), which already holds m_mutex.
    QSqlDatabase db = threadDb();
    if (!db.isOpen())
        return;

    QSqlQuery sel(db);
    sel.prepare(QStringLiteral(
        "SELECT id, full_name, org, phone, email FROM clients WHERE search_name = ''"));
    if (!exec(sel, &m_lastError))
        return;
    QSqlQuery upd(db);
    upd.prepare(QStringLiteral("UPDATE clients SET search_name = :search_name WHERE id = :id"));
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return;
    }
    while (sel.next()) {
        Client c;
        c.id = sel.value(0).toLongLong();
        c.fullName = sel.value(1).toString();
        c.org = sel.value(2).toString();
        c.phone = sel.value(3).toString();
        c.email = sel.value(4).toString();
        upd.bindValue(QStringLiteral(":search_name"), searchNameFor(c));
        upd.bindValue(QStringLiteral(":id"), c.id);
        if (!exec(upd, &m_lastError)) {
            db.rollback();
            return;
        }
    }
    if (!db.commit())
        m_lastError = db.lastError().text();
}

QVariantMap Database::clientToRow(const Client &c)
{
    QVariantMap row;
    const auto safe = [](const QString &s) -> QVariant {
        return s.isNull() ? QVariant(QStringLiteral("")) : QVariant(s);
    };
    row.insert(QStringLiteral("full_name"), safe(c.fullName));
    row.insert(QStringLiteral("org"), safe(c.org));
    row.insert(QStringLiteral("phone"), safe(c.phone));
    row.insert(QStringLiteral("email"), safe(c.email));
    row.insert(QStringLiteral("notes"), safe(c.notes));
    row.insert(QStringLiteral("status"), safe(c.status));
    row.insert(QStringLiteral("search_name"), searchNameFor(c));
    row.insert(QStringLiteral("updated_at"), utcIso());
    return row;
}

Client Database::rowToClient(const QVariantMap &row)
{
    Client c;
    c.id = row.value(QStringLiteral("id")).toLongLong();
    c.fullName = row.value(QStringLiteral("full_name")).toString();
    c.org = row.value(QStringLiteral("org")).toString();
    c.phone = row.value(QStringLiteral("phone")).toString();
    c.email = row.value(QStringLiteral("email")).toString();
    c.notes = row.value(QStringLiteral("notes")).toString();
    c.status = row.value(QStringLiteral("status")).toString();
    c.createdAt = row.value(QStringLiteral("created_at")).toString();
    c.updatedAt = row.value(QStringLiteral("updated_at")).toString();
    return c;
}

qint64 Database::addClient(const Client &c)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO clients (full_name, org, phone, email, notes, status,"
                             " search_name, created_at, updated_at)"
                             " VALUES (:full_name, :org, :phone, :email, :notes, :status,"
                             " :search_name, :created_at, :updated_at)"));
    QVariantMap row = clientToRow(c);
    row.insert(QStringLiteral("created_at"), utcIso());
    for (auto it = row.constBegin(); it != row.constEnd(); ++it)
        q.bindValue(QStringLiteral(":%1").arg(it.key()), it.value());

    if (!exec(q, &m_lastError))
        return 0;
    return q.lastInsertId().toLongLong();
}

bool Database::seedDemoClients(int count)
{
    if (count <= 0)
        return true;

    struct NameSet {
        QStringList maleGiven;
        QStringList femaleGiven;
        QStringList lastNames;
        QStringList malePatronymic;
        QStringList femalePatronymic;
        QStringList orgTypes;
        QStringList orgBrands;
    };

    static const NameSet names{
        {QStringLiteral("Иван"),    QStringLiteral("Сергей"),   QStringLiteral("Дмитрий"),
         QStringLiteral("Андрей"),  QStringLiteral("Алексей"),  QStringLiteral("Михаил"),
         QStringLiteral("Николай"), QStringLiteral("Владимир"), QStringLiteral("Павел"),
         QStringLiteral("Артём"),   QStringLiteral("Максим"),   QStringLiteral("Антон"),
         QStringLiteral("Игорь"),   QStringLiteral("Олег"),     QStringLiteral("Юрий"),
         QStringLiteral("Егор"),    QStringLiteral("Роман"),    QStringLiteral("Виктор"),
         QStringLiteral("Денис"),   QStringLiteral("Кирилл")},
        {QStringLiteral("Анна"),       QStringLiteral("Мария"),    QStringLiteral("Ольга"),
         QStringLiteral("Наталья"),    QStringLiteral("Елена"),    QStringLiteral("Светлана"),
         QStringLiteral("Татьяна"),    QStringLiteral("Ирина"),    QStringLiteral("Екатерина"),
         QStringLiteral("Юлия"),       QStringLiteral("Оксана"),   QStringLiteral("Вера"),
         QStringLiteral("Надежда"),    QStringLiteral("Людмила"),  QStringLiteral("Галина"),
         QStringLiteral("Александра"), QStringLiteral("Виктория"), QStringLiteral("Дарья"),
         QStringLiteral("Алиса"),      QStringLiteral("Ксения")},
        {QStringLiteral("Иванов"),   QStringLiteral("Петров"),  QStringLiteral("Сидоров"),
         QStringLiteral("Кузнецов"), QStringLiteral("Смирнов"), QStringLiteral("Попов"),
         QStringLiteral("Соколов"),  QStringLiteral("Лебедев"), QStringLiteral("Козлов"),
         QStringLiteral("Новиков"),  QStringLiteral("Морозов"), QStringLiteral("Соловьёв"),
         QStringLiteral("Васильев"), QStringLiteral("Зайцев"),  QStringLiteral("Павлов"),
         QStringLiteral("Семёнов"),  QStringLiteral("Голубев"), QStringLiteral("Виноградов"),
         QStringLiteral("Богданов"), QStringLiteral("Воробьёв")},
        {QStringLiteral("Иванович"),      QStringLiteral("Петрович"),
         QStringLiteral("Сергеевич"),     QStringLiteral("Андреевич"),
         QStringLiteral("Алексеевич"),    QStringLiteral("Владимирович"),
         QStringLiteral("Николаевич"),    QStringLiteral("Михайлович"),
         QStringLiteral("Александрович"), QStringLiteral("Дмитриевич"),
         QStringLiteral("Викторович"),    QStringLiteral("Олегович"),
         QStringLiteral("Павлович"),      QStringLiteral("Юрьевич"),
         QStringLiteral("Игоревич"),      QStringLiteral("Максимович"),
         QStringLiteral("Антонович"),     QStringLiteral("Романович"),
         QStringLiteral("Артёмович"),     QStringLiteral("Денисович")},
        {QStringLiteral("Ивановна"),      QStringLiteral("Петровна"),
         QStringLiteral("Сергеевна"),     QStringLiteral("Андреевна"),
         QStringLiteral("Алексеевна"),    QStringLiteral("Владимировна"),
         QStringLiteral("Николаевна"),    QStringLiteral("Михайловна"),
         QStringLiteral("Александровна"), QStringLiteral("Дмитриевна"),
         QStringLiteral("Викторовна"),    QStringLiteral("Олеговна"),
         QStringLiteral("Павловна"),      QStringLiteral("Юрьевна"),
         QStringLiteral("Игоревна"),      QStringLiteral("Максимовна"),
         QStringLiteral("Антоновна"),     QStringLiteral("Романовна"),
         QStringLiteral("Артёмовна"),     QStringLiteral("Денисовна")},
        {QStringLiteral("Торговая компания"), QStringLiteral("Производственное объединение"),
         QStringLiteral("Строительная фирма"), QStringLiteral("Логистический центр"),
         QStringLiteral("Агентство недвижимости"), QStringLiteral("Мебельная фабрика"),
         QStringLiteral("Транспортная компания"), QStringLiteral("Ресторанная группа"),
         QStringLiteral("Цифровые решения"), QStringLiteral("Сельхозпредприятие")},
        {QStringLiteral("Север"), QStringLiteral("Юг"), QStringLiteral("Восток"),
         QStringLiteral("Запад"), QStringLiteral("Урал"), QStringLiteral("Сибирь"),
         QStringLiteral("Атлант"), QStringLiteral("Беркут"), QStringLiteral("Сокол"),
         QStringLiteral("Волга"), QStringLiteral("Кедр"), QStringLiteral("Вершина"),
         QStringLiteral("Формат"), QStringLiteral("Контур"), QStringLiteral("Горизонт")}};

    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO clients (full_name, org, phone, email, status,"
                             " search_name, created_at, updated_at)"
                             " VALUES (:full_name, :org, :phone, :email, :status, :search_name,"
                             " :created_at, :updated_at)"));

    QRandomGenerator *rng = QRandomGenerator::global();
    const QDateTime epoch(QDate(2019, 1, 1), QTime(9, 0, 0), QTimeZone::UTC);
    const qint64 epochMs = epoch.toMSecsSinceEpoch();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 spanMs = now.toMSecsSinceEpoch() - epochMs;

    const auto digits = [rng](int n) {
        QString s;
        s.reserve(n);
        for (int i = 0; i < n; ++i)
            s.append(QLatin1Char(static_cast<char>('0' + rng->bounded(10))));
        return s;
    };

    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }

    for (int i = 0; i < count; ++i) {
        const bool female = rng->bounded(2) == 0;
        const QStringList &givenNames = female ? names.femaleGiven : names.maleGiven;
        const QStringList &patronymics = female ? names.femalePatronymic : names.malePatronymic;
        const QString &firstName = givenNames.at(rng->bounded(givenNames.size()));
        const QString lastName = names.lastNames.at(rng->bounded(names.lastNames.size())) +
                                 (female ? QStringLiteral("а") : QString());
        const QString &patronymic = patronymics.at(rng->bounded(patronymics.size()));
        const QString fullName = QStringLiteral("%1 %2 %3").arg(lastName, firstName, patronymic);
        const QString org =
            QStringLiteral("%1 «%2»").arg(names.orgTypes.at(rng->bounded(names.orgTypes.size())),
                                          names.orgBrands.at(rng->bounded(names.orgBrands.size())));
        const QString phone =
            QStringLiteral("+7 9%1 %2-%3-%4").arg(digits(2), digits(3), digits(2), digits(2));
        const QString email = QStringLiteral("%1%2@example.ru").arg(lastName.toLower(), digits(3));
        const QString status =
            rng->bounded(100) < 8 ? QLatin1String(kStatusArchived) : QLatin1String(kStatusActive);
        const qint64 createdMs =
            epochMs + static_cast<qint64>(rng->generate64() % static_cast<quint64>(spanMs));
        const QString createdAt =
            QDateTime::fromMSecsSinceEpoch(createdMs, QTimeZone::UTC).toString(Qt::ISODateWithMs);
        QDateTime updated =
            QDateTime::fromMSecsSinceEpoch(createdMs, QTimeZone::UTC).addDays(rng->bounded(0, 181));
        if (updated > now)
            updated = now;
        const QString updatedAt = updated.toString(Qt::ISODateWithMs);

        q.bindValue(QStringLiteral(":full_name"), fullName);
        q.bindValue(QStringLiteral(":org"), org);
        q.bindValue(QStringLiteral(":phone"), phone);
        q.bindValue(QStringLiteral(":email"), email);
        q.bindValue(QStringLiteral(":status"), status);
        q.bindValue(QStringLiteral(":search_name"),
                    searchNameFor(Client{{}, fullName, org, phone, email, {}, status, {}, {}}));
        q.bindValue(QStringLiteral(":created_at"), createdAt);
        q.bindValue(QStringLiteral(":updated_at"), updatedAt);
        if (!q.exec()) {
            m_lastError = q.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        m_lastError = db.lastError().text();
        db.rollback();
        return false;
    }
    // Make the seed visible in the main file right away, even if the server
    // is later killed without a graceful SQLite close.
    if (!checkpoint(db))
        qCWarning(lcDb) << "wal checkpoint after seed failed:" << db.lastError().text();
    m_lastError.clear();
    return true;
}

bool Database::getClient(qint64 id, Client *out)
{
    QMutexLocker locker(&m_mutex);
    return getClientLocked(id, out);
}

bool Database::getClientLocked(qint64 id, Client *out)
{
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM clients WHERE id = :id AND deleted = 0")
                  .arg(clientColumns().join(QStringLiteral(", "))));
    q.bindValue(QStringLiteral(":id"), id);
    if (!exec(q, &m_lastError))
        return false;

    if (!q.next())
        return false;
    const QSqlRecord rec = q.record();
    QVariantMap row;
    for (const QString &field : clientColumns())
        row.insert(field, rec.value(field));
    if (out)
        *out = rowToClient(row);
    return true;
}

bool Database::updateClient(qint64 id, const Client &c, Client *out)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("UPDATE clients SET full_name = :full_name, org = :org, phone = :phone,"
                       " email = :email, notes = :notes, status = :status,"
                       " search_name = :search_name, updated_at = :updated_at"
                       " WHERE id = :id AND deleted = 0"));
    const QVariantMap row = clientToRow(c);
    for (auto it = row.constBegin(); it != row.constEnd(); ++it)
        q.bindValue(QStringLiteral(":%1").arg(it.key()), it.value());
    q.bindValue(QStringLiteral(":id"), id);

    if (!exec(q, &m_lastError))
        return false;

    Client tmp;
    const bool ok = getClientLocked(id, &tmp);
    if (ok && out)
        *out = tmp;
    return ok;
}

bool Database::deleteClient(qint64 id)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE clients SET deleted = 1, updated_at = :updated_at WHERE id = :id AND deleted = 0"));
    q.bindValue(QStringLiteral(":updated_at"), utcIso());
    q.bindValue(QStringLiteral(":id"), id);

    if (!exec(q, &m_lastError))
        return false;
    return q.numRowsAffected() == 1;
}

ClientPage Database::listClients(const ClientFilter &f)
{
    QMutexLocker locker(&m_mutex);
    ClientPage page;

    QStringList where;
    where << QStringLiteral("deleted = 0");

    QString statusBind;
    QString searchBind;
    QString digitsBind;

    if (!f.status.isEmpty()) {
        where << QStringLiteral("status = :status");
        statusBind = f.status;
    }
    if (!f.search.isEmpty()) {
        const QString needle = f.search.toLower();
        // SQLite LIKE is case-sensitive for Cyrillic; both sides are lowered
        // here (QString::toLower is Unicode-aware), so the match is
        // case-insensitive for any script. Escape LIKE wildcards first.
        const auto escapeLike = [](QString needle) {
            needle.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            needle.replace(QLatin1Char('%'), QStringLiteral("\\%"));
            needle.replace(QLatin1Char('_'), QStringLiteral("\\_"));
            return QStringLiteral("%%1%").arg(needle);
        };
        QStringList clauses{QStringLiteral("search_name LIKE :s1 ESCAPE '\\'")};
        searchBind = escapeLike(needle);
        // Phone numbers are searchable by digits only ("9123456789" finds
        // "+7 (912) 345-67-89"), regardless of the display mask; both
        // variants are OR-ed so either form of the query matches. The
        // digit-only variant needs at least 4 digits so short numeric
        // fragments of other fields ("100_") do not create false matches.
        QString digitsOnly;
        for (const QChar ch : needle) {
            if (ch.isDigit())
                digitsOnly.append(ch);
        }
        if (digitsOnly.size() >= 4 && digitsOnly != needle) {
            clauses << QStringLiteral("search_name LIKE :s2 ESCAPE '\\'");
            digitsBind = escapeLike(digitsOnly);
        }
        where << QStringLiteral("(%1)").arg(clauses.join(QStringLiteral(" OR ")));
    }

    const QString whereSql = where.join(QStringLiteral(" AND "));

    QSqlDatabase db = threadDb();

    const auto bindFilters = [&statusBind, &searchBind, &digitsBind](QSqlQuery &q) {
        if (!statusBind.isEmpty())
            q.bindValue(QStringLiteral(":status"), statusBind);
        if (!searchBind.isEmpty())
            q.bindValue(QStringLiteral(":s1"), searchBind);
        if (!digitsBind.isEmpty())
            q.bindValue(QStringLiteral(":s2"), digitsBind);
    };

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM clients WHERE %1").arg(whereSql));
        bindFilters(q);
        if (!exec(q, &m_lastError))
            return page;
        page.total = q.next() ? q.value(0).toLongLong() : 0;
    }

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT %1 FROM clients WHERE %2 ORDER BY id DESC"
                                 " LIMIT :limit OFFSET :offset")
                      .arg(clientColumns().join(QStringLiteral(", ")), whereSql));
        bindFilters(q);
        q.bindValue(QStringLiteral(":limit"),
                    f.limit < 0 ? kMaxPageSize : qMin(f.limit, kMaxPageSize));
        q.bindValue(QStringLiteral(":offset"), f.offset < 0 ? 0 : f.offset);
        if (!exec(q, &m_lastError))
            return page;

        const QSqlRecord rec = q.record();
        while (q.next()) {
            QVariantMap row;
            for (const QString &field : clientColumns())
                row.insert(field, q.value(rec.indexOf(field)));
            page.items.append(rowToClient(row));
        }
    }
    return page;
}

UserRecord Database::rowToUser(const QVariantMap &row)
{
    UserRecord u;
    u.id = row.value(QStringLiteral("id")).toLongLong();
    u.username = row.value(QStringLiteral("username")).toString();
    u.passwordHash = row.value(QStringLiteral("password_hash")).toString();
    u.role = row.value(QStringLiteral("role")).toString();
    return u;
}
qint64 Database::addUser(const QString &username, const QString &passwordHash, const QString &role)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO users (username, password_hash, role)"
                             " VALUES (:username, :password_hash, :role)"));
    q.bindValue(QStringLiteral(":username"), username);
    q.bindValue(QStringLiteral(":password_hash"), passwordHash);
    q.bindValue(QStringLiteral(":role"), role);
    if (!exec(q, &m_lastError))
        return 0;
    return q.lastInsertId().toLongLong();
}

bool Database::getUserByUsername(const QString &username, UserRecord *out)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE username = :username AND deleted = 0")
                  .arg(userColumns().join(QStringLiteral(", "))));
    q.bindValue(QStringLiteral(":username"), username);
    if (!exec(q, &m_lastError))
        return false;
    if (!q.next())
        return false;
    QVariantMap row;
    for (const QString &field : userColumns())
        row.insert(field, q.value(field));
    if (out)
        *out = rowToUser(row);
    return true;
}

bool Database::getUserByTokenHash(const QString &tokenHash, UserRecord *out)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT u.%1 FROM tokens t JOIN users u ON u.id = t.user_id"
                             " WHERE t.token_hash = :token_hash AND u.deleted = 0"
                             " AND (t.expires_at IS NULL OR t.expires_at > :now)")
                  .arg(userColumns().join(QStringLiteral(", u."))));
    q.bindValue(QStringLiteral(":token_hash"), tokenHash);
    q.bindValue(QStringLiteral(":now"), utcIso());
    if (!exec(q, &m_lastError))
        return false;
    if (!q.next())
        return false;
    QVariantMap row;
    for (const QString &field : userColumns())
        row.insert(field, q.value(field));
    if (out)
        *out = rowToUser(row);
    return true;
}

bool Database::storeToken(const QString &tokenHash, qint64 userId, int ttlSeconds)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO tokens (token_hash, user_id, expires_at)"
                             " VALUES (:token_hash, :user_id, :expires_at)"));
    q.bindValue(QStringLiteral(":token_hash"), tokenHash);
    q.bindValue(QStringLiteral(":user_id"), userId);
    q.bindValue(QStringLiteral(":expires_at"),
                QDateTime::currentDateTimeUtc().addSecs(ttlSeconds).toString(Qt::ISODateWithMs));
    return exec(q, &m_lastError);
}

int Database::deleteExpiredTokens()
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(
        QStringLiteral("DELETE FROM tokens WHERE expires_at IS NOT NULL AND expires_at <= :now"));
    q.bindValue(QStringLiteral(":now"), utcIso());
    if (!exec(q, &m_lastError))
        return -1;
    return q.numRowsAffected();
}

QVector<UserRecord> Database::listUsers()
{
    QMutexLocker locker(&m_mutex);
    QVector<UserRecord> users;
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM users WHERE deleted = 0 ORDER BY id")
                  .arg(userColumns().join(QStringLiteral(", "))));
    if (!exec(q, &m_lastError))
        return users;
    const QSqlRecord rec = q.record();
    while (q.next()) {
        QVariantMap row;
        for (const QString &field : userColumns())
            row.insert(field, q.value(rec.indexOf(field)));
        users.append(rowToUser(row));
    }
    return users;
}

qint64 Database::userCount()
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM users WHERE deleted = 0"));
    if (!exec(q, &m_lastError))
        return -1;
    return q.next() ? q.value(0).toLongLong() : 0;
}

qint64 Database::clientCount()
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM clients WHERE deleted = 0"));
    if (!exec(q, &m_lastError))
        return -1;
    return q.next() ? q.value(0).toLongLong() : 0;
}

AuditEntry Database::rowToAudit(const QVariantMap &row)
{
    AuditEntry a;
    a.id = row.value(QStringLiteral("id")).toLongLong();
    a.userId = row.value(QStringLiteral("user_id")).toLongLong();
    a.action = row.value(QStringLiteral("action")).toString();
    a.entity = row.value(QStringLiteral("entity")).toString();
    a.entityId = row.value(QStringLiteral("entity_id")).toLongLong();
    a.createdAt = row.value(QStringLiteral("created_at")).toString();
    return a;
}

qint64 Database::addAudit(qint64 userId, const QString &action, const QString &entity,
                          qint64 entityId)
{
    QMutexLocker locker(&m_mutex);
    QSqlDatabase db = threadDb();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO audit (user_id, action, entity, entity_id, created_at)"
                             " VALUES (:user_id, :action, :entity, :entity_id, :created_at)"));
    q.bindValue(QStringLiteral(":user_id"), userId > 0 ? QVariant(userId) : QVariant());
    q.bindValue(QStringLiteral(":action"), action);
    q.bindValue(QStringLiteral(":entity"), entity);
    q.bindValue(QStringLiteral(":entity_id"), entityId);
    q.bindValue(QStringLiteral(":created_at"), utcIso());
    if (!exec(q, &m_lastError))
        return 0;
    return q.lastInsertId().toLongLong();
}

AuditPage Database::listAudit(const AuditFilter &f)
{
    QMutexLocker locker(&m_mutex);
    AuditPage page;
    QSqlDatabase db = threadDb();

    {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM audit"));
        if (!exec(q, &m_lastError))
            return page;
        page.total = q.next() ? q.value(0).toLongLong() : 0;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT %1 FROM audit ORDER BY id DESC LIMIT :limit OFFSET :offset")
                  .arg(auditColumns().join(QStringLiteral(", "))));
    q.bindValue(QStringLiteral(":limit"), f.limit < 0 ? kMaxPageSize : qMin(f.limit, kMaxPageSize));
    q.bindValue(QStringLiteral(":offset"), f.offset < 0 ? 0 : f.offset);
    if (!exec(q, &m_lastError))
        return page;
    const QSqlRecord rec = q.record();
    while (q.next()) {
        QVariantMap row;
        for (const QString &field : auditColumns())
            row.insert(field, q.value(rec.indexOf(field)));
        page.items.append(rowToAudit(row));
    }
    return page;
}

} // namespace registro
