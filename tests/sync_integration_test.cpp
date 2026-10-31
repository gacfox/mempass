#include "mempass/sync_service.h"
#include "mempass/vault.h"

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include <memory>

using namespace mempass;

class SyncIntegrationTest : public QObject
{
    Q_OBJECT

public:
    ~SyncIntegrationTest() override;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void endToEndSyncAndLww();
    void serverDatabaseContainsNoPlaintextPassword();

private:
    bool waitForServer(const QUrl &url) const;
    bool synchronize(Vault *vault, QString *error);
    Account *findAccount(Vault *vault, const QString &accountId);

    std::unique_ptr<QTemporaryDir> m_directory;
    QProcess m_server;
    QUrl m_url;
    QString m_accessToken;
    QString m_configPath;
    QString m_dbPath;
};

SyncIntegrationTest::~SyncIntegrationTest()
{
    if (m_server.state() != QProcess::NotRunning) {
        m_server.kill();
        m_server.waitForFinished(3000);
    }
}

void SyncIntegrationTest::initTestCase()
{
    QVERIFY(initCrypto());
    m_directory = std::make_unique<QTemporaryDir>();
    QVERIFY(m_directory->isValid());

    quint16 port = 0;
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost));
        port = probe.serverPort();
    }
    QVERIFY(port > 0);

    const QString configPath = m_directory->filePath(QStringLiteral("server.json"));
    QProcess initProcess;
    initProcess.setProgram(QStringLiteral(MEMPASS_SERVER_EXE));
    initProcess.setArguments({
        QStringLiteral("--init"),
        QStringLiteral("--data-dir"), m_directory->path(),
        QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
        QStringLiteral("--port"), QString::number(port),
    });
    initProcess.start();
    QVERIFY2(initProcess.waitForFinished(30000),
             qPrintable(initProcess.errorString()));
    const QString output = QString::fromLocal8Bit(initProcess.readAllStandardOutput()
                                                  + initProcess.readAllStandardError());
    QCOMPARE(initProcess.exitCode(), 0);
    QVERIFY2(QFile::exists(configPath), qPrintable(output));

    const QRegularExpression tokenExpression(QStringLiteral("[0-9a-f]{64}"));
    const QRegularExpressionMatch match = tokenExpression.match(output);
    QVERIFY2(match.hasMatch(), qPrintable(output));
    m_accessToken = match.captured(0);
    m_configPath = configPath;
    m_dbPath = m_directory->filePath(QStringLiteral("vault.db"));
    m_url = QUrl(QStringLiteral("http://127.0.0.1:%1").arg(port));

    m_server.setProgram(QStringLiteral(MEMPASS_SERVER_EXE));
    m_server.setArguments({QStringLiteral("--config"), m_configPath});
    m_server.start();
    QVERIFY2(m_server.waitForStarted(10000), qPrintable(m_server.errorString()));
    QVERIFY2(waitForServer(m_url), "服务端没有在预期时间内启动");
}

void SyncIntegrationTest::cleanupTestCase()
{
    if (m_server.state() != QProcess::NotRunning) {
        m_server.terminate();
        if (!m_server.waitForFinished(5000)) {
            m_server.kill();
            m_server.waitForFinished(3000);
        }
    }
}

