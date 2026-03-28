#include "ItchClient.h"
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

ItchClient::ItchClient(QObject *parent)
    : QObject(parent)
{
    loadApiKey();
}

bool ItchClient::hasApiKey() const {
    return !m_apiKey.isEmpty();
}

void ItchClient::setApiKey(const QString &key) {
    m_apiKey = key;
    saveApiKey();
}

void ItchClient::clearApiKey() {
    m_apiKey.clear();
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    settings.remove(QStringLiteral("itch/apiKey"));
}

void ItchClient::saveApiKey() {
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    settings.setValue(QStringLiteral("itch/apiKey"), m_apiKey);
}

void ItchClient::loadApiKey() {
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    m_apiKey = settings.value(QStringLiteral("itch/apiKey")).toString();
}

void ItchClient::getGameId(const QString &creator, const QString &gameName) {
    if (m_gameIdReply) {
        m_gameIdReply->abort();
        m_gameIdReply->deleteLater();
    }

    // data.json is public, no auth required
    QUrl url(QStringLiteral("https://%1.itch.io/%2/data.json").arg(creator, gameName));

    m_gameIdReply = m_nam.get(makeRequest(url, false));

    connect(m_gameIdReply, &QNetworkReply::finished, this, [this]() {
        if (!m_gameIdReply) {
            return;
        }

        QNetworkReply *reply = m_gameIdReply;
        m_gameIdReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 404) {
                emit errorOccurred(QStringLiteral("Game not found. Check URL and try again."));
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to fetch game ID"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("Invalid response format from itch.io"));
            reply->deleteLater();
            return;
        }

        QJsonObject gameObj = doc.object();

        if (!gameObj.contains("id")) {
            emit errorOccurred(QStringLiteral("Unable to find game ID in response"));
            reply->deleteLater();
            return;
        }

        QString gameId = QString::number(gameObj["id"].toInt());
        QString title = gameObj["title"].toString();

        // Extract author from user object
        QString author;
        if (gameObj.contains("user") && gameObj["user"].isObject()) {
            QJsonObject userObj = gameObj["user"].toObject();
            author = userObj["name"].toString();
        }

        emit gameIdReceived(gameId, title, author);
        reply->deleteLater();
    });
}

void ItchClient::getGameUploads(const QString &gameId) {
    if (m_uploadsReply) {
        m_uploadsReply->abort();
        m_uploadsReply->deleteLater();
    }

    QUrl url(QStringLiteral("%1/%2/game/%3/uploads")
                .arg(API_BASE, m_apiKey, gameId));

    m_uploadsReply = m_nam.get(makeRequest(url));

    connect(m_uploadsReply, &QNetworkReply::finished, this, [this]() {
        if (!m_uploadsReply) {
            return;
        }

        QNetworkReply *reply = m_uploadsReply;
        m_uploadsReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401 || httpStatus == 403) {
                clearApiKey();
                emit errorOccurred(QStringLiteral("Invalid API key. Please authenticate again."));
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to fetch uploads"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("Invalid response format from itch.io"));
            reply->deleteLater();
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray uploadsArray = rootObj["uploads"].toArray();
        QList<ItchUploadInfo> uploads;

        for (const QJsonValue &value : uploadsArray) {
            if (value.isObject()) {
                uploads.append(ItchUploadInfo::fromJson(value.toObject()));
            }
        }

        emit uploadsReceived(uploads);
        reply->deleteLater();
    });
}

void ItchClient::getDownloadLink(const QString &uploadId) {
    if (m_linkReply) {
        m_linkReply->abort();
        m_linkReply->deleteLater();
    }

    QUrl url(QStringLiteral("%1/%2/upload/%3/download")
                .arg(API_BASE, m_apiKey, uploadId));

    m_linkReply = m_nam.get(makeRequest(url));

    connect(m_linkReply, &QNetworkReply::finished, this, [this]() {
        if (!m_linkReply) {
            return;
        }

        QNetworkReply *reply = m_linkReply;
        m_linkReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401 || httpStatus == 403) {
                clearApiKey();
                emit errorOccurred(QStringLiteral("Invalid API key. Please authenticate again."));
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to get download link"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("Invalid response format from itch.io"));
            reply->deleteLater();
            return;
        }

        QJsonObject linkObj = doc.object();

        if (!linkObj.contains("url")) {
            emit errorOccurred(QStringLiteral("Download URL not found in response"));
            reply->deleteLater();
            return;
        }

        QString downloadUrl = linkObj["url"].toString();
        emit downloadLinkReceived(downloadUrl);
        reply->deleteLater();
    });
}

void ItchClient::downloadFile(const QUrl &url, const QString &savePath) {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }

    if (m_downloadFile.isOpen()) {
        m_downloadFile.close();
    }

    m_downloadFile.setFileName(savePath);
    if (!m_downloadFile.open(QIODevice::WriteOnly)) {
        emit errorOccurred(QStringLiteral("Failed to create file: ") + savePath);
        return;
    }

    m_downloadReply = m_nam.get(makeRequest(url, false));

    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &ItchClient::downloadProgress);

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadReply && m_downloadFile.isOpen()) {
            m_downloadFile.write(m_downloadReply->readAll());
        }
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this, savePath]() {
        if (!m_downloadReply) {
            return;
        }

        QNetworkReply *reply = m_downloadReply;
        m_downloadReply = nullptr;

        if (m_downloadFile.isOpen()) {
            m_downloadFile.write(reply->readAll());
            m_downloadFile.close();
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QByteArray body;  // Body already written to file
            emit errorOccurred(formatNetworkError(QStringLiteral("Download failed"), reply, body));

            // Clean up partial file
            if (QFile::exists(savePath)) {
                QFile::remove(savePath);
            }

            reply->deleteLater();
            return;
        }

        emit downloadFinished(savePath);
        reply->deleteLater();
    });
}

void ItchClient::cancelDownload() {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_downloadFile.isOpen()) {
        QString filePath = m_downloadFile.fileName();
        m_downloadFile.close();

        // Remove partial download
        if (QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
    }
}

QNetworkRequest ItchClient::makeRequest(const QUrl &url, bool useAuth) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TrenchKit/1.0.1"));
    request.setTransferTimeout(30000);

    if (useAuth && !m_apiKey.isEmpty()) {
        // itch.io API uses Bearer token authentication
        request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());
    }

    return request;
}

QString ItchClient::formatNetworkError(const QString &context,
                                      QNetworkReply *reply,
                                      const QByteArray &body) const {
    QStringList parts;
    parts << context;

    if (reply) {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus > 0) {
            parts << QStringLiteral("HTTP %1").arg(httpStatus);
        }

        const QString err = reply->errorString().trimmed();
        if (!err.isEmpty()) {
            parts << err;
        }
    }

    const QString snippet = QString::fromUtf8(body.left(200)).trimmed();
    if (!snippet.isEmpty()) {
        QString cleaned = snippet;
        cleaned.replace('\n', ' ');
        cleaned.replace('\r', ' ');
        parts << QStringLiteral("Response: %1").arg(cleaned);
    }

    return parts.join(QStringLiteral(" - "));
}
