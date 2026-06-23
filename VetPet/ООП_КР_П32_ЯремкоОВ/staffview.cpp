#include "staffview.h"
#include "./ui_staffview.h"

#include <algorithm>
#include <QLocale>

#include <QComboBox>
#include <QFont>
#include <QHeaderView>
#include <QPainter>
#include <QResizeEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QStyledItemDelegate>
#include <QTableWidgetItem>
#include <QTime>

namespace {

class StaffNameDelegate : public QStyledItemDelegate
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

constexpr auto kArchivedMarker = "__archived__";
constexpr int kPhoneColumnWidth = 150;
constexpr int kArrowColumnWidth = 60;
constexpr int kRowHeight = 55;

QTime parseTimeOrDefault(const QString &text, const QTime &fallback)
{
    const QTime parsed = QTime::fromString(text.trimmed(), QStringLiteral("HH:mm"));
    return parsed.isValid() ? parsed : fallback;
}

} // namespace

StaffView::StaffView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StaffView)
{
    ui->setupUi(this);
    setupUiBehavior();
    setupConnections();
}

StaffView::~StaffView()
{
    delete ui;
}

void StaffView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
    reload();
}

void StaffView::setupUiBehavior()
{
    ui->addOverlay->raise();
    ui->addOverlay->hide();
    ui->addOverlay->setGeometry(rect());
    ui->mainLayout->insertSpacing(1, 20);

    ui->staffTable->setColumnCount(3);
    ui->staffTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->staffTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->staffTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->staffTable->setColumnWidth(0, kPhoneColumnWidth);
    ui->staffTable->setColumnWidth(2, kArrowColumnWidth);
    ui->staffTable->verticalHeader()->setDefaultSectionSize(kRowHeight);
    ui->staffTable->verticalHeader()->setVisible(false);
    ui->staffTable->horizontalHeader()->setVisible(false);
    ui->staffTable->setShowGrid(false);
    ui->staffTable->setFocusPolicy(Qt::NoFocus);
    ui->staffTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->staffTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->staffTable->setItemDelegateForColumn(1, new StaffNameDelegate(ui->staffTable));

    ui->positionCombo->setVisible(true);
    ui->addPositionBtn->setVisible(true);
    ui->positionPanel->setVisible(false);
}

void StaffView::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &StaffView::onSearchTextChanged);
    connect(ui->sortBtn, &QPushButton::clicked, this, &StaffView::onSortClicked);
    connect(ui->newBtn, &QPushButton::clicked, this, &StaffView::onAddClicked);
    connect(ui->cancelAddBtn, &QPushButton::clicked, this, &StaffView::onCancelAddClicked);
    connect(ui->saveAddBtn, &QPushButton::clicked, this, &StaffView::onSaveAddClicked);
    connect(ui->staffTable, &QTableWidget::cellClicked, this, &StaffView::onTableCellClicked);
    connect(ui->addPositionBtn, &QPushButton::clicked, this, &StaffView::onShowAddPositionClicked);
    connect(ui->cancelPositionBtn, &QPushButton::clicked, this, &StaffView::onCancelAddPositionClicked);
    connect(ui->savePositionBtn, &QPushButton::clicked, this, &StaffView::onSavePositionClicked);
}

void StaffView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    ui->addOverlay->setGeometry(rect());
}

void StaffView::reload()
{
    m_allStaff.clear();
    if (!m_db.isValid() || !m_db.isOpen()) {
        updateList();
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, phone, first_name, last_name FROM staff "
        "WHERE COALESCE(work_days, '') <> :archived"));
    query.bindValue(QStringLiteral(":archived"), QString::fromLatin1(kArchivedMarker));
    if (query.exec()) {
        while (query.next()) {
            StaffRecord record;
            record.id = query.value(0).toInt();
            record.phone = query.value(1).toString();
            record.firstName = query.value(2).toString();
            record.lastName = query.value(3).toString();
            m_allStaff.append(record);
        }
    }

    updateList();
}

