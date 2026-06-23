#include "quickappointmentview.h"
#include "./ui_quickappointmentview.h"

#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>
#include <QVariant>
#include <QVBoxLayout>

namespace {

const QStringList kMonthNames = {
    QStringLiteral("Січень"),   QStringLiteral("Лютий"),
    QStringLiteral("Березень"), QStringLiteral("Квітень"),
    QStringLiteral("Травень"),  QStringLiteral("Червень"),
    QStringLiteral("Липень"),   QStringLiteral("Серпень"),
    QStringLiteral("Вересень"), QStringLiteral("Жовтень"),
    QStringLiteral("Листопад"), QStringLiteral("Грудень"),
};

const QString kDialogStyle = QStringLiteral(
    "QDialog { background-color: #2B2B2B; }"
    "QLabel { color: #CFCFCF; font-size: 13px; }"
    "QLabel#titleLabel { color: white; font-size: 20px; font-weight: bold; }"
    "QLabel#headerLabel { color: #8C8C8C; font-size: 12px; font-weight: 600; }"
    "QLabel#errorLabel { color: #FF6B6B; font-size: 13px; }"
    "QLineEdit {"
    "  background-color: #3A3A3A; color: white; border: none;"
    "  border-radius: 10px; padding: 0 14px; min-height: 42px;"
    "}"
    "QComboBox {"
    "  background-color: #3A3A3A; color: white;"
    "  border: 1px solid #343434; border-radius: 12px;"
    "  min-height: 44px; padding: 0 12px;"
    "}"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView {"
    "  background-color: #2B2B2B; color: white;"
    "  selection-background-color: #3A3A3A; border: 1px solid #4A4A4A;"
    "}"
    "QPushButton#cancelBtn {"
    "  background-color: #3A3A3A; border: none; border-radius: 10px;"
    "  color: #AAAAAA; min-height: 42px; font-weight: 600;"
    "}"
    "QPushButton#cancelBtn:hover { background-color: #4A4A4A; }"
    "QPushButton#saveBtn {"
    "  background-color: #3A3A3A; border: 1.5px solid #5E5E5E;"
    "  border-radius: 10px; color: white; min-height: 42px; font-weight: 600;"
    "}"
    "QPushButton#saveBtn:hover { background-color: #454545; }");

} // namespace

QuickAppointmentView::QuickAppointmentView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::QuickAppointmentView)
{
    ui->setupUi(this);

    connect(ui->phoneEdit, &QLineEdit::textChanged,
            this, &QuickAppointmentView::onPhoneTextChanged);
    connect(ui->clientCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QuickAppointmentView::onClientIndexChanged);
    connect(ui->addClientBtn, &QPushButton::clicked,
            this, &QuickAppointmentView::onAddClientClicked);
    connect(ui->addPetBtn, &QPushButton::clicked,
            this, &QuickAppointmentView::onAddPetClicked);
    connect(ui->cancelBtn, &QPushButton::clicked,
            this, &QuickAppointmentView::onCancelClicked);
    connect(ui->saveBtn, &QPushButton::clicked,
            this, &QuickAppointmentView::onSaveClicked);
}

QuickAppointmentView::~QuickAppointmentView()
{
    delete ui;
}

void QuickAppointmentView::setDatabase(const QSqlDatabase &database)
{
    m_db = database;
}

void QuickAppointmentView::reset()
{
    clearError();
    ui->phoneEdit->blockSignals(true);
    ui->phoneEdit->clear();
    ui->phoneEdit->blockSignals(false);
    ui->clientCombo->blockSignals(true);
    ui->clientCombo->clear();
    ui->clientCombo->blockSignals(false);
    ui->petCombo->clear();
    ui->addPetBtn->setEnabled(false);
    ui->descriptionEdit->clear();
    m_clients.clear();
    m_pets.clear();

    seedDateTimeCombos();
    loadDropdowns();
}

