#include "stockview.h"
#include "./ui_stockview.h"

#include <algorithm>
#include <QLocale>

#include <QDateTime>
#include <QHeaderView>
#include <QPushButton>
#include <QSet>
#include <QResizeEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidgetItem>

namespace {
constexpr int kDefaultLowStockThreshold = 10;
constexpr int kRowHeight = 66;
}

StockView::StockView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StockView)
{
    ui->setupUi(this);
    setupUiBehavior();
    setupConnections();
}

StockView::~StockView()
{
    delete ui;
}

void StockView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
    reload();
}

void StockView::setupUiBehavior()
{
    ui->mainLayout->insertSpacing(1, 16);
    ui->lowStockLabel->setObjectName(QStringLiteral("lowStockValue"));

    ui->addOverlay->raise();
    ui->editOverlay->raise();
    ui->addOverlay->hide();
    ui->editOverlay->hide();
    ui->addOverlay->setGeometry(rect());
    ui->editOverlay->setGeometry(rect());

    ui->stockTable->setColumnCount(7);
    ui->stockTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int col = 1; col < 7; ++col) {
        ui->stockTable->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }
    ui->stockTable->verticalHeader()->setDefaultSectionSize(kRowHeight);
    ui->stockTable->verticalHeader()->setVisible(false);
    ui->stockTable->horizontalHeader()->setVisible(false);
    ui->stockTable->setShowGrid(false);
    ui->stockTable->setFocusPolicy(Qt::NoFocus);
    updateSortLabels();
}

void StockView::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &StockView::onSearchTextChanged);
    connect(ui->newBtn, &QPushButton::clicked, this, &StockView::onAddClicked);
    connect(ui->cancelAddBtn, &QPushButton::clicked, this, &StockView::onCancelAddClicked);
    connect(ui->saveAddBtn, &QPushButton::clicked, this, &StockView::onSaveAddClicked);
    connect(ui->sortMedicineBtn, &QPushButton::clicked, this, &StockView::onSortMedicineClicked);
    connect(ui->sortPriceBtn, &QPushButton::clicked, this, &StockView::onSortPriceClicked);
    connect(ui->sortQuantityBtn, &QPushButton::clicked, this, &StockView::onSortQuantityClicked);
    connect(ui->cancelEditBtn, &QPushButton::clicked, this, &StockView::onCancelEditClicked);
    connect(ui->saveEditBtn, &QPushButton::clicked, this, &StockView::onSaveEditClicked);
    connect(ui->deleteEditBtn, &QPushButton::clicked, this, &StockView::onDeleteEditClicked);
    connect(ui->stockTable, &QTableWidget::cellClicked, this, &StockView::onTableCellClicked);
}

void StockView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->addOverlay->setGeometry(rect());
    ui->editOverlay->setGeometry(rect());
}

