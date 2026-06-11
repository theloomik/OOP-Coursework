#include "staffdetailsview.h"
#include "ui_staffdetailsview.h"

#include <QHash>
#include <QMessageBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

StaffDetailsView::StaffDetailsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StaffDetailsView)
{
    ui->setupUi(this);

    connect(ui->backBtn,       &QPushButton::clicked, this, &StaffDetailsView::onBackClicked);
    connect(ui->editBtn,       &QPushButton::clicked, this, &StaffDetailsView::onEditClicked);
    connect(ui->archiveBtn,    &QPushButton::clicked, this, &StaffDetailsView::onArchiveClicked);
    connect(ui->cancelEditBtn, &QPushButton::clicked, this, &StaffDetailsView::onCancelEditClicked);
    connect(ui->saveEditBtn,   &QPushButton::clicked, this, &StaffDetailsView::onSaveEditClicked);
}

StaffDetailsView::~StaffDetailsView()
{
    delete ui;
}

void StaffDetailsView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
}

void StaffDetailsView::showStaff(int staffId)
{
    m_staffId = staffId;

    ui->loadErrorLabel->setVisible(false);
    ui->loadErrorLabel->setText(QString());

    if (!m_db.isValid() || !m_db.isOpen()) {
        setLoadError(QStringLiteral("Немає підключення до бази даних"));
        return;
    }

    loadPositions();

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT s.first_name, s.last_name, s.phone, "
        "sp.position, sp.salary, s.work_days, "
        "s.start_time, s.end_time, s.created_at, s.staff_position_id "
        "FROM staff s "
        "LEFT JOIN staff_position sp ON sp.id = s.staff_position_id "
        "WHERE s.id = :id"
    ));
    q.bindValue(QStringLiteral(":id"), staffId);

    if (!q.exec() || !q.next()) {
        setLoadError(QStringLiteral("Не вдалося завантажити дані працівника"));
        return;
    }

    m_firstName    = q.value(0).toString();
    m_lastName     = q.value(1).toString();
    m_phone        = q.value(2).toString();
    m_positionName = q.value(3).toString();
    m_salary       = q.value(4).toDouble();
    m_workDays     = q.value(5).toString();
    m_startTime    = q.value(6).toTime();
    m_endTime      = q.value(7).toTime();
    m_createdAt    = q.value(8).toDateTime();
    m_positionId   = q.value(9).toInt();

    m_isArchived = (m_workDays == QLatin1String("__archived__"));

    refreshDisplay();
}

void StaffDetailsView::refreshDisplay()
{
    const QString fullName = QString(QStringLiteral("%1 %2")).arg(m_lastName, m_firstName).trimmed();
    ui->nameLabel->setText(fullName.isEmpty() ? QStringLiteral("—") : fullName);

    ui->phoneValue->setText(m_phone.trimmed().isEmpty() ? QStringLiteral("—") : m_phone);
    ui->positionValue->setText(m_positionName.isEmpty() ? QStringLiteral("—") : m_positionName);
    ui->salaryValue->setText(m_positionName.isEmpty()
                             ? QStringLiteral("—")
                             : formatSalaryNumber(m_salary) + QStringLiteral(" ₴"));

    ui->workDaysValue->setText(formatWorkDays(m_workDays));

    const QString wh = (m_startTime.isValid() && m_endTime.isValid())
                       ? formatTime(m_startTime) + QStringLiteral(" – ") + formatTime(m_endTime)
                       : QStringLiteral("—");
    ui->workHoursValue->setText(wh);

    ui->createdAtValue->setText(m_createdAt.isValid()
                                ? m_createdAt.toString(QStringLiteral("dd.MM.yyyy HH:mm"))
                                : QStringLiteral("—"));

    ui->archivedLabel->setVisible(m_isArchived);
    ui->actionWidget->setVisible(!m_isArchived);

    ui->detailsWidget->setVisible(true);
    ui->editWidget->setVisible(false);
}

void StaffDetailsView::loadPositions()
{
    m_positions.clear();
    ui->positionCombo->blockSignals(true);
    ui->positionCombo->clear();

    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT id, position, salary FROM staff_position ORDER BY position"))) {
        while (q.next()) {
            PositionEntry e;
            e.id       = q.value(0).toInt();
            e.position = q.value(1).toString();
            e.salary   = q.value(2).toDouble();
            m_positions.append(e);
            ui->positionCombo->addItem(
                QStringLiteral("%1     (%2 ₴)").arg(e.position, formatSalaryNumber(e.salary))
            );
        }
    }

    ui->positionCombo->blockSignals(false);
}

void StaffDetailsView::startEditMode()
{
    ui->lastNameEdit->setText(m_lastName);
    ui->firstNameEdit->setText(m_firstName);
    ui->phoneEditForm->setText(m_phone);
    ui->workDaysEdit->setText(m_isArchived ? QString() : m_workDays);
    ui->startTimeEdit->setText(m_startTime.isValid() ? m_startTime.toString(QStringLiteral("HH:mm")) : QString());
    ui->endTimeEdit->setText(m_endTime.isValid() ? m_endTime.toString(QStringLiteral("HH:mm")) : QString());

    int posIdx = 0;
    for (int i = 0; i < m_positions.size(); ++i) {
        if (m_positions[i].id == m_positionId) {
            posIdx = i;
            break;
        }
    }
    ui->positionCombo->setCurrentIndex(posIdx);

    clearEditError();

    ui->detailsWidget->setVisible(false);
    ui->actionWidget->setVisible(false);
    ui->editWidget->setVisible(true);
}

