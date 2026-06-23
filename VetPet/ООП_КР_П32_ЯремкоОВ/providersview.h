#ifndef PROVIDERSVIEW_H
#define PROVIDERSVIEW_H

#include <QSqlDatabase>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class ProvidersView;
}
QT_END_NAMESPACE

struct ProviderRecord {
    int id = 0;
    QString phone;
    QString name;
    QString contactPerson;
};

class ProvidersView : public QWidget
{
    Q_OBJECT

public:
    explicit ProvidersView(QWidget *parent = nullptr);
    ~ProvidersView() override;

    void setDatabase(const QSqlDatabase &database);
    void reload();

signals:
    void providerSelected(int providerId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onSortClicked();
    void onAddClicked();
    void onCancelAddClicked();
    void onSaveAddClicked();
    void onTableCellClicked(int row, int column);

private:
    Ui::ProvidersView *ui;
    QSqlDatabase m_db;
    QList<ProviderRecord> m_allProviders;
    bool m_sortAsc = true;

    void setupUiBehavior();
    void setupConnections();
    void updateList();
    void showAddModal(bool visible);
    void clearAddForm();
    void setErrorMessage(const QString &message);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // PROVIDERSVIEW_H
