#include "mempass/crypto.h"
#include "mempass/vault.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include <sodium.h>

using namespace mempass;

class CryptoVaultTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cryptoRoundTrip();
    void tamperedCiphertextFails();
    void vaultCreateOpenAndChangePassword();
    void vaultSyncRecordRoundTrip();
};

void CryptoVaultTest::initTestCase()
{
    QVERIFY(initCrypto());
}

void CryptoVaultTest::cryptoRoundTrip()
{
    const QByteArray password = "correct horse battery staple";
    const QByteArray dataKey = randomBytes(VaultDataKeySize);
    const QString vaultId = createId();

    const CryptoResult headerResult = createVaultHeader(vaultId,
                                                        password,
                                                        dataKey,
                                                        crypto_pwhash_OPSLIMIT_MIN,
                                                        crypto_pwhash_MEMLIMIT_MIN);
    QVERIFY2(headerResult.ok, qPrintable(headerResult.error));

    VaultFileHeader header;
    QString error;
    QVERIFY2(parseKeyEnvelope(headerResult.data, &header, &error), qPrintable(error));
    const CryptoResult unwrapped = unwrapVaultDataKey(header, password);
    QVERIFY2(unwrapped.ok, qPrintable(unwrapped.error));
    QCOMPARE(unwrapped.data, dataKey);

    const CryptoResult wrong = unwrapVaultDataKey(header, "wrong-password");
    QVERIFY(!wrong.ok);

    const QByteArray plaintext = QByteArrayLiteral("{\"name\":\"机密数据\"}");
    const CryptoResult encrypted = encryptData(dataKey, plaintext, QByteArrayLiteral("aad"));
    QVERIFY2(encrypted.ok, qPrintable(encrypted.error));
    const CryptoResult decrypted = decryptData(dataKey, encrypted.data, QByteArrayLiteral("aad"));
    QVERIFY2(decrypted.ok, qPrintable(decrypted.error));
    QCOMPARE(decrypted.data, plaintext);
}

void CryptoVaultTest::tamperedCiphertextFails()
{
    const QByteArray dataKey = randomBytes(VaultDataKeySize);
    CryptoResult encrypted = encryptData(dataKey,
                                         QByteArrayLiteral("secret"),
                                         QByteArrayLiteral("aad"));
    QVERIFY(encrypted.ok);
    encrypted.data[encrypted.data.size() - 1] ^= 0x01;
    const CryptoResult decrypted = decryptData(dataKey, encrypted.data, QByteArrayLiteral("aad"));
    QVERIFY(!decrypted.ok);
}

void CryptoVaultTest::vaultCreateOpenAndChangePassword()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("vault.mempass"));

    Vault created;
    QString error;
    QVERIFY2(created.create(path, QStringLiteral("old-password"), createId(), &error),
             qPrintable(error));
    QVERIFY2(created.addCategory(QStringLiteral("工作"), &error), qPrintable(error));
    const QString defaultCategory = [&created]() {
        for (const Category &category : created.data().categories) {
            if (category.isDefault) {
                return category.id;
            }
        }
        return QString();
    }();
    Account account;
    account.categoryId = defaultCategory;
    account.itemName = QStringLiteral("邮箱");
    account.username = QStringLiteral("user@example.com");
    account.password = QStringLiteral("secret-password");
    account.status = static_cast<int>(AccountStatus::Available);
    QVERIFY2(created.addAccount(account, &error), qPrintable(error));

    Vault opened;
    QVERIFY2(opened.open(path, QStringLiteral("old-password"), &error), qPrintable(error));
    QCOMPARE(opened.data().categories.size(), 2);
    QCOMPARE(opened.data().accounts.size(), 1);
    QCOMPARE(opened.data().accounts.first().password, QStringLiteral("secret-password"));

    Vault wrongPassword;
    QVERIFY(!wrongPassword.open(path, QStringLiteral("bad-password"), &error));

    QVERIFY2(created.changePassword(QStringLiteral("old-password"),
                                    QStringLiteral("new-password"),
                                    &error),
             qPrintable(error));

    QFile vaultFile(path);
    QVERIFY(vaultFile.open(QIODevice::ReadOnly));
    VaultFileHeader changedHeader;
    QVERIFY2(parseVaultFile(vaultFile.readAll(), &changedHeader, &error), qPrintable(error));
    const CryptoResult changedKey = unwrapVaultDataKey(changedHeader, "new-password");
    QVERIFY2(changedKey.ok, qPrintable(changedKey.error));
    QCOMPARE(changedKey.data.size(), VaultDataKeySize);
    QCOMPARE(changedKey.data, created.dataKey());
    QVERIFY(!unwrapVaultDataKey(changedHeader, "old-password").ok);
    const QByteArray payloadAad = QByteArrayLiteral("mempass/vault-payload/v1/")
        + changedHeader.vaultId.toUtf8();
    const CryptoResult payload = decryptData(changedKey.data,
                                             changedHeader.payloadNonce
                                                 + changedHeader.payloadCiphertext,
                                             payloadAad);
    QVERIFY2(payload.ok, qPrintable(payload.error));
    VaultData parsedData;
    QVERIFY2(vaultDataFromJson(QJsonDocument::fromJson(payload.data).object(),
                               &parsedData,
                               &error),
             qPrintable(error));
    QCOMPARE(parsedData.accounts.size(), 1);
    created.lock();

    Vault reopened;
    QVERIFY2(reopened.open(path, QStringLiteral("new-password"), &error), qPrintable(error));
    QCOMPARE(reopened.data().accounts.first().itemName, QStringLiteral("邮箱"));
    QVERIFY(!reopened.open(path, QStringLiteral("old-password"), &error));
}

void CryptoVaultTest::vaultSyncRecordRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString sourcePath = directory.filePath(QStringLiteral("source.mempass"));
    const QString targetPath = directory.filePath(QStringLiteral("target.mempass"));

    Vault source;
    QString error;
    const QString deviceId = createId();
    QVERIFY2(source.create(sourcePath, QStringLiteral("password"), deviceId, &error),
             qPrintable(error));
    QVERIFY2(source.addCategory(QStringLiteral("同步分类"), &error), qPrintable(error));

    QList<SyncRecord> records;
    QVERIFY2(source.beginSync(&records, &error), qPrintable(error));
    QVERIFY(records.size() >= 2);
    QVERIFY2(source.completeSync(true, &error), qPrintable(error));

    Vault target;
    QVERIFY2(target.restoreFromServer(targetPath,
                                      QStringLiteral("password"),
                                      createId(),
                                      source.keyEnvelope(),
                                      records,
                                      &error),
             qPrintable(error));
    QCOMPARE(target.data().categories.size(), source.data().categories.size());
    QCOMPARE(target.vaultId(), source.vaultId());

    // Stale records must not overwrite newer local values.
    const qint64 oldEnvelopeTime = target.data().keyEnvelopeUpdatedAtMs;
    QList<SyncRecord> staleRecords;
    QVERIFY2(target.beginSync(&staleRecords, &error), qPrintable(error));
    QVERIFY(staleRecords.isEmpty());
    QVERIFY2(target.completeSync(true, &error), qPrintable(error));
    QCOMPARE(target.data().keyEnvelopeUpdatedAtMs, oldEnvelopeTime);
}

QTEST_GUILESS_MAIN(CryptoVaultTest)
#include "crypto_vault_test.moc"
