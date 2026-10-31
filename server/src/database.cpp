#include "database.h"

#include "mempass/models.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace mempass {
namespace {

const char kSchema[] = R"SQL(
CREATE TABLE IF NOT EXISTS vault_state(
    vault_id TEXT PRIMARY KEY,
    token_hash TEXT NOT NULL,
    key_envelope BLOB,
    envelope_updated_at INTEGER,
    envelope_device_id TEXT,
    next_cursor INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS records(
    vault_id TEXT NOT NULL,
    record_id TEXT NOT NULL,
    record_type TEXT NOT NULL,
    updated_at_ms INTEGER NOT NULL,
    device_id TEXT NOT NULL,
    deleted INTEGER NOT NULL,
    ciphertext BLOB NOT NULL,
    server_cursor INTEGER NOT NULL,
    PRIMARY KEY(vault_id, record_id)
);
CREATE INDEX IF NOT EXISTS idx_records_server_cursor ON records(vault_id, server_cursor);
)SQL";

bool execSql(QSqlDatabase &db, const QString &sql, QString *error)
{
    QSqlQuery query(db);
    if (!query.exec(sql)) {
        *error = QStringLiteral("数据库操作失败：%1").arg(query.lastError().text());
        return false;
    }
    return true;
}

bool execStatement(QSqlQuery *query, QString *error)
{
    if (!query->exec()) {
        *error = QStringLiteral("数据库操作失败：%1").arg(query->lastError().text());
        return false;
    }
    return true;
}

SyncRecord recordFromQuery(const QSqlQuery &query)
{
    SyncRecord record;
    record.recordId = query.value(0).toString();
    recordTypeFromString(query.value(1).toString(), &record.recordType);
    record.updatedAtMs = query.value(2).toLongLong();
    record.deviceId = query.value(3).toString();
    record.deleted = query.value(4).toInt() != 0;
    record.ciphertext = query.value(5).toByteArray();
    return record;
}

} // namespace

Database::~Database()
{
    if (db_.isValid()) {
        db_.close();
        QSqlDatabase::removeDatabase(db_.connectionName());
    }
}

bool Database::open(const QString &path, QString *error)
{
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db_.setDatabaseName(path);
    if (!db_.open()) {
        *error = QStringLiteral("无法打开数据库 %1：%2").arg(path, db_.lastError().text());
        return false;
    }

    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("PRAGMA journal_mode=WAL"))
        || !query.exec(QStringLiteral("PRAGMA busy_timeout=5000"))) {
        *error = QStringLiteral("无法设置数据库参数：%1").arg(query.lastError().text());
        return false;
    }

    for (const QString &statement : QString::fromLatin1(kSchema).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const QString trimmed = statement.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!execSql(db_, trimmed, error)) {
            return false;
        }
    }
    return true;
}

bool Database::initialize(const QString &vaultId, const QByteArray &tokenHash, QString *error)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO vault_state(vault_id, token_hash) VALUES(?, ?)"));
    query.addBindValue(vaultId);
    query.addBindValue(QString::fromLatin1(tokenHash));
    return execStatement(&query, error);
}

bool Database::verify(const QString &vaultId, const QByteArray &tokenHash, QString *error)
{
    QSqlQuery query(const_cast<QSqlDatabase &>(db_));
    query.prepare(QStringLiteral("SELECT vault_id, token_hash FROM vault_state"));
    if (!query.exec()) {
        *error = QStringLiteral("数据库操作失败：%1").arg(query.lastError().text());
        return false;
    }
    if (!query.next()) {
        *error = QStringLiteral("数据库尚未初始化，请先运行 --init");
        return false;
    }
    if (query.value(0).toString() != vaultId) {
        *error = QStringLiteral("数据库 vaultId 与配置不一致");
        return false;
    }
    if (query.value(1).toString().toLatin1() != tokenHash) {
        *error = QStringLiteral("数据库令牌哈希与配置不一致");
        return false;
    }
    if (query.next()) {
        *error = QStringLiteral("数据库包含多个 vault，仅支持单 vault");
        return false;
    }
    vaultId_ = vaultId;
    tokenHash_ = tokenHash;
    return true;
}

bool Database::reloadHead(qint64 *head, QString *error) const
{
    QSqlQuery query(const_cast<QSqlDatabase &>(db_));
    query.prepare(QStringLiteral("SELECT next_cursor FROM vault_state WHERE vault_id = ?"));
    query.addBindValue(vaultId_);
    if (!query.exec() || !query.next()) {
        *error = QStringLiteral("无法读取同步游标：%1").arg(query.lastError().text());
        return false;
    }
    *head = query.value(0).toLongLong();
    return true;
}

bool Database::beginTransaction(QString *error)
{
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        *error = QStringLiteral("无法开启事务：%1").arg(query.lastError().text());
        return false;
    }
    return true;
}

bool Database::commitTransaction(QString *error)
{
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("COMMIT"))) {
        *error = QStringLiteral("无法提交事务：%1").arg(query.lastError().text());
        return false;
    }
    return true;
}

void Database::rollbackTransaction()
{
    QSqlQuery query(db_);
    query.exec(QStringLiteral("ROLLBACK"));
}