void QuickAppointmentView::loadDropdowns()
{
    if (!m_db.isValid() || !m_db.isOpen())
        return;

    m_staff.clear();
    ui->staffCombo->clear();
    QSqlQuery staffQ(m_db);
    if (staffQ.exec(QStringLiteral(
            "SELECT s.id, s.last_name, s.first_name, sp.position "
            "FROM staff s LEFT JOIN staff_position sp ON sp.id = s.staff_position_id "
            "WHERE s.work_days <> '__archived__' "
            "ORDER BY s.last_name, s.first_name"))) {
        while (staffQ.next()) {
            IdEntry e;
            e.id = staffQ.value(0).toInt();
            const QString pos = staffQ.value(3).toString();
            e.display = QStringLiteral("%1 %2 (%3)")
                            .arg(staffQ.value(1).toString(),
                                 staffQ.value(2).toString(),
                                 pos.isEmpty() ? QStringLiteral("—") : pos)
                            .trimmed();
            m_staff.append(e);
            ui->staffCombo->addItem(e.display);
        }
    }

    m_services.clear();
    ui->serviceCombo->clear();
    m_services.append({0, QStringLiteral("")});
    ui->serviceCombo->addItem(QStringLiteral("— Без послуги —"));
    QSqlQuery svcQ(m_db);
    if (svcQ.exec(QStringLiteral("SELECT id, name FROM services ORDER BY name"))) {
        while (svcQ.next()) {
            IdEntry e;
            e.id = svcQ.value(0).toInt();
            e.display = svcQ.value(1).toString();
            m_services.append(e);
            ui->serviceCombo->addItem(e.display);
        }
    }

    m_petTypes.clear();
    QSqlQuery ptQ(m_db);
    if (ptQ.exec(QStringLiteral(
            "SELECT id, species, breed FROM pet_type ORDER BY species, breed"))) {
        while (ptQ.next()) {
            IdEntry e;
            e.id = ptQ.value(0).toInt();
            const QString breed = ptQ.value(2).toString();
            e.display = breed.isEmpty()
                            ? ptQ.value(1).toString()
                            : QStringLiteral("%1 %2").arg(ptQ.value(1).toString(), breed);
            m_petTypes.append(e);
        }
    }
}

void QuickAppointmentView::seedDateTimeCombos()
{
    ui->dayCombo->clear();
    for (int d = 1; d <= 31; ++d)
        ui->dayCombo->addItem(QString::number(d));

    ui->monthCombo->clear();
    for (const QString &name : kMonthNames)
        ui->monthCombo->addItem(name);

    const int curYear = QDate::currentDate().year();
    ui->yearCombo->clear();
    for (int y = curYear + 1; y >= 2024; --y)
        ui->yearCombo->addItem(QString::number(y));

    ui->hourCombo->clear();
    for (int h = 0; h < 24; ++h)
        ui->hourCombo->addItem(QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));

    ui->minuteCombo->clear();
    for (int m = 0; m < 60; m += 5)
        ui->minuteCombo->addItem(QStringLiteral("%1").arg(m, 2, 10, QLatin1Char('0')));

    const QDateTime initial = QDateTime::currentDateTime().addSecs(3600);
    const QDate d = initial.date();
    const QTime t = initial.time();

    ui->dayCombo->setCurrentIndex(d.day() - 1);
    ui->monthCombo->setCurrentIndex(d.month() - 1);
    for (int i = 0; i < ui->yearCombo->count(); ++i) {
        if (ui->yearCombo->itemText(i).toInt() == d.year()) {
            ui->yearCombo->setCurrentIndex(i);
            break;
        }
    }
    ui->hourCombo->setCurrentIndex(t.hour());
    ui->minuteCombo->setCurrentIndex((t.minute() / 5) % 12);
}

