#pragma once

#include <QDialog>
#include <QString>

namespace Ui {
class PasswordGeneratorDialog;
}

class PasswordGeneratorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordGeneratorDialog(QWidget *parent = nullptr);
    ~PasswordGeneratorDialog() override;

    static QString generatePassword(int charSpace, int length);

private:
    void generate();

    Ui::PasswordGeneratorDialog *ui;
};
