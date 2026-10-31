#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>

#include "mempass/sync_protocol.h"

namespace mempass {

class SyncService : public QObject
{
    Q_OBJECT

public:
    explicit SyncService(QObject *parent = nullptr);

    bool fetchState(const QUrl &url, const QString &accessToken, VaultSyncState *state, QString *error);
    bool downloadVault(const QUrl &url,
                       const QString &accessToken,
                       VaultSyncState *state,
                       QList<SyncRecord> *records,
                       QString *error);

    // Blocking helper for setup dialogs and tests. GUI synchronization should
    // use perform() on a worker thread instead.
    bool synchronize(const QUrl &url,
                     const QString &accessToken,
                     qint64 cursor,
                     const QByteArray &pushJson,
                     const QByteArray &keyEnvelopeJson,
                     QByteArray *resultJson,
                     QString *error);

public slots:
    void perform(const QUrl &url,
                 const QString &accessToken,
                 qint64 cursor,
                 const QByteArray &pushJson,
                 const QByteArray &keyEnvelopeJson);

signals:
    void finished(bool ok, const QString &error, const QByteArray &resultJson);

private:
    QByteArray get(const QUrl &url, const QString &accessToken, QString *error, int *status);
    QByteArray post(const QUrl &url,
                    const QString &accessToken,
                    const QByteArray &body,
                    QString *error,
                    int *status);
};

} // namespace mempass
