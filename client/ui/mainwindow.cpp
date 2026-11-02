#include "mainwindow.h"
#include "ui_MainWindow.h"

#include "accountdialog.h"
#include "aboutdialog.h"
#include "categorydialog.h"
#include "changepassworddialog.h"
#include "passwordgeneratordialog.h"
#include "syncsettingsdialog.h"

#include "mempass/sync_protocol.h"
#include "mempass/sync_service.h"
#include "mempass/vault.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidgetItem>
#include <QUrl>

#include <algorithm>

using namespace mempass;

namespace {

constexpr int NodeTypeRole = Qt::UserRole;
constexpr int NodeIdRole = Qt::UserRole + 1;

bool accountMatches(const Account &account, const QString &pattern)
{
    if (pattern.isEmpty()) {
        return true;
    }
    return account.itemName.contains(pattern, Qt::CaseInsensitive)
        || account.username.contains(pattern, Qt::CaseInsensitive)
        || account.description.contains(pattern, Qt::CaseInsensitive)
        || account.note.contains(pattern, Qt::CaseInsensitive);
}

QColor statusColor(int status)
{
    switch (static_cast<AccountStatus>(status)) {
    case AccountStatus::Available:
        return QColor(Qt::black);
    case AccountStatus::Unavailable:
        return QColor(240, 173, 78);
    case AccountStatus::Deprecated:
        return QColor(160, 160, 160);
    case AccountStatus::Cancelled:
        return QColor(220, 45, 45);
    }
    return QColor(Qt::black);
}

QString statusText(int status)
{
    switch (static_cast<AccountStatus>(status)) {
    case AccountStatus::Available:
        return QStringLiteral("正常");
    case AccountStatus::Unavailable:
        return QStringLiteral("不可用");
    case AccountStatus::Deprecated:
        return QStringLiteral("废弃");
    case AccountStatus::Cancelled:
        return QStringLiteral("已注销");
    }
    return QStringLiteral("正常");
}

} // namespace

MainWindow::MainWindow(Vault *vault, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_vault(vault)
{
    ui->setupUi(this);
    ui->splitter->setSizes({300, 580});
    ui->treeCategories->expandAll();
    ui->tableDetails->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableDetails->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    connect(ui->actionNewCategory, &QAction::triggered, this, &MainWindow::newCategory);
    connect(ui->actionNewAccount, &QAction::triggered, this, &MainWindow::newAccount);
    connect(ui->actionDelete, &QAction::triggered, this, &MainWindow::deleteSelected);
    connect(ui->actionRandomPassword, &QAction::triggered, this, &MainWindow::showPasswordGenerator);
    connect(ui->actionSyncNow, &QAction::triggered, this, &MainWindow::startSync);
    connect(ui->actionSyncSettings, &QAction::triggered, this, &MainWindow::showSyncSettings);
    connect(ui->actionExportBackup, &QAction::triggered, this, &MainWindow::exportEncryptedBackup);
    connect(ui->actionExportCsv, &QAction::triggered, this, &MainWindow::exportCsv);
    connect(ui->actionChangePassword, &QAction::triggered, this, &MainWindow::changePassword);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionLock, &QAction::triggered, this, &MainWindow::lock);

    connect(ui->btnSearch, &QPushButton::clicked, this, &MainWindow::search);
    connect(ui->btnClear, &QPushButton::clicked, this, &MainWindow::clearSearch);
    connect(ui->lineSearch, &QLineEdit::returnPressed, this, &MainWindow::search);
    connect(ui->treeCategories, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);
    connect(ui->treeCategories, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::selectionChanged);
    connect(ui->treeCategories, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *, int) { editSelected(); });

    connect(ui->tableDetails, &QTableWidget::itemSelectionChanged,
            this, [this] {
                if (m_passwordRow < 0 || m_passwordValueLabel == nullptr) {
                    return;
                }
                const QList<QTableWidgetItem *> selectedItems = ui->tableDetails->selectedItems();
                const bool passwordRowSelected = std::any_of(
                    selectedItems.begin(),
                    selectedItems.end(),
                    [this](const QTableWidgetItem *item) {
                        return item->row() == m_passwordRow;
                    });
                m_passwordValueLabel->setText(passwordRowSelected
                                                  ? m_currentPassword
                                                  : QStringLiteral("••••••••"));
            });

    m_syncThread = new QThread(this);
    m_syncService = new SyncService;
    m_syncService->moveToThread(m_syncThread);
    connect(m_syncService, &SyncService::finished,
            this, &MainWindow::handleSyncFinished);
    m_syncThread->start();

    m_syncTimer = new QTimer(this);
    m_syncTimer->setInterval(5 * 60 * 1000);
    connect(m_syncTimer, &QTimer::timeout, this, &MainWindow::startSync);
    configureAutoSync();

    refreshTree();
    statusBar()->showMessage(QStringLiteral("本地数据已就绪"));
}

