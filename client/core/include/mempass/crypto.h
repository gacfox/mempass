#pragma once

#include "mempass/sync_protocol.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace mempass {

inline constexpr quint32 VaultFileFormatVersion = 1;
inline constexpr int VaultDataKeySize = 32;
inline constexpr int VaultSaltSize = 16;

struct CryptoResult {
    bool ok = false;
    QString error;
    QByteArray data;
};

struct VaultFileHeader {
    quint32 formatVersion = VaultFileFormatVersion;
    QString vaultId;
    QString kdf = QStringLiteral("argon2id");
    QByteArray salt;
    quint64 opsLimit = 0;
    quint64 memLimit = 0;
    QByteArray keyNonce;
    QByteArray wrappedKey;
    QByteArray payloadNonce;
    QByteArray payloadCiphertext;
};

bool initCrypto();
QByteArray randomBytes(qsizetype size);
qint64 utcNowMs();

CryptoResult deriveKey(const QByteArray &password,
                       const QByteArray &salt,
                       quint64 opsLimit,
                       quint64 memLimit);

CryptoResult createVaultHeader(const QString &vaultId,
                               const QByteArray &password,
                               const QByteArray &dataKey,
                               quint64 opsLimit,
                               quint64 memLimit);

CryptoResult changeHeaderPassword(const VaultFileHeader &header,
                                  const QByteArray &oldPassword,
                                  const QByteArray &newPassword);

CryptoResult unwrapVaultDataKey(const VaultFileHeader &header, const QByteArray &password);

CryptoResult encryptData(const QByteArray &dataKey,
                         const QByteArray &plaintext,
                         const QByteArray &associatedData);

CryptoResult decryptData(const QByteArray &dataKey,
                         const QByteArray &nonceAndCiphertext,
                         const QByteArray &associatedData);

CryptoResult encryptJson(const QByteArray &dataKey,
                         const QJsonObject &object,
                         const QByteArray &associatedData);

CryptoResult decryptJson(const QByteArray &dataKey,
                         const QByteArray &nonceAndCiphertext,
                         const QByteArray &associatedData);

QByteArray recordAssociatedData(const SyncRecord &record);

CryptoResult encryptSyncRecord(const QByteArray &dataKey,
                               const QJsonObject &payload,
                               SyncRecord *record);

CryptoResult decryptSyncRecord(const QByteArray &dataKey,
                               const SyncRecord &record,
                               QJsonObject *payload);

QByteArray serializeVaultFile(const VaultFileHeader &header);
bool parseVaultFile(const QByteArray &serialized, VaultFileHeader *header, QString *error);
QByteArray serializeKeyEnvelope(const VaultFileHeader &header);
bool parseKeyEnvelope(const QByteArray &serialized, VaultFileHeader *header, QString *error);

} // namespace mempass