void StaffView::loadPositions()
{
    ui->positionCombo->clear();
    if (!m_db.isValid() || !m_db.isOpen()) {
        return;
    }

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT id, position, salary FROM staff_position ORDER BY position"))) {
        while (query.next()) {
            const int id = query.value(0).toInt();
            const QString title = query.value(1).toString();
            const QString salary = QLocale().toString(query.value(2).toDouble(), 'f', 0);
            ui->positionCombo->addItem(QStringLiteral("%1 (%2 ₴)").arg(title, salary), id);
        }
    }
}

void StaffView::onSearchTextChanged(const QString &)
{
    updateList();
}

void StaffView::onSortClicked()
{
    m_sortAsc = !m_sortAsc;
    ui->sortBtn->setText(m_sortAsc ? QStringLiteral("А → Я") : QStringLiteral("Я → А"));
    updateList();
}

void StaffView::onAddClicked()
{
    clearAddForm();
    setErrorMessage(QString());
    loadPositions();
    showAddModal(true);
}

void StaffView::onCancelAddClicked()
{
    showAddModal(false);
}

void StaffView::onShowAddPositionClicked()
{
    ui->positionPanel->setVisible(true);
    ui->positionCombo->setVisible(false);
    ui->addPositionBtn->setVisible(false);
    ui->newPositionNameEdit->clear();
    ui->newPositionSalaryEdit->clear();
    setPositionError(QString());
}

void StaffView::onCancelAddPositionClicked()
{
    ui->positionPanel->setVisible(false);
    ui->positionCombo->setVisible(true);
    ui->addPositionBtn->setVisible(true);
}

void StaffView::onSavePositionClicked()
{
    const QString name = ui->newPositionNameEdit->text().trimmed();
    const QString salaryText = ui->newPositionSalaryEdit->text().trimmed();

    if (name.isEmpty()) {
        setPositionError(QStringLiteral("Введіть назву посади"));
        return;
    }

    bool ok = false;
    const double salary = QLocale().toDouble(QString(salaryText).replace(QLatin1Char(','), QLatin1Char('.')), &ok);
    if (!ok) {
        setPositionError(QStringLiteral("Невірний формат зарплати"));
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO staff_position (position, salary) VALUES (:position, :salary)"));
    query.bindValue(QStringLiteral(":position"), name);
    query.bindValue(QStringLiteral(":salary"), salary);
    if (!query.exec()) {
        setPositionError(query.lastError().text());
        return;
    }

    loadPositions();
    ui->positionCombo->setCurrentIndex(ui->positionCombo->count() - 1);
    onCancelAddPositionClicked();
}

void StaffView::onSaveAddClicked()
{
    const QString firstName = ui->addFirstNameEdit->text().trimmed();
    const QString lastName = ui->addLastNameEdit->text().trimmed();
    const QString phone = ui->addPhoneEdit->text().trimmed();
    const int positionId = ui->positionCombo->currentData().toInt();

    if (firstName.isEmpty() || lastName.isEmpty() || phone.isEmpty() || positionId <= 0) {
        setErrorMessage(QStringLiteral("Прізвище, ім'я, телефон та посада є обов'язковими"));
        return;
    }

    const QTime start = parseTimeOrDefault(ui->startTimeEdit->text(), QTime(9, 0));
    const QTime end = parseTimeOrDefault(ui->endTimeEdit->text(), QTime(18, 0));

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO staff (first_name, last_name, phone, staff_position_id, work_days, start_time, end_time) "
        "VALUES (:first_name, :last_name, :phone, :staff_position_id, :work_days, :start_time, :end_time)"));
    query.bindValue(QStringLiteral(":first_name"), firstName);
    query.bindValue(QStringLiteral(":last_name"), lastName);
    query.bindValue(QStringLiteral(":phone"), phone);
    query.bindValue(QStringLiteral(":staff_position_id"), positionId);
    query.bindValue(QStringLiteral(":work_days"), buildWorkDays());
    query.bindValue(QStringLiteral(":start_time"), start.toString(QStringLiteral("HH:mm:ss")));
    query.bindValue(QStringLiteral(":end_time"), end.toString(QStringLiteral("HH:mm:ss")));

    if (!query.exec()) {
        setErrorMessage(query.lastError().text());
        return;
    }

    reload();
    showAddModal(false);
}

void StaffView::onTableCellClicked(int row, int)
{
    const QTableWidgetItem *item = ui->staffTable->item(row, 0);
    if (item) {
        emit staffSelected(item->data(Qt::UserRole).toInt());
    }
}

