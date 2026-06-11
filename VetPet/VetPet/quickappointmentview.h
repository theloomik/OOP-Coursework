#ifndef QUICKAPPOINTMENTVIEW_H
#define QUICKAPPOINTMENTVIEW_H

#include <QSqlDatabase>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class QuickAppointmentView;
}
QT_END_NAMESPACE

class QuickAppointmentView : public QWidget
{
    Q_OBJECT

public:
    explicit QuickAppointmentView(QWidget *parent = nullptr);
    ~QuickAppointmentView() override;

    void setDatabase(const QSqlDatabase &database);
    void reset();

signals:
    void cancelled();
    void appointmentSaved(int clientId);

private slots:
    void onPhoneTextChanged(const QString &text);
    void onClientIndexChanged(int index);
    void onAddClientClicked();
    void onAddPetClicked();
    void onCancelClicked();
    void onSaveClicked();

private:
    Ui::QuickAppointmentView *ui;
    QSqlDatabase m_db;

    struct IdEntry {
        int id = 0;
        QString display;
    };

    QList<IdEntry> m_clients;
    QList<IdEntry> m_pets;
    QList<IdEntry> m_staff;
    QList<IdEntry> m_services;
    QList<IdEntry> m_petTypes;

    void loadDropdowns();
    void searchClients(const QString &phone);
    void loadPetsForClient(int clientId);
    void seedDateTimeCombos();
    void setError(const QString &message);
    void clearError();

    int currentClientId() const;
    int currentPetId() const;
    int currentStaffId() const;
    int currentServiceId() const;

    bool execAddClientDialog();
    bool execAddPetDialog(int clientId);
};

#endif // QUICKAPPOINTMENTVIEW_H
