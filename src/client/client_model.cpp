#include "client_model.h"

namespace registro {

namespace {
constexpr int kSearchDebounceMs = 300;
}

ClientModel::ClientModel(ApiClient *api, QObject *parent)
    : QAbstractListModel(parent), m_api(api), m_debounce(this)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kSearchDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, [this]() {
        if (m_busy) {
            m_debounce.start();
            return;
        }
        reload();
    });
    connect(m_api, &ApiClient::clientsLoaded, this,
            [this](qint64 total, const QVector<Client> &items) { onClientsLoaded(total, items); });
    connect(m_api, &ApiClient::errorOccurred, this, [this](const QString &) { onFetchFailed(); });
    connect(m_api, &ApiClient::networkErrorOccurred, this,
            [this](const QString &) { onFetchFailed(); });
    connect(m_api, &ApiClient::sessionChanged, this, [this]() {
        if (m_api->loggedIn())
            return;
        m_pendingReload = false;
        onFetchFailed();
    });
}

void ClientModel::setSearch(const QString &s)
{
    if (m_search == s)
        return;
    m_search = s;
    resetToFirstPage();
    emit searchChanged();
    m_debounce.start();
}

void ClientModel::setStatusFilter(const QString &s)
{
    if (m_status == s)
        return;
    m_status = s;
    resetToFirstPage();
    emit statusFilterChanged();
    reload();
}

void ClientModel::reload()
{
    if (m_busy) {
        m_pendingReload = true;
        return;
    }
    fetchPage();
}

void ClientModel::reloadFromFirstPage()
{
    if (m_busy) {
        m_pendingFirstPage = true;
        return;
    }
    resetToFirstPage();
    fetchPage();
}

void ClientModel::goToPage(int page)
{
    const int target = qBound(1, page, qMax(1, m_pageCount));
    if (m_busy || target == m_currentPage)
        return;
    m_currentPage = target;
    emit currentPageChanged();
    fetchPage();
}

void ClientModel::nextPage()
{
    if (m_currentPage < m_pageCount)
        goToPage(m_currentPage + 1);
}

void ClientModel::prevPage()
{
    if (m_currentPage > 1)
        goToPage(m_currentPage - 1);
}

void ClientModel::firstPage()
{
    goToPage(1);
}

void ClientModel::lastPage()
{
    goToPage(m_pageCount);
}

void ClientModel::resetToFirstPage()
{
    if (m_currentPage == 1)
        return;
    m_currentPage = 1;
    emit currentPageChanged();
}

void ClientModel::fetchPage()
{
    m_busy = true;
    emit busyChanged();
    const qint64 offset = static_cast<qint64>(m_currentPage - 1) * kPageSize;
    m_api->fetchClients(m_search, m_status, offset, kPageSize);
}

void ClientModel::onClientsLoaded(qint64 total, const QVector<Client> &items)
{
    const int newPageCount = total > 0 ? static_cast<int>((total + kPageSize - 1) / kPageSize) : 0;
    if (newPageCount != m_pageCount) {
        m_pageCount = newPageCount;
        emit pageCountChanged();
    }

    if (m_pageCount > 0 && m_currentPage > m_pageCount) {
        // The response belongs to a page beyond the (shrunk) range: discard it
        // and refetch the clamped page with the correct offset.
        m_currentPage = m_pageCount;
        emit currentPageChanged();
        beginResetModel();
        m_items.clear();
        endResetModel();
        m_total = total;
        emit totalChanged();
        fetchPage();
        return;
    }

    beginResetModel();
    m_items = items;
    endResetModel();

    m_total = total;
    m_busy = false;
    emit totalChanged();
    emit busyChanged();

    if (m_pendingFirstPage) {
        m_pendingFirstPage = false;
        resetToFirstPage();
        fetchPage();
        return;
    }
    if (m_pendingReload) {
        m_pendingReload = false;
        reload();
    }
}

void ClientModel::onFetchFailed()
{
    if (!m_busy)
        return;
    m_busy = false;
    emit busyChanged();

    if (m_pendingFirstPage) {
        m_pendingFirstPage = false;
        resetToFirstPage();
        fetchPage();
        return;
    }
    if (m_pendingReload) {
        m_pendingReload = false;
        reload();
    }
}

QVariantMap ClientModel::clientAt(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    // camelCase keys to match the model roles (the card form consumes this
    // map, the list roles use the same names).
    const Client &c = m_items.at(row);
    return {{QStringLiteral("id"), c.id},
            {QStringLiteral("fullName"), c.fullName},
            {QStringLiteral("org"), c.org},
            {QStringLiteral("phone"), c.phone},
            {QStringLiteral("email"), c.email},
            {QStringLiteral("notes"), c.notes},
            {QStringLiteral("status"), c.status},
            {QStringLiteral("createdAt"), c.createdAt},
            {QStringLiteral("updatedAt"), c.updatedAt}};
}

int ClientModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ClientModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Client &c = m_items.at(index.row());
    switch (role) {
    case FullNameRole:
        return c.fullName;
    case OrgRole:
        return c.org;
    case PhoneRole:
        return c.phone;
    case EmailRole:
        return c.email;
    case StatusRole:
        return c.status;
    case IdRole:
        return static_cast<qlonglong>(c.id);
    case CreatedAtRole:
        return c.createdAt;
    case UpdatedAtRole:
        return c.updatedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> ClientModel::roleNames() const
{
    return {
        {FullNameRole, "fullName"},   {OrgRole, "org"},
        {PhoneRole, "phone"},         {EmailRole, "email"},
        {StatusRole, "status"},       {IdRole, "id"},
        {CreatedAtRole, "createdAt"}, {UpdatedAtRole, "updatedAt"},
    };
}

} // namespace registro