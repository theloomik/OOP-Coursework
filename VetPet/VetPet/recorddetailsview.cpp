#include "recorddetailsview.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

namespace {

QString dashIfEmpty(const QString &value)
{
    return value.trimmed().isEmpty() ? QStringLiteral("—") : value;
}

QString formatDateTime(const QVariant &value)
{
    const QDateTime date = value.toDateTime();
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy HH:mm")) : QStringLiteral("—");
}

const QString kTableStyle = QStringLiteral(
    "QTableWidget { background-color: #2F2F2F; color: #E0E0E0; border: 1px solid #3B3B3B; border-radius: 14px; gridline-color: rgba(255,255,255,13); }"
    "QHeaderView::section { background-color: #2F2F2F; color: #757575; border: none; padding: 10px; font-size: 12px; font-weight: 600; letter-spacing: 0.5px; }"
    "QTableWidget::item { padding: 10px; border-bottom: 1px solid rgba(255,255,255,13); font-size: 14px; }"
    "QTableWidget::item:hover { background-color: #333333; }"
    "QTableWidget::item:selected { background-color: #333333; color: #FFFFFF; }");

} // namespace

RecordDetailsView::RecordDetailsView(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void RecordDetailsView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
}

void RecordDetailsView::showClient(int clientId)
{
    m_kind = Kind::Client;
    m_recordId = clientId;
    loadClient();
}

void RecordDetailsView::showStaff(int staffId)
{
    m_kind = Kind::Staff;
    m_recordId = staffId;
    loadStaff();
}

void RecordDetailsView::showProvider(int providerId)
{
    m_kind = Kind::Provider;
    m_recordId = providerId;
    loadProvider();
}