bool Database::loadState(VaultSyncState *state, QString *error) const
{
    QSqlQuery query(const_cast<QSqlDatabase &>(db_));
    query.prepare(QStringLiteral(
        "SELECT key_envelope, envelope_updated_at, envelope_device_id, next_cursor "
        "FROM vault_state WHERE vault_id = ?"));
    query.addBindValue(vaultId_);
    if (!query.exec() || !query.next()) {
        *error = QStringLiteral("无法读取同步状态：%1").arg(query.lastError().text());
        return false;
    }

    state->vaultId = vaultId_;
    state->serverCursor = query.value(3).toLongLong();
    if (!query.value(0).isNull()) {
        state->keyEnvelope.data = query.value(0).toByteArray();
        state->keyEnvelope.updatedAtMs = query.value(1).toLongLong();
        state->keyEnvelope.deviceId = query.value(2).toString();
    }
    return true;
}

bool Database::applyKeyEnvelope(const KeyEnvelope &envelope, VaultSyncState *state, QString *error)
{
    if (!beginTransaction(error)) {
        return false;
    }

    bool applied = false;
    {
        QSqlQuery query(db_);
        query.prepare(QStringLiteral(
            "SELECT envelope_updated_at, envelope_device_id FROM vault_state WHERE vault_id = ?"));
        query.addBindValue(vaultId_);
        if (!query.exec() || !query.next()) {
            *error = QStringLiteral("无法读取密钥信封：%1").arg(query.lastError().text());
            rollbackTransaction();
            return false;
        }

        const LwwKey current{query.value(0).toLongLong(), query.value(1).toString()};
        if (!current.isValid() || isNewerLwwKey(envelope.lwwKey(), current)) {
            QSqlQuery update(db_);
            update.prepare(QStringLiteral(
                "UPDATE vault_state SET key_envelope = ?, envelope_updated_at = ?, envelope_device_id = ? "
                "WHERE vault_id = ?"));
            update.addBindValue(envelope.data);
            update.addBindValue(envelope.updatedAtMs);
            update.addBindValue(envelope.deviceId);
            update.addBindValue(vaultId_);
            if (!execStatement(&update, error)) {
                rollbackTransaction();
                return false;
            }
            applied = true;
        }
    }

    if (!commitTransaction(error)) {
        rollbackTransaction();
        return false;
    }

    if (!loadState(state, error)) {
        return false;
    }
    if (!applied) {
        qInfo().noquote() << QStringLiteral("密钥信封未更新：已存在相同或更新的版本");
    }
    return true;
}

bool Database::pushRecords(const QList<SyncRecord> &records, qint64 *head, QString *error)
{
    if (!beginTransaction(error)) {
        return false;
    }

    qint64 cursor = 0;
    bool ok = reloadHead(&cursor, error);
    if (ok) {
        QSqlQuery find(db_);
        find.prepare(QStringLiteral(
            "SELECT updated_at_ms, device_id FROM records WHERE vault_id = ? AND record_id = ?"));
        QSqlQuery upsert(db_);
        upsert.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO records"
            "(vault_id, record_id, record_type, updated_at_ms, device_id, deleted, ciphertext, server_cursor) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?)"));

        for (const SyncRecord &record : records) {
            find.bindValue(0, vaultId_);
            find.bindValue(1, record.recordId);
            if (!execStatement(&find, error)) {
                ok = false;
                break;
            }

            bool accept = true;
            if (find.next()) {
                const LwwKey current{find.value(0).toLongLong(), find.value(1).toString()};
                accept = isNewerLwwKey(record.lwwKey(), current);
            }
            find.finish();
            if (!accept) {
                continue;
            }

            upsert.bindValue(0, vaultId_);
            upsert.bindValue(1, record.recordId);
            upsert.bindValue(2, recordTypeToString(record.recordType));
            upsert.bindValue(3, record.updatedAtMs);
            upsert.bindValue(4, record.deviceId);
            upsert.bindValue(5, record.deleted ? 1 : 0);
            upsert.bindValue(6, record.ciphertext);
            upsert.bindValue(7, ++cursor);
            if (!execStatement(&upsert, error)) {
                ok = false;
                break;
            }
        }
    }

    if (ok) {
        QSqlQuery update(db_);
        update.prepare(QStringLiteral("UPDATE vault_state SET next_cursor = ? WHERE vault_id = ?"));
        update.addBindValue(cursor);
        update.addBindValue(vaultId_);
        ok = execStatement(&update, error);
    }

    if (!ok) {
        rollbackTransaction();
        return false;
    }
    if (!commitTransaction(error)) {
        rollbackTransaction();
        return false;
    }

    *head = cursor;
    return true;
}

bool Database::pullRecords(qint64 after,
                           int limit,
                           QList<SyncRecord> *records,
                           bool *hasMore,
                           QString *error) const
{
    QSqlQuery query(const_cast<QSqlDatabase &>(db_));
    query.prepare(QStringLiteral(
        "SELECT record_id, record_type, updated_at_ms, device_id, deleted, ciphertext "
        "FROM records WHERE vault_id = ? AND server_cursor > ? "
        "ORDER BY server_cursor ASC LIMIT ?"));
    query.addBindValue(vaultId_);
    query.addBindValue(after);
    query.addBindValue(limit + 1);
    if (!execStatement(&query, error)) {
        return false;
    }

    records->clear();
    while (query.next()) {
        records->append(recordFromQuery(query));
    }
    *hasMore = records->size() > limit;
    if (*hasMore) {
        records->removeLast();
    }
    return true;
}

qint64 Database::head(QString *error) const
{
    qint64 cursor = 0;
    if (!reloadHead(&cursor, error)) {
        return -1;
    }
    return cursor;
}

} // namespace mempass