void QuickAppointmentView::searchClients(const QString &phone)
{
    ui->clientCombo->blockSignals(true);
    m_clients.clear();
    ui->clientCombo->clear();
    ui->clientCombo->blockSignals(false);

    m_pets.clear();
    ui->petCombo->clear();
    ui->addPetBtn->setEnabled(false);

    const QString trimmed = phone.trimmed();
    if (trimmed.isEmpty() || !m_db.isValid() || !m_db.isOpen())
        return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, last_name, first_name, phone "
        "FROM clients WHERE phone LIKE :ph "
        "ORDER BY last_name, first_name LIMIT 20"));
    q.bindValue(QStringLiteral(":ph"), QStringLiteral("%%1%").arg(trimmed));

    if (q.exec()) {
        while (q.next()) {
            IdEntry e;
            e.id = q.value(0).toInt();
            e.display = QStringLiteral("%1 %2 | %3")
                            .arg(q.value(1).toString(),
                                 q.value(2).toString(),
                                 q.value(3).toString())
                            .trimmed();
            m_clients.append(e);
        }
    }

    ui->clientCombo->blockSignals(true);
    for (const IdEntry &e : m_clients)
        ui->clientCombo->addItem(e.display);
    ui->clientCombo->blockSignals(false);

    if (m_clients.isEmpty())
        return;

    int selectIdx = 0;
    for (int i = 0; i < m_clients.size(); ++i) {
        if (m_clients[i].display.endsWith(QStringLiteral("| %1").arg(trimmed))) {
            selectIdx = i;
            break;
        }
    }
    ui->clientCombo->setCurrentIndex(selectIdx);
    loadPetsForClient(m_clients[selectIdx].id);
}

void QuickAppointmentView::loadPetsForClient(int clientId)
{
    m_pets.clear();
    ui->petCombo->clear();
    ui->addPetBtn->setEnabled(clientId > 0);

    if (clientId <= 0 || !m_db.isValid() || !m_db.isOpen())
        return;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT p.id, p.name, pt.species "
        "FROM pets p LEFT JOIN pet_type pt ON pt.id = p.pet_type_id "
        "WHERE p.client_id = :id ORDER BY p.name"));
    q.bindValue(QStringLiteral(":id"), clientId);

    if (q.exec()) {
        while (q.next()) {
            IdEntry e;
            e.id = q.value(0).toInt();
            const QString species = q.value(2).toString();
            e.display = species.isEmpty()
                            ? q.value(1).toString()
                            : QStringLiteral("%1 (%2)").arg(q.value(1).toString(), species);
            m_pets.append(e);
            ui->petCombo->addItem(e.display);
        }
    }
}

void QuickAppointmentView::onPhoneTextChanged(const QString &text)
{
    searchClients(text);
}

void QuickAppointmentView::onClientIndexChanged(int index)
{
    const int clientId = (index >= 0 && index < m_clients.size())
                             ? m_clients[index].id : 0;
    loadPetsForClient(clientId);
}

void QuickAppointmentView::onAddClientClicked()
{
    if (execAddClientDialog())
        searchClients(ui->phoneEdit->text());
}

void QuickAppointmentView::onAddPetClicked()
{
    const int clientId = currentClientId();
    if (clientId <= 0) {
        setError(QStringLiteral("Спочатку оберіть клієнта"));
        return;
    }
    clearError();
    if (execAddPetDialog(clientId))
        loadPetsForClient(clientId);
}

void QuickAppointmentView::onCancelClicked()
{
    emit cancelled();
}

