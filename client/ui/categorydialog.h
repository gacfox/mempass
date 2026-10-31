#pragma once

#include <QDialog>

namespace Ui {
class CategoryDialog;
}

class CategoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CategoryDialog(QWidget *parent = nullptr);
    ~CategoryDialog() override;

    void setName(const QString &name);
    QString name() const;

private:
    Ui::CategoryDialog *ui;
};
