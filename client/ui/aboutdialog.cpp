#include "aboutdialog.h"
#include "ui_AboutDialog.h"

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    setModal(true);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