void QuickAppointmentView::onSaveClicked()
{
    clearError();

    const int clientId = currentClientId();
    if (clientId <= 0) { setError(QStringLiteral("Оберіть клієнта")); return; }

    const int petId = currentPetId();
    if (petId <= 0) { setError(QStringLiteral("Оберіть тварину")); return; }

    const int staffId = currentStaffId();
    if (staffId <= 0) { setError(QStringLiteral("Оберіть лікаря")); return; }

    const int day   = ui->dayCombo->currentIndex() + 1;
    const int month = ui->monthCombo->currentIndex() + 1;
    const int year  = ui->yearCombo->currentText().toInt();
    const int hour  = ui->hourCombo->currentText().toInt();
    const int min   = ui->minuteCombo->currentText().toInt();

    const QDate apptDate(year, month, day);
    if (!apptDate.isValid()) {
        setError(QStringLiteral("Невірна дата"));
        return;
    }

    const QDateTime apptDateTime(apptDate, QTime(hour, min));
    const int serviceId   = currentServiceId();
    const QString desc    = ui->descriptionEdit->toPlainText().trimmed();
    const QString status  = ui->statusCombo->currentText();

    if (!m_db.isValid() || !m_db.isOpen()) {
        setError(QStringLiteral("Немає підключення до бази даних"));
        return;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO appointments "
        "  (client_id, pet_id, staff_id, service_id, date, description, status) "
        "VALUES (:cl, :pt, :st, :sv, :dt, :ds, :sx)"));
    q.bindValue(QStringLiteral(":cl"), clientId);
    q.bindValue(QStringLiteral(":pt"), petId);
    q.bindValue(QStringLiteral(":st"), staffId);
    q.bindValue(QStringLiteral(":sv"), serviceId > 0 ? QVariant(serviceId) : QVariant());
    q.bindValue(QStringLiteral(":dt"), apptDateTime);
    q.bindValue(QStringLiteral(":ds"), desc.isEmpty() ? QVariant() : QVariant(desc));
    q.bindValue(QStringLiteral(":sx"), status.isEmpty()
                                           ? QStringLiteral("заплановано") : status);

    if (!q.exec()) {
        setError(QStringLiteral("Помилка збереження: ") + q.lastError().text());
        return;
    }

    emit appointmentSaved(clientId);
}

int QuickAppointmentView::currentClientId() const
{
    const int idx = ui->clientCombo->currentIndex();
    return (idx >= 0 && idx < m_clients.size()) ? m_clients[idx].id : 0;
}

int QuickAppointmentView::currentPetId() const
{
    const int idx = ui->petCombo->currentIndex();
    return (idx >= 0 && idx < m_pets.size()) ? m_pets[idx].id : 0;
}

int QuickAppointmentView::currentStaffId() const
{
    const int idx = ui->staffCombo->currentIndex();
    return (idx >= 0 && idx < m_staff.size()) ? m_staff[idx].id : 0;
}

int QuickAppointmentView::currentServiceId() const
{
    const int idx = ui->serviceCombo->currentIndex();
    return (idx > 0 && idx < m_services.size()) ? m_services[idx].id : 0;
}

void QuickAppointmentView::setError(const QString &message)
{
    ui->errorLabel->setText(message);
    ui->errorLabel->setVisible(!message.isEmpty());
}

void QuickAppointmentView::clearError()
{
    ui->errorLabel->clear();
    ui->errorLabel->setVisible(false);
}