MainWindow::~MainWindow()
{
    if (m_syncThread != nullptr) {
        m_syncThread->quit();
        m_syncThread->wait(3000);
    }
    delete m_syncService;
    delete ui;
}

void MainWindow::setVault(Vault *vault)
{
    m_vault = vault;
    clearDetails();
    ui->lineSearch->clear();
    refreshTree();
    configureAutoSync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_syncing) {
        QMessageBox::information(this,
                                 QStringLiteral("同步进行中"),
                                 QStringLiteral("同步正在进行，请稍后关闭程序。"));
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::refreshTree()
{
    ui->treeCategories->clear();
    clearDetails();
    if (m_vault == nullptr || !m_vault->isOpen()) {
        return;
    }

    const QString pattern = ui->lineSearch->text().trimmed();
    QTreeWidgetItem *root = new QTreeWidgetItem(ui->treeCategories,
                                                {QStringLiteral("账号目录")});
    root->setData(0, NodeTypeRole, QStringLiteral("root"));

    QList<const Category *> categories;
    for (const Category &category : m_vault->data().categories) {
        if (!category.deleted) {
            categories.append(&category);
        }
    }
    std::sort(categories.begin(), categories.end(), [](const Category *left, const Category *right) {
        if (left->isDefault != right->isDefault) {
            return left->isDefault;
        }
        return QString::compare(left->name, right->name, Qt::CaseInsensitive) < 0;
    });

    for (const Category *category : categories) {
        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(root, {category->name});
        categoryItem->setData(0, NodeTypeRole, QStringLiteral("category"));
        categoryItem->setData(0, NodeIdRole, category->id);

        QList<const Account *> accounts;
        for (const Account &account : m_vault->data().accounts) {
            if (!account.deleted && account.categoryId == category->id
                && accountMatches(account, pattern)) {
                accounts.append(&account);
            }
        }
        std::sort(accounts.begin(), accounts.end(), [](const Account *left, const Account *right) {
            return QString::compare(left->itemName, right->itemName, Qt::CaseInsensitive) < 0;
        });
        for (const Account *account : accounts) {
            QTreeWidgetItem *accountItem = new QTreeWidgetItem(categoryItem, {account->itemName});
            accountItem->setData(0, NodeTypeRole, QStringLiteral("account"));
            accountItem->setData(0, NodeIdRole, account->id);
            accountItem->setForeground(0, statusColor(account->status));
        }
        categoryItem->setExpanded(true);
    }
    ui->treeCategories->expandItem(root);
}

void MainWindow::search()
{
    refreshTree();
}

void MainWindow::clearSearch()
{
    ui->lineSearch->clear();
    refreshTree();
}

void MainWindow::newCategory()
{
    if (m_syncing) {
        return;
    }
    CategoryDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QString error;
    if (!m_vault->addCategory(dialog.name(), &error)) {
        QMessageBox::warning(this, QStringLiteral("新建类别失败"), error);
        return;
    }
    refreshTree();
}

void MainWindow::newAccount()
{
    if (m_syncing) {
        return;
    }
    AccountDialog dialog(m_vault, selectedCategoryId(), {}, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QString error;
    if (!m_vault->addAccount(dialog.account(), &error)) {
        QMessageBox::warning(this, QStringLiteral("新建账户失败"), error);
        return;
    }
    refreshTree();
}

void MainWindow::editSelected()
{
    if (m_syncing) {
        return;
    }
    const QString type = selectedNodeType();
    const QString id = selectedNodeId();
    if (type == QLatin1String("category")) {
        const Category *category = m_vault->findCategory(id);
        if (category == nullptr) {
            return;
        }
        CategoryDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("修改类别"));
        dialog.setName(category->name);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        QString error;
        if (!m_vault->updateCategory(id, dialog.name(), &error)) {
            QMessageBox::warning(this, QStringLiteral("修改类别失败"), error);
            return;
        }
        refreshTree();
        return;
    }
    if (type == QLatin1String("account")) {
        AccountDialog dialog(m_vault, {}, id, this);
        dialog.setWindowTitle(QStringLiteral("修改账户"));
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        QString error;
        if (!m_vault->updateAccount(dialog.account(), &error)) {
            QMessageBox::warning(this, QStringLiteral("修改账户失败"), error);
            return;
        }
        refreshTree();
    }
}

void MainWindow::deleteSelected()
{
    if (m_syncing) {
        return;
    }
    const QString type = selectedNodeType();
    const QString id = selectedNodeId();
    if (type.isEmpty() || type == QLatin1String("root")) {
        return;
    }

    QString message;
    if (type == QLatin1String("category")) {
        message = QStringLiteral("你真的要删除该分类吗？其中的账户会移动到默认分类。");
    } else {
        message = QStringLiteral("你真的要彻底删除该账户信息吗？删除会同步到所有设备。");
    }
    const auto choice = QMessageBox::question(this, QStringLiteral("确认删除"),
                                              message,
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }

    QString error;
    const bool deleted = type == QLatin1String("category")
        ? m_vault->deleteCategory(id, &error)
        : m_vault->deleteAccount(id, &error);
    if (!deleted) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), error);
        return;
    }
    refreshTree();
}