void RecordDetailsView::setupUi()
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(30, 20, 30, 20);
    outerLayout->setSpacing(14);

    // === Header card ===
    auto *headerCard = new QFrame;
    headerCard->setObjectName(QStringLiteral("headerCard"));
    headerCard->setStyleSheet(QStringLiteral(
        "QFrame#headerCard { background-color: #2F2F2F; border: 1px solid #3B3B3B; border-radius: 14px; }"));

    auto *headerInner = new QHBoxLayout(headerCard);
    headerInner->setContentsMargins(16, 16, 16, 16);
    headerInner->setSpacing(16);

    auto *backButton = new QPushButton(QStringLiteral("←"));
    backButton->setFixedSize(42, 42);
    backButton->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3A3A3A; color: white; border: none; border-radius: 10px; font-size: 18px; }"
        "QPushButton:hover { background-color: #555555; }"));
    connect(backButton, &QPushButton::clicked, this, &RecordDetailsView::onBackClicked);
    headerInner->addWidget(backButton, 0, Qt::AlignTop);

    // Center: name + phone/email + stats
    auto *centerStack = new QWidget;
    centerStack->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *centerLayout = new QVBoxLayout(centerStack);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(4);

    m_titleLabel = new QLabel;
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: white; font-size: 24px; font-weight: bold; background: transparent;"));

    m_subtitleLabel = new QLabel;
    m_subtitleLabel->setStyleSheet(QStringLiteral(
        "color: #A0A0A0; font-size: 13px; background: transparent;"));
    m_subtitleLabel->setVisible(false);

    m_statsLabel = new QLabel;
    m_statsLabel->setStyleSheet(QStringLiteral(
        "font-size: 14px; background: transparent;"));
    m_statsLabel->setTextFormat(Qt::RichText);
    m_statsLabel->setVisible(false);

    centerLayout->addWidget(m_titleLabel);
    centerLayout->addWidget(m_subtitleLabel);
    centerLayout->addWidget(m_statsLabel);
    headerInner->addWidget(centerStack, 1);

    // Right: action buttons (client-only)
    m_actionButtonsWidget = new QWidget;
    m_actionButtonsWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *actionLayout = new QHBoxLayout(m_actionButtonsWidget);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(8);

    struct BtnDef { QString label; QString border; };
    const QList<BtnDef> btnDefs = {
        {QStringLiteral("+ Тварина"), QStringLiteral("#4A7C59")},
        {QStringLiteral("+ Запис"),   QStringLiteral("#4A6A7C")},
        {QStringLiteral("+ Рахунок"), QStringLiteral("#7C6E4A")},
        {QStringLiteral("Редагувати"), QString()},
        {QStringLiteral("Зберегти зміни"), QStringLiteral("#4A6A7C")},
    };
    for (const auto &def : btnDefs) {
        auto *btn = new QPushButton(def.label);
        btn->setFixedHeight(42);
        const QString borderPart = def.border.isEmpty()
            ? QStringLiteral("border: none;")
            : QStringLiteral("border: 1.5px solid %1;").arg(def.border);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { background-color: #3A3A3A; color: white; border-radius: 10px; "
            "padding: 0 16px; font-weight: 600; font-size: 13px; %1 }"
            "QPushButton:hover { background-color: #4A4A4A; }").arg(borderPart));
        actionLayout->addWidget(btn);
    }
    m_actionButtonsWidget->setVisible(false);
    headerInner->addWidget(m_actionButtonsWidget, 0, Qt::AlignTop);

    outerLayout->addWidget(headerCard);

    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet(QStringLiteral("color: #FF8C8C;"));
    m_errorLabel->setVisible(false);
    outerLayout->addWidget(m_errorLabel);

    // === Scroll area ===
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #555555; border-radius: 4px; min-height: 28px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));

    auto *content = new QWidget;
    m_rootLayout = new QVBoxLayout(content);
    m_rootLayout->setContentsMargins(0, 0, 4, 20);
    m_rootLayout->setSpacing(14);

    // Section title labels
    m_infoTitleLabel = new QLabel;
    m_firstTitleLabel = new QLabel;
    m_secondTitleLabel = new QLabel;
    m_thirdTitleLabel = new QLabel;

    for (QLabel *label : {m_infoTitleLabel, m_firstTitleLabel, m_secondTitleLabel, m_thirdTitleLabel}) {
        label->setStyleSheet(QStringLiteral(
            "color: white; font-size: 20px; font-weight: bold; margin-top: 8px; margin-bottom: 4px;"));
        label->setVisible(false);
    }

    // Tables (shared for staff/provider; also appointments+bills for clients)
    m_infoTable = new QTableWidget;
    m_firstTable = new QTableWidget;
    m_secondTable = new QTableWidget;
    m_thirdTable = new QTableWidget;

    for (QTableWidget *table : {m_infoTable, m_firstTable, m_secondTable, m_thirdTable}) {
        table->setStyleSheet(kTableStyle);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setShowGrid(false);
        table->setFocusPolicy(Qt::NoFocus);
        table->setVisible(false);
    }

    // Pets section card (client-only)
    m_petsSectionFrame = new QFrame;
    m_petsSectionFrame->setObjectName(QStringLiteral("petsSectionFrame"));
    m_petsSectionFrame->setStyleSheet(QStringLiteral(
        "QFrame#petsSectionFrame { background-color: #2F2F2F; border: 1px solid #3B3B3B; border-radius: 14px; }"));

    auto *petsSectionLayout = new QVBoxLayout(m_petsSectionFrame);
    petsSectionLayout->setContentsMargins(16, 16, 16, 16);
    petsSectionLayout->setSpacing(12);

    auto *petsTitle = new QLabel(QStringLiteral("Улюбленці"));
    petsTitle->setStyleSheet(QStringLiteral(
        "color: white; font-size: 20px; font-weight: bold; background: transparent;"));
    petsSectionLayout->addWidget(petsTitle);

    auto *petsCardsContainer = new QWidget;
    petsCardsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_petsCardsLayout = new QVBoxLayout(petsCardsContainer);
    m_petsCardsLayout->setContentsMargins(0, 0, 0, 0);
    m_petsCardsLayout->setSpacing(12);
    petsSectionLayout->addWidget(petsCardsContainer);

    m_petsSectionFrame->setVisible(false);

    m_rootLayout->addWidget(m_infoTitleLabel);
    m_rootLayout->addWidget(m_infoTable);
    m_rootLayout->addWidget(m_firstTitleLabel);
    m_rootLayout->addWidget(m_firstTable);
    m_rootLayout->addWidget(m_petsSectionFrame);
    m_rootLayout->addWidget(m_secondTitleLabel);
    m_rootLayout->addWidget(m_secondTable);
    m_rootLayout->addWidget(m_thirdTitleLabel);
    m_rootLayout->addWidget(m_thirdTable);
    m_rootLayout->addStretch();

    scroll->setWidget(content);
    outerLayout->addWidget(scroll, 1);
}

