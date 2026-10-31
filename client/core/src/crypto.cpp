#include "mempass/crypto.h"

#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <QJsonDocument>

#include <sodium.h>

namespace mempass {
namespace {

constexpr QByteArrayView FileMagic("MEP1");
constexpr qsizetype AeadNonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;

QByteArray keyAssociatedData(const QString &vaultId)
{
    return QByteArrayLiteral("mempass/vault-key/v1/") + vaultId.toUtf8();
}

CryptoResult makeError(const QString &error)
{
    return {.ok = false, .error = error, .data = {}};
}

CryptoResult makeData(QByteArray data)
{
    return {.ok = true, .error = {}, .data = std::move(data)};
}

} // namespace

bool initCrypto()
{
    return sodium_init() >= 0;
}

QByteArray randomBytes(qsizetype size)
{
    QByteArray result(size, Qt::Uninitialized);
    randombytes_buf(result.data(), static_cast<unsigned long long>(result.size()));
    return result;
}

qint64 utcNowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

CryptoResult deriveKey(const QByteArray &password,
                       const QByteArray &salt,
                       quint64 opsLimit,
                       quint64 memLimit)
{
    if (password.isEmpty() || salt.size() != crypto_pwhash_SALTBYTES || opsLimit == 0
        || memLimit == 0) {
        return makeError(QStringLiteral("密钥派生参数无效"));
    }

    QByteArray key(VaultDataKeySize, Qt::Uninitialized);
    if (crypto_pwhash(reinterpret_cast<unsigned char *>(key.data()),
                      key.size(),
                      password.constData(),
                      static_cast<unsigned long long>(password.size()),
                      reinterpret_cast<const unsigned char *>(salt.constData()),
                      opsLimit,
                      memLimit,
                      crypto_pwhash_ALG_ARGON2ID13)
        != 0) {
        return makeError(QStringLiteral("主密码密钥派生失败"));
    }
    return makeData(key);
}

CryptoResult createVaultHeader(const QString &vaultId,
                               const QByteArray &password,
                               const QByteArray &dataKey,
                               quint64 opsLimit,
                               quint64 memLimit)
{
    if (vaultId.size() != 32 || dataKey.size() != VaultDataKeySize) {
        return makeError(QStringLiteral("保险库初始化参数无效"));
    }

    VaultFileHeader header;
    header.vaultId = vaultId;
    header.salt = randomBytes(crypto_pwhash_SALTBYTES);
    header.opsLimit = opsLimit;
    header.memLimit = memLimit;

    CryptoResult kek = deriveKey(password,
                                 header.salt,
                                 opsLimit,
                                 memLimit);
    if (!kek.ok) {
        return kek;
    }
    const CryptoResult keyEncryption = encryptData(kek.data,
                                                   dataKey,
                                                   keyAssociatedData(vaultId));
    sodium_memzero(kek.data.data(), static_cast<size_t>(kek.data.size()));
    if (!keyEncryption.ok) {
        return keyEncryption;
    }
    header.keyNonce = keyEncryption.data.left(AeadNonceSize);
    header.wrappedKey = keyEncryption.data.mid(AeadNonceSize);

    CryptoResult result;
    result.ok = true;
    result.error = {};
    result.data = serializeKeyEnvelope(header);
    return result;
}

CryptoResult changeHeaderPassword(const VaultFileHeader &header,
                                  const QByteArray &oldPassword,
                                  const QByteArray &newPassword)
{
    CryptoResult oldKey = unwrapVaultDataKey(header, oldPassword);
    if (!oldKey.ok) {
        return oldKey;
    }

    VaultFileHeader changed = header;
    changed.salt = randomBytes(crypto_pwhash_SALTBYTES);
    CryptoResult newKek = deriveKey(newPassword,
                                    changed.salt,
                                    header.opsLimit,
                                    header.memLimit);
    if (!newKek.ok) {
        sodium_memzero(oldKey.data.data(), static_cast<size_t>(oldKey.data.size()));
        return newKek;
    }
    const CryptoResult keyEncryption = encryptData(newKek.data,
                                                   oldKey.data,
                                                   keyAssociatedData(header.vaultId));
    sodium_memzero(newKek.data.data(), static_cast<size_t>(newKek.data.size()));
    sodium_memzero(oldKey.data.data(), static_cast<size_t>(oldKey.data.size()));
    if (!keyEncryption.ok) {
        return keyEncryption;
    }
    changed.keyNonce = keyEncryption.data.left(AeadNonceSize);
    changed.wrappedKey = keyEncryption.data.mid(AeadNonceSize);
    changed.payloadNonce.clear();
    changed.payloadCiphertext.clear();

    CryptoResult result;
    result.ok = true;
    result.error = {};
    result.data = serializeKeyEnvelope(changed);
    return result;
}

CryptoResult unwrapVaultDataKey(const VaultFileHeader &header, const QByteArray &password)
{
    if (header.vaultId.size() != 32 || header.keyNonce.size() != AeadNonceSize
        || header.wrappedKey.isEmpty()) {
        return makeError(QStringLiteral("保险库密钥信封无效"));
    }
    CryptoResult kek = deriveKey(password,
                                 header.salt,
                                 header.opsLimit,
                                 header.memLimit);
    if (!kek.ok) {
        return kek;
    }
    CryptoResult key = decryptData(kek.data,
                                   header.keyNonce + header.wrappedKey,
                                   keyAssociatedData(header.vaultId));
    sodium_memzero(kek.data.data(), static_cast<size_t>(kek.data.size()));
    if (!key.ok) {
        return makeError(QStringLiteral("主密码错误或保险库密钥已损坏"));
    }
    if (key.data.size() != VaultDataKeySize) {
        sodium_memzero(key.data.data(), static_cast<size_t>(key.data.size()));
        return makeError(QStringLiteral("保险库密钥长度无效"));
    }
    return key;
}

CryptoResult encryptData(const QByteArray &dataKey,
                         const QByteArray &plaintext,
                         const QByteArray &associatedData)
{
    if (dataKey.size() != VaultDataKeySize) {
        return makeError(QStringLiteral("数据密钥无效"));
    }

    const QByteArray nonce = randomBytes(AeadNonceSize);
    QByteArray ciphertext(plaintext.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES,
                          Qt::Uninitialized);
    unsigned long long ciphertextLength = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            reinterpret_cast<unsigned char *>(ciphertext.data()),
            &ciphertextLength,
            reinterpret_cast<const unsigned char *>(plaintext.constData()),
            static_cast<unsigned long long>(plaintext.size()),
            reinterpret_cast<const unsigned char *>(associatedData.constData()),
            static_cast<unsigned long long>(associatedData.size()),
            nullptr,
            reinterpret_cast<const unsigned char *>(nonce.constData()),
            reinterpret_cast<const unsigned char *>(dataKey.constData()))
        != 0) {
        return makeError(QStringLiteral("数据加密失败"));
    }
    ciphertext.resize(static_cast<qsizetype>(ciphertextLength));
    return makeData(nonce + ciphertext);
}

