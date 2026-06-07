#include "providersview.h"
#include "./ui_providersview.h"

#include <algorithm>

#include <QHeaderView>
#include <QResizeEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QTableWidgetItem>

namespace {
constexpr int kPhoneColumnWidth = 150;
constexpr int kArrowColumnWidth = 60;
constexpr int kRowHeight = 55;
}

ProvidersView::ProvidersView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProvidersView)
{
    ui->setupUi(this);
    setupUiBehavior();
    setupConnections();
}

ProvidersView::~ProvidersView()
{
    delete ui;
}

void ProvidersView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
    reload();
}

void ProvidersView::setupUiBehavior()
{
    ui->addOverlay->raise();
    ui->addOverlay->hide();
    ui->addOverlay->setGeometry(rect());

    ui->mainLayout->insertSpacing(1, 20);

    ui->providersTable->setColumnCount(3);
    ui->providersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->providersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->providersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->providersTable->setColumnWidth(0, kPhoneColumnWidth);
    ui->providersTable->setColumnWidth(2, kArrowColumnWidth);
    ui->providersTable->verticalHeader()->setDefaultSectionSize(kRowHeight);
    ui->providersTable->verticalHeader()->setVisible(false);
    ui->providersTable->horizontalHeader()->setVisible(false);
    ui->providersTable->setShowGrid(false);
    ui->providersTable->setFocusPolicy(Qt::NoFocus);
    ui->providersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->providersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void ProvidersView::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &ProvidersView::onSearchTextChanged);
    connect(ui->sortBtn, &QPushButton::clicked, this, &ProvidersView::onSortClicked);
    connect(ui->newBtn, &QPushButton::clicked, this, &ProvidersView::onAddClicked);
    connect(ui->cancelAddBtn, &QPushButton::clicked, this, &ProvidersView::onCancelAddClicked);
    connect(ui->saveAddBtn, &QPushButton::clicked, this, &ProvidersView::onSaveAddClicked);
    connect(ui->providersTable, &QTableWidget::cellClicked, this, &ProvidersView::onTableCellClicked);
}

void ProvidersView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->addOverlay->setGeometry(rect());
}

void ProvidersView::reload()
{
    m_allProviders.clear();
    if (!m_db.isValid() || !m_db.isOpen()) {
        updateList();
        return;
    }

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT id, phone, name, contact_person FROM providers"))) {
        while (query.next()) {
            ProviderRecord record;
            record.id = query.value(0).toInt();
            record.phone = query.value(1).toString();
            record.name = query.value(2).toString();
            record.contactPerson = query.value(3).toString();
            m_allProviders.append(record);
        }
    }
    updateList();
}

void ProvidersView::onSearchTextChanged(const QString &)
{
    updateList();
}

void ProvidersView::onSortClicked()
{
    m_sortAsc = !m_sortAsc;
    ui->sortBtn->setText(m_sortAsc ? QStringLiteral("А → Я") : QStringLiteral("Я → А"));
    updateList();
}

void ProvidersView::onAddClicked()
{
    clearAddForm();
    setErrorMessage(QString());
    showAddModal(true);
}

void ProvidersView::onCancelAddClicked()
{
    showAddModal(false);
}

void ProvidersView::onSaveAddClicked()
{
    const QString name = ui->addNameEdit->text().trimmed();
    const QString phone = ui->addPhoneEdit->text().trimmed();

    if (name.isEmpty() || phone.isEmpty()) {
        setErrorMessage(QStringLiteral("Назва та телефон є обов'язковими"));
        return;
    }

    if (!m_db.isValid() || !m_db.isOpen()) {
        setErrorMessage(QStringLiteral("Немає підключення до бази даних"));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO providers (name, phone, contact_person, email, address) "
        "VALUES (:name, :phone, :contact_person, :email, :address)"));
    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":contact_person"),
                    ui->addContactEdit->text().trimmed().isEmpty()
                        ? QVariant()
                        : ui->addContactEdit->text().trimmed());
    query.bindValue(QStringLiteral(":email"),
                    ui->addEmailEdit->text().trimmed().isEmpty() ? QVariant()
                                                                 : ui->addEmailEdit->text().trimmed());
    query.bindValue(QStringLiteral(":address"),
                    ui->addAddressEdit->text().trimmed().isEmpty()
                        ? QVariant()
                        : ui->addAddressEdit->text().trimmed());

    if (!query.exec()) {
        setErrorMessage(query.lastError().text());
        return;
    }

    reload();
    showAddModal(false);
}

void ProvidersView::onTableCellClicked(int row, int)
{
    const QTableWidgetItem *item = ui->providersTable->item(row, 0);
    if (item) {
        emit providerSelected(item->data(Qt::UserRole).toInt());
    }
}

void ProvidersView::updateList()
{
    const QString needle = ui->searchEdit->text().trimmed();
    QList<ProviderRecord> filtered;

    for (const ProviderRecord &provider : m_allProviders) {
        if (needle.isEmpty()
            || provider.name.contains(needle, Qt::CaseInsensitive)
            || provider.phone.contains(needle)
            || provider.contactPerson.contains(needle, Qt::CaseInsensitive)) {
            filtered.append(provider);
        }
    }

    std::sort(filtered.begin(), filtered.end(),
              [this](const ProviderRecord &a, const ProviderRecord &b) {
                  const int cmp = QString::compare(a.name, b.name, Qt::CaseInsensitive);
                  return m_sortAsc ? cmp < 0 : cmp > 0;
              });

    ui->providersTable->setRowCount(0);
    const QFont phoneFont(QStringLiteral("Segoe UI"), 15);
    const QFont nameFont(QStringLiteral("Segoe UI"), 15);
    const QFont arrowFont(QStringLiteral("Segoe UI"), 18, QFont::Bold);

    int row = 0;
    for (const ProviderRecord &provider : filtered) {
        ui->providersTable->insertRow(row);

        auto *phoneItem = new QTableWidgetItem(provider.phone);
        phoneItem->setData(Qt::UserRole, provider.id);
        phoneItem->setFont(phoneFont);
        phoneItem->setForeground(QColor(QStringLiteral("#E0E0E0")));
        phoneItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->providersTable->setItem(row, 0, phoneItem);

        auto *nameItem = new QTableWidgetItem(provider.name);
        nameItem->setData(Qt::UserRole, provider.id);
        nameItem->setFont(nameFont);
        nameItem->setForeground(QColor(QStringLiteral("#E0E0E0")));
        nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->providersTable->setItem(row, 1, nameItem);

        auto *arrowItem = new QTableWidgetItem(QStringLiteral("→"));
        arrowItem->setData(Qt::UserRole, provider.id);
        arrowItem->setFont(arrowFont);
        arrowItem->setForeground(QColor(QStringLiteral("#8E8E8E")));
        arrowItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->providersTable->setItem(row, 2, arrowItem);

        ++row;
    }
}

void ProvidersView::showAddModal(bool visible)
{
    ui->addOverlay->setVisible(visible);
    if (visible) {
        ui->addOverlay->raise();
    }
}

void ProvidersView::clearAddForm()
{
    ui->addNameEdit->clear();
    ui->addPhoneEdit->clear();
    ui->addContactEdit->clear();
    ui->addEmailEdit->clear();
    ui->addAddressEdit->clear();
}

void ProvidersView::setErrorMessage(const QString &message)
{
    ui->errorLabel->setVisible(!message.isEmpty());
    ui->errorLabel->setText(message);
}
