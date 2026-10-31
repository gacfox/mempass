#include "mempass/sync_protocol.h"

#include <QCryptographicHash>
#include <QUuid>

namespace mempass {
namespace {

QByteArray valueToBase64(const QJsonValue &value)
{
    return QByteArray::fromBase64(value.toString().toLatin1());
}

} // namespace

int compareLwwKeys(const LwwKey &left, const LwwKey &right)
{
    if (left.updatedAtMs < right.updatedAtMs) {
        return -1;
    }
    if (left.updatedAtMs > right.updatedAtMs) {
        return 1;
    }
    if (left.deviceId < right.deviceId) {
        return -1;
    }
    if (left.deviceId > right.deviceId) {
        return 1;
    }
    return 0;
}

bool isNewerLwwKey(const LwwKey &candidate, const LwwKey &current)
{
    return compareLwwKeys(candidate, current) > 0;
}

QJsonObject syncRecordToJson(const SyncRecord &record)
{
    return {
        {QStringLiteral("recordId"), record.recordId},
        {QStringLiteral("recordType"), recordTypeToString(record.recordType)},
        {QStringLiteral("updatedAtMs"), record.updatedAtMs},
        {QStringLiteral("deviceId"), record.deviceId},
        {QStringLiteral("deleted"), record.deleted},
        {QStringLiteral("ciphertext"), QString::fromLatin1(record.ciphertext.toBase64())},
    };
}

bool syncRecordFromJson(const QJsonObject &object, SyncRecord *record, QString *error)
{
    error->clear();
    SyncRecord result;
    result.recordId = object.value(QStringLiteral("recordId")).toString();
    result.updatedAtMs = static_cast<qint64>(object.value(QStringLiteral("updatedAtMs")).toDouble());
    result.deviceId = object.value(QStringLiteral("deviceId")).toString();
    result.deleted = object.value(QStringLiteral("deleted")).toBool(false);
    const QString typeText = object.value(QStringLiteral("recordType")).toString();
    const QString cipherText = object.value(QStringLiteral("ciphertext")).toString();
    result.ciphertext = QByteArray::fromBase64(cipherText.toLatin1());

    if (result.recordId.size() != 32 || result.updatedAtMs <= 0 || result.deviceId.size() != 32
        || cipherText.isEmpty() || result.ciphertext.isEmpty()
        || !recordTypeFromString(typeText, &result.recordType)) {
        *error = QStringLiteral("同步记录格式无效");
        return false;
    }
    *record = result;
    return true;
}

QJsonArray syncRecordsToJson(const QList<SyncRecord> &records)
{
    QJsonArray array;
    for (const SyncRecord &record : records) {
        array.append(syncRecordToJson(record));
    }
    return array;
}

bool syncRecordsFromJson(const QJsonArray &array, QList<SyncRecord> *records, QString *error)
{
    error->clear();
    QList<SyncRecord> result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            *error = QStringLiteral("同步记录数组包含无效元素");
            return false;
        }
        SyncRecord record;
        if (!syncRecordFromJson(value.toObject(), &record, error)) {
            return false;
        }
        result.append(record);
    }
    *records = result;
    return true;
}

QJsonObject keyEnvelopeToJson(const KeyEnvelope &envelope)
{
    return {
        {QStringLiteral("data"), QString::fromLatin1(envelope.data.toBase64())},
        {QStringLiteral("updatedAtMs"), envelope.updatedAtMs},
        {QStringLiteral("deviceId"), envelope.deviceId},
    };
}

bool keyEnvelopeFromJson(const QJsonObject &object, KeyEnvelope *envelope, QString *error)
{
    error->clear();
    KeyEnvelope result;
    const QString dataText = object.value(QStringLiteral("data")).toString();
    result.data = QByteArray::fromBase64(dataText.toLatin1());
    result.updatedAtMs = static_cast<qint64>(object.value(QStringLiteral("updatedAtMs")).toDouble());
    result.deviceId = object.value(QStringLiteral("deviceId")).toString();
    if (result.data.isEmpty() || dataText.isEmpty() || result.updatedAtMs <= 0
        || result.deviceId.size() != 32) {
        *error = QStringLiteral("密钥信封格式无效");
        return false;
    }
    *envelope = result;
    return true;
}

