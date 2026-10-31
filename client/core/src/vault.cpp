#include "mempass/vault.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <sodium.h>

#include <utility>

namespace mempass {
namespace {

QByteArray payloadAssociatedData(const QString &vaultId)
{
    return QByteArrayLiteral("mempass/vault-payload/v1/") + vaultId.toUtf8();
}

void clearAccounts(QList<Account> *accounts)
{
    for (Account &account : *accounts) {
        account.password.fill(QChar());
    }
    accounts->clear();
}

} // namespace

Vault::Vault(QObject *parent)
    : QObject(parent)
{
    initCrypto();
}

Vault::~Vault()
{
    lock();
}

bool Vault::create(const QString &path,
                   const QString &password,
                   const QString &deviceId,
                   QString *error)
{
    lock();
    if (path.isEmpty() || password.isEmpty() || deviceId.size() != 32) {
        *error = QStringLiteral("保险库创建参数无效");
        return false;
    }
    if (QFile::exists(path)) {
        *error = QStringLiteral("保险库文件已经存在");
        return false;
    }

    const QString vaultId = createId();
    m_dataKey = randomBytes(VaultDataKeySize);
    const CryptoResult created = createVaultHeader(vaultId,
                                                   password.toUtf8(),
                                                   m_dataKey,
                                                   crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                                   crypto_pwhash_MEMLIMIT_INTERACTIVE);
    if (!created.ok || !parseKeyEnvelope(created.data, &m_header, error)) {
        if (error->isEmpty()) {
            *error = created.error;
        }
        lock();
        return false;
    }

    const qint64 now = utcNowMs();
    m_data.formatVersion = 1;
    m_data.vaultId = vaultId;
    m_data.deviceId = deviceId;
    m_data.serverCursor = 0;
    m_data.keyEnvelopeUpdatedAtMs = now;
    m_data.keyEnvelopeDeviceId = deviceId;
    m_data.categories.clear();
    m_data.accounts.clear();

    Category defaultCategory;
    defaultCategory.id = createId();
    defaultCategory.name = QStringLiteral("default");
    defaultCategory.createdAtMs = now;
    defaultCategory.updatedAtMs = now;
    defaultCategory.deviceId = deviceId;
    defaultCategory.isDefault = true;
    defaultCategory.dirty = true;
    m_data.categories.append(defaultCategory);

    m_path = path;
    m_open = true;
    if (!save(error)) {
        lock();
        return false;
    }
    emit changed();
    return true;
}

bool Vault::open(const QString &path, const QString &password, QString *error)
{
    lock();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法打开保险库文件：%1").arg(file.errorString());
        return false;
    }
    const QByteArray serialized = file.readAll();
    file.close();

    VaultFileHeader header;
    if (!parseVaultFile(serialized, &header, error)) {
        return false;
    }
    if (!loadFromHeader(header, password.toUtf8(), error)) {
        return false;
    }
    m_path = path;
    return true;
}

bool Vault::restoreFromServer(const QString &path,
                              const QString &password,
                              const QString &deviceId,
                              const KeyEnvelope &keyEnvelope,
                              const QList<SyncRecord> &records,
                              QString *error)
{
    lock();
    if (path.isEmpty() || password.isEmpty() || deviceId.size() != 32) {
        *error = QStringLiteral("恢复参数无效");
        return false;
    }
    if (QFile::exists(path)) {
        *error = QStringLiteral("本地保险库文件已经存在");
        return false;
    }

    VaultFileHeader header;
    if (!parseKeyEnvelope(keyEnvelope.data, &header, error) || header.vaultId.size() != 32) {
        if (error->isEmpty()) {
            *error = QStringLiteral("服务端密钥信封无效");
        }
        return false;
    }
    const CryptoResult dataKey = unwrapVaultDataKey(header, password.toUtf8());
    if (!dataKey.ok) {
        *error = dataKey.error;
        return false;
    }

    m_path = path;
    m_header = header;
    m_dataKey = dataKey.data;
    m_data.formatVersion = 1;
    m_data.vaultId = header.vaultId;
    m_data.deviceId = deviceId;
    m_data.serverCursor = 0;
    m_data.keyEnvelopeUpdatedAtMs = keyEnvelope.updatedAtMs;
    m_data.keyEnvelopeDeviceId = keyEnvelope.deviceId;
    m_data.categories.clear();
    m_data.accounts.clear();
    m_open = true;

    for (const SyncRecord &record : records) {
        if (!applyRemoteRecord(record, error)) {
            lock();
            return false;
        }
    }
    if (!save(error)) {
        lock();
        return false;
    }
    emit changed();
    return true;
}

