#include "clientsview.h"
#include "./ui_clientsview.h"

#include <algorithm>

#include <QFont>
#include <QHeaderView>
#include <QPainter>
#include <QResizeEvent>
#include <QSqlQuery>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QSqlError>

namespace {

class ClientNameDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QStringList lines = index.data(Qt::DisplayRole).toString().split(QLatin1Char('\n'));
        if (lines.size() < 2) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.state &= ~QStyle::State_HasFocus;

        painter->save();
        painter->fillRect(opt.rect, opt.state.testFlag(QStyle::State_Selected)
                                         ? QColor(QStringLiteral("#333333"))
                                         : Qt::transparent);

        const QRect textRect = opt.rect.adjusted(10, 0, -10, 0);
        QFont lastNameFont(QStringLiteral("Segoe UI"), 15, QFont::Medium);
        QFont firstNameFont(QStringLiteral("Segoe UI"), 13);

        const QFontMetrics lastNameMetrics(lastNameFont);
        const QFontMetrics firstNameMetrics(firstNameFont);
        const int totalHeight = lastNameMetrics.height() + firstNameMetrics.height() + 2;
        const int top = opt.rect.center().y() - totalHeight / 2;

        painter->setPen(QColor(QStringLiteral("#FFFFFF")));
        painter->setFont(lastNameFont);
        painter->drawText(QRect(textRect.left(), top, textRect.width(), lastNameMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, lines.at(0));

        painter->setPen(QColor(QStringLiteral("#A3A3A3")));
        painter->setFont(firstNameFont);
        painter->drawText(QRect(textRect.left(), top + lastNameMetrics.height() + 2, textRect.width(),
                                firstNameMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, lines.at(1));

        painter->restore();
    }
};

constexpr int kPhoneColumnWidth = 150;
constexpr int kArrowColumnWidth = 60;
constexpr int kRowHeight = 55;
}

ClientsView::ClientsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ClientsView)
{
    ui->setupUi(this);
    setupUiBehavior();
    setupConnections();
}

ClientsView::~ClientsView()
{
    delete ui;
}

void ClientsView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
    reload();
}

void ClientsView::setupUiBehavior()
{
    ui->addOverlay->raise();
    ui->addOverlay->hide();
    ui->addOverlay->setGeometry(rect());

    ui->mainLayout->setSpacing(0);
    ui->mainLayout->insertSpacing(1, 20);

    ui->clientsTable->setColumnCount(3);
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->clientsTable->setColumnWidth(0, kPhoneColumnWidth);
    ui->clientsTable->setColumnWidth(2, kArrowColumnWidth);
    ui->clientsTable->verticalHeader()->setDefaultSectionSize(kRowHeight);
    ui->clientsTable->verticalHeader()->setVisible(false);
    ui->clientsTable->horizontalHeader()->setVisible(false);
    ui->clientsTable->setShowGrid(false);
    ui->clientsTable->setFocusPolicy(Qt::NoFocus);
    ui->clientsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->clientsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->clientsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->clientsTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->clientsTable->setItemDelegateForColumn(1, new ClientNameDelegate(ui->clientsTable));
}

void ClientsView::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &ClientsView::onSearchTextChanged);
    connect(ui->sortBtn, &QPushButton::clicked, this, &ClientsView::onSortClicked);
    connect(ui->newBtn, &QPushButton::clicked, this, &ClientsView::onAddClicked);
    connect(ui->cancelAddBtn, &QPushButton::clicked, this, &ClientsView::onCancelAddClicked);
    connect(ui->saveAddBtn, &QPushButton::clicked, this, &ClientsView::onSaveAddClicked);
    connect(ui->clientsTable, &QTableWidget::cellClicked, this, &ClientsView::onTableCellClicked);
}

void ClientsView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->addOverlay->setGeometry(rect());
}

void ClientsView::reload()
{
    m_allClients.clear();

    if (!m_db.isValid() || !m_db.isOpen()) {
        updateList();
        return;
    }

    QSqlQuery clientsQuery(m_db);
    if (!clientsQuery.exec(QStringLiteral(
            "SELECT id, phone, first_name, last_name FROM clients"))) {
        updateList();
        return;
    }

    while (clientsQuery.next()) {
        ClientRecord record;
        record.id = clientsQuery.value(0).toInt();
        record.phone = clientsQuery.value(1).toString();
        record.firstName = clientsQuery.value(2).toString();
        record.lastName = clientsQuery.value(3).toString();
        m_allClients.append(record);
    }

    QSqlQuery petsQuery(m_db);
    if (petsQuery.exec(QStringLiteral("SELECT client_id, name FROM pets"))) {
        while (petsQuery.next()) {
            const int clientId = petsQuery.value(0).toInt();
            const QString petName = petsQuery.value(1).toString();

            for (ClientRecord &record : m_allClients) {
                if (record.id == clientId) {
                    record.petNames.append(petName);
                    break;
                }
            }
        }
    }

    updateList();
}

void ClientsView::onSearchTextChanged(const QString &)
{
    updateList();
}

