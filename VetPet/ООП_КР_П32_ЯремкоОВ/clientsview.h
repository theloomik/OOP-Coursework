#ifndef CLIENTSVIEW_H
#define CLIENTSVIEW_H

#include <QSqlDatabase>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class ClientsView;
}
QT_END_NAMESPACE

struct ClientRecord {
    int id = 0;
    QString phone;
    QString firstName;
    QString lastName;
    QStringList petNames;
};

class ClientsView : public QWidget
{
    Q_OBJECT

public:
    explicit ClientsView(QWidget *parent = nullptr);
    ~ClientsView() override;

    void setDatabase(const QSqlDatabase &database);
    void reload();

signals:
    void clientSelected(int clientId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onSortClicked();
    void onAddClicked();
    void onCancelAddClicked();
    void onSaveAddClicked();
    void onTableCellClicked(int row, int column);

private:
    Ui::ClientsView *ui;
    QSqlDatabase m_db;
    QList<ClientRecord> m_allClients;
    bool m_sortAsc = true;

    void setupUiBehavior();
    void setupConnections();
    void updateList();
    void populateTable(const QList<ClientRecord> &clients);
    void showAddModal(bool visible);
    void clearAddForm();
    void setErrorMessage(const QString &message);
    bool matchesSearch(const ClientRecord &client, const QString &searchText) const;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // CLIENTSVIEW_H
