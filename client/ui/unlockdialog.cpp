#include "unlockdialog.h"
#include "ui_UnlockDialog.h"

#include <QMessageBox>

using namespace mempass;

UnlockDialog::UnlockDialog(const QString &vaultPath, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UnlockDialog)
    , m_vaultPath(vaultPath)
    , m_vault(std::make_unique<Vault>())
{
    ui->setupUi(this);
    setModal(true);
    ui->editPassword->setFocus();
}

UnlockDialog::~UnlockDialog()
{
    delete ui;
}

Vault *UnlockDialog::takeVault()
{
    return m_vault.release();
}

void UnlockDialog::accept()
{
    QString error;
    if (!m_vault->open(m_vaultPath, ui->editPassword->text(), &error)) {
        QMessageBox::warning(this, QStringLiteral("登录失败"), error);
        ui->editPassword->clear();
        ui->editPassword->setFocus();
        return;
    }
    QDialog::accept();
}