bool QuickAppointmentView::execAddClientDialog()
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("Новий клієнт"));
    dialog->setModal(true);
    dialog->setMinimumWidth(420);
    dialog->setStyleSheet(kDialogStyle);

    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(28, 28, 28, 28);
    root->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("Новий клієнт"));
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setContentsMargins(0, 0, 0, 6);
    root->addWidget(titleLabel);

    auto *lastNameEdit  = new QLineEdit;
    auto *firstNameEdit = new QLineEdit;
    auto *phoneEdit     = new QLineEdit;
    auto *emailEdit     = new QLineEdit;

    lastNameEdit->setPlaceholderText(QStringLiteral("Прізвище *"));
    firstNameEdit->setPlaceholderText(QStringLiteral("Ім'я *"));
    phoneEdit->setPlaceholderText(QStringLiteral("Номер телефону *"));
    emailEdit->setPlaceholderText(QStringLiteral("Email (необов'язково)"));
    phoneEdit->setText(ui->phoneEdit->text().trimmed());

    root->addWidget(lastNameEdit);
    root->addWidget(firstNameEdit);
    root->addWidget(phoneEdit);
    root->addWidget(emailEdit);

    auto *errorLabel = new QLabel;
    errorLabel->setObjectName(QStringLiteral("errorLabel"));
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setWordWrap(true);
    errorLabel->setVisible(false);
    root->addWidget(errorLabel);

    auto *btnRow  = new QHBoxLayout;
    auto *cancelBtn = new QPushButton(QStringLiteral("Скасувати"));
    auto *saveBtn   = new QPushButton(QStringLiteral("Зберегти"));
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    saveBtn->setObjectName(QStringLiteral("saveBtn"));
    btnRow->setSpacing(10);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    bool saved = false;

    QObject::connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    QObject::connect(saveBtn, &QPushButton::clicked, dialog, [&]() {
        errorLabel->setVisible(false);
        const QString ln = lastNameEdit->text().trimmed();
        const QString fn = firstNameEdit->text().trimmed();
        const QString ph = phoneEdit->text().trimmed();
        const QString em = emailEdit->text().trimmed();

        if (ln.isEmpty() || fn.isEmpty() || ph.isEmpty()) {
            errorLabel->setText(QStringLiteral("Прізвище, ім'я та телефон є обов'язковими"));
            errorLabel->setVisible(true);
            return;
        }

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO clients (first_name, last_name, phone, email) "
            "VALUES (:fn, :ln, :ph, :em)"));
        q.bindValue(QStringLiteral(":fn"), fn);
        q.bindValue(QStringLiteral(":ln"), ln);
        q.bindValue(QStringLiteral(":ph"), ph);
        q.bindValue(QStringLiteral(":em"), em.isEmpty() ? QVariant() : QVariant(em));

        if (!q.exec()) {
            errorLabel->setText(QStringLiteral("Помилка: ") + q.lastError().text());
            errorLabel->setVisible(true);
            return;
        }

        ui->phoneEdit->blockSignals(true);
        ui->phoneEdit->setText(ph);
        ui->phoneEdit->blockSignals(false);

        saved = true;
        dialog->accept();
    });

    dialog->exec();
    dialog->deleteLater();
    return saved;
}

