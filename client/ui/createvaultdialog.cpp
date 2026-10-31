#include "createvaultdialog.h"
#include "ui_CreateVaultDialog.h"

#include "mempass/sync_service.h"

#include <QGuiApplication>
#include <QMessageBox>
#include <QSettings>

using namespace mempass;

CreateVaultDialog::CreateVaultDialog(const QString &vaultPath, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CreateVaultDialog)
    , m_vaultPath(vaultPath)
    , m_vault(std::make_unique<Vault>())
{
    ui->setupUi(this);
    setModal(true);
    QSettings settings;
    ui->editRestoreUrl->setText(settings.value(QStringLiteral("sync/serverUrl")).toString());
    ui->editRestoreToken->setText(settings.value(QStringLiteral("sync/accessToken")).toString());
}

CreateVaultDialog::~CreateVaultDialog()
{
    delete ui;
}

Vault *CreateVaultDialog::takeVault()
{
    return m_vault.release();
}

void CreateVaultDialog::accept()
{
    QString error;

    if (ui->tabWidget->currentIndex() == 0) {
        const QString password = ui->editCreatePassword->text();
        if (password.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("初始化失败"), QStringLiteral("主密码不能为空！"));
            return;
        }
        if (password != ui->editCreateConfirm->text()) {
            QMessageBox::warning(this, QStringLiteral("初始化失败"), QStringLiteral("两次输入的主密码不一致！"));
            return;
        }

        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        const bool ok = m_vault->create(m_vaultPath, password, createId(), &error);
        QGuiApplication::restoreOverrideCursor();
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("初始化失败"), error);
            return;
        }
        QDialog::accept();
        return;
    }

    const QString urlText = ui->editRestoreUrl->text().trimmed();
    const QString token = ui->editRestoreToken->text().trimmed();
    const QString password = ui->editRestorePassword->text();
    if (urlText.isEmpty() || token.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("恢复失败"), QStringLiteral("服务器地址、访问令牌和主密码不能为空！"));
        return;
    }
    if (password != ui->editRestoreConfirm->text()) {
        QMessageBox::warning(this, QStringLiteral("恢复失败"), QStringLiteral("两次输入的主密码不一致！"));
        return;
    }

    VaultSyncState state;
    QList<SyncRecord> records;
    SyncService service;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const bool downloaded = service.downloadVault(QUrl(urlText), token, &state, &records, &error);
    const bool restored = downloaded
        && m_vault->restoreFromServer(m_vaultPath, password, createId(), state.keyEnvelope, records, &error);
    QGuiApplication::restoreOverrideCursor();
    if (!restored) {
        QMessageBox::warning(this, QStringLiteral("恢复失败"), error);
        return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("sync/serverUrl"), urlText);
    settings.setValue(QStringLiteral("sync/accessToken"), token);
    QDialog::accept();
}
