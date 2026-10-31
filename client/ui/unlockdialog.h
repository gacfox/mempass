#pragma once

#include "mempass/vault.h"

#include <QDialog>
#include <memory>

namespace Ui {
class UnlockDialog;
}

class UnlockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UnlockDialog(const QString &vaultPath, QWidget *parent = nullptr);
    ~UnlockDialog() override;

    mempass::Vault *takeVault();

private:
    void accept() override;

    Ui::UnlockDialog *ui;
    QString m_vaultPath;
    std::unique_ptr<mempass::Vault> m_vault;
};