void MainWindow::showContextMenu(const QPoint &position)
{
    QTreeWidgetItem *item = ui->treeCategories->itemAt(position);
    if (item == nullptr) {
        return;
    }
    const QString type = item->data(0, NodeTypeRole).toString();
    QMenu menu(this);
    if (type == QLatin1String("category")) {
        menu.addAction(QStringLiteral("新建类别"), this, &MainWindow::newCategory);
        menu.addAction(QStringLiteral("新建账户"), this, &MainWindow::newAccount);
        menu.addAction(QStringLiteral("修改类别"), this, &MainWindow::editSelected);
        menu.addAction(QStringLiteral("删除类别"), this, &MainWindow::deleteSelected);
    } else if (type == QLatin1String("account")) {
        menu.addAction(QStringLiteral("新建账户"), this, &MainWindow::newAccount);
        menu.addAction(QStringLiteral("修改账户"), this, &MainWindow::editSelected);
        menu.addAction(QStringLiteral("删除账户"), this, &MainWindow::deleteSelected);
    } else {
        menu.addAction(QStringLiteral("新建类别"), this, &MainWindow::newCategory);
        menu.addAction(QStringLiteral("新建账户"), this, &MainWindow::newAccount);
    }
    menu.exec(ui->treeCategories->viewport()->mapToGlobal(position));
}

void MainWindow::selectionChanged()
{
    ui->actionDelete->setEnabled(!m_syncing && selectedNodeType() != QLatin1String("root")
                                 && !selectedNodeType().isEmpty());
    const QString type = selectedNodeType();
    if (type == QLatin1String("account")) {
        loadAccountDetails(selectedAccount());
    } else {
        clearDetails();
    }
}

QString MainWindow::selectedNodeType() const
{
    QTreeWidgetItem *item = ui->treeCategories->currentItem();
    return item == nullptr ? QString() : item->data(0, NodeTypeRole).toString();
}

QString MainWindow::selectedNodeId() const
{
    QTreeWidgetItem *item = ui->treeCategories->currentItem();
    return item == nullptr ? QString() : item->data(0, NodeIdRole).toString();
}

QString MainWindow::selectedCategoryId() const
{
    const QString type = selectedNodeType();
    const QString id = selectedNodeId();
    if (type == QLatin1String("category")) {
        return id;
    }
    if (type == QLatin1String("account")) {
        const Account *account = m_vault->findAccount(id);
        return account == nullptr ? QString() : account->categoryId;
    }
    for (const Category &category : m_vault->data().categories) {
        if (!category.deleted && category.isDefault) {
            return category.id;
        }
    }
    return {};
}

Account MainWindow::selectedAccount() const
{
    const Account *account = m_vault->findAccount(selectedNodeId());
    return account == nullptr ? Account() : *account;
}

