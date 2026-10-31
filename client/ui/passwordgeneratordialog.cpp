#include "passwordgeneratordialog.h"
#include "ui_PasswordGeneratorDialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QList>

#include <sodium.h>

PasswordGeneratorDialog::PasswordGeneratorDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PasswordGeneratorDialog)
{
    ui->setupUi(this);
    setModal(true);

    ui->comboCharSpace->addItem(QStringLiteral("数字字母和符号"), 0);
    ui->comboCharSpace->addItem(QStringLiteral("数字和字母"), 1);
    ui->comboCharSpace->addItem(QStringLiteral("数字和小写字母"), 2);
    ui->comboCharSpace->addItem(QStringLiteral("仅数字"), 3);

    connect(ui->btnGenerate, &QPushButton::clicked, this, &PasswordGeneratorDialog::generate);
    connect(ui->btnCopy, &QPushButton::clicked, this, [this] {
        if (!ui->editResult->text().isEmpty()) {
            QGuiApplication::clipboard()->setText(ui->editResult->text());
        }
    });
    generate();
}

PasswordGeneratorDialog::~PasswordGeneratorDialog()
{
    delete ui;
}

void PasswordGeneratorDialog::generate()
{
    ui->editResult->setText(generatePassword(ui->comboCharSpace->currentData().toInt(),
                                             ui->spinLength->value()));
}

QString PasswordGeneratorDialog::generatePassword(int charSpace, int length)
{
    const QString numbers = QStringLiteral("0123456789");
    const QString lower = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    const QString upper = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const QString symbols = QStringLiteral("!@#$%^&*");

    QList<QString> requiredGroups;
    QString alphabet;
    switch (charSpace) {
    case 0:
        requiredGroups = {numbers, lower, upper, symbols};
        alphabet = numbers + lower + upper + symbols;
        break;
    case 1:
        requiredGroups = {numbers, lower, upper};
        alphabet = numbers + lower + upper;
        break;
    case 2:
        requiredGroups = {numbers, lower};
        alphabet = numbers + lower;
        break;
    case 3:
        requiredGroups = {numbers};
        alphabet = numbers;
        break;
    default:
        requiredGroups = {numbers, lower, upper, symbols};
        alphabet = numbers + lower + upper + symbols;
        break;
    }

    length = qMax(length, requiredGroups.size());

    QString result;
    result.reserve(length);
    for (const QString &group : requiredGroups) {
        const uint32_t index = randombytes_uniform(static_cast<uint32_t>(group.size()));
        result.append(group.at(static_cast<int>(index)));
    }
    for (int i = requiredGroups.size(); i < length; ++i) {
        const uint32_t index = randombytes_uniform(static_cast<uint32_t>(alphabet.size()));
        result.append(alphabet.at(static_cast<int>(index)));
    }

    for (int i = result.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(randombytes_uniform(static_cast<uint32_t>(i + 1)));
        const QChar character = result.at(i);
        result[i] = result.at(j);
        result[j] = character;
    }

    return result;
}
