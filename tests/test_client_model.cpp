#include "client_model.h"

#include <QSignalSpy>
#include <QtTest>

using namespace registro;

class StubApi final : public ApiClient {
public:
    StubApi() : ApiClient(QUrl(QStringLiteral("http://stub"))) {}

    QVector<Client> page;
    qint64 totalOverride = -1;
    int fetchCalls = 0;
    bool failNetwork = false;
    bool failHttp = false;
    bool hold = false;
    qint64 lastOffset = -1;
    qint64 lastLimit = -1;

    void flush()
    {
        Q_ASSERT(hold);
        hold = false;
        emit clientsLoaded(totalOverride >= 0 ? totalOverride : page.size(), page);
    }

protected:
    void doFetchClients(const QString &search, const QString &status, qint64 offset,
                        qint64 limit) override
    {
        ++fetchCalls;
        Q_UNUSED(search)
        Q_UNUSED(status)
        lastOffset = offset;
        lastLimit = limit;
        if (hold)
            return;
        if (failNetwork) {
            failNetwork = false;
            emit networkErrorOccurred(QStringLiteral("connection refused"));
            return;
        }
        if (failHttp) {
            failHttp = false;
            emit errorOccurred(QStringLiteral("HTTP 500"));
            return;
        }
        emit clientsLoaded(totalOverride >= 0 ? totalOverride : page.size(), page);
    }
};

