#include "sync_server.h"

#include "mempass/sync_protocol.h"

#include <QFile>
#include <QHostAddress>
#include <QHttpHeaders>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QTcpServer>
#include <QUrlQuery>

namespace mempass {
namespace {

QHttpServerResponse errorResponse(const QString &message, QHttpServerResponse::StatusCode status)
{
    return QHttpServerResponse(QJsonObject{{QStringLiteral("error"), message}}, status);
}

QHttpServerResponse badRequest(const QString &message)
{
    return errorResponse(message, QHttpServerResponse::StatusCode::BadRequest);
}

QHttpServerResponse unauthorized(const QString &message)
{
    return errorResponse(message, QHttpServerResponse::StatusCode::Unauthorized);
}

QHttpServerResponse serverError(const QString &message)
{
    return errorResponse(message, QHttpServerResponse::StatusCode::InternalServerError);
}

bool parseBody(const QHttpServerRequest &request, QJsonObject *object, QString *error)
{
    const QJsonDocument document = QJsonDocument::fromJson(request.body());
    if (!document.isObject()) {
        *error = QStringLiteral("请求体不是有效的 JSON 对象");
        return false;
    }
    *object = document.object();
    return true;
}

} // namespace

SyncServer::SyncServer(Database *database, QString certPath, QString keyPath)
    : database_(database)
    , certPath_(std::move(certPath))
    , keyPath_(std::move(keyPath))
{
    httpServer_.route(QStringLiteral("/api/v1/health"), []() {
        return QHttpServerResponse(QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")}});
    });

    httpServer_.route(QStringLiteral("/api/v1/sync/state"),
                      QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest &request) {
                          return handleState(request);
                      });
    httpServer_.route(QStringLiteral("/api/v1/sync/key-envelope"),
                      QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                          return handleKeyEnvelope(request);
                      });
    httpServer_.route(QStringLiteral("/api/v1/sync/push"),
                      QHttpServerRequest::Method::Post,
                      [this](const QHttpServerRequest &request) {
                          return handlePush(request);
                      });
    httpServer_.route(QStringLiteral("/api/v1/sync/pull"),
                      QHttpServerRequest::Method::Get,
                      [this](const QHttpServerRequest &request) {
                          return handlePull(request);
                      });
}

bool SyncServer::authorize(const QHttpServerRequest &request, QString *error) const
{
    const QByteArray authorization =
        request.headers().value(QHttpHeaders::WellKnownHeader::Authorization).toByteArray();
    if (!authorization.startsWith(QByteArrayLiteral("Bearer "))) {
        *error = QStringLiteral("缺少 Bearer 访问令牌");
        return false;
    }
    const QByteArray token = authorization.mid(7).trimmed();
    if (token.isEmpty() || !secureCompare(hashAccessToken(token), database_->tokenHash())) {
        *error = QStringLiteral("访问令牌无效");
        return false;
    }
    return true;
}

bool SyncServer::listen(const QString &host, quint16 port, QString *error)
{
    std::unique_ptr<QTcpServer> server;
    if (certPath_.isEmpty() && keyPath_.isEmpty()) {
        server = std::make_unique<QTcpServer>();
    } else {
        if (certPath_.isEmpty() || keyPath_.isEmpty()) {
            *error = QStringLiteral("证书路径与私钥路径必须同时配置");
            return false;
        }

        QFile certFile(certPath_);
        if (!certFile.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("无法读取证书文件 %1：%2").arg(certPath_, certFile.errorString());
            return false;
        }
        const QSslCertificate certificate(&certFile, QSsl::Pem);
        if (certificate.isNull()) {
            *error = QStringLiteral("证书文件无效：%1").arg(certPath_);
            return false;
        }

        QFile keyFile(keyPath_);
        if (!keyFile.open(QIODevice::ReadOnly)) {
            *error = QStringLiteral("无法读取私钥文件 %1：%2").arg(keyPath_, keyFile.errorString());
            return false;
        }
        const QByteArray keyData = keyFile.readAll();
        QSslKey key(keyData, QSsl::Rsa, QSsl::Pem);
        if (key.isNull()) {
            key = QSslKey(keyData, QSsl::Ec, QSsl::Pem);
        }
        if (key.isNull()) {
            *error = QStringLiteral("私钥文件无效（仅支持 PEM 格式）：%1").arg(keyPath_);
            return false;
        }

        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setLocalCertificate(certificate);
        sslConfig.setPrivateKey(key);

        auto sslServer = std::make_unique<QSslServer>();
        sslServer->setSslConfiguration(sslConfig);
        server = std::move(sslServer);
    }

    if (!server->listen(QHostAddress(host), port)) {
        *error = QStringLiteral("无法绑定 %1:%2：%3")
                     .arg(host)
                     .arg(port)
                     .arg(server->errorString());
        return false;
    }
    if (!httpServer_.bind(server.get())) {
        *error = QStringLiteral("无法绑定 HTTP 服务到 %1:%2").arg(host).arg(port);
        return false;
    }
    tcpServer_ = std::move(server);
    return true;
}