void StockView::reload()
{
    m_allStock.clear();
    if (!m_db.isValid() || !m_db.isOpen()) {
        updateList();
        return;
    }

    QHash<int, QString> providerNames;
    QSqlQuery providersQuery(m_db);
    if (providersQuery.exec(QStringLiteral("SELECT id, name FROM providers"))) {
        while (providersQuery.next()) {
            providerNames.insert(providersQuery.value(0).toInt(), providersQuery.value(1).toString());
        }
    }

    struct LatestOrderInfo {
        int orderId = 0;
        int providerId = 0;
    };
    QHash<int, LatestOrderInfo> latestOrderByMedicine;
    QHash<int, QDateTime> latestRestockByMedicine;
    QSqlQuery orderItemsQuery(m_db);
    if (orderItemsQuery.exec(QStringLiteral(
            "SELECT poi.medicine_id, poi.order_id, po.provider_id, po.date "
            "FROM provider_order_items poi "
            "INNER JOIN provider_orders po ON po.id = poi.order_id"))) {
        while (orderItemsQuery.next()) {
            const int medicineId = orderItemsQuery.value(0).toInt();
            const int orderId = orderItemsQuery.value(1).toInt();
            const int providerId = orderItemsQuery.value(2).toInt();
            const QDateTime orderDate = orderItemsQuery.value(3).toDateTime();

            LatestOrderInfo &latest = latestOrderByMedicine[medicineId];
            if (orderId > latest.orderId) {
                latest.orderId = orderId;
                latest.providerId = providerId;
            }
            if (!latestRestockByMedicine.contains(medicineId)
                || orderDate > latestRestockByMedicine.value(medicineId)) {
                latestRestockByMedicine.insert(medicineId, orderDate);
            }
        }
    }

    QSqlQuery stockQuery(m_db);
    if (stockQuery.exec(QStringLiteral(
            "SELECT s.id, s.medicine_id, s.quantity, m.name, m.price "
            "FROM stock s INNER JOIN medicines m ON m.id = s.medicine_id"))) {
        while (stockQuery.next()) {
            StockRowData row;
            row.stockId = stockQuery.value(0).toInt();
            row.medicineId = stockQuery.value(1).toInt();
            row.quantity = stockQuery.value(2).toInt();
            row.medicineName = stockQuery.value(3).toString();
            row.price = stockQuery.value(4).toDouble();

            if (!m_minimumStockByMedicine.contains(row.medicineId)) {
                m_minimumStockByMedicine.insert(row.medicineId, kDefaultLowStockThreshold);
            }
            row.minimumStock = m_minimumStockByMedicine.value(row.medicineId);

            int providerId = latestOrderByMedicine.value(row.medicineId).providerId;
            if (m_providerOverrideByMedicine.contains(row.medicineId)) {
                providerId = m_providerOverrideByMedicine.value(row.medicineId);
            }
            row.providerId = providerId;
            row.providerName = providerId > 0 ? providerNames.value(providerId, QStringLiteral("—"))
                                              : QStringLiteral("—");

            if (latestRestockByMedicine.contains(row.medicineId)) {
                row.lastRestock = latestRestockByMedicine.value(row.medicineId).toString(QStringLiteral("dd.MM.yyyy"));
            } else {
                row.lastRestock = QStringLiteral("—");
            }

            row.isLowStock = row.quantity < row.minimumStock;
            m_allStock.append(row);
        }
    }

    updateList();
}

void StockView::updateStats()
{
    ui->totalPositionsLabel->setText(QString::number(m_allStock.size()));

    int lowStock = 0;
    QSet<int> supplierIds;
    for (const StockRowData &row : m_allStock) {
        if (row.isLowStock) {
            ++lowStock;
        }
        if (row.providerId > 0) {
            supplierIds.insert(row.providerId);
        }
    }
    ui->lowStockLabel->setText(QString::number(lowStock));
    ui->suppliersCountLabel->setText(QString::number(supplierIds.size()));
}

void StockView::updateSortLabels()
{
    ui->sortMedicineBtn->setText(QStringLiteral("ПРЕПАРАТ %1").arg(sortArrow(SortField::Medicine)));
    ui->sortPriceBtn->setText(QStringLiteral("ЦІНА %1").arg(sortArrow(SortField::Price)));
    ui->sortQuantityBtn->setText(QStringLiteral("КІЛЬКІСТЬ %1").arg(sortArrow(SortField::Quantity)));
}

QString StockView::sortArrow(SortField field) const
{
    if (m_sortField != field) {
        return QString();
    }
    return m_sortAsc ? QStringLiteral("↑") : QStringLiteral("↓");
}

QList<StockRowData> StockView::applySorting(QList<StockRowData> rows) const
{
    std::sort(rows.begin(), rows.end(), [this](const StockRowData &a, const StockRowData &b) {
        switch (m_sortField) {
        case SortField::Price:
            return m_sortAsc ? (a.price < b.price || (a.price == b.price && a.medicineName < b.medicineName))
                             : (a.price > b.price || (a.price == b.price && a.medicineName < b.medicineName));
        case SortField::Quantity:
            return m_sortAsc ? (a.quantity < b.quantity
                                || (a.quantity == b.quantity && a.medicineName < b.medicineName))
                             : (a.quantity > b.quantity
                                || (a.quantity == b.quantity && a.medicineName < b.medicineName));
        case SortField::Medicine:
        default:
            return m_sortAsc ? a.medicineName < b.medicineName : a.medicineName > b.medicineName;
        }
    });
    return rows;
}

