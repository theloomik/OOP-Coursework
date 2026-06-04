#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QHeaderView>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    if (!connectToDatabase()) {
        QMessageBox::critical(this, "Помилка", "Не вдалося підключитися до бази даних:\n" + db.lastError().text());
    }

    setupConnections();
    setupTable();
}

MainWindow::~MainWindow()
{
    db.close();
    delete ui;
}

bool MainWindow::connectToDatabase()
{
    db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName("DRIVER={MySQL ODBC 9.7 Unicode Driver};SERVER=localhost;PORT=3306;DATABASE=vetpet;UID=root;PWD=;");
    return db.open();
}

void MainWindow::setupConnections()
{
    connect(ui->navClientsBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navRecordsBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navStaffBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navProvidersBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
    connect(ui->navStockBtn, &QPushButton::clicked, this, &MainWindow::switchPage);
}

void MainWindow::switchPage()
{
    QObject* senderObj = sender();
    if (senderObj == ui->navClientsBtn) {
        ui->stackedWidget->setCurrentIndex(0);
    } else if (senderObj == ui->navRecordsBtn) {
        ui->stackedWidget->setCurrentIndex(1);
    } else if (senderObj == ui->navStaffBtn) {
        ui->stackedWidget->setCurrentIndex(2);
    } else if (senderObj == ui->navProvidersBtn) {
        ui->stackedWidget->setCurrentIndex(3);
    } else if (senderObj == ui->navStockBtn) {
        ui->stackedWidget->setCurrentIndex(4);
    }
}

void MainWindow::setupTable()
{
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->clientsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    
    ui->clientsTable->setShowGrid(false);
    ui->clientsTable->setFocusPolicy(Qt::NoFocus);
    
    QSqlQuery query("SELECT phone, first_name, last_name FROM clients ORDER BY last_name ASC");
    int row = 0;
    ui->clientsTable->setRowCount(0);

    while (query.next()) {
        ui->clientsTable->insertRow(row);
        
        QString phone = query.value(0).toString();
        QString fullName = query.value(2).toString() + "\n" + query.value(1).toString();
        
        ui->clientsTable->setItem(row, 0, new QTableWidgetItem(phone));
        ui->clientsTable->setItem(row, 1, new QTableWidgetItem(fullName));
        ui->clientsTable->setItem(row, 2, new QTableWidgetItem("→"));
        
        row++;
    }
    
    ui->clientsTable->resizeRowsToContents();
}