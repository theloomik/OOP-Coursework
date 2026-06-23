#include "appointmentsview.h"
#include "./ui_appointmentsview.h"

#include <algorithm>

#include <QDateTime>
#include <QTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSqlQuery>
#include <QVBoxLayout>

namespace {

bool isCancelledStatus(const QString &status)
{
    return status.trimmed().compare(QStringLiteral("скасовано"), Qt::CaseInsensitive) == 0;
}

} // namespace

AppointmentsView::AppointmentsView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AppointmentsView)
{
    ui->setupUi(this);
    ui->mainLayout->insertSpacing(1, 20);

    m_refreshTimer.setInterval(60 * 1000);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AppointmentsView::onRefreshTimer);
    m_refreshTimer.start();

    setupConnections();
}

AppointmentsView::~AppointmentsView()
{
    delete ui;
}

void AppointmentsView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
    reload();
}

void AppointmentsView::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &AppointmentsView::onSearchTextChanged);
    connect(ui->sortBtn, &QPushButton::clicked, this, &AppointmentsView::onSortClicked);
}

void AppointmentsView::reload()
{
    m_allAppointments.clear();
    if (!m_db.isValid() || !m_db.isOpen()) {
        updateList();
        return;
    }

    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT a.id, a.date, a.status, a.client_id, "
            "c.phone, c.first_name, c.last_name, p.name "
            "FROM appointments a "
            "INNER JOIN clients c ON c.id = a.client_id "
            "LEFT JOIN pets p ON p.id = a.pet_id"))) {
        while (query.next()) {
            AppointmentRecord record;
            record.id = query.value(0).toInt();
            record.date = query.value(1).toDateTime();
            record.status = query.value(2).toString();
            record.clientId = query.value(3).toInt();
            record.clientPhone = query.value(4).toString();
            record.clientFirstName = query.value(5).toString();
            record.clientLastName = query.value(6).toString();
            record.petName = query.value(7).toString();
            if (record.petName.isEmpty()) {
                record.petName = QStringLiteral("—");
            }
            m_allAppointments.append(record);
        }
    }

    updateList();
}

void AppointmentsView::onSearchTextChanged(const QString &)
{
    updateList();
}

void AppointmentsView::onSortClicked()
{
    m_sortAsc = !m_sortAsc;
    ui->sortBtn->setText(m_sortAsc ? QStringLiteral("Дата ↑") : QStringLiteral("Дата ↓"));
    updateList();
}

void AppointmentsView::onRefreshTimer()
{
    updateList();
}

void AppointmentsView::onRowClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button) {
        return;
    }
    emit clientSelected(button->property("clientId").toInt());
}

void AppointmentsView::updateList()
{
    rebuildList();
}

void AppointmentsView::rebuildList()
{
    const QString needle = ui->searchEdit->text().trimmed();
    QList<AppointmentRecord> filtered;

    for (const AppointmentRecord &record : m_allAppointments) {
        const QString fullName = QStringLiteral("%1 %2").arg(record.clientLastName, record.clientFirstName).trimmed();
        if (needle.isEmpty()
            || record.clientPhone.contains(needle, Qt::CaseInsensitive)
            || fullName.contains(needle, Qt::CaseInsensitive)
            || record.petName.contains(needle, Qt::CaseInsensitive)) {
            filtered.append(record);
        }
    }

    std::sort(filtered.begin(), filtered.end(),
              [this](const AppointmentRecord &a, const AppointmentRecord &b) {
                  return m_sortAsc ? a.date < b.date : a.date > b.date;
              });

    QLayoutItem *child = nullptr;
    while ((child = ui->listLayout->takeAt(0)) != nullptr) {
        if (QWidget *widget = child->widget()) {
            widget->deleteLater();
        }
        delete child;
    }

    struct GroupSpec {
        GroupKey key;
        QString title;
        QString color;
    };

    const GroupSpec groups[] = {
        {GroupKey::Today, QStringLiteral("Сьогодні"), QStringLiteral("#4A7C59")},
        {GroupKey::Tomorrow, QStringLiteral("Завтра"), QStringLiteral("#7C6E4A")},
        {GroupKey::Future, QStringLiteral("Майбутні"), QStringLiteral("#4A6A7C")},
        {GroupKey::Past, QStringLiteral("Минулі"), QStringLiteral("#6A6A6A")},
        {GroupKey::Cancelled, QStringLiteral("Скасовані"), QStringLiteral("#7C4A4A")},
    };

    bool hasAny = false;
    for (const GroupSpec &group : groups) {
        QList<AppointmentRecord> items;
        for (const AppointmentRecord &record : filtered) {
            if (groupKeyFor(record) == group.key) {
                items.append(record);
            }
        }
        if (items.isEmpty()) {
            continue;
        }

        hasAny = true;
        ui->listLayout->addWidget(createGroupHeader(group.title, group.color));
        for (const AppointmentRecord &record : items) {
            ui->listLayout->addWidget(createAppointmentRow(record));
        }
        ui->listLayout->addSpacing(16);
    }

    ui->emptyLabel->setVisible(!hasAny);
    ui->listScroll->setVisible(hasAny);
}

QWidget *AppointmentsView::createGroupHeader(const QString &title, const QString &markerColor) const
{
    auto *frame = new QFrame;
    frame->setStyleSheet(QStringLiteral(
        "QFrame { background-color: #323232; border-radius: 10px; padding: 8px 12px; margin: 0 10px 8px 10px; }"));

    auto *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *marker = new QFrame;
    marker->setFixedSize(10, 10);
    marker->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 5px;").arg(markerColor));

    auto *label = new QLabel(title);
    label->setStyleSheet(QStringLiteral("color: white; font-weight: 600;"));

    layout->addWidget(marker);
    layout->addWidget(label);
    layout->addStretch();
    return frame;
}