void StockView::updateList()
{
    updateStats();
    const QString needle = ui->searchEdit->text().trimmed();
    QList<StockRowData> filtered;

    for (const StockRowData &row : m_allStock) {
        if (needle.isEmpty()
            || row.medicineName.contains(needle, Qt::CaseInsensitive)
            || row.providerName.contains(needle, Qt::CaseInsensitive)) {
            filtered.append(row);
        }
    }

    populateTable(applySorting(filtered));
}

void StockView::populateTable(const QList<StockRowData> &rows)
{
    ui->stockTable->setRowCount(0);
    int rowIndex = 0;
    for (const StockRowData &row : rows) {
        ui->stockTable->insertRow(rowIndex);

        const QString rowBg = row.isLowStock ? QStringLiteral("#3D2B2B") : QStringLiteral("transparent");

        auto makeItem = [&](const QString &text, int column) {
            auto *item = new QTableWidgetItem(text);
            item->setData(Qt::UserRole, row.stockId);
            item->setData(Qt::UserRole + 1, row.medicineId);
            item->setData(Qt::UserRole + 2, row.providerId);
            item->setBackground(QColor(rowBg));
            if (column == 0) {
                item->setForeground(QColor(QStringLiteral("#E0E0E0")));
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
            }
            ui->stockTable->setItem(rowIndex, column, item);
        };

        makeItem(row.medicineName, 0);
        makeItem(QStringLiteral("%1 ₴").arg(QLocale().toString(row.price, 'f', 2)), 1);
        makeItem(QStringLiteral("%1\n%2").arg(row.quantity).arg(row.isLowStock ? QStringLiteral("Низько")
                                                                               : QStringLiteral("Норма")),
                 2);
        makeItem(QString::number(row.minimumStock), 3);
        makeItem(row.providerName, 4);
        makeItem(row.lastRestock, 5);
        makeItem(QStringLiteral("Редагувати"), 6);

        if (row.isLowStock) {
            ui->stockTable->item(rowIndex, 2)->setForeground(QColor(QStringLiteral("#FF8C8C")));
        }

        ++rowIndex;
    }
}

void StockView::loadProvidersToCombos()
{
    ui->addProviderCombo->clear();
    ui->editProviderCombo->clear();

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT id, name FROM providers ORDER BY name"))) {
        while (query.next()) {
            const int id = query.value(0).toInt();
            const QString name = query.value(1).toString();
            ui->addProviderCombo->addItem(name, id);
            ui->editProviderCombo->addItem(name, id);
        }
    }
}

void StockView::onSearchTextChanged(const QString &)
{
    updateList();
}

void StockView::onSortMedicineClicked()
{
    if (m_sortField == SortField::Medicine) {
        m_sortAsc = !m_sortAsc;
    } else {
        m_sortField = SortField::Medicine;
        m_sortAsc = true;
    }
    updateSortLabels();
    updateList();
}

void StockView::onSortPriceClicked()
{
    if (m_sortField == SortField::Price) {
        m_sortAsc = !m_sortAsc;
    } else {
        m_sortField = SortField::Price;
        m_sortAsc = true;
    }
    updateSortLabels();
    updateList();
}

void StockView::onSortQuantityClicked()
{
    if (m_sortField == SortField::Quantity) {
        m_sortAsc = !m_sortAsc;
    } else {
        m_sortField = SortField::Quantity;
        m_sortAsc = true;
    }
    updateSortLabels();
    updateList();
}

void StockView::onAddClicked()
{
    ui->addMedicineEdit->clear();
    ui->addPriceEdit->clear();
    ui->addQuantityEdit->setText(QStringLiteral("1"));
    ui->errorLabel->clear();
    ui->errorLabel->setVisible(false);
    loadProvidersToCombos();
    showAddModal(true);
}

void StockView::onCancelAddClicked()
{
    showAddModal(false);
}