class TestClientModel final : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        m_api.reset(new StubApi());
        m_model.reset(new ClientModel(m_api.get()));
        m_calls = &m_api->fetchCalls;
        *m_calls = 0;
    }

    void populateAndRoles()
    {
        m_api->page = {client(1, QStringLiteral("Иван Иванов"), QStringLiteral("active")),
                       client(2, QStringLiteral("Пётр Петров"), QStringLiteral("archived"))};

        m_model->reload();
        QCOMPARE(*m_calls, 1);
        QCOMPARE(m_model->rowCount(), 2);
        QCOMPARE(m_model->total(), 2);
        QCOMPARE(m_model->pageCount(), 1);
        QCOMPARE(m_model->currentPage(), 1);
        QVERIFY(!m_model->busy());

        const QModelIndex first = m_model->index(0, 0);
        QCOMPARE(m_model->data(first, ClientModel::FullNameRole).toString(),
                 QStringLiteral("Иван Иванов"));
        QCOMPARE(m_model->data(first, ClientModel::StatusRole).toString(),
                 QStringLiteral("active"));
        QCOMPARE(m_model->data(m_model->index(1, 0), ClientModel::IdRole).toLongLong(), 2);

        const QVariantMap map = m_model->clientAt(0);
        QCOMPARE(map.value(QStringLiteral("fullName")).toString(), QStringLiteral("Иван Иванов"));

        QVERIFY(m_model->roleNames().value(ClientModel::FullNameRole) ==
                QByteArrayLiteral("fullName"));
        QVERIFY(m_model->roleNames().value(ClientModel::CreatedAtRole) ==
                QByteArrayLiteral("createdAt"));
        QVERIFY(m_model->roleNames().value(ClientModel::UpdatedAtRole) ==
                QByteArrayLiteral("updatedAt"));
    }

    void debouncedSearch()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_model->reload();
        QCOMPARE(*m_calls, 1);

        m_model->setSearch(QStringLiteral("a"));
        m_model->setSearch(QStringLiteral("ab"));
        m_model->setSearch(QStringLiteral("abc"));
        QCOMPARE(*m_calls, 1);
        QCOMPARE(m_model->search(), QStringLiteral("abc"));

        QTRY_COMPARE_WITH_TIMEOUT(*m_calls, 2, 2000);
        QCOMPARE(m_model->total(), 1);
    }

    void statusFilterReloadsImmediately()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("archived"))};
        m_model->setStatusFilter(QStringLiteral("archived"));
        QCOMPARE(*m_calls, 1);
        QCOMPARE(m_model->statusFilter(), QStringLiteral("archived"));
        QCOMPARE(m_model->rowCount(), 1);
    }

    void pageNavigation()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active")),
                       client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_api->totalOverride = 125;
        m_model->reload();
        QCOMPARE(*m_calls, 1);
        QCOMPARE(m_model->pageCount(), 3);
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_api->lastOffset, 0);
        QCOMPARE(m_api->lastLimit, ClientModel::kPageSize);

        m_model->goToPage(2);
        QCOMPARE(*m_calls, 2);
        QCOMPARE(m_model->currentPage(), 2);
        QCOMPARE(m_api->lastOffset, 50);

        m_model->goToPage(3);
        QCOMPARE(*m_calls, 3);
        QCOMPARE(m_model->currentPage(), 3);
        QCOMPARE(m_api->lastOffset, 100);

        m_model->goToPage(99);
        QCOMPARE(*m_calls, 3);
        QCOMPARE(m_model->currentPage(), 3);

        m_model->nextPage();
        QCOMPARE(*m_calls, 3);
        QCOMPARE(m_model->currentPage(), 3);

        m_model->prevPage();
        QCOMPARE(*m_calls, 4);
        QCOMPARE(m_model->currentPage(), 2);

        m_model->firstPage();
        QCOMPARE(*m_calls, 5);
        QCOMPARE(m_model->currentPage(), 1);

        m_model->firstPage();
        QCOMPARE(*m_calls, 5);

        m_model->lastPage();
        QCOMPARE(*m_calls, 6);
        QCOMPARE(m_model->currentPage(), 3);

        m_model->goToPage(0);
        QCOMPARE(*m_calls, 7);
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_api->lastOffset, 0);
    }

    void searchResetsToFirstPage()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->totalOverride = 125;
        m_model->reload();
        m_model->goToPage(3);
        QCOMPARE(m_model->currentPage(), 3);

        m_api->totalOverride = 40;
        m_api->page = {client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_model->setSearch(QStringLiteral("B"));
        QTRY_COMPARE_WITH_TIMEOUT(*m_calls, 3, 2000);
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_api->lastOffset, 0);
        QCOMPARE(m_model->pageCount(), 1);
        QCOMPARE(m_model->rowCount(), 1);
    }

    void pageCountClampsCurrentPage()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->totalOverride = 125;
        m_model->reload();
        m_model->goToPage(3);
        QCOMPARE(m_model->currentPage(), 3);

        m_api->totalOverride = 50;
        m_api->page = {client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_model->setStatusFilter(QStringLiteral("active"));
        QCOMPARE(*m_calls, 3);
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_model->pageCount(), 1);

        m_api->totalOverride = 60;
        m_api->page = {client(3, QStringLiteral("C"), QStringLiteral("active"))};
        m_model->reload();
        QCOMPARE(*m_calls, 4);
        QCOMPARE(m_model->pageCount(), 2);
        QCOMPARE(m_model->currentPage(), 1);

        m_model->goToPage(2);
        QCOMPARE(m_api->lastOffset, 50);

        m_api->totalOverride = 55;
        m_api->page = {client(4, QStringLiteral("D"), QStringLiteral("active"))};
        m_model->reload();
        QCOMPARE(*m_calls, 6);
        QCOMPARE(m_model->pageCount(), 2);
        QCOMPARE(m_model->currentPage(), 2);
    }

    void pageShrinkRefetchesClampedPage()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->totalOverride = 125;
        m_model->reload();
        m_model->goToPage(3);
        QCOMPARE(*m_calls, 2);
        QCOMPARE(m_model->currentPage(), 3);

        // Data shrinks to a single page while the user sits on page 3.
        m_api->totalOverride = 40;
        m_api->page = {client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_model->reload();
        QCOMPARE(*m_calls, 4);
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_api->lastOffset, 0);
        QCOMPARE(m_model->pageCount(), 1);
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(m_model->data(m_model->index(0, 0), ClientModel::FullNameRole).toString(),
                 QStringLiteral("B"));
    }

    void createWhileBusyJumpsToFirstPage()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->totalOverride = 125;
        m_model->reload();
        m_model->goToPage(3);
        QCOMPARE(*m_calls, 2);

        m_api->hold = true;
        m_api->page = {client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_model->goToPage(2);
        QVERIFY(m_model->busy());
        // A client creation lands while the page switch is in flight.
        m_model->reloadFromFirstPage();
        m_api->flush();
        QCOMPARE(m_model->currentPage(), 1);
        QCOMPARE(m_api->lastOffset, 0);
        QCOMPARE(*m_calls, 4);
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(m_model->data(m_model->index(0, 0), ClientModel::FullNameRole).toString(),
                 QStringLiteral("B"));
    }

    void busyFlagDuringFetch()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        QSignalSpy busySpy(m_model.get(), &ClientModel::busyChanged);
        m_model->reload();
        QVERIFY(busySpy.count() >= 2);
        QVERIFY(!m_model->busy());
    }

    void fetchFailureClearsBusyAndAllowsRetry()
    {
        QSignalSpy busySpy(m_model.get(), &ClientModel::busyChanged);

        m_api->failNetwork = true;
        m_model->reload();
        QCOMPARE(*m_calls, 1);
        QVERIFY(!m_model->busy());
        QCOMPARE(m_model->rowCount(), 0);

        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active")),
                       client(2, QStringLiteral("B"), QStringLiteral("active"))};
        m_api->totalOverride = 5;
        m_model->reload();
        QCOMPARE(*m_calls, 2);
        QCOMPARE(m_model->total(), 5);

        m_api->failHttp = true;
        m_model->reload();
        QVERIFY(!m_model->busy());
        QCOMPARE(m_model->rowCount(), 2);

        QVERIFY(busySpy.count() >= 5);
    }

    void reloadRequestedWhileBusyIsQueued()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->totalOverride = 3;

        m_api->hold = true;
        m_model->reload();
        QCOMPARE(*m_calls, 1);
        QVERIFY(m_model->busy());

        m_model->setStatusFilter(QStringLiteral("archived"));
        QCOMPARE(*m_calls, 1);

        m_api->page = {client(2, QStringLiteral("B"), QStringLiteral("archived"))};
        m_api->totalOverride = 3;
        m_api->flush();
        QCOMPARE(*m_calls, 2);
        QVERIFY(!m_model->busy());
        QCOMPARE(m_model->rowCount(), 1);
        QCOMPARE(m_model->data(m_model->index(0, 0), ClientModel::FullNameRole).toString(),
                 QStringLiteral("B"));
    }

    void logoutWhileBusyClearsBusyWithoutRefetch()
    {
        m_api->page = {client(1, QStringLiteral("A"), QStringLiteral("active"))};
        m_api->hold = true;
        m_model->reload();
        QVERIFY(m_model->busy());

        m_api->logout();
        QVERIFY(!m_model->busy());
        QCOMPARE(*m_calls, 1);

        m_api->hold = false;
        m_model->reload();
        QCOMPARE(*m_calls, 2);
        QVERIFY(!m_model->busy());
        QCOMPARE(m_model->rowCount(), 1);
    }

    void clientAtCarriesNotes()
    {
        Client c = client(1, QStringLiteral("Иван Иванов"), QStringLiteral("active"));
        c.notes = QStringLiteral("служебная заметка");
        m_api->page = {c};

        m_model->reload();
        QCOMPARE(*m_calls, 1);

        const QVariantMap card = m_model->clientAt(0);
        QCOMPARE(card.value(QStringLiteral("fullName")).toString(), QStringLiteral("Иван Иванов"));
        QCOMPARE(card.value(QStringLiteral("notes")).toString(),
                 QStringLiteral("служебная заметка"));
    }

private:
    static Client client(qint64 id, const QString &name, const QString &status)
    {
        Client c;
        c.id = id;
        c.fullName = name;
        c.status = status;
        return c;
    }

    QScopedPointer<StubApi> m_api;
    QScopedPointer<ClientModel> m_model;
    int *m_calls = nullptr;
};

QTEST_GUILESS_MAIN(TestClientModel)
#include "test_client_model.moc"