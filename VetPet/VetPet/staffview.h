#ifndef STAFFVIEW_H
#define STAFFVIEW_H

#include <QSqlDatabase>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class StaffView;
}
QT_END_NAMESPACE

struct StaffRecord {
    int id = 0;
    QString phone;
    QString firstName;
    QString lastName;
};

class StaffView : public QWidget
{
    Q_OBJECT

public:
    explicit StaffView(QWidget *parent = nullptr);
    ~StaffView() override;

    void setDatabase(const QSqlDatabase &database);
    void reload();

signals:
    void staffSelected(int staffId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onSortClicked();
    void onAddClicked();
    void onCancelAddClicked();
    void onSaveAddClicked();
    void onTableCellClicked(int row, int column);
    void onShowAddPositionClicked();
    void onCancelAddPositionClicked();
    void onSavePositionClicked();

private:
    Ui::StaffView *ui;
    QSqlDatabase m_db;
    QList<StaffRecord> m_allStaff;
    bool m_sortAsc = true;

    void setupUiBehavior();
    void setupConnections();
    void updateList();
    void showAddModal(bool visible);
    void clearAddForm();
    void setErrorMessage(const QString &message);
    void setPositionError(const QString &message);
    void loadPositions();
    QString buildWorkDays() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // STAFFVIEW_H
