#pragma once

#include <QDialog>

namespace Ui {
class SyncSettingsDialog;
}

class SyncSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncSettingsDialog(QWidget *parent = nullptr);
    ~SyncSettingsDialog() override;

    static QString serverUrl();
    static QString accessToken();
    static bool autoSyncEnabled();

private:
    void save();
    void testConnection();

    Ui::SyncSettingsDialog *ui;
};