bool QuickAppointmentView::execAddPetDialog(int clientId)
{
    auto *dialog = new QDialog(this);
    dialog->setWindowTitle(QStringLiteral("Новий улюбленець"));
    dialog->setModal(true);
    dialog->setMinimumWidth(460);
    dialog->setStyleSheet(kDialogStyle);

    auto *root = new QVBoxLayout(dialog);
    root->setContentsMargins(28, 28, 28, 28);
    root->setSpacing(14);

    auto *titleLabel = new QLabel(QStringLiteral("Новий улюбленець"));
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setContentsMargins(0, 0, 0, 4);
    root->addWidget(titleLabel);

    auto *nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(QStringLiteral("Кличка *"));
    root->addWidget(nameEdit);

    auto *typeHeader = new QLabel(QStringLiteral("Вид *"));
    typeHeader->setObjectName(QStringLiteral("headerLabel"));
    root->addWidget(typeHeader);

    auto *typeCombo = new QComboBox;
    for (const IdEntry &e : std::as_const(m_petTypes))
        typeCombo->addItem(e.display);
    root->addWidget(typeCombo);

    auto *genderHeader = new QLabel(QStringLiteral("Стать"));
    genderHeader->setObjectName(QStringLiteral("headerLabel"));
    root->addWidget(genderHeader);

    auto *genderCombo = new QComboBox;
    genderCombo->addItem(QStringLiteral("male"));
    genderCombo->addItem(QStringLiteral("female"));
    root->addWidget(genderCombo);

    auto *birthHeader = new QLabel(QStringLiteral("Дата народження"));
    birthHeader->setObjectName(QStringLiteral("headerLabel"));
    root->addWidget(birthHeader);

    auto *birthRow = new QHBoxLayout;
    birthRow->setSpacing(8);

    auto *bdDay   = new QComboBox;
    auto *bdMonth = new QComboBox;
    auto *bdYear  = new QComboBox;

    bdDay->addItem(QStringLiteral("—"));
    for (int i = 1; i <= 31; ++i)
        bdDay->addItem(QString::number(i));

    bdMonth->addItem(QStringLiteral("—"));
    for (const QString &m : kMonthNames)
        bdMonth->addItem(m);

    const int curYear = QDate::currentDate().year();
    bdYear->addItem(QStringLiteral("—"));
    for (int y = curYear; y >= 1990; --y)
        bdYear->addItem(QString::number(y));

    birthRow->addWidget(bdDay);
    birthRow->addWidget(bdMonth);
    birthRow->addWidget(bdYear);
    root->addLayout(birthRow);

    auto *errorLabel = new QLabel;
    errorLabel->setObjectName(QStringLiteral("errorLabel"));
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setWordWrap(true);
    errorLabel->setVisible(false);
    root->addWidget(errorLabel);

    auto *btnRow    = new QHBoxLayout;
    auto *cancelBtn = new QPushButton(QStringLiteral("Скасувати"));
    auto *saveBtn   = new QPushButton(QStringLiteral("Зберегти"));
    cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    saveBtn->setObjectName(QStringLiteral("saveBtn"));
    btnRow->setSpacing(10);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    bool saved = false;

    QObject::connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    QObject::connect(saveBtn, &QPushButton::clicked, dialog, [&]() {
        errorLabel->setVisible(false);

        const QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            errorLabel->setText(QStringLiteral("Кличка є обов'язковою"));
            errorLabel->setVisible(true);
            return;
        }

        const int typeIdx = typeCombo->currentIndex();
        if (m_petTypes.isEmpty() || typeIdx < 0 || typeIdx >= m_petTypes.size()) {
            errorLabel->setText(QStringLiteral("Оберіть вид тварини"));
            errorLabel->setVisible(true);
            return;
        }
        const int petTypeId = m_petTypes[typeIdx].id;
        const QString gender = genderCombo->currentText();

        const int bdi = bdDay->currentIndex();
        const int bmi = bdMonth->currentIndex();
        const int byi = bdYear->currentIndex();
        QDate birthDate;
        if (bdi > 0 || bmi > 0 || byi > 0) {
            if (bdi == 0 || bmi == 0 || byi == 0) {
                errorLabel->setText(
                    QStringLiteral("Оберіть повну дату народження або залиште порожньою"));
                errorLabel->setVisible(true);
                return;
            }
            birthDate = QDate(bdYear->currentText().toInt(), bmi, bdi);
            if (!birthDate.isValid()) {
                errorLabel->setText(QStringLiteral("Невірна дата народження"));
                errorLabel->setVisible(true);
                return;
            }
        }

        QSqlQuery q(m_db);
        q.prepare(QStringLiteral(
            "INSERT INTO pets (client_id, name, pet_type_id, gender, birth_date) "
            "VALUES (:cid, :nm, :pt, :gd, :bd)"));
        q.bindValue(QStringLiteral(":cid"), clientId);
        q.bindValue(QStringLiteral(":nm"),  name);
        q.bindValue(QStringLiteral(":pt"),  petTypeId);
        q.bindValue(QStringLiteral(":gd"),  gender.isEmpty() ? QVariant() : QVariant(gender));
        q.bindValue(QStringLiteral(":bd"),  birthDate.isValid() ? QVariant(birthDate) : QVariant());

        if (!q.exec()) {
            errorLabel->setText(QStringLiteral("Помилка: ") + q.lastError().text());
            errorLabel->setVisible(true);
            return;
        }

        saved = true;
        dialog->accept();
    });

    dialog->exec();
    dialog->deleteLater();
    return saved;
}
