#include "mainwindow.h"
#include "appointmentsview.h"
#include "clientsview.h"
#include "providersview.h"
#include "quickappointmentview.h"
#include "recorddetailsview.h"
#include "staffdetailsview.h"
#include "staffview.h"
#include "stockview.h"
#include "./ui_mainwindow.h"

#include <QMessageBox>
#include <QSqlError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    if (!connectToDatabase()) {
        QMessageBox::critical(this, QStringLiteral("Помилка"),
                              QStringLiteral("Не вдалося підключитися до бази даних:\n") + db.lastError().text());
    }

    setupConnections();
    setupPages();
}

MainWindow::~MainWindow()
{
    db.close();
    delete ui;
}

bool MainWindow::connectToDatabase()
{
    db = QSqlDatabase::addDatabase(QStringLiteral("QODBC"));
    db.setDatabaseName(QStringLiteral(
        "DRIVER={MySQL ODBC 9.7 Unicode Driver};SERVER=localhost;PORT=3306;DATABASE=vetpet;UID=root;PWD=;"));
    return db.open();
}

void MainWindow::setupConnections()
{
    connect(ui->navClientsBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navRecordsBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navStaffBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navProvidersBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navStockBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->quickRecordBtn, &QPushButton::clicked, this, &MainWindow::openQuickAppointment);
}

void MainWindow::setupPages()
{
    m_clientsView = new ClientsView(ui->pageClients);
    ui->verticalLayout_3->addWidget(m_clientsView);
    m_clientsView->setDatabase(db);

    m_appointmentsView = new AppointmentsView(ui->pageRecords);
    ui->verticalLayout_4->addWidget(m_appointmentsView);
    m_appointmentsView->setDatabase(db);

    m_staffView = new StaffView(ui->pageStaff);
    ui->verticalLayout_5->addWidget(m_staffView);
    m_staffView->setDatabase(db);

    m_providersView = new ProvidersView(ui->pageProviders);
    ui->verticalLayout_6->addWidget(m_providersView);
    m_providersView->setDatabase(db);

    m_stockView = new StockView(ui->pageStock);
    ui->verticalLayout_7->addWidget(m_stockView);
    m_stockView->setDatabase(db);

    m_detailsView = new RecordDetailsView(this);
    m_detailsView->setDatabase(db);
    m_detailsPageIndex = ui->stackedWidget->addWidget(m_detailsView);

    m_quickView = new QuickAppointmentView(this);
    m_quickView->setDatabase(db);
    m_quickViewPageIndex = ui->stackedWidget->addWidget(m_quickView);

    m_staffDetailsView = new StaffDetailsView(this);
    m_staffDetailsView->setDatabase(db);
    m_staffDetailsPageIndex = ui->stackedWidget->addWidget(m_staffDetailsView);

    connect(m_clientsView, &ClientsView::clientSelected, this, &MainWindow::openClientDetails);
    connect(m_appointmentsView, &AppointmentsView::clientSelected, this, &MainWindow::openClientDetails);
    connect(m_staffView, &StaffView::staffSelected, this, &MainWindow::openStaffDetails);
    connect(m_providersView, &ProvidersView::providerSelected, this, &MainWindow::openProviderDetails);
    connect(m_stockView, &StockView::providerSelected, this, &MainWindow::openProviderDetails);
    connect(m_detailsView, &RecordDetailsView::backRequested, this, &MainWindow::returnFromDetails);
    connect(m_staffDetailsView, &StaffDetailsView::backRequested,
            this, &MainWindow::onStaffDetailsBack);
    connect(m_quickView, &QuickAppointmentView::cancelled,
            this, &MainWindow::onQuickAppointmentCancelled);
    connect(m_quickView, &QuickAppointmentView::appointmentSaved,
            this, &MainWindow::onQuickAppointmentSaved);
}

void MainWindow::switchPage()
{
    QObject *senderObj = sender();
    if (senderObj == ui->navClientsBtn) {
        setCurrentSection(0);
    } else if (senderObj == ui->navRecordsBtn) {
        setCurrentSection(1);
    } else if (senderObj == ui->navStaffBtn) {
        setCurrentSection(2);
    } else if (senderObj == ui->navProvidersBtn) {
        setCurrentSection(3);
    } else if (senderObj == ui->navStockBtn) {
        setCurrentSection(4);
    }
}

void MainWindow::openClientDetails(int clientId)
{
    if (!m_detailsView || clientId <= 0) {
        return;
    }

    m_detailsView->showClient(clientId);
    ui->stackedWidget->setCurrentIndex(m_detailsPageIndex);
}

void MainWindow::openStaffDetails(int staffId)
{
    if (!m_staffDetailsView || staffId <= 0)
        return;

    m_staffDetailsView->showStaff(staffId);
    ui->navClientsBtn->setChecked(false);
    ui->navRecordsBtn->setChecked(false);
    ui->navStaffBtn->setChecked(false);
    ui->navProvidersBtn->setChecked(false);
    ui->navStockBtn->setChecked(false);
    ui->stackedWidget->setCurrentIndex(m_staffDetailsPageIndex);
}

void MainWindow::openProviderDetails(int providerId)
{
    if (!m_detailsView || providerId <= 0) {
        return;
    }

    m_detailsView->showProvider(providerId);
    ui->stackedWidget->setCurrentIndex(m_detailsPageIndex);
}

void MainWindow::returnFromDetails(RecordDetailsView::Kind kind)
{
    switch (kind) {
    case RecordDetailsView::Kind::Client:
        setCurrentSection(0);
        break;
    case RecordDetailsView::Kind::Staff:
        setCurrentSection(2);
        break;
    case RecordDetailsView::Kind::Provider:
        setCurrentSection(3);
        break;
    }
}

void MainWindow::setCurrentSection(int index)
{
    ui->stackedWidget->setCurrentIndex(index);

    ui->navClientsBtn->setChecked(index == 0);
    ui->navRecordsBtn->setChecked(index == 1);
    ui->navStaffBtn->setChecked(index == 2);
    ui->navProvidersBtn->setChecked(index == 3);
    ui->navStockBtn->setChecked(index == 4);
}

void MainWindow::onStaffDetailsBack()
{
    setCurrentSection(2);
}

void MainWindow::openQuickAppointment()
{
    if (!m_quickView)
        return;

    m_quickView->reset();

    ui->navClientsBtn->setChecked(false);
    ui->navRecordsBtn->setChecked(false);
    ui->navStaffBtn->setChecked(false);
    ui->navProvidersBtn->setChecked(false);
    ui->navStockBtn->setChecked(false);

    ui->stackedWidget->setCurrentIndex(m_quickViewPageIndex);
}

void MainWindow::onQuickAppointmentCancelled()
{
    setCurrentSection(1);
}

void MainWindow::onQuickAppointmentSaved(int clientId)
{
    if (m_appointmentsView)
        m_appointmentsView->reload();

    if (clientId > 0 && m_detailsView) {
        m_detailsView->showClient(clientId);
        ui->navClientsBtn->setChecked(false);
        ui->navRecordsBtn->setChecked(false);
        ui->navStaffBtn->setChecked(false);
        ui->navProvidersBtn->setChecked(false);
        ui->navStockBtn->setChecked(false);
        ui->stackedWidget->setCurrentIndex(m_detailsPageIndex);
    } else {
        setCurrentSection(1);
    }
}
