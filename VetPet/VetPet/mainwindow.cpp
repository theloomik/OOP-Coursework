#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QHeaderView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    setupConnections();
    setupTable();
}

MainWindow::~MainWindow()
{
    delete ui;
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
    
    ui->clientsTable->setRowCount(3);
    
    ui->clientsTable->setItem(0, 0, new QTableWidgetItem("+380501112233"));
    ui->clientsTable->setItem(0, 1, new QTableWidgetItem("Бондаренко\nДмитро"));
    ui->clientsTable->setItem(0, 2, new QTableWidgetItem("→"));
    
    ui->clientsTable->setItem(1, 0, new QTableWidgetItem("+380951837573"));
    ui->clientsTable->setItem(1, 1, new QTableWidgetItem("Вальчук\nСвітлана"));
    ui->clientsTable->setItem(1, 2, new QTableWidgetItem("→"));
    
    ui->clientsTable->setItem(2, 0, new QTableWidgetItem("+380661256567"));
    ui->clientsTable->setItem(2, 1, new QTableWidgetItem("Гринь\nОксана"));
    ui->clientsTable->setItem(2, 2, new QTableWidgetItem("→"));
    
    ui->clientsTable->resizeRowsToContents();
}