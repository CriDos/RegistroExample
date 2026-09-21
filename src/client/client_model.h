#pragma once

#include "api_client.h"

#include <QAbstractListModel>
#include <QTimer>

namespace registro {

class ClientModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(
        QString statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
    Q_PROPERTY(qint64 total READ total NOTIFY totalChanged)

public:
    static constexpr int kPageSize = 50;

    enum Role {
        FullNameRole = Qt::UserRole + 1,
        OrgRole,
        PhoneRole,
        EmailRole,
        StatusRole,
        IdRole,
        CreatedAtRole,
        UpdatedAtRole,
    };

    explicit ClientModel(ApiClient *api, QObject *parent = nullptr);

    QString search() const { return m_search; }
    void setSearch(const QString &s);
    QString statusFilter() const { return m_status; }
    void setStatusFilter(const QString &s);
    bool busy() const { return m_busy; }
    int currentPage() const { return m_currentPage; }
    int pageCount() const { return m_pageCount; }
    qint64 total() const { return m_total; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void reloadFromFirstPage();
    Q_INVOKABLE void goToPage(int page);
    Q_INVOKABLE void nextPage();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void firstPage();
    Q_INVOKABLE void lastPage();
    Q_INVOKABLE QVariantMap clientAt(int row) const;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void searchChanged();
    void statusFilterChanged();
    void busyChanged();
    void currentPageChanged();
    void pageCountChanged();
    void totalChanged();

private:
    void fetchPage();
    void resetToFirstPage();
    void onClientsLoaded(qint64 total, const QVector<Client> &items);
    void onFetchFailed();

    ApiClient *m_api;
    QVector<Client> m_items;
    QString m_search;
    QString m_status;
    QTimer m_debounce;
    bool m_busy = false;
    bool m_pendingReload = false;
    bool m_pendingFirstPage = false;
    int m_currentPage = 1;
    int m_pageCount = 0;
    qint64 m_total = 0;
};

} // namespace registro