void StockView::onSaveAddClicked()
{
    const QString medicineName = ui->addMedicineEdit->text().trimmed();
    const QString priceText = ui->addPriceEdit->text().trimmed();
    const QString quantityText = ui->addQuantityEdit->text().trimmed();
    const int providerId = ui->addProviderCombo->currentData().toInt();

    bool ok = false;
    const double price = QLocale().toDouble(QString(priceText).replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    if (medicineName.isEmpty() || priceText.isEmpty() || !ok) {
        ui->errorLabel->setText(QStringLiteral("Назва та ціна є обов'язковими"));
        ui->errorLabel->setVisible(true);
        return;
    }

    const int quantity = quantityText.toInt(&ok);
    if (!ok || quantity < 1) {
        ui->errorLabel->setText(QStringLiteral("Кількість має бути цілим числом > 0"));
        ui->errorLabel->setVisible(true);
        return;
    }

    if (providerId <= 0) {
        ui->errorLabel->setText(QStringLiteral("Оберіть провайдера"));
        ui->errorLabel->setVisible(true);
        return;
    }

    if (!m_db.transaction()) {
        ui->errorLabel->setText(m_db.lastError().text());
        ui->errorLabel->setVisible(true);
        return;
    }

    int medicineId = 0;
    QSqlQuery findMedicine(m_db);
    findMedicine.prepare(QStringLiteral("SELECT id FROM medicines WHERE LOWER(name) = LOWER(:name)"));
    findMedicine.bindValue(QStringLiteral(":name"), medicineName);
    if (findMedicine.exec() && findMedicine.next()) {
        medicineId = findMedicine.value(0).toInt();
        QSqlQuery updateMedicine(m_db);
        updateMedicine.prepare(QStringLiteral("UPDATE medicines SET price = :price WHERE id = :id"));
        updateMedicine.bindValue(QStringLiteral(":price"), price);
        updateMedicine.bindValue(QStringLiteral(":id"), medicineId);
        if (!updateMedicine.exec()) {
            m_db.rollback();
            ui->errorLabel->setText(updateMedicine.lastError().text());
            ui->errorLabel->setVisible(true);
            return;
        }
    } else {
        QSqlQuery insertMedicine(m_db);
        insertMedicine.prepare(QStringLiteral("INSERT INTO medicines (name, price) VALUES (:name, :price)"));
        insertMedicine.bindValue(QStringLiteral(":name"), medicineName);
        insertMedicine.bindValue(QStringLiteral(":price"), price);
        if (!insertMedicine.exec()) {
            m_db.rollback();
            ui->errorLabel->setText(insertMedicine.lastError().text());
            ui->errorLabel->setVisible(true);
            return;
        }
        medicineId = insertMedicine.lastInsertId().toInt();
    }

    QSqlQuery findStock(m_db);
    findStock.prepare(QStringLiteral("SELECT id, quantity FROM stock WHERE medicine_id = :medicine_id"));
    findStock.bindValue(QStringLiteral(":medicine_id"), medicineId);
    if (findStock.exec() && findStock.next()) {
        const int stockId = findStock.value(0).toInt();
        const int currentQty = findStock.value(1).toInt();
        QSqlQuery updateStock(m_db);
        updateStock.prepare(QStringLiteral("UPDATE stock SET quantity = :quantity WHERE id = :id"));
        updateStock.bindValue(QStringLiteral(":quantity"), currentQty + quantity);
        updateStock.bindValue(QStringLiteral(":id"), stockId);
        if (!updateStock.exec()) {
            m_db.rollback();
            ui->errorLabel->setText(updateStock.lastError().text());
            ui->errorLabel->setVisible(true);
            return;
        }
    } else {
        QSqlQuery insertStock(m_db);
        insertStock.prepare(QStringLiteral("INSERT INTO stock (medicine_id, quantity) VALUES (:medicine_id, :quantity)"));
        insertStock.bindValue(QStringLiteral(":medicine_id"), medicineId);
        insertStock.bindValue(QStringLiteral(":quantity"), quantity);
        if (!insertStock.exec()) {
            m_db.rollback();
            ui->errorLabel->setText(insertStock.lastError().text());
            ui->errorLabel->setVisible(true);
            return;
        }
    }

    QSqlQuery insertOrder(m_db);
    insertOrder.prepare(QStringLiteral(
        "INSERT INTO provider_orders (provider_id, date, total_cost, status) "
        "VALUES (:provider_id, NOW(), :total_cost, 'доставлено')"));
    insertOrder.bindValue(QStringLiteral(":provider_id"), providerId);
    insertOrder.bindValue(QStringLiteral(":total_cost"), price * quantity);
    if (!insertOrder.exec()) {
        m_db.rollback();
        ui->errorLabel->setText(insertOrder.lastError().text());
        ui->errorLabel->setVisible(true);
        return;
    }

    const int orderId = insertOrder.lastInsertId().toInt();
    QSqlQuery insertItem(m_db);
    insertItem.prepare(QStringLiteral(
        "INSERT INTO provider_order_items (order_id, medicine_id, quantity, price) "
        "VALUES (:order_id, :medicine_id, :quantity, :price)"));
    insertItem.bindValue(QStringLiteral(":order_id"), orderId);
    insertItem.bindValue(QStringLiteral(":medicine_id"), medicineId);
    insertItem.bindValue(QStringLiteral(":quantity"), quantity);
    insertItem.bindValue(QStringLiteral(":price"), price);
    if (!insertItem.exec()) {
        m_db.rollback();
        ui->errorLabel->setText(insertItem.lastError().text());
        ui->errorLabel->setVisible(true);
        return;
    }

    m_db.commit();
    reload();
    showAddModal(false);
}

void StockView::openEditForRow(int row)
{
    const QTableWidgetItem *item = ui->stockTable->item(row, 0);
    if (!item) {
        return;
    }

    const int stockId = item->data(Qt::UserRole).toInt();
    StockRowData *selected = nullptr;
    for (StockRowData &data : m_allStock) {
        if (data.stockId == stockId) {
            selected = &data;
            break;
        }
    }
    if (!selected) {
        return;
    }

    m_editingStockId = selected->stockId;
    m_editingMedicineId = selected->medicineId;
    m_initialQuantity = selected->quantity;
    m_initialProviderId = selected->providerId;

    loadProvidersToCombos();
    ui->editMedicineEdit->setText(selected->medicineName);
    ui->editPriceEdit->setText(QLocale().toString(selected->price, 'f', 2));
    ui->editQuantityEdit->setText(QString::number(selected->quantity));
    ui->editMinimumEdit->setText(QString::number(selected->minimumStock));
    const int comboIndex = ui->editProviderCombo->findData(selected->providerId);
    if (comboIndex >= 0) {
        ui->editProviderCombo->setCurrentIndex(comboIndex);
    }

    ui->editErrorLabel->clear();
    ui->editErrorLabel->setVisible(false);
    showEditModal(true);
}

void StockView::onTableCellClicked(int row, int column)
{
    if (column == 4) {
        const QTableWidgetItem *item = ui->stockTable->item(row, column);
        const int providerId = item ? item->data(Qt::UserRole + 2).toInt() : 0;
        if (providerId > 0) {
            emit providerSelected(providerId);
        }
        return;
    }
    if (column == 6 || column >= 0) {
        openEditForRow(row);
    }
}

void StockView::onCancelEditClicked()
{
    showEditModal(false);
}

void StockView::onSaveEditClicked()
{
    const QString medicineName = ui->editMedicineEdit->text().trimmed();
    const QString priceText = ui->editPriceEdit->text().trimmed();
    const QString quantityText = ui->editQuantityEdit->text().trimmed();
    const QString minimumText = ui->editMinimumEdit->text().trimmed();
    const int providerId = ui->editProviderCombo->currentData().toInt();

    bool ok = false;
    const double price = QLocale().toDouble(QString(priceText).replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    if (medicineName.isEmpty() || !ok) {
        ui->editErrorLabel->setText(QStringLiteral("Назва та ціна є обов'язковими"));
        ui->editErrorLabel->setVisible(true);
        return;
    }

    const int quantity = quantityText.toInt(&ok);
    if (!ok || quantity < 1) {
        ui->editErrorLabel->setText(QStringLiteral("Кількість має бути цілим числом > 0"));
        ui->editErrorLabel->setVisible(true);
        return;
    }

    const int minimumStock = minimumText.toInt(&ok);
    if (!ok || minimumStock < 1) {
        ui->editErrorLabel->setText(QStringLiteral("Мінімальний залишок має бути цілим числом > 0"));
        ui->editErrorLabel->setVisible(true);
        return;
    }

    if (!m_db.transaction()) {
        ui->editErrorLabel->setText(m_db.lastError().text());
        ui->editErrorLabel->setVisible(true);
        return;
    }

    QSqlQuery updateMedicine(m_db);
    updateMedicine.prepare(QStringLiteral("UPDATE medicines SET name = :name, price = :price WHERE id = :id"));
    updateMedicine.bindValue(QStringLiteral(":name"), medicineName);
    updateMedicine.bindValue(QStringLiteral(":price"), price);
    updateMedicine.bindValue(QStringLiteral(":id"), m_editingMedicineId);
    if (!updateMedicine.exec()) {
        m_db.rollback();
        ui->editErrorLabel->setText(updateMedicine.lastError().text());
        ui->editErrorLabel->setVisible(true);
        return;
    }

    QSqlQuery updateStock(m_db);
    updateStock.prepare(QStringLiteral("UPDATE stock SET quantity = :quantity WHERE id = :id"));
    updateStock.bindValue(QStringLiteral(":quantity"), quantity);
    updateStock.bindValue(QStringLiteral(":id"), m_editingStockId);
    if (!updateStock.exec()) {
        m_db.rollback();
        ui->editErrorLabel->setText(updateStock.lastError().text());
        ui->editErrorLabel->setVisible(true);
        return;
    }

    m_minimumStockByMedicine.insert(m_editingMedicineId, minimumStock);
    if (providerId > 0) {
        m_providerOverrideByMedicine.insert(m_editingMedicineId, providerId);

        const int delta = quantity - m_initialQuantity;
        const bool providerChanged = m_initialProviderId != providerId;
        if (delta > 0 || providerChanged) {
            const int recordedQuantity = qMax(delta, 0);
            QSqlQuery insertOrder(m_db);
            insertOrder.prepare(QStringLiteral(
                "INSERT INTO provider_orders (provider_id, date, total_cost, status) "
                "VALUES (:provider_id, NOW(), :total_cost, 'доставлено')"));
            insertOrder.bindValue(QStringLiteral(":provider_id"), providerId);
            insertOrder.bindValue(QStringLiteral(":total_cost"), price * recordedQuantity);
            if (!insertOrder.exec()) {
                m_db.rollback();
                ui->editErrorLabel->setText(insertOrder.lastError().text());
                ui->editErrorLabel->setVisible(true);
                return;
            }

            const int orderId = insertOrder.lastInsertId().toInt();
            QSqlQuery insertItem(m_db);
            insertItem.prepare(QStringLiteral(
                "INSERT INTO provider_order_items (order_id, medicine_id, quantity, price) "
                "VALUES (:order_id, :medicine_id, :quantity, :price)"));
            insertItem.bindValue(QStringLiteral(":order_id"), orderId);
            insertItem.bindValue(QStringLiteral(":medicine_id"), m_editingMedicineId);
            insertItem.bindValue(QStringLiteral(":quantity"), recordedQuantity);
            insertItem.bindValue(QStringLiteral(":price"), price);
            if (!insertItem.exec()) {
                m_db.rollback();
                ui->editErrorLabel->setText(insertItem.lastError().text());
                ui->editErrorLabel->setVisible(true);
                return;
            }
        }
    }

    m_db.commit();
    reload();
    showEditModal(false);
}

void StockView::onDeleteEditClicked()
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM stock WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), m_editingStockId);
    if (!query.exec()) {
        ui->editErrorLabel->setText(query.lastError().text());
        ui->editErrorLabel->setVisible(true);
        return;
    }

    reload();
    showEditModal(false);
}

void StockView::showAddModal(bool visible)
{
    ui->addOverlay->setVisible(visible);
    if (visible) {
        ui->addOverlay->raise();
    }
}

void StockView::showEditModal(bool visible)
{
    ui->editOverlay->setVisible(visible);
    if (visible) {
        ui->editOverlay->raise();
    }
}
