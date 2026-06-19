#ifndef RECORDDETAILSVIEW_H
#define RECORDDETAILSVIEW_H

#include <QSqlDatabase>
#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

class RecordDetailsView : public QWidget
{
    Q_OBJECT

public:
    enum class Kind {
        Client,
        Staff,
        Provider
    };

    explicit RecordDetailsView(QWidget *parent = nullptr);

    void setDatabase(const QSqlDatabase &database);
    void showClient(int clientId);
    void showStaff(int staffId);
    void showProvider(int providerId);

signals:
    void backRequested(RecordDetailsView::Kind kind);

private slots:
    void onBackClicked();

private:
    QSqlDatabase m_db;
    Kind m_kind = Kind::Client;
    int m_recordId = 0;

    QVBoxLayout *m_rootLayout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QWidget *m_actionButtonsWidget = nullptr;
    QLabel *m_errorLabel = nullptr;
    QLabel *m_infoTitleLabel = nullptr;
    QLabel *m_firstTitleLabel = nullptr;
    QLabel *m_secondTitleLabel = nullptr;
    QLabel *m_thirdTitleLabel = nullptr;
    QTableWidget *m_infoTable = nullptr;
    QTableWidget *m_firstTable = nullptr;
    QTableWidget *m_secondTable = nullptr;
    QTableWidget *m_thirdTable = nullptr;
    QFrame *m_petsSectionFrame = nullptr;
    QVBoxLayout *m_petsCardsLayout = nullptr;

    void setupUi();
    void resetTables();
    void loadClient();
    void loadStaff();
    void loadProvider();
    void setInfoRows(const QList<QPair<QString, QString>> &rows);
    void prepareTable(QLabel *titleLabel, QTableWidget *table, const QString &title, const QStringList &headers);
    void addRow(QTableWidget *table, const QStringList &values);
    void addPetCard(const QString &name, const QString &species, const QString &breed,
                    const QString &gender, const QString &birthDate, const QString &lastVisit);
    QString scalarString(const QString &sql, const QVariantList &binds = {}) const;
};

#endif // RECORDDETAILSVIEW_H
