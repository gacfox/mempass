#include "categorydialog.h"
#include "ui_CategoryDialog.h"

CategoryDialog::CategoryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CategoryDialog)
{
    ui->setupUi(this);
    setModal(true);
}

CategoryDialog::~CategoryDialog()
{
    delete ui;
}

void CategoryDialog::setName(const QString &name)
{
    ui->editName->setText(name);
}

QString CategoryDialog::name() const
{
    return ui->editName->text().trimmed();
}