void MainWindow::loadAccountDetails(const Account &account)
{
    if (account.id.isEmpty()) {
        clearDetails();
        return;
    }
    clearPasswordCell();
    clearCopyCells();

    m_currentAccountId = account.id;
    m_currentPassword = account.password;
    m_passwordRow = 2;

    const QStringList keys = {
        QStringLiteral("账户项"),
        QStringLiteral("账户名"),
        QStringLiteral("密码"),
        QStringLiteral("描述"),
        QStringLiteral("备注"),
        QStringLiteral("创建时间"),
        QStringLiteral("最后修改时间"),
        QStringLiteral("可用状态"),
    };
    const QStringList values = {
        account.itemName,
        QString(),
        QString(),
        QString(),
        QString(),
        formatTimestamp(account.createdAtMs),
        formatTimestamp(account.updatedAtMs),
        statusText(account.status),
    };

    ui->tableDetails->clearContents();
    ui->tableDetails->setRowCount(keys.size());
    for (int row = 0; row < keys.size(); ++row) {
        QTableWidgetItem *keyItem = new QTableWidgetItem(keys.at(row));
        QTableWidgetItem *valueItem = new QTableWidgetItem(values.at(row));
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        ui->tableDetails->setItem(row, 0, keyItem);
        ui->tableDetails->setItem(row, 1, valueItem);
    }
    setupPasswordCell();
    setupCopyCells(account);
    ui->tableDetails->clearSelection();
}

void MainWindow::clearPasswordCell()
{
    if (m_passwordRow < 0) {
        return;
    }

    if (QWidget *cellWidget = ui->tableDetails->cellWidget(m_passwordRow, 1)) {
        ui->tableDetails->removeCellWidget(m_passwordRow, 1);
        delete cellWidget;
    }
    m_passwordValueLabel = nullptr;
}

void MainWindow::setupPasswordCell()
{
    if (m_passwordRow < 0) {
        return;
    }

    QToolButton *copyButton = nullptr;
    QWidget *container = buildValueCell(QStringLiteral("••••••••"),
                                        QStringLiteral("passwordValueLabel"),
                                        &m_passwordValueLabel,
                                        &copyButton);
    connect(copyButton, &QToolButton::clicked, this, &MainWindow::copyCurrentPassword);

    ui->tableDetails->setCellWidget(m_passwordRow, 1, container);
    ui->tableDetails->resizeRowToContents(m_passwordRow);
}

QWidget *MainWindow::buildValueCell(const QString &text,
                                    const QString &labelObjectName,
                                    QLabel **labelOut,
                                    QToolButton **buttonOut)
{
    QWidget *container = new QWidget(ui->tableDetails);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);

    QLabel *valueLabel = new QLabel(text, container);
    valueLabel->setObjectName(labelObjectName);
    valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QToolButton *copyButton = new QToolButton(container);
    copyButton->setText(QStringLiteral("复制"));
    copyButton->setAutoRaise(true);

    layout->addWidget(valueLabel, 1);
    layout->addWidget(copyButton, 0);

    if (labelOut != nullptr) {
        *labelOut = valueLabel;
    }
    if (buttonOut != nullptr) {
        *buttonOut = copyButton;
    }
    return container;
}

void MainWindow::setupCopyCells(const mempass::Account &account)
{
    struct CellSpec {
        int row;
        QString text;
        QString objectName;
        QString statusMessage;
    };

    const CellSpec cells[] = {
        {kUsernameRow,
         account.username,
         QStringLiteral("copyUsernameLabel"),
         QStringLiteral("账户名已复制到剪贴板")},
        {kDescriptionRow,
         account.description,
         QStringLiteral("copyDescriptionLabel"),
         QStringLiteral("描述已复制到剪贴板")},
        {kNoteRow,
         account.note,
         QStringLiteral("copyNoteLabel"),
         QStringLiteral("备注已复制到剪贴板")},
    };

    for (const CellSpec &cell : cells) {
        QToolButton *copyButton = nullptr;
        QWidget *container = buildValueCell(cell.text, cell.objectName, nullptr, &copyButton);
        copyButton->setEnabled(!cell.text.isEmpty());
        connect(copyButton, &QToolButton::clicked, this, [this, cell] {
            if (cell.text.isEmpty()) {
                return;
            }
            QGuiApplication::clipboard()->setText(cell.text);
            statusBar()->showMessage(cell.statusMessage, 5000);
        });

        ui->tableDetails->setCellWidget(cell.row, 1, container);
        ui->tableDetails->resizeRowToContents(cell.row);
    }
}

