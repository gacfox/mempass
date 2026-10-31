#pragma once

#include <QDialog>

namespace Ui {
class ChangePasswordDialog;
}

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);
    ~ChangePasswordDialog() override;

    QString oldPassword() const;
    QString newPassword() const;

private:
    void accept() override;

    Ui::ChangePasswordDialog *ui;
};
