#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace mempass {

struct CommandLineOptions {
    bool help = false;
    bool init = false;
    QString dataDir;
    QString host;
    int port = 0;
    QString configPath;
};

struct ServerConfig {
    QString vaultId;
    QByteArray tokenHash;
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 8080;
    QString dbPath;
    QString certPath;
    QString keyPath;
    QString configPath;
};

bool parseCommandLine(const QStringList &arguments, CommandLineOptions *options, QString *error);

// Initializes the data directory, database and server.json. The generated
// access token is returned via accessToken and must be printed exactly once.
bool runInit(const CommandLineOptions &options,
             ServerConfig *config,
             QString *accessToken,
             QString *error);

bool loadConfig(const QString &path, ServerConfig *config, QString *error);

QString usageText();

} // namespace mempass
