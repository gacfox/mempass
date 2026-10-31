#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace mempass {

enum class RecordType {
    Category,
    Account
};

enum class AccountStatus {
    Available = 1,
    Unavailable = 2,
    Deprecated = 3,
    Cancelled = 4
};

struct Category {
    QString id;
    QString name;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
    QString deviceId;
    bool deleted = false;
    bool isDefault = false;

    // Local-only synchronization metadata. It is stored in the encrypted
    // local vault, but never sent inside a record payload.
    bool dirty = false;
};

struct Account {
    QString id;
    QString categoryId;
    QString itemName;
    QString username;
    QString password;
    QString description;
    QString note;
    int status = static_cast<int>(AccountStatus::Available);
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
    QString deviceId;
    bool deleted = false;

    // Local-only synchronization metadata.
    bool dirty = false;
};

struct VaultData {
    int formatVersion = 1;
    QString vaultId;
    QString deviceId;
    qint64 serverCursor = 0;
    qint64 keyEnvelopeUpdatedAtMs = 0;
    QString keyEnvelopeDeviceId;
    QList<Category> categories;
    QList<Account> accounts;
};

QString createId();
QString recordTypeToString(RecordType type);
bool recordTypeFromString(const QString &value, RecordType *type);
QString accountStatusToString(int status);
bool accountStatusFromString(const QString &value, int *status);

QJsonObject categoryPayloadToJson(const Category &category);
bool categoryFromPayload(const QJsonObject &object, Category *category, QString *error);
QJsonObject accountPayloadToJson(const Account &account);
bool accountFromPayload(const QJsonObject &object, Account *account, QString *error);

QJsonObject vaultDataToJson(const VaultData &data);
bool vaultDataFromJson(const QJsonObject &object, VaultData *data, QString *error);

QString formatTimestamp(qint64 timestampMs);

} // namespace mempass
