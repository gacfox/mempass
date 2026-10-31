#include "config.h"

#include "database.h"

#include "mempass/models.h"
#include "mempass/sync_protocol.h"

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace mempass {
namespace {

bool takeValue(const QStringList &arguments, int *index, const QString &name, QString *value, QString *error)
{
    const QString &argument = arguments.at(*index);
    const int equals = argument.indexOf(QLatin1Char('='));
    if (equals >= 0) {
        *value = argument.mid(equals + 1);
        return true;
    }
    if (*index + 1 >= arguments.size()) {
        *error = QStringLiteral("参数 %1 缺少值").arg(name);
        return false;
    }
    *value = arguments.at(++(*index));
    return true;
}

bool parsePort(const QString &text, int *port, QString *error)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < 1 || value > 65535) {
        *error = QStringLiteral("端口无效：%1").arg(text);
        return false;
    }
    *port = value;
    return true;
}

QJsonObject configToJson(const ServerConfig &config)
{
    return {
        {QStringLiteral("vaultId"), config.vaultId},
        {QStringLiteral("tokenHash"), QString::fromLatin1(config.tokenHash)},
        {QStringLiteral("host"), config.host},
        {QStringLiteral("port"), static_cast<int>(config.port)},
        {QStringLiteral("dbPath"), config.dbPath},
        {QStringLiteral("certPath"), config.certPath},
        {QStringLiteral("keyPath"), config.keyPath},
    };
}

bool saveConfig(const QString &path, const ServerConfig &config, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = QStringLiteral("无法写入配置文件 %1：%2").arg(path, file.errorString());
        return false;
    }
    file.write(QJsonDocument(configToJson(config)).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

QString usageText()
{
    return QStringLiteral(
        "用法：\n"
        "  mempass_server --init --data-dir <dir> [--host <host>] [--port <port>]\n"
        "  mempass_server [--config <file>]\n"
        "\n"
        "选项：\n"
        "  --init            初始化数据目录、数据库并生成访问令牌\n"
        "  --data-dir <dir>  数据目录（初始化时创建 server.json 与 vault.db）\n"
        "  --host <host>     监听地址，默认 127.0.0.1\n"
        "  --port <port>     监听端口，默认 8080\n"
        "  --config <file>   配置文件路径，默认 ./server.json\n"
        "  -h, --help        显示帮助\n");
}

bool parseCommandLine(const QStringList &arguments, CommandLineOptions *options, QString *error)
{
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        QString value;
        if (argument == QLatin1String("-h") || argument == QLatin1String("--help")) {
            options->help = true;
        } else if (argument.startsWith(QLatin1String("--init"))) {
            options->init = true;
        } else if (argument.startsWith(QLatin1String("--data-dir"))) {
            if (!takeValue(arguments, &i, QStringLiteral("--data-dir"), &value, error)) {
                return false;
            }
            options->dataDir = value;
        } else if (argument.startsWith(QLatin1String("--host"))) {
            if (!takeValue(arguments, &i, QStringLiteral("--host"), &value, error)) {
                return false;
            }
            options->host = value;
        } else if (argument.startsWith(QLatin1String("--port"))) {
            if (!takeValue(arguments, &i, QStringLiteral("--port"), &value, error)) {
                return false;
            }
            if (!parsePort(value, &options->port, error)) {
                return false;
            }
        } else if (argument.startsWith(QLatin1String("--config"))) {
            if (!takeValue(arguments, &i, QStringLiteral("--config"), &value, error)) {
                return false;
            }
            options->configPath = value;
        } else {
            *error = QStringLiteral("未知参数：%1").arg(argument);
            return false;
        }
    }
    return true;
}

bool runInit(const CommandLineOptions &options,
             ServerConfig *config,
             QString *accessToken,
             QString *error)
{
    if (options.dataDir.isEmpty()) {
        *error = QStringLiteral("初始化时必须指定 --data-dir");
        return false;
    }

    const QDir dataDir(options.dataDir);
    if (!dataDir.mkpath(QStringLiteral("."))) {
        *error = QStringLiteral("无法创建数据目录：%1").arg(options.dataDir);
        return false;
    }

    config->vaultId = createId();
    *accessToken = generateAccessToken();
    config->tokenHash = hashAccessToken(accessToken->toUtf8());
    config->host = options.host.isEmpty() ? QStringLiteral("127.0.0.1") : options.host;
    config->port = options.port > 0 ? static_cast<quint16>(options.port) : quint16(8080);
    config->dbPath = dataDir.filePath(QStringLiteral("vault.db"));
    config->configPath = dataDir.filePath(QStringLiteral("server.json"));

    Database database;
    if (!database.open(config->dbPath, error)) {
        return false;
    }
    if (!database.initialize(config->vaultId, config->tokenHash, error)) {
        return false;
    }
    return saveConfig(config->configPath, *config, error);
}

bool loadConfig(const QString &path, ServerConfig *config, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("无法读取配置文件 %1：%2").arg(path, file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("配置文件不是有效的 JSON：%1").arg(parseError.errorString());
        return false;
    }

    const QJsonObject object = document.object();
    config->configPath = path;
    config->vaultId = object.value(QStringLiteral("vaultId")).toString();
    config->tokenHash = object.value(QStringLiteral("tokenHash")).toString().toLatin1();
    config->host = object.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
    config->dbPath = object.value(QStringLiteral("dbPath")).toString();
    config->certPath = object.value(QStringLiteral("certPath")).toString();
    config->keyPath = object.value(QStringLiteral("keyPath")).toString();

    const int port = object.value(QStringLiteral("port")).toInt(8080);
    if (port < 1 || port > 65535) {
        *error = QStringLiteral("配置端口无效：%1").arg(port);
        return false;
    }
    config->port = static_cast<quint16>(port);

    if (config->vaultId.size() != 32) {
        *error = QStringLiteral("配置缺少有效的 vaultId");
        return false;
    }
    if (config->tokenHash.isEmpty()) {
        *error = QStringLiteral("配置缺少 tokenHash，拒绝启动");
        return false;
    }
    if (config->dbPath.isEmpty()) {
        *error = QStringLiteral("配置缺少 dbPath");
        return false;
    }
    if (QHostAddress(config->host).isNull()) {
        *error = QStringLiteral("配置主机地址无效：%1").arg(config->host);
        return false;
    }
    if (config->certPath.isEmpty() != config->keyPath.isEmpty()) {
        *error = QStringLiteral("证书路径与私钥路径必须同时配置或同时为空");
        return false;
    }
    return true;
}

} // namespace mempass