void RecordDetailsView::resetTables()
{
    m_errorLabel->setVisible(false);
    m_subtitleLabel->setVisible(false);
    m_statsLabel->setVisible(false);
    m_actionButtonsWidget->setVisible(false);
    m_petsSectionFrame->setVisible(false);

    while (QLayoutItem *item = m_petsCardsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    for (QLabel *label : {m_infoTitleLabel, m_firstTitleLabel, m_secondTitleLabel, m_thirdTitleLabel}) {
        label->clear();
        label->setVisible(false);
    }
    for (QTableWidget *table : {m_infoTable, m_firstTable, m_secondTable, m_thirdTable}) {
        table->clear();
        table->setRowCount(0);
        table->setVisible(false);
    }
}

void RecordDetailsView::loadClient()
{
    resetTables();
    m_titleLabel->setText(QStringLiteral("Профіль клієнта"));

    if (!m_db.isValid() || !m_db.isOpen()) {
        m_errorLabel->setText(QStringLiteral("Немає підключення до бази даних"));
        m_errorLabel->setVisible(true);
        return;
    }

    QSqlQuery clientQuery(m_db);
    clientQuery.prepare(QStringLiteral("SELECT first_name, last_name, phone, email FROM clients WHERE id = :id"));
    clientQuery.bindValue(QStringLiteral(":id"), m_recordId);
    if (!clientQuery.exec() || !clientQuery.next()) {
        m_errorLabel->setText(QStringLiteral("Клієнта не знайдено"));
        m_errorLabel->setVisible(true);
        return;
    }

    const QString firstName = clientQuery.value(0).toString();
    const QString lastName = clientQuery.value(1).toString();
    const QString phone = clientQuery.value(2).toString();
    const QString email = clientQuery.value(3).toString();
    m_titleLabel->setText(QStringLiteral("%1 %2").arg(lastName, firstName).trimmed());

    // Phone / email subtitle
    QStringList subtitleParts;
    if (!phone.trimmed().isEmpty()) subtitleParts << phone.trimmed();
    if (!email.trimmed().isEmpty()) subtitleParts << email.trimmed();
    m_subtitleLabel->setText(subtitleParts.join(QStringLiteral("  •  ")));
    m_subtitleLabel->setVisible(!subtitleParts.isEmpty());

    // Stats
    const QString petsCount = scalarString(
        QStringLiteral("SELECT COUNT(*) FROM pets WHERE client_id = ?"), {m_recordId});
    const QString appointmentsCount = scalarString(
        QStringLiteral("SELECT COUNT(*) FROM appointments WHERE client_id = ?"), {m_recordId});
    const double debt = scalarString(QStringLiteral(
        "SELECT COALESCE(SUM(b.total_amount), 0) "
        "FROM bills b INNER JOIN appointments a ON a.id = b.appointment_id "
        "WHERE a.client_id = ? AND COALESCE(b.paid, 'без оплати') <> 'оплачено'"),
        {m_recordId}).toDouble();

    m_statsLabel->setText(QStringLiteral(
        "<span style='color:#CFCFCF'>Тварин: %1</span>"
        "&nbsp;&nbsp;&nbsp;&nbsp;"
        "<span style='color:#CFCFCF'>Записів: %2</span>"
        "&nbsp;&nbsp;&nbsp;&nbsp;"
        "<span style='color:#E3A8A8'>Борг: %3 ₴</span>")
        .arg(petsCount, appointmentsCount, QString::number(debt, 'f', 2)));
    m_statsLabel->setVisible(true);
    m_actionButtonsWidget->setVisible(true);

    // Pets as cards
    m_petsSectionFrame->setVisible(true);

    QSqlQuery petsQuery(m_db);
    petsQuery.prepare(QStringLiteral(
        "SELECT p.name, pt.species, pt.breed, p.gender, p.birth_date, "
        "(SELECT MAX(a2.date) FROM appointments a2 WHERE a2.pet_id = p.id) "
        "FROM pets p LEFT JOIN pet_type pt ON pt.id = p.pet_type_id "
        "WHERE p.client_id = :id ORDER BY p.name"));
    petsQuery.bindValue(QStringLiteral(":id"), m_recordId);
    if (petsQuery.exec()) {
        while (petsQuery.next()) {
            const QString birthDate = petsQuery.value(4).isNull()
                ? QStringLiteral("—")
                : petsQuery.value(4).toDate().toString(QStringLiteral("dd.MM.yyyy"));
            const QDateTime lastVisitDt = petsQuery.value(5).toDateTime();
            const QString lastVisit = lastVisitDt.isValid()
                ? lastVisitDt.toString(QStringLiteral("dd.MM.yyyy"))
                : QStringLiteral("—");
            addPetCard(petsQuery.value(0).toString(), petsQuery.value(1).toString(),
                       petsQuery.value(2).toString(), petsQuery.value(3).toString(),
                       birthDate, lastVisit);
        }
    }

    // Appointments table
    prepareTable(m_secondTitleLabel, m_secondTable, QStringLiteral("Записи"),
                 {QStringLiteral("Дата"), QStringLiteral("Тварина"), QStringLiteral("Лікар"),
                  QStringLiteral("Послуга"), QStringLiteral("Статус")});
    QSqlQuery appointmentsQuery(m_db);
    appointmentsQuery.prepare(QStringLiteral(
        "SELECT a.date, p.name, s.last_name, s.first_name, sv.name, a.status "
        "FROM appointments a "
        "LEFT JOIN pets p ON p.id = a.pet_id "
        "LEFT JOIN staff s ON s.id = a.staff_id "
        "LEFT JOIN services sv ON sv.id = a.service_id "
        "WHERE a.client_id = :id ORDER BY a.date DESC"));
    appointmentsQuery.bindValue(QStringLiteral(":id"), m_recordId);
    if (appointmentsQuery.exec()) {
        while (appointmentsQuery.next()) {
            addRow(m_secondTable, {
                formatDateTime(appointmentsQuery.value(0)),
                dashIfEmpty(appointmentsQuery.value(1).toString()),
                QStringLiteral("%1 %2").arg(appointmentsQuery.value(2).toString(),
                                            appointmentsQuery.value(3).toString()).trimmed(),
                dashIfEmpty(appointmentsQuery.value(4).toString()),
                dashIfEmpty(appointmentsQuery.value(5).toString())
            });
        }
    }

    // Bills table
    prepareTable(m_thirdTitleLabel, m_thirdTable, QStringLiteral("Рахунки"),
                 {QStringLiteral("Дата"), QStringLiteral("Сума"), QStringLiteral("Оплата"), QStringLiteral("Метод")});
    QSqlQuery billsQuery(m_db);
    billsQuery.prepare(QStringLiteral(
        "SELECT b.date, b.total_amount, b.paid, b.payment_method "
        "FROM bills b INNER JOIN appointments a ON a.id = b.appointment_id "
        "WHERE a.client_id = :id ORDER BY b.date DESC"));
    billsQuery.bindValue(QStringLiteral(":id"), m_recordId);
    if (billsQuery.exec()) {
        while (billsQuery.next()) {
            addRow(m_thirdTable, {
                formatDateTime(billsQuery.value(0)),
                QStringLiteral("%1 ₴").arg(billsQuery.value(1).toDouble(), 0, 'f', 2),
                dashIfEmpty(billsQuery.value(2).toString()),
                dashIfEmpty(billsQuery.value(3).toString())
            });
        }
    }
}

void RecordDetailsView::loadStaff()
{
    resetTables();
    m_titleLabel->setText(QStringLiteral("Профіль працівника"));

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT s.first_name, s.last_name, s.phone, sp.position, sp.salary, s.work_days, "
        "s.start_time, s.end_time, s.created_at "
        "FROM staff s LEFT JOIN staff_position sp ON sp.id = s.staff_position_id "
        "WHERE s.id = :id"));
    query.bindValue(QStringLiteral(":id"), m_recordId);
    if (!query.exec() || !query.next()) {
        m_errorLabel->setText(QStringLiteral("Працівника не знайдено"));
        m_errorLabel->setVisible(true);
        return;
    }

    m_titleLabel->setText(QStringLiteral("%1 %2").arg(query.value(1).toString(), query.value(0).toString()).trimmed());
    setInfoRows({
        {QStringLiteral("Телефон"), dashIfEmpty(query.value(2).toString())},
        {QStringLiteral("Посада"), dashIfEmpty(query.value(3).toString())},
        {QStringLiteral("Зарплата"), QStringLiteral("%1 ₴").arg(query.value(4).toDouble(), 0, 'f', 2)},
        {QStringLiteral("Робочі дні"), dashIfEmpty(query.value(5).toString())},
        {QStringLiteral("Години роботи"), QStringLiteral("%1 - %2").arg(query.value(6).toTime().toString(QStringLiteral("HH:mm")),
                                                                      query.value(7).toTime().toString(QStringLiteral("HH:mm")))},
        {QStringLiteral("Створено"), formatDateTime(query.value(8))}
    });
}