QWidget *AppointmentsView::createAppointmentRow(const AppointmentRecord &record) const
{
    auto *button = new QPushButton;
    button->setFlat(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(55);
    button->setProperty("clientId", record.clientId);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; text-align: left; margin: 0 10px; }"
        "QPushButton:hover { background-color: #333333; }"));

    connect(button, &QPushButton::clicked, const_cast<AppointmentsView *>(this), &AppointmentsView::onRowClicked);

    auto *layout = new QHBoxLayout(button);
    layout->setContentsMargins(10, 0, 10, 0);
    layout->setSpacing(10);

    auto *dateBox = new QFrame;
    dateBox->setMinimumWidth(170);
    dateBox->setMaximumWidth(170);
    dateBox->setStyleSheet(QStringLiteral(
                               "QFrame { background-color: #2F2F2F; border-radius: 8px; border-left: 3px solid %1; padding: 4px 10px; }")
                           .arg(dateMarkerColor(record)));

    auto *dateLayout = new QVBoxLayout(dateBox);
    auto *dateLabel = new QLabel(record.date.toString(QStringLiteral("dd.MM.yyyy HH:mm")));
    dateLabel->setAlignment(Qt::AlignCenter);
    dateLabel->setStyleSheet(QStringLiteral("color: #E0E0E0;"));
    dateLayout->addWidget(dateLabel);

    auto *clientBox = new QVBoxLayout;
    clientBox->setSpacing(1);
    auto *clientName = new QLabel(QStringLiteral("%1 %2").arg(record.clientLastName, record.clientFirstName).trimmed());
    clientName->setStyleSheet(QStringLiteral("color: #E0E0E0; font-weight: 600;"));
    auto *clientPhone = new QLabel(record.clientPhone);
    clientPhone->setStyleSheet(QStringLiteral("color: #9A9A9A; font-size: 12px;"));
    clientBox->addWidget(clientName);
    clientBox->addWidget(clientPhone);

    auto *petLabel = new QLabel(record.petName);
    petLabel->setMinimumWidth(130);
    petLabel->setMaximumWidth(130);
    petLabel->setAlignment(Qt::AlignCenter);
    petLabel->setStyleSheet(QStringLiteral("color: #CFCFCF;"));

    auto *statusFrame = new QFrame;
    statusFrame->setMinimumWidth(130);
    statusFrame->setMaximumWidth(130);
    statusFrame->setFixedHeight(30);
    const QString statusColorValue = statusColor(record.status);
    statusFrame->setStyleSheet(QStringLiteral(
        "QFrame { border: 1.5px solid %1; border-radius: 8px; padding: 0 10px; }").arg(statusColorValue));
    auto *statusLayout = new QHBoxLayout(statusFrame);
    auto *statusLabel = new QLabel(record.status);
    statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(statusColorValue));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusLabel);

    auto *arrow = new QLabel(QStringLiteral("→"));
    arrow->setStyleSheet(QStringLiteral("color: #8E8E8E; font-size: 18px;"));
    arrow->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    layout->addWidget(dateBox);
    layout->addLayout(clientBox, 1);
    layout->addWidget(petLabel);
    layout->addWidget(statusFrame);
    layout->addWidget(arrow);

    return button;
}

QString AppointmentsView::statusColor(const QString &status)
{
    const QString normalized = status.trimmed().toLower();
    if (normalized == QStringLiteral("виконано")) {
        return QStringLiteral("#4A7C59");
    }
    if (normalized == QStringLiteral("скасовано")) {
        return QStringLiteral("#7C4A4A");
    }
    return QStringLiteral("#7C6E4A");
}

QString AppointmentsView::dateMarkerColor(const AppointmentRecord &record)
{
    if (isCancelledStatus(record.status)) {
        return QStringLiteral("#7C4A4A");
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime todayStart(now.date(), QTime(0, 0));
    const QDateTime tomorrowStart = todayStart.addDays(1);
    const QDateTime dayAfterTomorrowStart = todayStart.addDays(2);

    if (record.date < now) {
        return QStringLiteral("#6A6A6A");
    }
    if (record.date >= todayStart && record.date < tomorrowStart) {
        return QStringLiteral("#4A7C59");
    }
    if (record.date >= tomorrowStart && record.date < dayAfterTomorrowStart) {
        return QStringLiteral("#7C6E4A");
    }
    return QStringLiteral("#4A6A7C");
}

AppointmentsView::GroupKey AppointmentsView::groupKeyFor(const AppointmentRecord &record)
{
    if (isCancelledStatus(record.status)) {
        return GroupKey::Cancelled;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime todayStart(now.date(), QTime(0, 0));
    const QDateTime tomorrowStart = todayStart.addDays(1);
    const QDateTime dayAfterTomorrowStart = todayStart.addDays(2);

    if (record.date < now) {
        return GroupKey::Past;
    }
    if (record.date >= todayStart && record.date < tomorrowStart) {
        return GroupKey::Today;
    }
    if (record.date >= tomorrowStart && record.date < dayAfterTomorrowStart) {
        return GroupKey::Tomorrow;
    }
    return GroupKey::Future;
}

void AppointmentsView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}
