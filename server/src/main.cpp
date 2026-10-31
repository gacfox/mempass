#include "config.h"
#include "database.h"
#include "sync_server.h"

#include <QCoreApplication>
#include <QDebug>

#include <cstdio>

namespace {

void consoleMessageHandler(QtMsgType, const QMessageLogContext &, const QString &message)
{
    const QByteArray local = message.toLocal8Bit();
    fwrite(local.constData(), 1, static_cast<size_t>(local.size()), stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler(consoleMessageHandler);
    QCoreApplication::setApplicationName(QStringLiteral("mempass_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    mempass::CommandLineOptions options;
    QString error;
    if (!mempass::parseCommandLine(app.arguments(), &options, &error)) {
        qCritical().noquote() << error;
        qInfo().noquote() << mempass::usageText();
        return 2;
    }
    if (options.help) {
        qInfo().noquote() << mempass::usageText();
        return 0;
    }

    mempass::ServerConfig config;
    if (options.init) {
        QString accessToken;
        if (!mempass::runInit(options, &config, &accessToken, &error)) {
            qCritical().noquote() << QStringLiteral("初始化失败：%1").arg(error);
            return 1;
        }
        qInfo().noquote() << QStringLiteral("初始化完成，配置文件：%1").arg(config.configPath);
        qInfo().noquote() << QStringLiteral("请立即保存以下访问令牌（仅显示一次）：");
        qInfo().noquote() << accessToken;
        return 0;
    }

    const QString configPath =
        options.configPath.isEmpty() ? QStringLiteral("./server.json") : options.configPath;
    if (!mempass::loadConfig(configPath, &config, &error)) {
        qCritical().noquote() << QStringLiteral("读取配置失败：%1").arg(error);
        return 1;
    }
    if (config.tokenHash.isEmpty()) {
        qCritical().noquote() << QStringLiteral("配置缺少 tokenHash，拒绝启动");
        return 1;
    }

    mempass::Database database;
    if (!database.open(config.dbPath, &error)) {
        qCritical().noquote() << error;
        return 1;
    }
    if (!database.verify(config.vaultId, config.tokenHash, &error)) {
        qCritical().noquote() << error;
        return 1;
    }

    mempass::SyncServer server(&database, config.certPath, config.keyPath);
    if (!server.listen(config.host, config.port, &error)) {
        qCritical().noquote() << error;
        return 1;
    }

    const bool tls = !config.certPath.isEmpty();
    qInfo().noquote() << QStringLiteral("服务已启动：%1://%2:%3（vaultId=%4）")
                             .arg(tls ? QStringLiteral("https") : QStringLiteral("http"),
                                  config.host)
                             .arg(config.port)
                             .arg(config.vaultId);
    return QCoreApplication::exec();
}