void StaffDetailsView::stopEditMode()
{
    ui->detailsWidget->setVisible(true);
    ui->actionWidget->setVisible(!m_isArchived);
    ui->editWidget->setVisible(false);
}

void StaffDetailsView::setLoadError(const QString &message)
{
    ui->loadErrorLabel->setText(message);
    ui->loadErrorLabel->setVisible(true);
}

void StaffDetailsView::setEditError(const QString &message)
{
    ui->editErrorLabel->setText(message);
    ui->editErrorLabel->setVisible(true);
}

void StaffDetailsView::clearEditError()
{
    ui->editErrorLabel->setText(QString());
    ui->editErrorLabel->setVisible(false);
}

// ── Slots ──────────────────────────────────────────────────────────────────

void StaffDetailsView::onBackClicked()
{
    emit backRequested();
}

void StaffDetailsView::onEditClicked()
{
    startEditMode();
}

void StaffDetailsView::onArchiveClicked()
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE staff SET work_days='__archived__' WHERE id=:id"));
    q.bindValue(QStringLiteral(":id"), m_staffId);

    if (!q.exec()) {
        setLoadError(QStringLiteral("Помилка архівування: ") + q.lastError().text());
        return;
    }

    emit backRequested();
}

void StaffDetailsView::onCancelEditClicked()
{
    stopEditMode();
}

void StaffDetailsView::onSaveEditClicked()
{
    const QString lastName  = ui->lastNameEdit->text().trimmed();
    const QString firstName = ui->firstNameEdit->text().trimmed();
    const QString phone     = ui->phoneEditForm->text().trimmed();
    const int posIdx        = ui->positionCombo->currentIndex();
    const QString workDays  = ui->workDaysEdit->text().trimmed();
    const QString startStr  = ui->startTimeEdit->text().trimmed();
    const QString endStr    = ui->endTimeEdit->text().trimmed();

    if (lastName.isEmpty() || firstName.isEmpty()) {
        setEditError(QStringLiteral("Введіть прізвище та ім'я"));
        return;
    }
    if (posIdx < 0 || posIdx >= m_positions.size()) {
        setEditError(QStringLiteral("Оберіть посаду"));
        return;
    }
    if (workDays.isEmpty()) {
        setEditError(QStringLiteral("Введіть робочі дні"));
        return;
    }

    const QTime startTime = QTime::fromString(startStr, QStringLiteral("HH:mm"));
    const QTime endTime   = QTime::fromString(endStr,   QStringLiteral("HH:mm"));
    if (!startTime.isValid() || !endTime.isValid()) {
        setEditError(QStringLiteral("Введіть час у форматі ГГ:ХХ (наприклад, 09:00)"));
        return;
    }

    const int posId = m_positions[posIdx].id;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE staff SET first_name=:fn, last_name=:ln, phone=:ph, "
        "staff_position_id=:posId, work_days=:wd, start_time=:st, end_time=:et "
        "WHERE id=:id"
    ));
    q.bindValue(QStringLiteral(":fn"),    firstName);
    q.bindValue(QStringLiteral(":ln"),    lastName);
    q.bindValue(QStringLiteral(":ph"),    phone);
    q.bindValue(QStringLiteral(":posId"), posId);
    q.bindValue(QStringLiteral(":wd"),    workDays);
    q.bindValue(QStringLiteral(":st"),    startStr);
    q.bindValue(QStringLiteral(":et"),    endStr);
    q.bindValue(QStringLiteral(":id"),    m_staffId);

    if (!q.exec()) {
        setEditError(QStringLiteral("Помилка збереження: ") + q.lastError().text());
        return;
    }

    showStaff(m_staffId);
}

// ── Static helpers ─────────────────────────────────────────────────────────

QString StaffDetailsView::formatWorkDays(const QString &raw)
{
    if (raw.isEmpty() || raw == QLatin1String("__archived__"))
        return QStringLiteral("—");

    static const QHash<QString, QString> dayMap = {
        {QStringLiteral("Mon"), QStringLiteral("Понеділок")},
        {QStringLiteral("Пн"),  QStringLiteral("Понеділок")},
        {QStringLiteral("Tue"), QStringLiteral("Вівторок")},
        {QStringLiteral("Вт"),  QStringLiteral("Вівторок")},
        {QStringLiteral("Wed"), QStringLiteral("Середа")},
        {QStringLiteral("Ср"),  QStringLiteral("Середа")},
        {QStringLiteral("Thu"), QStringLiteral("Четвер")},
        {QStringLiteral("Чт"),  QStringLiteral("Четвер")},
        {QStringLiteral("Fri"), QStringLiteral("П'ятниця")},
        {QStringLiteral("Пт"),  QStringLiteral("П'ятниця")},
        {QStringLiteral("Sat"), QStringLiteral("Субота")},
        {QStringLiteral("Сб"),  QStringLiteral("Субота")},
        {QStringLiteral("Sun"), QStringLiteral("Неділя")},
        {QStringLiteral("Нд"),  QStringLiteral("Неділя")},
    };

    const QStringList tokens = raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList result;
    result.reserve(tokens.size());
    for (const QString &tok : tokens)
        result.append(dayMap.value(tok.trimmed(), tok.trimmed()));

    return result.join(QStringLiteral(", "));
}

QString StaffDetailsView::formatTime(const QTime &t)
{
    return t.isValid() ? t.toString(QStringLiteral("HH:mm")) : QStringLiteral("—");
}

QString StaffDetailsView::formatSalaryNumber(double salary)
{
    const auto whole = static_cast<qlonglong>(salary);
    if (salary == static_cast<double>(whole))
        return QString::number(whole);

    QString s = QString::number(salary, 'f', 2);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s;
}
