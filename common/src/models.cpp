#include "mempass/models.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimeZone>
#include <QUuid>

namespace mempass {
namespace {

QString readString(const QJsonObject &object, const QString &key, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isString()) {
        *error = QStringLiteral("字段 %1 缺失或类型错误").arg(key);
        return {};
    }
    return value.toString();
}

qint64 readInteger(const QJsonObject &object, const QString &key, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        *error = QStringLiteral("字段 %1 缺失或类型错误").arg(key);
        return 0;
    }
    return static_cast<qint64>(value.toDouble());
}

bool readBool(const QJsonObject &object, const QString &key, bool defaultValue, QString *error)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return defaultValue;
    }
    if (!value.isBool()) {
        *error = QStringLiteral("字段 %1 类型错误").arg(key);
        return defaultValue;
    }
    return value.toBool();
}

QJsonObject readObject(const QJsonObject &object, const QString &key, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isObject()) {
        *error = QStringLiteral("字段 %1 缺失或类型错误").arg(key);
        return {};
    }
    return value.toObject();
}

QJsonArray readArray(const QJsonObject &object, const QString &key, QString *error)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray()) {
        *error = QStringLiteral("字段 %1 缺失或类型错误").arg(key);
        return {};
    }
    return value.toArray();
}

} // namespace

QString createId()
{
    return QUuid::createUuid().toString(QUuid::Id128);
}

QString recordTypeToString(RecordType type)
{
    switch (type) {
    case RecordType::Category:
        return QStringLiteral("category");
    case RecordType::Account:
        return QStringLiteral("account");
    }
    return {};
}

bool recordTypeFromString(const QString &value, RecordType *type)
{
    if (value == QLatin1String("category")) {
        *type = RecordType::Category;
        return true;
    }
    if (value == QLatin1String("account")) {
        *type = RecordType::Account;
        return true;
    }
    return false;
}

QString accountStatusToString(int status)
{
    switch (static_cast<AccountStatus>(status)) {
    case AccountStatus::Available:
        return QStringLiteral("available");
    case AccountStatus::Unavailable:
        return QStringLiteral("unavailable");
    case AccountStatus::Deprecated:
        return QStringLiteral("deprecated");
    case AccountStatus::Cancelled:
        return QStringLiteral("cancelled");
    }
    return QStringLiteral("available");
}

bool accountStatusFromString(const QString &value, int *status)
{
    if (value == QLatin1String("available")) {
        *status = static_cast<int>(AccountStatus::Available);
    } else if (value == QLatin1String("unavailable")) {
        *status = static_cast<int>(AccountStatus::Unavailable);
    } else if (value == QLatin1String("deprecated")) {
        *status = static_cast<int>(AccountStatus::Deprecated);
    } else if (value == QLatin1String("cancelled")) {
        *status = static_cast<int>(AccountStatus::Cancelled);
    } else {
        return false;
    }
    return true;
}

QJsonObject categoryPayloadToJson(const Category &category)
{
    return {
        {QStringLiteral("id"), category.id},
        {QStringLiteral("name"), category.name},
        {QStringLiteral("createdAtMs"), category.createdAtMs},
        {QStringLiteral("updatedAtMs"), category.updatedAtMs},
        {QStringLiteral("deviceId"), category.deviceId},
        {QStringLiteral("deleted"), category.deleted},
        {QStringLiteral("default"), category.isDefault},
    };
}

bool categoryFromPayload(const QJsonObject &object, Category *category, QString *error)
{
    error->clear();
    Category result;
    result.id = readString(object, QStringLiteral("id"), error);
    result.name = readString(object, QStringLiteral("name"), error);
    result.createdAtMs = readInteger(object, QStringLiteral("createdAtMs"), error);
    result.updatedAtMs = readInteger(object, QStringLiteral("updatedAtMs"), error);
    result.deviceId = readString(object, QStringLiteral("deviceId"), error);
    result.deleted = readBool(object, QStringLiteral("deleted"), false, error);
    result.isDefault = readBool(object, QStringLiteral("default"), false, error);
    if (!error->isEmpty()) {
        return false;
    }
    if (result.id.isEmpty() || result.deviceId.isEmpty() || result.createdAtMs <= 0
        || result.updatedAtMs <= 0) {
        *error = QStringLiteral("分类数据不完整");
        return false;
    }
    *category = result;
    return true;
}

QJsonObject accountPayloadToJson(const Account &account)
{
    return {
        {QStringLiteral("id"), account.id},
        {QStringLiteral("categoryId"), account.categoryId},
        {QStringLiteral("itemName"), account.itemName},
        {QStringLiteral("username"), account.username},
        {QStringLiteral("password"), account.password},
        {QStringLiteral("description"), account.description},
        {QStringLiteral("note"), account.note},
        {QStringLiteral("status"), accountStatusToString(account.status)},
        {QStringLiteral("createdAtMs"), account.createdAtMs},
        {QStringLiteral("updatedAtMs"), account.updatedAtMs},
        {QStringLiteral("deviceId"), account.deviceId},
        {QStringLiteral("deleted"), account.deleted},
    };
}

