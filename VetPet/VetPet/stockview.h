#ifndef STOCKVIEW_H
#define STOCKVIEW_H

#include <QHash>
#include <QSqlDatabase>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class StockView;
}
QT_END_NAMESPACE

struct StockRowData {
    int stockId = 0;
    int medicineId = 0;
    QString medicineName;
    double price = 0.0;
    int quantity = 0;
    int minimumStock = 10;
    QString providerName;
    int providerId = 0;
    QString lastRestock;
    bool isLowStock = false;
};

class StockView : public QWidget
{
    Q_OBJECT

public:
    explicit StockView(QWidget *parent = nullptr);
    ~StockView() override;

    void setDatabase(const QSqlDatabase &database);
    void reload();

signals:
    void providerSelected(int providerId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onAddClicked();
    void onCancelAddClicked();
    void onSaveAddClicked();
    void onSortMedicineClicked();
    void onSortPriceClicked();
    void onSortQuantityClicked();
    void onCancelEditClicked();
    void onSaveEditClicked();
    void onDeleteEditClicked();
    void onTableCellClicked(int row, int column);

private:
    enum class SortField { Medicine, Price, Quantity };

    Ui::StockView *ui;
    QSqlDatabase m_db;
    QList<StockRowData> m_allStock;
    QHash<int, int> m_minimumStockByMedicine;
    QHash<int, int> m_providerOverrideByMedicine;
    SortField m_sortField = SortField::Medicine;
    bool m_sortAsc = true;
    int m_editingStockId = 0;
    int m_editingMedicineId = 0;
    int m_initialQuantity = 0;
    int m_initialProviderId = 0;

    void setupUiBehavior();
    void setupConnections();
    void updateList();
    void populateTable(const QList<StockRowData> &rows);
    void updateStats();
    void updateSortLabels();
    void loadProvidersToCombos();
    void showAddModal(bool visible);
    void showEditModal(bool visible);
    void openEditForRow(int row);
    QString sortArrow(SortField field) const;
    QList<StockRowData> applySorting(QList<StockRowData> rows) const;

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // STOCKVIEW_H
