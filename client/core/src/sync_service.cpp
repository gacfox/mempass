#include "mempass/sync_service.h"

#include "mempass/sync_protocol.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace mempass {
namespace {

QUrl endpoint(const QUrl &base, const QString &path)
{
    QUrl result = base;
    QString basePath = result.path();
    while (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    result.setPath(basePath + path);
    return result;
}

QString replyError(const QByteArray &body)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (document.isObject()) {
        const QString message = document.object().value(QStringLiteral("error")).toString();
        if (!message.isEmpty()) {
            return message;
        }
    }
    return QString::fromUtf8(body.left(500));
}

} // namespace

SyncService::SyncService(QObject *parent)
    : QObject(parent)
{
}

bool SyncService::fetchState(const QUrl &url,
                             const QString &accessToken,
                             VaultSyncState *state,
                             QString *error)
{
    int status = 0;
    const QByteArray body = get(endpoint(url, QStringLiteral("/api/v1/sync/state")),
                                accessToken,
                                error,
                                &status);
    if (body.isEmpty() || status < 200 || status >= 300) {
        if (error->isEmpty()) {
            *error = QStringLiteral("读取同步状态失败");
        }
        return false;
    }
    return syncStateFromJson(QJsonDocument::fromJson(body).object(), state, error);
}

