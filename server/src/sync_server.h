#pragma once

#include "database.h"

#include <QHttpServer>
#include <QString>
#include <QTcpServer>

#include <memory>

namespace mempass {

class SyncServer
{
public:
    SyncServer(Database *database, QString certPath, QString keyPath);

    bool listen(const QString &host, quint16 port, QString *error);

private:
    QHttpServerResponse handleState(const QHttpServerRequest &request) const;
    QHttpServerResponse handleKeyEnvelope(const QHttpServerRequest &request) const;
    QHttpServerResponse handlePush(const QHttpServerRequest &request) const;
    QHttpServerResponse handlePull(const QHttpServerRequest &request) const;

    bool authorize(const QHttpServerRequest &request, QString *error) const;

    Database *database_;
    QString certPath_;
    QString keyPath_;
    QHttpServer httpServer_;
    std::unique_ptr<QTcpServer> tcpServer_;
};

} // namespace mempass
