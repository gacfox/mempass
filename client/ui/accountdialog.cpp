#include "accountdialog.h"
#include "ui_AccountDialog.h"

#include <QMessageBox>

using namespace mempass;

AccountDialog::AccountDialog(Vault *vault,
                             const QString &categoryId,
                             const QString &accountId,
                             QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AccountDialog)
{
    ui->setupUi(this);
    setModal(true);

    const QStringList statusTexts = {
        QStringLiteral("正常"),
        QStringLiteral("不可用"),
        QStringLiteral("废弃"),
        QStringLiteral("已注销"),
    };
    const QList<int> statuses = {
        static_cast<int>(AccountStatus::Available),
        static_cast<int>(AccountStatus::Unavailable),
        static_cast<int>(AccountStatus::Deprecated),
        static_cast<int>(AccountStatus::Cancelled),
    };
    for (int i = 0; i < statusTexts.size(); ++i) {
        ui->comboStatus->addItem(statusTexts.at(i), statuses.at(i));
        m_statusValues.append(statuses.at(i));
    }

    for (const Category &category : vault->data().categories) {
        if (category.deleted) {
            continue;
        }
        ui->comboCategory->addItem(category.name, category.id);
        m_categoryIds.append(category.id);
    }

    if (!accountId.isEmpty()) {
        const Account *account = vault->findAccount(accountId);
        if (account != nullptr) {
            m_account = *account;
            const int categoryIndex = ui->comboCategory->findData(account->categoryId);
            if (categoryIndex >= 0) {
                ui->comboCategory->setCurrentIndex(categoryIndex);
            }
            ui->editItemName->setText(account->itemName);
            ui->editUsername->setText(account->username);
            ui->editPassword->setText(account->password);
            ui->editConfirmPassword->setText(account->password);
            ui->editDescription->setText(account->description);
            ui->editNote->setPlainText(account->note);
            const int statusIndex = m_statusValues.indexOf(account->status);
            if (statusIndex >= 0) {
                ui->comboStatus->setCurrentIndex(statusIndex);
            }
        }
    } else if (!categoryId.isEmpty()) {
        const int categoryIndex = ui->comboCategory->findData(categoryId);
        if (categoryIndex >= 0) {
            ui->comboCategory->setCurrentIndex(categoryIndex);
        }
    }
}

AccountDialog::~AccountDialog()
{
    delete ui;
}

Account AccountDialog::account() const
{
    return m_account;
}

void AccountDialog::accept()
{
    if (ui->comboCategory->currentIndex() < 0) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请选择所属分类！"));
        return;
    }
    if (ui->editItemName->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("账户项不能为空！"));
        return;
    }
    if (ui->editPassword->text() != ui->editConfirmPassword->text()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("两次输入的密码不一致！"));
        return;
    }

    m_account.categoryId = ui->comboCategory->currentData().toString();
    m_account.itemName = ui->editItemName->text();
    m_account.username = ui->editUsername->text();
    m_account.password = ui->editPassword->text();
    m_account.description = ui->editDescription->text();
    m_account.note = ui->editNote->toPlainText();
    m_account.status = ui->comboStatus->currentData().toInt();
    QDialog::accept();
}