bool accountFromPayload(const QJsonObject &object, Account *account, QString *error)
{
    error->clear();
    Account result;
    result.id = readString(object, QStringLiteral("id"), error);
    result.categoryId = readString(object, QStringLiteral("categoryId"), error);
    result.itemName = readString(object, QStringLiteral("itemName"), error);
    result.username = readString(object, QStringLiteral("username"), error);
    result.password = readString(object, QStringLiteral("password"), error);
    result.description = readString(object, QStringLiteral("description"), error);
    result.note = readString(object, QStringLiteral("note"), error);
    const QString statusText = readString(object, QStringLiteral("status"), error);
    result.createdAtMs = readInteger(object, QStringLiteral("createdAtMs"), error);
    result.updatedAtMs = readInteger(object, QStringLiteral("updatedAtMs"), error);
    result.deviceId = readString(object, QStringLiteral("deviceId"), error);
    result.deleted = readBool(object, QStringLiteral("deleted"), false, error);
    if (!error->isEmpty()) {
        return false;
    }
    if (!accountStatusFromString(statusText, &result.status)) {
        *error = QStringLiteral("账号状态无效");
        return false;
    }
    if (result.id.isEmpty() || result.categoryId.isEmpty() || result.deviceId.isEmpty()
        || result.createdAtMs <= 0 || result.updatedAtMs <= 0) {
        *error = QStringLiteral("账号数据不完整");
        return false;
    }
    *account = result;
    return true;
}

QJsonObject categoryLocalToJson(const Category &category)
{
    QJsonObject object = categoryPayloadToJson(category);
    object.insert(QStringLiteral("dirty"), category.dirty);
    return object;
}

bool categoryFromLocal(const QJsonObject &object, Category *category, QString *error)
{
    if (!categoryFromPayload(object, category, error)) {
        return false;
    }
    category->dirty = readBool(object, QStringLiteral("dirty"), true, error);
    return error->isEmpty();
}

QJsonObject accountLocalToJson(const Account &account)
{
    QJsonObject object = accountPayloadToJson(account);
    object.insert(QStringLiteral("dirty"), account.dirty);
    return object;
}

bool accountFromLocal(const QJsonObject &object, Account *account, QString *error)
{
    if (!accountFromPayload(object, account, error)) {
        return false;
    }
    account->dirty = readBool(object, QStringLiteral("dirty"), true, error);
    return error->isEmpty();
}

QJsonObject vaultDataToJson(const VaultData &data)
{
    QJsonArray categories;
    for (const Category &category : data.categories) {
        categories.append(categoryLocalToJson(category));
    }

    QJsonArray accounts;
    for (const Account &account : data.accounts) {
        accounts.append(accountLocalToJson(account));
    }

    return {
        {QStringLiteral("formatVersion"), data.formatVersion},
        {QStringLiteral("vaultId"), data.vaultId},
        {QStringLiteral("deviceId"), data.deviceId},
        {QStringLiteral("serverCursor"), data.serverCursor},
        {QStringLiteral("keyEnvelopeUpdatedAtMs"), data.keyEnvelopeUpdatedAtMs},
        {QStringLiteral("keyEnvelopeDeviceId"), data.keyEnvelopeDeviceId},
        {QStringLiteral("categories"), categories},
        {QStringLiteral("accounts"), accounts},
    };
}

bool vaultDataFromJson(const QJsonObject &object, VaultData *data, QString *error)
{
    error->clear();
    VaultData result;
    result.formatVersion = static_cast<int>(readInteger(object, QStringLiteral("formatVersion"), error));
    result.vaultId = readString(object, QStringLiteral("vaultId"), error);
    result.deviceId = readString(object, QStringLiteral("deviceId"), error);
    result.serverCursor = readInteger(object, QStringLiteral("serverCursor"), error);
    result.keyEnvelopeUpdatedAtMs = readInteger(object, QStringLiteral("keyEnvelopeUpdatedAtMs"), error);
    result.keyEnvelopeDeviceId = readString(object, QStringLiteral("keyEnvelopeDeviceId"), error);
    if (!error->isEmpty()) {
        return false;
    }
    if (result.formatVersion != 1 || result.vaultId.isEmpty() || result.deviceId.isEmpty()
        || result.serverCursor < 0 || result.keyEnvelopeUpdatedAtMs <= 0
        || result.keyEnvelopeDeviceId.isEmpty()) {
        *error = QStringLiteral("本地保险库数据不完整或版本不受支持");
        return false;
    }

    const QJsonArray categories = readArray(object, QStringLiteral("categories"), error);
    if (!error->isEmpty()) {
        return false;
    }
    result.categories.reserve(categories.size());
    for (const QJsonValue &value : categories) {
        if (!value.isObject()) {
            *error = QStringLiteral("分类数组包含无效元素");
            return false;
        }
        Category category;
        if (!categoryFromLocal(value.toObject(), &category, error)) {
            return false;
        }
        result.categories.append(category);
    }

    const QJsonArray accounts = readArray(object, QStringLiteral("accounts"), error);
    if (!error->isEmpty()) {
        return false;
    }
    result.accounts.reserve(accounts.size());
    for (const QJsonValue &value : accounts) {
        if (!value.isObject()) {
            *error = QStringLiteral("账号数组包含无效元素");
            return false;
        }
        Account account;
        if (!accountFromLocal(value.toObject(), &account, error)) {
            return false;
        }
        result.accounts.append(account);
    }

    *data = result;
    return true;
}

QString formatTimestamp(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::UTC)
        .toLocalTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

} // namespace mempass
