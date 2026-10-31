#pragma once

#include "mempass/sync_protocol.h"

#include <QSqlDatabase>
#include <QString>

namespace mempass {

// SQLite storage for a single-user, single-vault sync server.
class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const QString &path, QString *error);
    bool initialize(const QString &vaultId, const QByteArray &tokenHash, QString *error);

    // Checks that the vault row exists and matches the configured identity.
    bool verify(const QString &vaultId, const QByteArray &tokenHash, QString *error);

    bool loadState(VaultSyncState *state, QString *error) const;

    // Applies the envelope when it wins the LWW conflict. Always returns the
    // latest vault state via state.
    bool applyKeyEnvelope(const KeyEnvelope &envelope, VaultSyncState *state, QString *error);

    // Applies per-record LWW updates inside one transaction and assigns an
    // increasing server cursor to every accepted record.
    bool pushRecords(const QList<SyncRecord> &records, qint64 *head, QString *error);

    bool pullRecords(qint64 after, int limit, QList<SyncRecord> *records, bool *hasMore, QString *error) const;

    // Current vault next_cursor, or -1 on error.
    qint64 head(QString *error) const;

    QString vaultId() const { return vaultId_; }
    QByteArray tokenHash() const { return tokenHash_; }

private:
    bool reloadHead(qint64 *head, QString *error) const;
    bool beginTransaction(QString *error);
    bool commitTransaction(QString *error);
    void rollbackTransaction();

    QSqlDatabase db_;
    QString vaultId_;
    QByteArray tokenHash_;
};

} // namespace mempass
