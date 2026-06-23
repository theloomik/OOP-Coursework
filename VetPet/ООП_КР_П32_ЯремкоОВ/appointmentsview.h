#ifndef APPOINTMENTSVIEW_H
#define APPOINTMENTSVIEW_H

#include <QSqlDatabase>
#include <QTimer>
#include <QWidget>
#include <QDateTime>

QT_BEGIN_NAMESPACE
namespace Ui {
class AppointmentsView;
}
QT_END_NAMESPACE

struct AppointmentRecord {
    int id = 0;
    int clientId = 0;
    QDateTime date;
    QString status;
    QString clientPhone;
    QString clientFirstName;
    QString clientLastName;
    QString petName;
};

class AppointmentsView : public QWidget
{
    Q_OBJECT

public:
    explicit AppointmentsView(QWidget *parent = nullptr);
    ~AppointmentsView() override;

    void setDatabase(const QSqlDatabase &database);
    void reload();

signals:
    void clientSelected(int clientId);

private slots:
    void onSearchTextChanged(const QString &text);
    void onSortClicked();
    void onRefreshTimer();
    void onRowClicked();

private:
    Ui::AppointmentsView *ui;
    QSqlDatabase m_db;
    QList<AppointmentRecord> m_allAppointments;
    bool m_sortAsc = true;
    QTimer m_refreshTimer;

    void setupConnections();
    void updateList();
    void rebuildList();
    QWidget *createGroupHeader(const QString &title, const QString &markerColor) const;
    QWidget *createAppointmentRow(const AppointmentRecord &record) const;
    static QString statusColor(const QString &status);
    static QString dateMarkerColor(const AppointmentRecord &record);
    enum class GroupKey { Today, Tomorrow, Future, Past, Cancelled };
    static GroupKey groupKeyFor(const AppointmentRecord &record);

protected:
    void resizeEvent(QResizeEvent *event) override;
};

#endif // APPOINTMENTSVIEW_H