bool SyncService::downloadVault(const QUrl &url,
                                const QString &accessToken,
                                VaultSyncState *state,
                                QList<SyncRecord> *records,
                                QString *error)
{
    records->clear();
    if (!fetchState(url, accessToken, state, error)) {
        return false;
    }

    qint64 cursor = 0;
    qint64 serverHead = state->serverCursor;
    bool hasMore = true;
    while (hasMore) {
        QUrl pullUrl = endpoint(url, QStringLiteral("/api/v1/sync/pull"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("after"), QString::number(cursor));
        query.addQueryItem(QStringLiteral("limit"), QString::number(200));
        pullUrl.setQuery(query);

        int status = 0;
        const QByteArray body = get(pullUrl, accessToken, error, &status);
        QList<SyncRecord> page;
        if (body.isEmpty() || status < 200 || status >= 300
            || !syncPullResponseFromJson(QJsonDocument::fromJson(body).object(),
                                         &page,
                                         &serverHead,
                                         &hasMore,
                                         error)) {
            if (error->isEmpty()) {
                *error = QStringLiteral("拉取数据失败");
            }
            return false;
        }
        records->append(page);
        cursor = serverHead;
    }
    state->serverCursor = serverHead;
    return true;
}

bool SyncService::synchronize(const QUrl &url,
                              const QString &accessToken,
                              qint64 cursor,
                              const QByteArray &pushJson,
                              const QByteArray &keyEnvelopeJson,
                              QByteArray *resultJson,
                              QString *error)
{
    bool finished = false;
    bool succeeded = false;
    QString finishedError;
    QByteArray finishedBody;

    const QMetaObject::Connection connection = connect(
        this,
        &SyncService::finished,
        this,
        [&](bool ok, const QString &message, const QByteArray &body) {
            finished = true;
            succeeded = ok;
            finishedError = message;
            finishedBody = body;
        });

    perform(url, accessToken, cursor, pushJson, keyEnvelopeJson);
    disconnect(connection);

    if (!finished) {
        *error = QStringLiteral("同步服务没有返回结果");
        return false;
    }
    if (!succeeded) {
        *error = finishedError;
        return false;
    }
    *resultJson = finishedBody;
    return true;
}

void SyncService::perform(const QUrl &url,
                          const QString &accessToken,
                          qint64 cursor,
                          const QByteArray &pushJson,
                          const QByteArray &keyEnvelopeJson)
{
    QString error;
    int status = 0;
    QNetworkAccessManager manager;

    const QByteArray initialBody = get(endpoint(url, QStringLiteral("/api/v1/sync/state")),
                                       accessToken,
                                       &error,
                                       &status);
    VaultSyncState initialState;
    if (initialBody.isEmpty() || status < 200 || status >= 300
        || !syncStateFromJson(QJsonDocument::fromJson(initialBody).object(),
                              &initialState,
                              &error)) {
        emit finished(false,
                      error.isEmpty() ? QStringLiteral("读取同步状态失败") : error,
                      {});
        return;
    }

    if (!keyEnvelopeJson.isEmpty()) {
        const QByteArray envelopeBody = post(endpoint(url,
                                                      QStringLiteral("/api/v1/sync/key-envelope")),
                                             accessToken,
                                             keyEnvelopeJson,
                                             &error,
                                             &status);
        if (envelopeBody.isEmpty() || status < 200 || status >= 300) {
            emit finished(false,
                          error.isEmpty() ? QStringLiteral("上传密钥信封失败") : error,
                          {});
            return;
        }
    }

    if (!pushJson.isEmpty()) {
        const QByteArray pushBody = post(endpoint(url, QStringLiteral("/api/v1/sync/push")),
                                         accessToken,
                                         pushJson,
                                         &error,
                                         &status);
        if (pushBody.isEmpty() || status < 200 || status >= 300) {
            emit finished(false,
                          error.isEmpty() ? QStringLiteral("推送数据失败") : error,
                          {});
            return;
        }
    }

    QList<SyncRecord> pulledRecords;
    qint64 pullCursor = cursor;
    qint64 serverHead = cursor;
    bool hasMore = true;
    while (hasMore) {
        QUrl pullUrl = endpoint(url, QStringLiteral("/api/v1/sync/pull"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("after"), QString::number(pullCursor));
        query.addQueryItem(QStringLiteral("limit"), QString::number(200));
        pullUrl.setQuery(query);

        const QByteArray pullBody = get(pullUrl, accessToken, &error, &status);
        QList<SyncRecord> page;
        if (pullBody.isEmpty() || status < 200 || status >= 300
            || !syncPullResponseFromJson(QJsonDocument::fromJson(pullBody).object(),
                                         &page,
                                         &serverHead,
                                         &hasMore,
                                         &error)) {
            emit finished(false,
                          error.isEmpty() ? QStringLiteral("拉取数据失败") : error,
                          {});
            return;
        }
        pulledRecords.append(page);
        pullCursor = serverHead;
    }

    // Re-read the metadata after the pull loop and repeat if another client
    // committed records while pagination was running.
    VaultSyncState finalState;
    const QByteArray finalBody = get(endpoint(url, QStringLiteral("/api/v1/sync/state")),
                                     accessToken,
                                     &error,
                                     &status);
    if (finalBody.isEmpty() || status < 200 || status >= 300
        || !syncStateFromJson(QJsonDocument::fromJson(finalBody).object(), &finalState, &error)) {
        emit finished(false,
                      error.isEmpty() ? QStringLiteral("读取最终同步状态失败") : error,
                      {});
        return;
    }
    while (finalState.serverCursor > serverHead) {
        QUrl pullUrl = endpoint(url, QStringLiteral("/api/v1/sync/pull"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("after"), QString::number(serverHead));
        query.addQueryItem(QStringLiteral("limit"), QString::number(200));
        pullUrl.setQuery(query);
        const QByteArray pullBody = get(pullUrl, accessToken, &error, &status);
        QList<SyncRecord> page;
        if (pullBody.isEmpty() || status < 200 || status >= 300
            || !syncPullResponseFromJson(QJsonDocument::fromJson(pullBody).object(),
                                         &page,
                                         &serverHead,
                                         &hasMore,
                                         &error)) {
            emit finished(false,
                          error.isEmpty() ? QStringLiteral("补充拉取数据失败") : error,
                          {});
            return;
        }
        pulledRecords.append(page);
    }
    finalState.serverCursor = serverHead;

    QJsonObject result;
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("state"), syncStateToJson(finalState));
    result.insert(QStringLiteral("records"), syncRecordsToJson(pulledRecords));
    emit finished(true, {}, QJsonDocument(result).toJson(QJsonDocument::Compact));
}

QByteArray SyncService::get(const QUrl &url,
                            const QString &accessToken,
                            QString *error,
                            int *status)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        *error = QStringLiteral("网络请求失败：%1").arg(reply->errorString());
        if (!body.isEmpty()) {
            const QString serverError = replyError(body);
            if (!serverError.isEmpty()) {
                *error += QStringLiteral("；服务端：%1").arg(serverError);
            }
        }
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();
    return body;
}

QByteArray SyncService::post(const QUrl &url,
                             const QString &accessToken,
                             const QByteArray &body,
                             QString *error,
                             int *status)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", "Bearer " + accessToken.toUtf8());

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    *status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray response = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || *status < 200 || *status >= 300) {
        const QString serverError = replyError(response);
        *error = QStringLiteral("HTTP %1").arg(*status);
        if (!serverError.isEmpty()) {
            *error += QStringLiteral("：%1").arg(serverError);
        } else if (reply->error() != QNetworkReply::NoError) {
            *error = QStringLiteral("网络请求失败：%1").arg(reply->errorString());
        }
        reply->deleteLater();
        return {};
    }
    reply->deleteLater();
    return response;
}

} // namespace mempass
