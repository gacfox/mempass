#pragma once

#include "mempass/models.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace mempass {

struct LwwKey {
    qint64 updatedAtMs = 0;
    QString deviceId;

    bool isValid() const
    {
        return updatedAtMs > 0 && !deviceId.isEmpty();
    }
};

struct SyncRecord {
    QString recordId;
    RecordType recordType = RecordType::Category;
    qint64 updatedAtMs = 0;
    QString deviceId;
    bool deleted = false;
    QByteArray ciphertext;

    LwwKey lwwKey() const
    {
        return {updatedAtMs, deviceId};
    }
};

struct KeyEnvelope {
    // Opaque serialized vault key envelope. The server never interprets it.
    QByteArray data;
    qint64 updatedAtMs = 0;
    QString deviceId;

    LwwKey lwwKey() const
    {
        return {updatedAtMs, deviceId};
    }
};

struct VaultSyncState {
    QString vaultId;
    qint64 serverCursor = 0;
    KeyEnvelope keyEnvelope;
};

// Returns -1 when left is older, 0 when equal, and 1 when left is newer.
int compareLwwKeys(const LwwKey &left, const LwwKey &right);
bool isNewerLwwKey(const LwwKey &candidate, const LwwKey &current);

QJsonObject syncRecordToJson(const SyncRecord &record);
bool syncRecordFromJson(const QJsonObject &object, SyncRecord *record, QString *error);
QJsonArray syncRecordsToJson(const QList<SyncRecord> &records);
bool syncRecordsFromJson(const QJsonArray &array, QList<SyncRecord> *records, QString *error);

QJsonObject keyEnvelopeToJson(const KeyEnvelope &envelope);
bool keyEnvelopeFromJson(const QJsonObject &object, KeyEnvelope *envelope, QString *error);

QJsonObject syncStateToJson(const VaultSyncState &state);
bool syncStateFromJson(const QJsonObject &object, VaultSyncState *state, QString *error);

QJsonObject syncPushRequestToJson(const QList<SyncRecord> &records);
bool syncPushRequestFromJson(const QJsonObject &object, QList<SyncRecord> *records, QString *error);

QJsonObject syncPullResponseToJson(const QList<SyncRecord> &records,
                                   qint64 serverCursor,
                                   bool hasMore);
bool syncPullResponseFromJson(const QJsonObject &object,
                              QList<SyncRecord> *records,
                              qint64 *serverCursor,
                              bool *hasMore,
                              QString *error);

QByteArray hashAccessToken(const QByteArray &token);
QString generateAccessToken();
bool secureCompare(const QByteArray &left, const QByteArray &right);

} // namespace mempass
