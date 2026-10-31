#include "syncsettingsdialog.h"
#include "ui_SyncSettingsDialog.h"

#include "mempass/sync_service.h"

#include <QGuiApplication>
#include <QMessageBox>
#include <QSettings>

using namespace mempass;

SyncSettingsDialog::SyncSettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SyncSettingsDialog)
{
    ui->setupUi(this);
    setModal(true);

    QSettings settings;
    ui->editServerUrl->setText(settings.value(QStringLiteral("sync/serverUrl")).toString());
    ui->editAccessToken->setText(settings.value(QStringLiteral("sync/accessToken")).toString());
    ui->checkAutoSync->setChecked(settings.value(QStringLiteral("sync/auto"), false).toBool());

    connect(ui->btnSave, &QPushButton::clicked, this, &SyncSettingsDialog::save);
    connect(ui->btnTest, &QPushButton::clicked, this, &SyncSettingsDialog::testConnection);
}

SyncSettingsDialog::~SyncSettingsDialog()
{
    delete ui;
}

QString SyncSettingsDialog::serverUrl()
{
    QSettings settings;
    return settings.value(QStringLiteral("sync/serverUrl")).toString().trimmed();
}

QString SyncSettingsDialog::accessToken()
{
    QSettings settings;
    return settings.value(QStringLiteral("sync/accessToken")).toString().trimmed();
}

bool SyncSettingsDialog::autoSyncEnabled()
{
    QSettings settings;
    return settings.value(QStringLiteral("sync/auto"), false).toBool();
}

void SyncSettingsDialog::save()
{
    QSettings settings;
    settings.setValue(QStringLiteral("sync/serverUrl"), ui->editServerUrl->text().trimmed());
    settings.setValue(QStringLiteral("sync/accessToken"), ui->editAccessToken->text().trimmed());
    settings.setValue(QStringLiteral("sync/auto"), ui->checkAutoSync->isChecked());
    QMessageBox::information(this, QStringLiteral("同步设置"), QStringLiteral("同步设置已保存。"));
}

void SyncSettingsDialog::testConnection()
{
    const QString urlText = ui->editServerUrl->text().trimmed();
    const QString token = ui->editAccessToken->text().trimmed();
    if (urlText.isEmpty() || token.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("同步设置"), QStringLiteral("服务器地址和访问令牌不能为空！"));
        return;
    }

    VaultSyncState state;
    QString error;
    SyncService service;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = service.fetchState(QUrl(urlText), token, &state, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("连接失败"), error);
        return;
    }
    QMessageBox::information(this,
                             QStringLiteral("连接成功"),
                             QStringLiteral("连接成功，服务端保险库 ID：\n%1").arg(state.vaultId));
}
