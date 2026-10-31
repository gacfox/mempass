#pragma once

#include "mempass/vault.h"

#include <QDialog>

namespace Ui {
class AccountDialog;
}

class AccountDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AccountDialog(mempass::Vault *vault,
                           const QString &categoryId = {},
                           const QString &accountId = {},
                           QWidget *parent = nullptr);
    ~AccountDialog() override;

    mempass::Account account() const;

private:
    void accept() override;

    Ui::AccountDialog *ui;
    mempass::Account m_account;
    QList<QString> m_categoryIds;
    QList<int> m_statusValues;
};