QHttpServerResponse SyncServer::handleState(const QHttpServerRequest &request) const
{
    QString error;
    if (!authorize(request, &error)) {
        return unauthorized(error);
    }

    VaultSyncState state;
    if (!database_->loadState(&state, &error)) {
        return serverError(error);
    }
    QJsonObject body = syncStateToJson(state);
    if (state.keyEnvelope.data.isEmpty()) {
        // A fresh server has no key envelope yet. Omit the field instead of
        // returning an invalid empty envelope object.
        body.remove(QStringLiteral("keyEnvelope"));
    }
    return QHttpServerResponse(body);
}

QHttpServerResponse SyncServer::handleKeyEnvelope(const QHttpServerRequest &request) const
{
    QString error;
    if (!authorize(request, &error)) {
        return unauthorized(error);
    }

    QJsonObject body;
    if (!parseBody(request, &body, &error)) {
        return badRequest(error);
    }

    KeyEnvelope envelope;
    if (!keyEnvelopeFromJson(body, &envelope, &error)) {
        return badRequest(error);
    }

    VaultSyncState state;
    if (!database_->applyKeyEnvelope(envelope, &state, &error)) {
        return serverError(error);
    }
    return QHttpServerResponse(syncStateToJson(state));
}

QHttpServerResponse SyncServer::handlePush(const QHttpServerRequest &request) const
{
    QString error;
    if (!authorize(request, &error)) {
        return unauthorized(error);
    }

    QJsonObject body;
    if (!parseBody(request, &body, &error)) {
        return badRequest(error);
    }

    QList<SyncRecord> records;
    if (!syncPushRequestFromJson(body, &records, &error)) {
        return badRequest(error);
    }
    qint64 head = 0;
    if (!database_->pushRecords(records, &head, &error)) {
        return serverError(error);
    }
    return QHttpServerResponse(QJsonObject{{QStringLiteral("serverCursor"), head}});
}

QHttpServerResponse SyncServer::handlePull(const QHttpServerRequest &request) const
{
    QString error;
    if (!authorize(request, &error)) {
        return unauthorized(error);
    }

    qint64 after = 0;
    int limit = 200;
    const QUrlQuery query = request.query();
    if (query.hasQueryItem(QStringLiteral("after"))) {
        bool ok = false;
        after = query.queryItemValue(QStringLiteral("after")).toLongLong(&ok);
        if (!ok || after < 0) {
            return badRequest(QStringLiteral("after 参数无效"));
        }
    }
    if (query.hasQueryItem(QStringLiteral("limit"))) {
        bool ok = false;
        limit = query.queryItemValue(QStringLiteral("limit")).toInt(&ok);
        if (!ok || limit < 1 || limit > 500) {
            return badRequest(QStringLiteral("limit 参数必须在 1 到 500 之间"));
        }
    }

    QList<SyncRecord> records;
    bool hasMore = false;
    if (!database_->pullRecords(after, limit, &records, &hasMore, &error)) {
        return serverError(error);
    }
    const qint64 head = database_->head(&error);
    if (head < 0) {
        return serverError(error);
    }
    return QHttpServerResponse(syncPullResponseToJson(records, head, hasMore));
}

} // namespace mempass