void RecordDetailsView::loadProvider()
{
    resetTables();
    m_titleLabel->setText(QStringLiteral("Профіль провайдера"));

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT name, phone, contact_person, email, address FROM providers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), m_recordId);
    if (!query.exec() || !query.next()) {
        m_errorLabel->setText(QStringLiteral("Провайдера не знайдено"));
        m_errorLabel->setVisible(true);
        return;
    }

    m_titleLabel->setText(dashIfEmpty(query.value(0).toString()));
    const QString ordersCount = scalarString(
        QStringLiteral("SELECT COUNT(*) FROM provider_orders WHERE provider_id = ?"), {m_recordId});
    const QString medicinesCount = scalarString(QStringLiteral(
        "SELECT COUNT(DISTINCT i.medicine_id) "
        "FROM provider_order_items i INNER JOIN provider_orders o ON o.id = i.order_id "
        "WHERE o.provider_id = ?"),
        {m_recordId});
    const QString lastOrder = scalarString(
        QStringLiteral("SELECT MAX(date) FROM provider_orders WHERE provider_id = ?"), {m_recordId});

    setInfoRows({
        {QStringLiteral("Телефон"), dashIfEmpty(query.value(1).toString())},
        {QStringLiteral("Контактна особа"), dashIfEmpty(query.value(2).toString())},
        {QStringLiteral("Email"), dashIfEmpty(query.value(3).toString())},
        {QStringLiteral("Адреса"), dashIfEmpty(query.value(4).toString())},
        {QStringLiteral("Замовлень"), ordersCount},
        {QStringLiteral("Препаратів"), medicinesCount},
        {QStringLiteral("Останнє замовлення"), dashIfEmpty(lastOrder)}
    });
}