void ClientsView::onSortClicked()
{
    m_sortAsc = !m_sortAsc;
    ui->sortBtn->setText(m_sortAsc ? QStringLiteral("А → Я") : QStringLiteral("Я → А"));
    updateList();
}

void ClientsView::onAddClicked()
{
    clearAddForm();
    setErrorMessage(QString());
    showAddModal(true);
}

void ClientsView::onCancelAddClicked()
{
    showAddModal(false);
}

void ClientsView::onSaveAddClicked()
{
    const QString firstName = ui->addFirstNameEdit->text().trimmed();
    const QString lastName = ui->addLastNameEdit->text().trimmed();
    const QString phone = ui->addPhoneEdit->text().trimmed();
    const QString email = ui->addEmailEdit->text().trimmed();

    if (firstName.isEmpty() || lastName.isEmpty() || phone.isEmpty()) {
        setErrorMessage(QStringLiteral("Прізвище, ім'я та телефон є обов'язковими"));
        return;
    }

    if (!m_db.isValid() || !m_db.isOpen()) {
        setErrorMessage(QStringLiteral("Немає підключення до бази даних"));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO clients (first_name, last_name, phone, email) "
        "VALUES (:first_name, :last_name, :phone, :email)"));
    query.bindValue(QStringLiteral(":first_name"), firstName);
    query.bindValue(QStringLiteral(":last_name"), lastName);
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":email"), email.isEmpty() ? QVariant() : email);

    if (!query.exec()) {
        setErrorMessage(query.lastError().text());
        return;
    }

    reload();
    showAddModal(false);
}

void ClientsView::onTableCellClicked(int row, int)
{
    if (row < 0 || row >= ui->clientsTable->rowCount()) {
        return;
    }

    const QTableWidgetItem *idItem = ui->clientsTable->item(row, 0);
    if (!idItem) {
        return;
    }

    emit clientSelected(idItem->data(Qt::UserRole).toInt());
}

bool ClientsView::matchesSearch(const ClientRecord &client, const QString &searchText) const
{
    if (searchText.trimmed().isEmpty()) {
        return true;
    }

    const QString needle = searchText.trimmed();

    if (client.firstName.contains(needle, Qt::CaseInsensitive)) {
        return true;
    }
    if (client.lastName.contains(needle, Qt::CaseInsensitive)) {
        return true;
    }
    if (client.phone.contains(needle)) {
        return true;
    }

    for (const QString &petName : client.petNames) {
        if (petName.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

void ClientsView::updateList()
{
    QList<ClientRecord> filtered;
    const QString searchText = ui->searchEdit->text();

    for (const ClientRecord &client : m_allClients) {
        if (matchesSearch(client, searchText)) {
            filtered.append(client);
        }
    }

    std::sort(filtered.begin(), filtered.end(),
              [this](const ClientRecord &a, const ClientRecord &b) {
                  const int cmp = QString::compare(a.lastName, b.lastName, Qt::CaseInsensitive);
                  return m_sortAsc ? cmp < 0 : cmp > 0;
              });

    populateTable(filtered);
}

void ClientsView::populateTable(const QList<ClientRecord> &clients)
{
    ui->clientsTable->setRowCount(0);

    const QFont phoneFont(QStringLiteral("Segoe UI"), 15);
    const QFont arrowFont(QStringLiteral("Segoe UI"), 18, QFont::Bold);

    int row = 0;
    for (const ClientRecord &client : clients) {
        ui->clientsTable->insertRow(row);

        auto *phoneItem = new QTableWidgetItem(client.phone);
        phoneItem->setData(Qt::UserRole, client.id);
        phoneItem->setFont(phoneFont);
        phoneItem->setForeground(QColor(QStringLiteral("#E0E0E0")));
        phoneItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->clientsTable->setItem(row, 0, phoneItem);

        const QString fullName = client.lastName + QStringLiteral("\n") + client.firstName;
        auto *nameItem = new QTableWidgetItem(fullName);
        nameItem->setData(Qt::UserRole, client.id);
        nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->clientsTable->setItem(row, 1, nameItem);

        auto *arrowItem = new QTableWidgetItem(QStringLiteral("→"));
        arrowItem->setData(Qt::UserRole, client.id);
        arrowItem->setFont(arrowFont);
        arrowItem->setForeground(QColor(QStringLiteral("#8E8E8E")));
        arrowItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->clientsTable->setItem(row, 2, arrowItem);

        row++;
    }
}

void ClientsView::showAddModal(bool visible)
{
    ui->addOverlay->setVisible(visible);
    if (visible) {
        ui->addOverlay->raise();
    }
}

void ClientsView::clearAddForm()
{
    ui->addLastNameEdit->clear();
    ui->addFirstNameEdit->clear();
    ui->addPhoneEdit->clear();
    ui->addEmailEdit->clear();
}

void ClientsView::setErrorMessage(const QString &message)
{
    const bool hasError = !message.isEmpty();
    ui->errorLabel->setVisible(hasError);
    ui->errorLabel->setText(message);
}
