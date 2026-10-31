#include "mempass/models.h"
#include "mempass/sync_protocol.h"

#include <QJsonDocument>
#include <QTest>

using namespace mempass;

class CommonTest : public QObject
{
    Q_OBJECT

private slots:
    void lwwComparison();
    void syncRecordJsonRoundTrip();
    void syncStateJsonRoundTrip();
    void invalidSyncRecordIsRejected();
};

void CommonTest::lwwComparison()
{
    const LwwKey older{100, QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    const LwwKey newer{200, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    QCOMPARE(compareLwwKeys(older, newer), -1);
    QCOMPARE(compareLwwKeys(newer, older), 1);
    QVERIFY(isNewerLwwKey(newer, older));
    QVERIFY(!isNewerLwwKey(older, newer));

    const LwwKey tieA{200, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")};
    const LwwKey tieB{200, QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")};
    QCOMPARE(compareLwwKeys(tieA, tieB), -1);
    QVERIFY(isNewerLwwKey(tieB, tieA));
    QCOMPARE(compareLwwKeys(tieB, tieB), 0);
    QVERIFY(!isNewerLwwKey(tieB, tieB));
}

void CommonTest::syncRecordJsonRoundTrip()
{
    SyncRecord record;
    record.recordId = createId();
    record.recordType = RecordType::Account;
    record.updatedAtMs = 1760000000000;
    record.deviceId = createId();
    record.deleted = true;
    record.ciphertext = QByteArrayLiteral("cipher-text");

    const QJsonObject object = syncRecordToJson(record);
    SyncRecord parsed;
    QString error;
    QVERIFY2(syncRecordFromJson(object, &parsed, &error), qPrintable(error));
    QCOMPARE(parsed.recordId, record.recordId);
    QCOMPARE(parsed.recordType, record.recordType);
    QCOMPARE(parsed.updatedAtMs, record.updatedAtMs);
    QCOMPARE(parsed.deviceId, record.deviceId);
    QCOMPARE(parsed.deleted, record.deleted);
    QCOMPARE(parsed.ciphertext, record.ciphertext);

    const QJsonArray array = syncRecordsToJson({record});
    QList<SyncRecord> records;
    QVERIFY2(syncRecordsFromJson(array, &records, &error), qPrintable(error));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.first().recordId, record.recordId);
}

void CommonTest::syncStateJsonRoundTrip()
{
    VaultSyncState state;
    state.vaultId = createId();
    state.serverCursor = 42;
    state.keyEnvelope.data = QByteArrayLiteral("opaque-envelope");
    state.keyEnvelope.updatedAtMs = 1760000000000;
    state.keyEnvelope.deviceId = createId();

    VaultSyncState parsed;
    QString error;
    QVERIFY2(syncStateFromJson(syncStateToJson(state), &parsed, &error), qPrintable(error));
    QCOMPARE(parsed.vaultId, state.vaultId);
    QCOMPARE(parsed.serverCursor, state.serverCursor);
    QCOMPARE(parsed.keyEnvelope.data, state.keyEnvelope.data);
    QCOMPARE(parsed.keyEnvelope.updatedAtMs, state.keyEnvelope.updatedAtMs);
    QCOMPARE(parsed.keyEnvelope.deviceId, state.keyEnvelope.deviceId);
}

void CommonTest::invalidSyncRecordIsRejected()
{
    SyncRecord record;
    record.recordId = createId();
    record.recordType = RecordType::Category;
    record.updatedAtMs = 1760000000000;
    record.deviceId = createId();
    record.ciphertext = QByteArrayLiteral("data");

    QJsonObject object = syncRecordToJson(record);
    object.remove(QStringLiteral("recordType"));
    SyncRecord parsed;
    QString error;
    QVERIFY(!syncRecordFromJson(object, &parsed, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(CommonTest)
#include "common_test.moc"
