#pragma once

#include "mempass/crypto.h"

#include <QObject>
#include <QString>

namespace mempass {

class Vault : public QObject
{
    Q_OBJECT

public:
    explicit Vault(QObject *parent = nullptr);
    ~Vault() override;

    bool create(const QString &path,
                const QString &password,
                const QString &deviceId,
                QString *error);
    bool open(const QString &path, const QString &password, QString *error);
    bool restoreFromServer(const QString &path,
                           const QString &password,
                           const QString &deviceId,
                           const KeyEnvelope &keyEnvelope,
                           const QList<SyncRecord> &records,
                           QString *error);

    bool save(QString *error);
    bool changePassword(const QString &oldPassword,
                        const QString &newPassword,
                        QString *error);
    void lock();

    bool isOpen() const;
    QString path() const;
    QString vaultId() const;
    QString deviceId() const;
    VaultData &data();
    const VaultData &data() const;

    QByteArray keyEnvelopeData() const;
    KeyEnvelope keyEnvelope() const;
    QByteArray dataKey() const;

    bool beginSync(QList<SyncRecord> *records, QString *error);
    bool applySyncResult(const VaultSyncState &state,
                         const QList<SyncRecord> &records,
                         QString *error);
    bool completeSync(bool success, QString *error);

    bool addCategory(const QString &name, QString *error);
    bool updateCategory(const QString &id, const QString &name, QString *error);
    bool deleteCategory(const QString &id, QString *error);

    bool addAccount(const Account &account, QString *error);
    bool updateAccount(const Account &account, QString *error);
    bool deleteAccount(const QString &id, QString *error);

    Category *findCategory(const QString &id);
    Account *findAccount(const QString &id);
    const Category *findCategory(const QString &id) const;
    const Account *findAccount(const QString &id) const;

    static QString defaultVaultPath();

signals:
    void changed();

private:
    bool ensureMutable() const;
    bool ensureLoaded(QString *error) const;
    bool loadFromHeader(const VaultFileHeader &header,
                        const QByteArray &password,
                        QString *error);
    bool writeFile(QString *error) const;
    bool applyRemoteRecord(const SyncRecord &record, QString *error);
    qint64 nextTimestamp(qint64 previous) const;
    void touchCategory(Category *category);
    void touchAccount(Account *account);

    QString m_path;
    VaultFileHeader m_header;
    VaultData m_data;
    QByteArray m_dataKey;
    bool m_open = false;
    bool m_syncing = false;
};

} // namespace mempass
