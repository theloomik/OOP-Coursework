#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

#include "recorddetailsview.h"

class AppointmentsView;
class ClientsView;
class ProvidersView;
class StaffView;
class StockView;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void switchPage();
    void openClientDetails(int clientId);
    void openStaffDetails(int staffId);
    void openProviderDetails(int providerId);
    void returnFromDetails(RecordDetailsView::Kind kind);

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    ClientsView *m_clientsView = nullptr;
    AppointmentsView *m_appointmentsView = nullptr;
    StaffView *m_staffView = nullptr;
    ProvidersView *m_providersView = nullptr;
    StockView *m_stockView = nullptr;
    RecordDetailsView *m_detailsView = nullptr;
    int m_detailsPageIndex = -1;

    void setupConnections();
    void setupPages();
    bool connectToDatabase();
    void setCurrentSection(int index);
};

#endif // MAINWINDOW_H