void SyncIntegrationTest::endToEndSyncAndLww()
{
    QString error;
    SyncService service;

    VaultSyncState wrongTokenState;
    QVERIFY(!service.fetchState(m_url,
                                QStringLiteral("wrong-token"),
                                &wrongTokenState,
                                &error));

    Vault first;
    const QString firstPath = m_directory->filePath(QStringLiteral("first.mempass"));
    QVERIFY2(first.create(firstPath, QStringLiteral("master-password"), createId(), &error),
             qPrintable(error));
    QVERIFY2(first.addCategory(QStringLiteral("工作"), &error), qPrintable(error));
    QString defaultCategoryId;
    for (const Category &category : first.data().categories) {
        if (category.isDefault) {
            defaultCategoryId = category.id;
        }
    }
    QVERIFY(!defaultCategoryId.isEmpty());

    Account firstAccount;
    firstAccount.categoryId = defaultCategoryId;
    firstAccount.itemName = QStringLiteral("邮箱");
    firstAccount.username = QStringLiteral("user@example.com");
    firstAccount.password = QStringLiteral("secret-password");
    QVERIFY2(first.addAccount(firstAccount, &error), qPrintable(error));
    const QString accountId = first.data().accounts.first().id;
    QVERIFY(!accountId.isEmpty());

    QVERIFY2(synchronize(&first, &error), qPrintable(error));

    VaultSyncState serverState;
    QList<SyncRecord> serverRecords;
    QVERIFY2(service.downloadVault(m_url, m_accessToken, &serverState, &serverRecords, &error),
             qPrintable(error));
    QVERIFY(!serverRecords.isEmpty());
    QCOMPARE(serverRecords.size(), first.data().categories.size() + first.data().accounts.size());

    const QString secondPath = m_directory->filePath(QStringLiteral("second.mempass"));
    Vault second;
    QVERIFY2(second.restoreFromServer(secondPath,
                                      QStringLiteral("master-password"),
                                      createId(),
                                      serverState.keyEnvelope,
                                      serverRecords,
                                      &error),
             qPrintable(error));
    second.data().serverCursor = serverState.serverCursor;
    QVERIFY2(second.save(&error), qPrintable(error));
    QCOMPARE(second.vaultId(), first.vaultId());
    QCOMPARE(second.data().accounts.size(), 1);
    QCOMPARE(second.data().accounts.first().password, QStringLiteral("secret-password"));

    // Two offline edits are synchronized out of order; the newer edit wins.
    Account *secondEdit = findAccount(&second, accountId);
    QVERIFY(secondEdit != nullptr);
    secondEdit->itemName = QStringLiteral("second-device-edit");
    QVERIFY2(second.updateAccount(*secondEdit, &error), qPrintable(error));
    QVERIFY2(synchronize(&second, &error), qPrintable(error));

    QTest::qWait(10);
    Account *firstEdit = findAccount(&first, accountId);
    QVERIFY(firstEdit != nullptr);
    firstEdit->itemName = QStringLiteral("first-device-newer-edit");
    QVERIFY2(first.updateAccount(*firstEdit, &error), qPrintable(error));
    QVERIFY2(synchronize(&first, &error), qPrintable(error));

    QVERIFY2(synchronize(&second, &error), qPrintable(error));
    QCOMPARE(findAccount(&first, accountId)->itemName,
             QStringLiteral("first-device-newer-edit"));
    QCOMPARE(findAccount(&second, accountId)->itemName,
             QStringLiteral("first-device-newer-edit"));

    // Deletion is represented by a tombstone and propagates to other devices.
    QVERIFY2(first.deleteAccount(accountId, &error), qPrintable(error));
    QVERIFY2(synchronize(&first, &error), qPrintable(error));
    QVERIFY2(synchronize(&second, &error), qPrintable(error));
    QVERIFY(findAccount(&second, accountId)->deleted);
}

void SyncIntegrationTest::serverDatabaseContainsNoPlaintextPassword()
{
    QFile databaseFile(m_dbPath);
    QVERIFY(databaseFile.open(QIODevice::ReadOnly));
    const QByteArray databaseBytes = databaseFile.readAll();
    QVERIFY(!databaseBytes.contains("secret-password"));
    QVERIFY(!databaseBytes.contains("user@example.com"));
}

bool SyncIntegrationTest::waitForServer(const QUrl &url) const
{
    QNetworkAccessManager manager;
    const QUrl healthUrl = url.toString() + QStringLiteral("/api/v1/health");
    for (int attempt = 0; attempt < 50; ++attempt) {
        QNetworkReply *reply = manager.get(QNetworkRequest(healthUrl));
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(500);
        loop.exec();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (status == 200) {
            return true;
        }
        QTest::qWait(100);
    }
    return false;
}

bool SyncIntegrationTest::synchronize(Vault *vault, QString *error)
{
    QList<SyncRecord> records;
    if (!vault->beginSync(&records, error)) {
        return false;
    }

    SyncService service;
    QByteArray resultJson;
    if (!service.synchronize(m_url,
                             m_accessToken,
                             vault->data().serverCursor,
                             QJsonDocument(syncPushRequestToJson(records))
                                 .toJson(QJsonDocument::Compact),
                             QJsonDocument(keyEnvelopeToJson(vault->keyEnvelope()))
                                 .toJson(QJsonDocument::Compact),
                             &resultJson,
                             error)) {
        vault->completeSync(false, error);
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(resultJson).object();
    VaultSyncState state;
    QList<SyncRecord> pulled;
    QString parseError;
    if (!syncStateFromJson(root.value(QStringLiteral("state")).toObject(), &state, &parseError)
        || !syncRecordsFromJson(root.value(QStringLiteral("records")).toArray(),
                                &pulled,
                                &parseError)) {
        *error = parseError;
        vault->completeSync(false, error);
        return false;
    }
    if (!vault->applySyncResult(state, pulled, error)) {
        vault->completeSync(false, error);
        return false;
    }
    return vault->completeSync(true, error);
}

Account *SyncIntegrationTest::findAccount(Vault *vault, const QString &accountId)
{
    return vault->findAccount(accountId);
}

QTEST_GUILESS_MAIN(SyncIntegrationTest)
#include "sync_integration_test.moc"
