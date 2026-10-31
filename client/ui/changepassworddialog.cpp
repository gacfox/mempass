#include "changepassworddialog.h"
#include "ui_ChangePasswordDialog.h"

#include <QMessageBox>

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ChangePasswordDialog)
{
    ui->setupUi(this);
    setModal(true);
}

ChangePasswordDialog::~ChangePasswordDialog()
{
    delete ui;
}

QString ChangePasswordDialog::oldPassword() const
{
    return ui->editOldPassword->text();
}

QString ChangePasswordDialog::newPassword() const
{
    return ui->editNewPassword->text();
}

void ChangePasswordDialog::accept()
{
    if (ui->editOldPassword->text().isEmpty()
        || ui->editNewPassword->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("密码字段不能为空！"));
        return;
    }
    if (ui->editNewPassword->text() != ui->editConfirmPassword->text()) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("两次输入的新密码不一致！"));
        return;
    }
    QDialog::accept();
}