void MainWindow::clearCopyCells()
{
    const int rows[] = {kUsernameRow, kDescriptionRow, kNoteRow};
    for (int row : rows) {
        if (QWidget *cellWidget = ui->tableDetails->cellWidget(row, 1)) {
            ui->tableDetails->removeCellWidget(row, 1);
            delete cellWidget;
        }
    }
}

void MainWindow::copyCurrentPassword()
{
    if (m_currentPassword.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("当前账户没有密码"), 3000);
        return;
    }

    QGuiApplication::clipboard()->setText(m_currentPassword);
    statusBar()->showMessage(QStringLiteral("密码已复制到剪贴板"), 5000);
}

void MainWindow::clearDetails()
{
    clearPasswordCell();
    clearCopyCells();
    m_currentAccountId.clear();
    m_currentPassword.clear();
    m_passwordRow = -1;
    ui->tableDetails->clearContents();
    ui->tableDetails->setRowCount(0);
}

void MainWindow::exportEncryptedBackup()
{
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("导出加密备份"),
                                                        QStringLiteral("mempass-vault.mempass"),
                                                        QStringLiteral("Mempass vault (*.mempass)"));
    if (target.isEmpty()) {
        return;
    }
    if (QFile::exists(target) && !QFile::remove(target)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法覆盖目标文件。"));
        return;
    }
    if (!QFile::copy(m_vault->path(), target)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("备份文件复制失败。"));
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("加密备份已导出。"));
}