QJsonObject syncStateToJson(const VaultSyncState &state)
{
    return {
        {QStringLiteral("vaultId"), state.vaultId},
        {QStringLiteral("serverCursor"), state.serverCursor},
        {QStringLiteral("keyEnvelope"), keyEnvelopeToJson(state.keyEnvelope)},
    };
}

bool syncStateFromJson(const QJsonObject &object, VaultSyncState *state, QString *error)
{
    error->clear();
    VaultSyncState result;
    result.vaultId = object.value(QStringLiteral("vaultId")).toString();
    result.serverCursor = static_cast<qint64>(object.value(QStringLiteral("serverCursor")).toDouble());
    const QJsonValue envelope = object.value(QStringLiteral("keyEnvelope"));
    if (envelope.isObject()) {
        const QJsonObject envelopeObject = envelope.toObject();
        const bool emptyEnvelope = envelopeObject.value(QStringLiteral("data")).toString().isEmpty()
            && static_cast<qint64>(envelopeObject.value(QStringLiteral("updatedAtMs")).toDouble()) == 0
            && envelopeObject.value(QStringLiteral("deviceId")).toString().isEmpty();
        if (emptyEnvelope) {
            result.keyEnvelope = {};
        } else if (!keyEnvelopeFromJson(envelopeObject, &result.keyEnvelope, error)) {
            return false;
        }
    } else if (envelope.isUndefined() || envelope.isNull()) {
        // A newly initialized server has no key envelope yet. Uploading the
        // local envelope is part of the first synchronization.
    } else {
        *error = QStringLiteral("同步状态格式无效");
        return false;
    }
    if (result.vaultId.size() != 32 || result.serverCursor < 0) {
        *error = QStringLiteral("同步状态格式无效");
        return false;
    }
    *state = result;
    return true;
}

QJsonObject syncPushRequestToJson(const QList<SyncRecord> &records)
{
    return {
        {QStringLiteral("records"), syncRecordsToJson(records)},
    };
}

bool syncPushRequestFromJson(const QJsonObject &object, QList<SyncRecord> *records, QString *error)
{
    error->clear();
    const QJsonValue value = object.value(QStringLiteral("records"));
    if (!value.isArray()) {
        *error = QStringLiteral("推送请求格式无效");
        return false;
    }
    return syncRecordsFromJson(value.toArray(), records, error);
}

QJsonObject syncPullResponseToJson(const QList<SyncRecord> &records,
                                   qint64 serverCursor,
                                   bool hasMore)
{
    return {
        {QStringLiteral("records"), syncRecordsToJson(records)},
        {QStringLiteral("serverCursor"), serverCursor},
        {QStringLiteral("hasMore"), hasMore},
    };
}

bool syncPullResponseFromJson(const QJsonObject &object,
                              QList<SyncRecord> *records,
                              qint64 *serverCursor,
                              bool *hasMore,
                              QString *error)
{
    error->clear();
    const QJsonValue recordsValue = object.value(QStringLiteral("records"));
    const QJsonValue cursorValue = object.value(QStringLiteral("serverCursor"));
    const QJsonValue moreValue = object.value(QStringLiteral("hasMore"));
    if (!recordsValue.isArray() || !cursorValue.isDouble() || !moreValue.isBool()) {
        *error = QStringLiteral("拉取响应格式无效");
        return false;
    }
    *serverCursor = static_cast<qint64>(cursorValue.toDouble());
    *hasMore = moreValue.toBool();
    return syncRecordsFromJson(recordsValue.toArray(), records, error);
}

QByteArray hashAccessToken(const QByteArray &token)
{
    return QCryptographicHash::hash(token, QCryptographicHash::Sha256).toHex();
}

QString generateAccessToken()
{
    // Two random UUIDs provide 244 random bits, which is ample for a bearer token.
    return QUuid::createUuid().toString(QUuid::Id128)
        + QUuid::createUuid().toString(QUuid::Id128);
}

bool secureCompare(const QByteArray &left, const QByteArray &right)
{
    return QCryptographicHash::hash(left, QCryptographicHash::Sha256)
        == QCryptographicHash::hash(right, QCryptographicHash::Sha256);
}

} // namespace mempass