void RecordDetailsView::setInfoRows(const QList<QPair<QString, QString>> &rows)
{
    prepareTable(m_infoTitleLabel, m_infoTable, QStringLiteral("Профіль"),
                 {QStringLiteral("Поле"), QStringLiteral("Значення")});
    m_infoTable->horizontalHeader()->setVisible(false);
    for (const auto &row : rows) {
        addRow(m_infoTable, {row.first, row.second});
    }
}

void RecordDetailsView::prepareTable(QLabel *titleLabel, QTableWidget *table, const QString &title, const QStringList &headers)
{
    titleLabel->setText(title);
    titleLabel->setVisible(true);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(0);
    table->setVisible(true);
    table->horizontalHeader()->setVisible(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void RecordDetailsView::addRow(QTableWidget *table, const QStringList &values)
{
    const int row = table->rowCount();
    table->insertRow(row);
    for (int column = 0; column < values.size(); ++column) {
        auto *item = new QTableWidgetItem(values.at(column));
        item->setForeground(QColor(QStringLiteral("#E0E0E0")));
        table->setItem(row, column, item);
    }
    table->resizeRowsToContents();

    const int headerHeight = table->horizontalHeader()->isVisible() ? table->horizontalHeader()->height() : 0;
    int rowsHeight = 0;
    for (int i = 0; i < table->rowCount(); ++i) {
        rowsHeight += table->rowHeight(i);
    }
    table->setFixedHeight(qMin(380, headerHeight + rowsHeight + 8));
}

void RecordDetailsView::addPetCard(const QString &name, const QString &species, const QString &breed,
                                    const QString &gender, const QString &birthDate, const QString &lastVisit)
{
    const QString speciesLower = species.toLower();
    QString icon = QStringLiteral("🐾");
    if (speciesLower.contains(QStringLiteral("кіт")) || speciesLower.contains(QStringLiteral("cat")))
        icon = QStringLiteral("🐱");
    else if (speciesLower.contains(QStringLiteral("собак")) || speciesLower.contains(QStringLiteral("пес")) || speciesLower.contains(QStringLiteral("dog")))
        icon = QStringLiteral("🐶");
    else if (speciesLower.contains(QStringLiteral("попуг")) || speciesLower.contains(QStringLiteral("parrot")))
        icon = QStringLiteral("🦜");
    else if (speciesLower.contains(QStringLiteral("хом")) || speciesLower.contains(QStringLiteral("hamster")))
        icon = QStringLiteral("🐹");

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("petCard"));
    card->setStyleSheet(QStringLiteral(
        "QFrame#petCard { background-color: #353535; border: 1px solid #454545; border-radius: 12px; }"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(6);

    auto addLabel = [&](const QString &text, const QString &color, int fontSize, bool bold = false) {
        auto *lbl = new QLabel(text);
        QString style = QStringLiteral(
            "background: transparent; border: none; color: %1; font-size: %2px;").arg(color).arg(fontSize);
        if (bold)
            style += QStringLiteral(" font-weight: bold;");
        lbl->setStyleSheet(style);
        layout->addWidget(lbl);
    };

    addLabel(icon + QStringLiteral(" ") + dashIfEmpty(name), QStringLiteral("#FFFFFF"), 16, true);

    const QString speciesBreed = (species.isEmpty() && breed.isEmpty())
        ? QStringLiteral("—")
        : (breed.isEmpty() ? species : (species.isEmpty() ? breed : species + QStringLiteral(" • ") + breed));
    addLabel(speciesBreed, QStringLiteral("#D3D3D3"), 14);
    addLabel(dashIfEmpty(gender), QStringLiteral("#B0B0B0"), 13);
    addLabel(birthDate, QStringLiteral("#B0B0B0"), 13);

    auto *lastVisitLabel = new QLabel(QStringLiteral("Останній візит: ") + lastVisit);
    lastVisitLabel->setStyleSheet(QStringLiteral(
        "background: transparent; border: none; color: #C6C6C6; font-size: 13px; margin-top: 4px;"));
    layout->addWidget(lastVisitLabel);

    // Edit / Hide buttons
    auto *btnsWidget = new QWidget;
    btnsWidget->setStyleSheet(QStringLiteral("background: transparent;"));
    auto *btnsLayout = new QHBoxLayout(btnsWidget);
    btnsLayout->setContentsMargins(0, 8, 0, 0);
    btnsLayout->setSpacing(8);

    auto *editBtn = new QPushButton(QStringLiteral("Редагувати"));
    editBtn->setFixedHeight(34);
    editBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3A3A3A; color: white; border: 1px solid #5A5A5A; border-radius: 8px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #4A4A4A; }"));

    auto *hideBtn = new QPushButton(QStringLiteral("Приховати"));
    hideBtn->setFixedHeight(34);
    hideBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #3A3A3A; color: #FFB0B0; border: 1px solid #7C4A4A; border-radius: 8px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #4A3A3A; }"));

    btnsLayout->addWidget(editBtn);
    btnsLayout->addWidget(hideBtn);
    btnsLayout->addStretch();
    layout->addWidget(btnsWidget);

    m_petsCardsLayout->addWidget(card);
}

QString RecordDetailsView::scalarString(const QString &sql, const QVariantList &binds) const
{
    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const QVariant &value : binds) {
        query.addBindValue(value);
    }

    if (!query.exec() || !query.next()) {
        return QStringLiteral("0");
    }

    return query.value(0).toString();
}

void RecordDetailsView::onBackClicked()
{
    emit backRequested(m_kind);
}