CryptoResult decryptData(const QByteArray &dataKey,
                         const QByteArray &nonceAndCiphertext,
                         const QByteArray &associatedData)
{
    if (dataKey.size() != VaultDataKeySize
        || nonceAndCiphertext.size()
            <= AeadNonceSize + crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        return makeError(QStringLiteral("密文数据无效"));
    }
    const QByteArray nonce = nonceAndCiphertext.left(AeadNonceSize);
    const QByteArray ciphertext = nonceAndCiphertext.mid(AeadNonceSize);
    QByteArray plaintext(ciphertext.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES,
                         Qt::Uninitialized);
    unsigned long long plaintextLength = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(plaintext.data()),
            &plaintextLength,
            nullptr,
            reinterpret_cast<const unsigned char *>(ciphertext.constData()),
            static_cast<unsigned long long>(ciphertext.size()),
            reinterpret_cast<const unsigned char *>(associatedData.constData()),
            static_cast<unsigned long long>(associatedData.size()),
            reinterpret_cast<const unsigned char *>(nonce.constData()),
            reinterpret_cast<const unsigned char *>(dataKey.constData()))
        != 0) {
        return makeError(QStringLiteral("数据解密失败，数据可能被篡改或密钥不匹配"));
    }
    plaintext.resize(static_cast<qsizetype>(plaintextLength));
    return makeData(plaintext);
}

CryptoResult encryptJson(const QByteArray &dataKey,
                         const QJsonObject &object,
                         const QByteArray &associatedData)
{
    return encryptData(dataKey,
                       QJsonDocument(object).toJson(QJsonDocument::Compact),
                       associatedData);
}

CryptoResult decryptJson(const QByteArray &dataKey,
                         const QByteArray &nonceAndCiphertext,
                         const QByteArray &associatedData)
{
    CryptoResult plain = decryptData(dataKey, nonceAndCiphertext, associatedData);
    if (!plain.ok) {
        return plain;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(plain.data, &parseError);
    sodium_memzero(plain.data.data(), static_cast<size_t>(plain.data.size()));
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return makeError(QStringLiteral("解密后的数据不是有效 JSON"));
    }
    CryptoResult result;
    result.ok = true;
    result.error = {};
    result.data = QJsonDocument(document.object()).toJson(QJsonDocument::Compact);
    return result;
}

QByteArray recordAssociatedData(const SyncRecord &record)
{
    return QByteArrayLiteral("mempass/record/v1/")
        + record.recordId.toUtf8() + '/'
        + recordTypeToString(record.recordType).toUtf8() + '/'
        + QByteArray::number(record.updatedAtMs) + '/'
        + record.deviceId.toUtf8() + '/'
        + (record.deleted ? QByteArrayLiteral("deleted") : QByteArrayLiteral("active"));
}