bool Vault::loadFromHeader(const VaultFileHeader &header,
                           const QByteArray &password,
                           QString *error)
{
    const CryptoResult dataKey = unwrapVaultDataKey(header, password);
    if (!dataKey.ok) {
        *error = dataKey.error;
        return false;
    }
    CryptoResult payload = decryptData(dataKey.data,
                                       header.payloadNonce + header.payloadCiphertext,
                                       payloadAssociatedData(header.vaultId));
    if (!payload.ok) {
        *error = payload.error;
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload.data, &parseError);
    sodium_memzero(payload.data.data(), static_cast<size_t>(payload.data.size()));
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("保险库数据格式无效");
        return false;
    }

    VaultData data;
    if (!vaultDataFromJson(document.object(), &data, error)) {
        return false;
    }
    if (data.vaultId != header.vaultId) {
        *error = QStringLiteral("保险库文件头与数据不一致");
        return false;
    }

    m_header = header;
    m_data = data;
    m_dataKey = dataKey.data;
    m_path.clear();
    m_open = true;
    return true;
}

bool Vault::writeFile(QString *error) const
{
    const QFileInfo info(m_path);
    QDir().mkpath(info.absolutePath());

    const QByteArray current = serializeVaultFile(m_header);
    if (QFile::exists(m_path)) {
        const QString backupPath = m_path + QStringLiteral(".bak");
        QFile::remove(backupPath);
        QFile::copy(m_path, backupPath);
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = QStringLiteral("无法写入保险库文件：%1").arg(file.errorString());
        return false;
    }
    if (file.write(current) != current.size()) {
        *error = QStringLiteral("保险库文件写入失败：%1").arg(file.errorString());
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        *error = QStringLiteral("保险库文件提交失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

bool Vault::save(QString *error)
{
    if (!m_open) {
        *error = QStringLiteral("保险库尚未解锁");
        return false;
    }

    const CryptoResult payload = encryptData(m_dataKey,
                                             QJsonDocument(vaultDataToJson(m_data))
                                                 .toJson(QJsonDocument::Compact),
                                             payloadAssociatedData(m_data.vaultId));
    if (!payload.ok) {
        *error = payload.error;
        return false;
    }
    m_header.payloadNonce = payload.data.left(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    m_header.payloadCiphertext = payload.data.mid(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    if (!writeFile(error)) {
        return false;
    }
    emit changed();
    return true;
}

bool Vault::changePassword(const QString &oldPassword,
                           const QString &newPassword,
                           QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("当前状态不允许修改主密码");
        return false;
    }
    if (newPassword.isEmpty()) {
        *error = QStringLiteral("新密码不能为空");
        return false;
    }

    const CryptoResult changed = changeHeaderPassword(m_header,
                                                      oldPassword.toUtf8(),
                                                      newPassword.toUtf8());
    if (!changed.ok) {
        *error = changed.error;
        return false;
    }
    VaultFileHeader header;
    if (!parseKeyEnvelope(changed.data, &header, error)) {
        return false;
    }
    header.payloadNonce = m_header.payloadNonce;
    header.payloadCiphertext = m_header.payloadCiphertext;
    m_header = header;
    m_data.keyEnvelopeUpdatedAtMs = nextTimestamp(m_data.keyEnvelopeUpdatedAtMs);
    m_data.keyEnvelopeDeviceId = m_data.deviceId;
    return save(error);
}

void Vault::lock()
{
    if (!m_dataKey.isEmpty()) {
        sodium_memzero(m_dataKey.data(), static_cast<size_t>(m_dataKey.size()));
        m_dataKey.clear();
    }
    clearAccounts(&m_data.accounts);
    m_data.categories.clear();
    m_data.vaultId.clear();
    m_data.deviceId.clear();
    m_data.serverCursor = 0;
    m_header = {};
    m_path.clear();
    m_open = false;
    m_syncing = false;
}

bool Vault::isOpen() const
{
    return m_open && !m_dataKey.isEmpty();
}

QString Vault::path() const
{
    return m_path;
}

QString Vault::vaultId() const
{
    return m_data.vaultId;
}

QString Vault::deviceId() const
{
    return m_data.deviceId;
}

VaultData &Vault::data()
{
    return m_data;
}

const VaultData &Vault::data() const
{
    return m_data;
}

QByteArray Vault::keyEnvelopeData() const
{
    return serializeKeyEnvelope(m_header);
}

KeyEnvelope Vault::keyEnvelope() const
{
    return {keyEnvelopeData(), m_data.keyEnvelopeUpdatedAtMs, m_data.keyEnvelopeDeviceId};
}

QByteArray Vault::dataKey() const
{
    return m_dataKey;
}

bool Vault::beginSync(QList<SyncRecord> *records, QString *error)
{
    records->clear();
    if (!ensureMutable()) {
        *error = QStringLiteral("当前状态不允许开始同步");
        return false;
    }

    for (const Category &category : std::as_const(m_data.categories)) {
        if (!category.dirty) {
            continue;
        }
        SyncRecord record;
        record.recordId = category.id;
        record.recordType = RecordType::Category;
        record.updatedAtMs = category.updatedAtMs;
        record.deviceId = category.deviceId;
        record.deleted = category.deleted;
        const CryptoResult encrypted = encryptSyncRecord(m_dataKey,
                                                         categoryPayloadToJson(category),
                                                         &record);
        if (!encrypted.ok) {
            *error = encrypted.error;
            return false;
        }
        records->append(record);
    }

    for (const Account &account : std::as_const(m_data.accounts)) {
        if (!account.dirty) {
            continue;
        }
        SyncRecord record;
        record.recordId = account.id;
        record.recordType = RecordType::Account;
        record.updatedAtMs = account.updatedAtMs;
        record.deviceId = account.deviceId;
        record.deleted = account.deleted;
        const CryptoResult encrypted = encryptSyncRecord(m_dataKey,
                                                         accountPayloadToJson(account),
                                                         &record);
        if (!encrypted.ok) {
            *error = encrypted.error;
            return false;
        }
        records->append(record);
    }

    m_syncing = true;
    return true;
}

bool Vault::applySyncResult(const VaultSyncState &state,
                            const QList<SyncRecord> &records,
                            QString *error)
{
    if (!m_open || !m_syncing) {
        *error = QStringLiteral("同步状态无效");
        return false;
    }
    // The server also has its own generated instance ID. The authoritative
    // client-side identity is embedded in the encrypted key envelope, so a
    // fresh server may use a different top-level ID until the first upload.
    for (const SyncRecord &record : records) {
        if (!applyRemoteRecord(record, error)) {
            return false;
        }
    }

    if (isNewerLwwKey(state.keyEnvelope.lwwKey(),
                      {m_data.keyEnvelopeUpdatedAtMs, m_data.keyEnvelopeDeviceId})) {
        VaultFileHeader header;
        if (!parseKeyEnvelope(state.keyEnvelope.data, &header, error)) {
            if (error->isEmpty()) {
                *error = QStringLiteral("服务端密钥信封无效");
            }
            return false;
        }
        if (header.vaultId != m_data.vaultId) {
            *error = QStringLiteral("服务端密钥信封属于另一个保险库，已停止同步");
            return false;
        }
        m_header = header;
        m_data.keyEnvelopeUpdatedAtMs = state.keyEnvelope.updatedAtMs;
        m_data.keyEnvelopeDeviceId = state.keyEnvelope.deviceId;
    }

    m_data.serverCursor = state.serverCursor;
    return true;
}

bool Vault::completeSync(bool success, QString *error)
{
    if (!m_syncing) {
        *error = QStringLiteral("当前没有正在进行的同步");
        return false;
    }
    m_syncing = false;
    if (!success) {
        return true;
    }
    for (Category &category : m_data.categories) {
        category.dirty = false;
    }
    for (Account &account : m_data.accounts) {
        account.dirty = false;
    }
    if (!save(error)) {
        return false;
    }
    emit changed();
    return true;
}

bool Vault::addCategory(const QString &name, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    if (name.trimmed().isEmpty()) {
        *error = QStringLiteral("分类名称不能为空");
        return false;
    }
    Category category;
    category.id = createId();
    category.name = name.trimmed();
    category.createdAtMs = utcNowMs();
    category.updatedAtMs = category.createdAtMs;
    category.deviceId = m_data.deviceId;
    category.dirty = true;
    m_data.categories.append(category);
    if (!save(error)) {
        m_data.categories.removeLast();
        return false;
    }
    emit changed();
    return true;
}

bool Vault::updateCategory(const QString &id, const QString &name, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    Category *category = findCategory(id);
    if (category == nullptr || category->deleted) {
        *error = QStringLiteral("分类不存在");
        return false;
    }
    if (name.trimmed().isEmpty()) {
        *error = QStringLiteral("分类名称不能为空");
        return false;
    }
    const QString oldName = category->name;
    category->name = name.trimmed();
    touchCategory(category);
    if (!save(error)) {
        category->name = oldName;
        return false;
    }
    emit changed();
    return true;
}

bool Vault::deleteCategory(const QString &id, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    const Category *selectedCategory = findCategory(id);
    if (selectedCategory == nullptr || selectedCategory->deleted) {
        *error = QStringLiteral("分类不存在");
        return false;
    }
    if (selectedCategory->isDefault) {
        *error = QStringLiteral("不允许删除默认分类");
        return false;
    }
    const Category *defaultCategory = nullptr;
    for (const Category &category : std::as_const(m_data.categories)) {
        if (!category.deleted && category.isDefault) {
            defaultCategory = &category;
            break;
        }
    }
    if (defaultCategory == nullptr) {
        *error = QStringLiteral("默认分类不存在");
        return false;
    }
    if (defaultCategory->deleted) {
        *error = QStringLiteral("分类不存在");
        return false;
    }

    Category *category = findCategory(id);
    category->deleted = true;
    touchCategory(category);
    for (Account &account : m_data.accounts) {
        if (!account.deleted && account.categoryId == id) {
            account.categoryId = defaultCategory->id;
            touchAccount(&account);
        }
    }
    if (!save(error)) {
        return false;
    }
    emit changed();
    return true;
}

bool Vault::addAccount(const Account &account, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    if (account.itemName.trimmed().isEmpty()) {
        *error = QStringLiteral("账户项不能为空");
        return false;
    }
    const Category *category = findCategory(account.categoryId);
    if (category == nullptr || category->deleted) {
        *error = QStringLiteral("所属分类不存在");
        return false;
    }

    Account result = account;
    if (result.id.isEmpty()) {
        result.id = createId();
    }
    result.itemName = result.itemName.trimmed();
    result.createdAtMs = utcNowMs();
    result.updatedAtMs = result.createdAtMs;
    result.deviceId = m_data.deviceId;
    result.deleted = false;
    result.dirty = true;
    m_data.accounts.append(result);
    if (!save(error)) {
        m_data.accounts.removeLast();
        return false;
    }
    emit changed();
    return true;
}

bool Vault::updateAccount(const Account &account, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    Account *current = findAccount(account.id);
    if (current == nullptr || current->deleted) {
        *error = QStringLiteral("账户不存在");
        return false;
    }
    if (account.itemName.trimmed().isEmpty()) {
        *error = QStringLiteral("账户项不能为空");
        return false;
    }
    const Category *category = findCategory(account.categoryId);
    if (category == nullptr || category->deleted) {
        *error = QStringLiteral("所属分类不存在");
        return false;
    }

    Account old = *current;
    current->categoryId = account.categoryId;
    current->itemName = account.itemName.trimmed();
    current->username = account.username;
    current->password = account.password;
    current->description = account.description;
    current->note = account.note;
    current->status = account.status;
    touchAccount(current);
    if (!save(error)) {
        *current = old;
        return false;
    }
    emit changed();
    return true;
}

bool Vault::deleteAccount(const QString &id, QString *error)
{
    if (!ensureMutable()) {
        *error = QStringLiteral("保险库尚未解锁或正在同步");
        return false;
    }
    Account *account = findAccount(id);
    if (account == nullptr || account->deleted) {
        *error = QStringLiteral("账户不存在");
        return false;
    }
    account->deleted = true;
    touchAccount(account);
    if (!save(error)) {
        return false;
    }
    emit changed();
    return true;
}

Category *Vault::findCategory(const QString &id)
{
    for (Category &category : m_data.categories) {
        if (category.id == id) {
            return &category;
        }
    }
    return nullptr;
}

Account *Vault::findAccount(const QString &id)
{
    for (Account &account : m_data.accounts) {
        if (account.id == id) {
            return &account;
        }
    }
    return nullptr;
}

const Category *Vault::findCategory(const QString &id) const
{
    for (const Category &category : m_data.categories) {
        if (category.id == id) {
            return &category;
        }
    }
    return nullptr;
}

const Account *Vault::findAccount(const QString &id) const
{
    for (const Account &account : m_data.accounts) {
        if (account.id == id) {
            return &account;
        }
    }
    return nullptr;
}

QString Vault::defaultVaultPath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QDir::homePath() + QStringLiteral("/mempass");
    }
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("vault.mempass"));
}

bool Vault::ensureMutable() const
{
    return m_open && !m_dataKey.isEmpty() && !m_syncing;
}

bool Vault::ensureLoaded(QString *error) const
{
    if (!isOpen()) {
        *error = QStringLiteral("保险库尚未解锁");
        return false;
    }
    return true;
}

bool Vault::applyRemoteRecord(const SyncRecord &record, QString *error)
{
    QJsonObject payload;
    const CryptoResult decrypted = decryptSyncRecord(m_dataKey, record, &payload);
    if (!decrypted.ok) {
        *error = QStringLiteral("无法解密同步记录 %1：%2")
                     .arg(record.recordId, decrypted.error);
        return false;
    }

    if (record.recordType == RecordType::Category) {
        Category candidate;
        if (!categoryFromPayload(payload, &candidate, error)) {
            return false;
        }
        if (candidate.id != record.recordId
            || candidate.updatedAtMs != record.updatedAtMs
            || candidate.deviceId != record.deviceId
            || candidate.deleted != record.deleted) {
            *error = QStringLiteral("同步记录元数据不一致");
            return false;
        }
        Category *current = findCategory(record.recordId);
        if (current != nullptr
            && !isNewerLwwKey(record.lwwKey(), {current->updatedAtMs, current->deviceId})) {
            return true;
        }
        candidate.dirty = false;
        if (current != nullptr) {
            *current = candidate;
        } else {
            m_data.categories.append(candidate);
        }
        return true;
    }

    Account candidate;
    if (!accountFromPayload(payload, &candidate, error)) {
        return false;
    }
    if (candidate.id != record.recordId
        || candidate.updatedAtMs != record.updatedAtMs
        || candidate.deviceId != record.deviceId
        || candidate.deleted != record.deleted) {
        *error = QStringLiteral("同步记录元数据不一致");
        return false;
    }
    Account *current = findAccount(record.recordId);
    if (current != nullptr
        && !isNewerLwwKey(record.lwwKey(), {current->updatedAtMs, current->deviceId})) {
        return true;
    }
    candidate.dirty = false;
    if (current != nullptr) {
        *current = candidate;
    } else {
        m_data.accounts.append(candidate);
    }
    return true;
}

qint64 Vault::nextTimestamp(qint64 previous) const
{
    return qMax(utcNowMs(), previous + 1);
}

void Vault::touchCategory(Category *category)
{
    category->updatedAtMs = nextTimestamp(category->updatedAtMs);
    category->deviceId = m_data.deviceId;
    category->dirty = true;
}

void Vault::touchAccount(Account *account)
{
    account->updatedAtMs = nextTimestamp(account->updatedAtMs);
    account->deviceId = m_data.deviceId;
    account->dirty = true;
}

} // namespace mempass