QString StaffView::buildWorkDays() const
{
    struct DayMap {
        QPushButton *button;
        const char *code;
    };

    const DayMap days[] = {
        {ui->dayMonBtn, "Mon"}, {ui->dayTueBtn, "Tue"}, {ui->dayWedBtn, "Wed"},
        {ui->dayThuBtn, "Thu"}, {ui->dayFriBtn, "Fri"}, {ui->daySatBtn, "Sat"},
        {ui->daySunBtn, "Sun"},
    };

    QStringList selected;
    for (const DayMap &day : days) {
        if (day.button->isChecked()) {
            selected.append(QString::fromLatin1(day.code));
        }
    }
    return selected.isEmpty() ? QString() : selected.join(QStringLiteral(", "));
}

void StaffView::updateList()
{
    const QString needle = ui->searchEdit->text().trimmed();
    QList<StaffRecord> filtered;

    for (const StaffRecord &staff : m_allStaff) {
        if (needle.isEmpty()
            || staff.firstName.contains(needle, Qt::CaseInsensitive)
            || staff.lastName.contains(needle, Qt::CaseInsensitive)
            || staff.phone.contains(needle)) {
            filtered.append(staff);
        }
    }

    std::sort(filtered.begin(), filtered.end(),
              [this](const StaffRecord &a, const StaffRecord &b) {
                  const int cmp = QString::compare(a.lastName, b.lastName, Qt::CaseInsensitive);
                  return m_sortAsc ? cmp < 0 : cmp > 0;
              });

    ui->staffTable->setRowCount(0);
    const QFont phoneFont(QStringLiteral("Segoe UI"), 15);
    const QFont arrowFont(QStringLiteral("Segoe UI"), 18, QFont::Bold);

    int row = 0;
    for (const StaffRecord &staff : filtered) {
        ui->staffTable->insertRow(row);

        auto *phoneItem = new QTableWidgetItem(staff.phone);
        phoneItem->setData(Qt::UserRole, staff.id);
        phoneItem->setFont(phoneFont);
        phoneItem->setForeground(QColor(QStringLiteral("#E0E0E0")));
        phoneItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->staffTable->setItem(row, 0, phoneItem);

        auto *nameItem = new QTableWidgetItem(staff.lastName + QStringLiteral("\n") + staff.firstName);
        nameItem->setData(Qt::UserRole, staff.id);
        nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        ui->staffTable->setItem(row, 1, nameItem);

        auto *arrowItem = new QTableWidgetItem(QStringLiteral("→"));
        arrowItem->setData(Qt::UserRole, staff.id);
        arrowItem->setFont(arrowFont);
        arrowItem->setForeground(QColor(QStringLiteral("#8E8E8E")));
        arrowItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
        ui->staffTable->setItem(row, 2, arrowItem);
        ++row;
    }
}

void StaffView::showAddModal(bool visible)
{
    ui->addOverlay->setVisible(visible);
    if (visible) {
        ui->addOverlay->raise();
        ui->positionPanel->setVisible(false);
        ui->positionCombo->setVisible(true);
        ui->addPositionBtn->setVisible(true);
    }
}

void StaffView::clearAddForm()
{
    ui->addLastNameEdit->clear();
    ui->addFirstNameEdit->clear();
    ui->addPhoneEdit->clear();
    ui->startTimeEdit->setText(QStringLiteral("09:00"));
    ui->endTimeEdit->setText(QStringLiteral("18:00"));
    ui->dayMonBtn->setChecked(true);
    ui->dayTueBtn->setChecked(true);
    ui->dayWedBtn->setChecked(true);
    ui->dayThuBtn->setChecked(true);
    ui->dayFriBtn->setChecked(true);
    ui->daySatBtn->setChecked(false);
    ui->daySunBtn->setChecked(false);
}

void StaffView::setErrorMessage(const QString &message)
{
    ui->errorLabel->setVisible(!message.isEmpty());
    ui->errorLabel->setText(message);
}

void StaffView::setPositionError(const QString &message)
{
    ui->positionErrorLabel->setVisible(!message.isEmpty());
    ui->positionErrorLabel->setText(message);
}
