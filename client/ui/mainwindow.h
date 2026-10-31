#pragma once

#include "mempass/models.h"

#include <QMainWindow>

class QLabel;
class QThread;

namespace mempass {
class SyncService;
class Vault;
}

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(mempass::Vault *vault, QWidget *parent = nullptr);
    ~MainWindow() override;

    void setVault(mempass::Vault *vault);

signals:
    void lockRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshTree();
    void search();
    void clearSearch();
    void newCategory();
    void newAccount();
    void editSelected();
    void deleteSelected();
    void showContextMenu(const QPoint &position);
    void selectionChanged();
    void exportEncryptedBackup();
    void exportCsv();
    void changePassword();
    void showPasswordGenerator();
    void showSyncSettings();
    void startSync();
    void handleSyncFinished(bool ok, const QString &error, const QByteArray &resultJson);
    void showAbout();
    void lock();

private:
    QString selectedNodeType() const;
    QString selectedNodeId() const;
    QString selectedCategoryId() const;
    mempass::Account selectedAccount() const;
    void loadAccountDetails(const mempass::Account &account);
    void setupPasswordCell();
    void clearPasswordCell();
    void copyCurrentPassword();
    void clearDetails();
    void configureAutoSync();
    void setSyncUiEnabled(bool enabled);
    static QString csvEscape(const QString &value);

    Ui::MainWindow *ui;
    mempass::Vault *m_vault = nullptr;
    QThread *m_syncThread = nullptr;
    mempass::SyncService *m_syncService = nullptr;
    class QTimer *m_syncTimer = nullptr;
    bool m_syncing = false;
    QString m_currentAccountId;
    QString m_currentPassword;
    QLabel *m_passwordValueLabel = nullptr;
    int m_passwordRow = -1;
};