CryptoResult encryptSyncRecord(const QByteArray &dataKey,
                               const QJsonObject &payload,
                               SyncRecord *record)
{
    const CryptoResult encrypted = encryptJson(dataKey, payload, recordAssociatedData(*record));
    if (!encrypted.ok) {
        return encrypted;
    }
    record->ciphertext = encrypted.data;
    return encrypted;
}

CryptoResult decryptSyncRecord(const QByteArray &dataKey,
                               const SyncRecord &record,
                               QJsonObject *payload)
{
    CryptoResult plain = decryptData(dataKey, record.ciphertext, recordAssociatedData(record));
    if (!plain.ok) {
        return plain;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(plain.data, &parseError);
    sodium_memzero(plain.data.data(), static_cast<size_t>(plain.data.size()));
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return makeError(QStringLiteral("同步记录解密结果无效"));
    }
    *payload = document.object();
    return {.ok = true, .error = {}, .data = {}};
}

QByteArray serializeVaultFile(const VaultFileHeader &header)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_7);
    stream << FileMagic.toByteArray()
           << header.formatVersion
           << header.vaultId
           << header.kdf
           << header.salt
           << header.opsLimit
           << header.memLimit
           << header.keyNonce
           << header.wrappedKey
           << header.payloadNonce
           << header.payloadCiphertext;
    return result;
}

bool parseVaultFile(const QByteArray &serialized, VaultFileHeader *header, QString *error)
{
    QDataStream stream(serialized);
    stream.setVersion(QDataStream::Qt_6_7);

    QByteArray magic;
    VaultFileHeader parsed;
    stream >> magic;
    stream >> parsed.formatVersion;
    stream >> parsed.vaultId;
    stream >> parsed.kdf;
    stream >> parsed.salt;
    stream >> parsed.opsLimit;
    stream >> parsed.memLimit;
    stream >> parsed.keyNonce;
    stream >> parsed.wrappedKey;
    stream >> parsed.payloadNonce;
    stream >> parsed.payloadCiphertext;

    if (stream.status() != QDataStream::Ok || !stream.atEnd() || magic != FileMagic.toByteArray()
        || parsed.formatVersion != VaultFileFormatVersion || parsed.vaultId.size() != 32
        || parsed.kdf != QLatin1String("argon2id")
        || parsed.salt.size() != crypto_pwhash_SALTBYTES || parsed.opsLimit == 0
        || parsed.memLimit == 0 || parsed.keyNonce.size() != AeadNonceSize
        || parsed.wrappedKey.size() <= crypto_aead_xchacha20poly1305_ietf_ABYTES
        || parsed.payloadNonce.size() != AeadNonceSize
        || parsed.payloadCiphertext.size() <= crypto_aead_xchacha20poly1305_ietf_ABYTES) {
        *error = QStringLiteral("保险库文件格式无效或已损坏");
        return false;
    }

    *header = parsed;
    return true;
}

QByteArray serializeKeyEnvelope(const VaultFileHeader &header)
{
    VaultFileHeader envelope = header;
    envelope.payloadNonce.clear();
    envelope.payloadCiphertext.clear();
    return serializeVaultFile(envelope);
}

bool parseKeyEnvelope(const QByteArray &serialized, VaultFileHeader *header, QString *error)
{
    QDataStream stream(serialized);
    stream.setVersion(QDataStream::Qt_6_7);

    QByteArray magic;
    VaultFileHeader parsed;
    stream >> magic;
    stream >> parsed.formatVersion;
    stream >> parsed.vaultId;
    stream >> parsed.kdf;
    stream >> parsed.salt;
    stream >> parsed.opsLimit;
    stream >> parsed.memLimit;
    stream >> parsed.keyNonce;
    stream >> parsed.wrappedKey;
    stream >> parsed.payloadNonce;
    stream >> parsed.payloadCiphertext;

    if (stream.status() != QDataStream::Ok || !stream.atEnd() || magic != FileMagic.toByteArray()
        || parsed.formatVersion != VaultFileFormatVersion || parsed.vaultId.size() != 32
        || parsed.kdf != QLatin1String("argon2id")
        || parsed.salt.size() != crypto_pwhash_SALTBYTES || parsed.opsLimit == 0
        || parsed.memLimit == 0 || parsed.keyNonce.size() != AeadNonceSize
        || parsed.wrappedKey.size() <= crypto_aead_xchacha20poly1305_ietf_ABYTES
        || !parsed.payloadNonce.isEmpty() || !parsed.payloadCiphertext.isEmpty()) {
        *error = QStringLiteral("密钥信封格式无效");
        return false;
    }

    *header = parsed;
    return true;
}

} // namespace mempass