void MainWindow::exportCsv()
{
    const auto choice = QMessageBox::warning(this,
                                             QStringLiteral("明文导出警告"),
                                             QStringLiteral("CSV 将包含全部账号和密码明文，请确认文件保存位置安全。是否继续？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (choice != QMessageBox::Yes) {
        return;
    }
    const QString target = QFileDialog::getSaveFileName(this,
                                                        QStringLiteral("导出 CSV"),
                                                        QStringLiteral("password.csv"),
                                                        QStringLiteral("CSV files (*.csv)"));
    if (target.isEmpty()) {
        return;
    }

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << "\xEF\xBB\xBF";
    stream << "分类,账户项,账户名,密码,描述,备注,状态,创建时间,最后修改时间\n";
    for (const Account &account : m_vault->data().accounts) {
        if (account.deleted) {
            continue;
        }
        const Category *category = m_vault->findCategory(account.categoryId);
        const QStringList values = {
            category == nullptr ? QString() : category->name,
            account.itemName,
            account.username,
            account.password,
            account.description,
            account.note,
            statusText(account.status),
            formatTimestamp(account.createdAtMs),
            formatTimestamp(account.updatedAtMs),
        };
        QStringList escaped;
        escaped.reserve(values.size());
        for (const QString &value : values) {
            escaped.append(csvEscape(value));
        }
        stream << escaped.join(QLatin1Char(',')) << '\n';
    }
    if (!file.commit()) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    QMessageBox::information(this, QStringLiteral("导出完成"), QStringLiteral("CSV 已导出。"));
}

void MainWindow::changePassword()
{
    if (m_syncing) {
        return;
    }
    ChangePasswordDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    QString error;
    if (!m_vault->changePassword(dialog.oldPassword(), dialog.newPassword(), &error)) {
        QMessageBox::warning(this, QStringLiteral("修改密码失败"), error);
        return;
    }
    QMessageBox::information(this,
                             QStringLiteral("修改密码"),
                             QStringLiteral("主密码已修改。其他设备解锁时也需要使用新密码。"));
    if (!SyncSettingsDialog::serverUrl().isEmpty() && !SyncSettingsDialog::accessToken().isEmpty()) {
        startSync();
    }
}

void MainWindow::showPasswordGenerator()
{
    PasswordGeneratorDialog dialog(this);
    dialog.exec();
}

void MainWindow::showSyncSettings()
{
    SyncSettingsDialog dialog(this);
    dialog.exec();
    configureAutoSync();
}

void MainWindow::startSync()
{
    if (m_syncing || m_vault == nullptr || !m_vault->isOpen()) {
        return;
    }
    const QString urlText = SyncSettingsDialog::serverUrl();
    const QString token = SyncSettingsDialog::accessToken();
    if (urlText.isEmpty() || token.isEmpty()) {
        showSyncSettings();
        return;
    }
    const QUrl url(urlText);
    if (!url.isValid() || (url.scheme() != QLatin1String("http")
                           && url.scheme() != QLatin1String("https"))) {
        QMessageBox::warning(this, QStringLiteral("同步失败"), QStringLiteral("服务器地址必须是有效的 HTTP 或 HTTPS 地址。"));
        return;
    }

    QList<SyncRecord> records;
    QString error;
    if (!m_vault->beginSync(&records, &error)) {
        QMessageBox::warning(this, QStringLiteral("同步失败"), error);
        return;
    }

    m_syncing = true;
    setSyncUiEnabled(false);
    statusBar()->showMessage(QStringLiteral("正在同步……"), 10000);
    QMetaObject::invokeMethod(m_syncService,
                              "perform",
                              Qt::QueuedConnection,
                              Q_ARG(QUrl, url),
                              Q_ARG(QString, token),
                              Q_ARG(qint64, m_vault->data().serverCursor),
                              Q_ARG(QByteArray,
                                    QJsonDocument(syncPushRequestToJson(records))
                                        .toJson(QJsonDocument::Compact)),
                              Q_ARG(QByteArray,
                                    QJsonDocument(keyEnvelopeToJson(m_vault->keyEnvelope()))
                                        .toJson(QJsonDocument::Compact)));
}

void MainWindow::handleSyncFinished(bool ok, const QString &error, const QByteArray &resultJson)
{
    const qint64 oldEnvelopeTime = m_vault->data().keyEnvelopeUpdatedAtMs;
    bool completed = false;
    QString completionError;

    if (!ok) {
        completionError = error;
    } else {
        const QJsonDocument document = QJsonDocument::fromJson(resultJson);
        const QJsonObject root = document.object();
        VaultSyncState state;
        QList<SyncRecord> records;
        QString parseError;
        if (!document.isObject()
            || !syncStateFromJson(root.value(QStringLiteral("state")).toObject(), &state, &parseError)
            || !syncRecordsFromJson(root.value(QStringLiteral("records")).toArray(), &records, &parseError)) {
            completionError = parseError.isEmpty() ? QStringLiteral("同步响应格式无效") : parseError;
        } else if (!m_vault->applySyncResult(state, records, &parseError)) {
            completionError = parseError;
        } else if (!m_vault->completeSync(true, &parseError)) {
            completionError = parseError;
        } else {
            completed = true;
            const QString time = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            statusBar()->showMessage(QStringLiteral("同步完成：%1").arg(time), 10000);
            if (state.keyEnvelope.updatedAtMs > oldEnvelopeTime) {
                QMessageBox::information(this,
                                         QStringLiteral("主密码已变更"),
                                         QStringLiteral("其他设备修改了主密码。当前会话仍可继续使用，锁定后请使用新主密码解锁。"));
            }
        }
    }

    if (!completed && !m_vault->completeSync(false, &completionError)) {
        statusBar()->showMessage(QStringLiteral("同步失败：%1").arg(completionError), 15000);
    } else if (!completed) {
        statusBar()->showMessage(QStringLiteral("同步失败：%1").arg(completionError), 15000);
    }

    m_syncing = false;
    setSyncUiEnabled(true);
    refreshTree();
}

void MainWindow::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::lock()
{
    if (!m_syncing) {
        emit lockRequested();
    }
}

void MainWindow::configureAutoSync()
{
    if (SyncSettingsDialog::autoSyncEnabled()
        && !SyncSettingsDialog::serverUrl().isEmpty()
        && !SyncSettingsDialog::accessToken().isEmpty()) {
        m_syncTimer->start();
    } else {
        m_syncTimer->stop();
    }
}

void MainWindow::setSyncUiEnabled(bool enabled)
{
    const QList<QAction *> actions = {
        ui->actionNewCategory,
        ui->actionNewAccount,
        ui->actionDelete,
        ui->actionRandomPassword,
        ui->actionSyncNow,
        ui->actionSyncSettings,
        ui->actionExportBackup,
        ui->actionExportCsv,
        ui->actionChangePassword,
        ui->actionLock,
    };
    for (QAction *action : actions) {
        action->setEnabled(enabled);
    }
    ui->btnSearch->setEnabled(enabled);
    ui->btnClear->setEnabled(enabled);
    if (enabled) {
        selectionChanged();
    }
}

QString MainWindow::csvEscape(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}
