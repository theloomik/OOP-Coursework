#ifndef STAFFDETAILSVIEW_H
#define STAFFDETAILSVIEW_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QTime>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class StaffDetailsView;
}
QT_END_NAMESPACE

class StaffDetailsView : public QWidget
{
    Q_OBJECT

public:
    explicit StaffDetailsView(QWidget *parent = nullptr);
    ~StaffDetailsView() override;

    void setDatabase(const QSqlDatabase &database);
    void showStaff(int staffId);

signals:
    void backRequested();

private slots:
    void onBackClicked();
    void onEditClicked();
    void onArchiveClicked();
    void onCancelEditClicked();
    void onSaveEditClicked();

private:
    Ui::StaffDetailsView *ui;
    QSqlDatabase m_db;

    int m_staffId = 0;
    bool m_isArchived = false;

    QString m_firstName;
    QString m_lastName;
    QString m_phone;
    QString m_positionName;
    QString m_workDays;
    double m_salary = 0.0;
    int m_positionId = 0;
    QTime m_startTime;
    QTime m_endTime;
    QDateTime m_createdAt;

    struct PositionEntry {
        int id = 0;
        QString position;
        double salary = 0.0;
    };
    QList<PositionEntry> m_positions;

    void loadPositions();
    void refreshDisplay();
    void startEditMode();
    void stopEditMode();
    void setLoadError(const QString &message);
    void setEditError(const QString &message);
    void clearEditError();

    static QString formatWorkDays(const QString &raw);
    static QString formatTime(const QTime &t);
    static QString formatSalaryNumber(double salary);
};

#endif // STAFFDETAILSVIEW_H
