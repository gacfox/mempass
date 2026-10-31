#pragma once

#include "mempass/vault.h"

#include <QDialog>
#include <memory>

namespace Ui {
class CreateVaultDialog;
}

class CreateVaultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CreateVaultDialog(const QString &vaultPath, QWidget *parent = nullptr);
    ~CreateVaultDialog() override;

    mempass::Vault *takeVault();

private:
    void accept() override;

    Ui::CreateVaultDialog *ui;
    QString m_vaultPath;
    std::unique_ptr<mempass::Vault> m_vault;
};
