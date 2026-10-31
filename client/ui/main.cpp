#include "createvaultdialog.h"
#include "mainwindow.h"
#include "unlockdialog.h"

#include "mempass/crypto.h"
#include "mempass/vault.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>
#include <memory>

int main(int argc, char *argv[])
{
    QApplication::setStyle(QStringLiteral("fusion"));
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("gacfox"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("gacfox.example"));
    QCoreApplication::setApplicationName(QStringLiteral("mempass"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));

    QTranslator qtTranslator;
    const QStringList translationDirectories = {
        QCoreApplication::applicationDirPath(),
        QLibraryInfo::path(QLibraryInfo::TranslationsPath),
    };
    for (const QString &directory : translationDirectories) {
        if (qtTranslator.load(QLocale::system(),
                              QStringLiteral("qt"),
                              QStringLiteral("_"),
                              directory)) {
            app.installTranslator(&qtTranslator);
            break;
        }
    }

    if (!mempass::initCrypto()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("启动失败"),
                              QStringLiteral("libsodium 初始化失败。"));
        return 1;
    }

    const QString vaultPath = mempass::Vault::defaultVaultPath();
    std::unique_ptr<mempass::Vault> vault;

    if (QFile::exists(vaultPath)) {
        UnlockDialog unlockDialog(vaultPath);
        if (unlockDialog.exec() != QDialog::Accepted) {
            return 0;
        }
        vault.reset(unlockDialog.takeVault());
    } else {
        CreateVaultDialog createDialog(vaultPath);
        if (createDialog.exec() != QDialog::Accepted) {
            return 0;
        }
        vault.reset(createDialog.takeVault());
    }

    MainWindow mainWindow(vault.get());
    QObject::connect(&mainWindow, &MainWindow::lockRequested, &mainWindow, [&] {
        mainWindow.hide();
        UnlockDialog unlockDialog(vaultPath);
        if (unlockDialog.exec() != QDialog::Accepted) {
            QApplication::quit();
            return;
        }
        vault.reset(unlockDialog.takeVault());
        mainWindow.setVault(vault.get());
        mainWindow.show();
    });
    mainWindow.show();
    return QApplication::exec();
}
