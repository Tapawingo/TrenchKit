#include "NexusModsClient.h"
#include <QSettings>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

NexusModsClient::NexusModsClient(QObject *parent)
    : QObject(parent)
{
    loadApiKey();
}

bool NexusModsClient::hasApiKey() const {
    return !m_apiKey.isEmpty();
}

void NexusModsClient::setApiKey(const QString &key) {
    m_apiKey = key;
    saveApiKey();
}

void NexusModsClient::clearApiKey() {
    m_apiKey.clear();
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    settings.remove(QStringLiteral("nexus/apiKey"));
}

void NexusModsClient::saveApiKey() {
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    settings.setValue(QStringLiteral("nexus/apiKey"), m_apiKey);
}

void NexusModsClient::loadApiKey() {
    QSettings settings(QStringLiteral("TrenchKit"), QStringLiteral("FoxholeModManager"));
    m_apiKey = settings.value(QStringLiteral("nexus/apiKey")).toString();
}

void NexusModsClient::getModInfo(const QString &modId) {
    if (m_modInfoReply) {
        m_modInfoReply->abort();
        m_modInfoReply->deleteLater();
    }

    QUrl url(QStringLiteral("%1/games/%2/mods/%3.json")
                .arg(API_BASE, GAME_DOMAIN, modId));

    m_modInfoReply = m_nam.get(makeRequest(url));

    connect(m_modInfoReply, &QNetworkReply::finished, this, [this]() {
        if (!m_modInfoReply) {
            return;
        }

        QNetworkReply *reply = m_modInfoReply;
        m_modInfoReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        handleRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401) {
                clearApiKey();
                emit errorOccurred(QStringLiteral("Invalid API key. Please authenticate again."));
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to fetch mod info"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("Invalid response format from Nexus Mods API"));
            reply->deleteLater();
            return;
        }

        QJsonObject modObj = doc.object();
        QString author = modObj[QStringLiteral("author")].toString();
        QString description = modObj[QStringLiteral("summary")].toString();
        QString version = modObj[QStringLiteral("version")].toString();

        emit modInfoReceived(author, description, version);
        reply->deleteLater();
    });
}

void NexusModsClient::getModFiles(const QString &modId) {
    if (m_filesReply) {
        m_filesReply->abort();
        m_filesReply->deleteLater();
    }

    QUrl url(QStringLiteral("%1/games/%2/mods/%3/files.json")
                .arg(API_BASE, GAME_DOMAIN, modId));

    m_filesReply = m_nam.get(makeRequest(url));

    connect(m_filesReply, &QNetworkReply::finished, this, [this]() {
        if (!m_filesReply) {
            return;
        }

        QNetworkReply *reply = m_filesReply;
        m_filesReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        handleRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401) {
                clearApiKey();
                emit errorOccurred(QStringLiteral("Invalid API key. Please authenticate again."));
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to fetch mod files"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit errorOccurred(QStringLiteral("Invalid response format from Nexus Mods API"));
            reply->deleteLater();
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray filesArray = rootObj[QStringLiteral("files")].toArray();
        QList<NexusFileInfo> files;

        for (const QJsonValue &value : filesArray) {
            if (value.isObject()) {
                files.append(NexusFileInfo::fromJson(value.toObject()));
            }
        }

        QJsonArray fileUpdatesArray = rootObj[QStringLiteral("file_updates")].toArray();
        qDebug() << "Found" << fileUpdatesArray.size() << "file_updates entries";

        for (const QJsonValue &updateValue : fileUpdatesArray) {
            if (!updateValue.isObject()) continue;

            QJsonObject updateObj = updateValue.toObject();
            QString oldFileId = QString::number(updateObj[QStringLiteral("old_file_id")].toInt());
            QString newFileId = QString::number(updateObj[QStringLiteral("new_file_id")].toInt());

            if (oldFileId == "0" || newFileId == "0") continue;

            for (NexusFileInfo &file : files) {
                if (file.id == newFileId) {
                    file.fileUpdates.append(qMakePair(oldFileId, newFileId));
                    qDebug() << "  File" << newFileId << "replaces" << oldFileId;
                    break;
                }
            }
        }

        emit modFilesReceived(files);
        reply->deleteLater();
    });
}

void NexusModsClient::getDownloadLink(const QString &modId, const QString &fileId) {
    if (m_linkReply) {
        m_linkReply->abort();
        m_linkReply->deleteLater();
    }

    QUrl url(QStringLiteral("%1/games/%2/mods/%3/files/%4/download_link.json")
                .arg(API_BASE, GAME_DOMAIN, modId, fileId));

    m_linkReply = m_nam.get(makeRequest(url));

    connect(m_linkReply, &QNetworkReply::finished, this, [this]() {
        if (!m_linkReply) {
            return;
        }

        QNetworkReply *reply = m_linkReply;
        m_linkReply = nullptr;

        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        handleRateLimitHeaders(reply);

        if (reply->error() != QNetworkReply::NoError) {
            if (httpStatus == 401) {
                clearApiKey();
                emit errorOccurred(QStringLiteral("Invalid API key. Please authenticate again."));
            } else if (httpStatus == 403) {
                // Parse the error response to check if it's actually a premium requirement
                QJsonDocument errorDoc = QJsonDocument::fromJson(body);
                QString errorMessage = errorDoc.object()[QStringLiteral("message")].toString();

                qDebug() << "403 error from Nexus Mods API. Response body:" << body;
                qDebug() << "Error message:" << errorMessage;

                // Only treat as premium requirement if the error message indicates it
                if (errorMessage.contains("premium", Qt::CaseInsensitive)) {
                    emit errorOccurred(QStringLiteral("PREMIUM_REQUIRED"));
                } else {
                    emit errorOccurred(formatNetworkError(QStringLiteral("Access denied"), reply, body));
                }
            } else {
                emit errorOccurred(formatNetworkError(QStringLiteral("Failed to get download link"), reply, body));
            }
            reply->deleteLater();
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isArray() || doc.array().isEmpty()) {
            emit errorOccurred(QStringLiteral("No download links available"));
            reply->deleteLater();
            return;
        }

        QString downloadUrl = doc.array().first().toObject()[QStringLiteral("URI")].toString();
        if (downloadUrl.isEmpty()) {
            emit errorOccurred(QStringLiteral("Invalid download link response"));
            reply->deleteLater();
            return;
        }

        emit downloadLinkReceived(downloadUrl);
        reply->deleteLater();
    });
}

void NexusModsClient::downloadFile(const QUrl &url, const QString &savePath) {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
    }

    if (m_downloadFile.isOpen()) {
        m_downloadFile.close();
    }

    m_downloadFile.setFileName(savePath);
    if (!m_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(QStringLiteral("Failed to open file for writing: ") + savePath);
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TrenchKit/1.0.1"));

    m_downloadReply = m_nam.get(request);

    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit downloadProgress(received, total);
    });

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_downloadReply || !m_downloadFile.isOpen()) {
            return;
        }
        m_downloadFile.write(m_downloadReply->readAll());
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
            emit errorOccurred(formatNetworkError(QStringLiteral("Download failed"), reply, QByteArray()));
            QFile::remove(savePath);
            reply->deleteLater();
            return;
        }

        emit downloadFinished(savePath);
        reply->deleteLater();
    });
}

void NexusModsClient::cancelDownload() {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    if (m_downloadFile.isOpen()) {
        QString path = m_downloadFile.fileName();
        m_downloadFile.close();
        QFile::remove(path);
    }
}

QNetworkRequest NexusModsClient::makeRequest(const QUrl &url) const {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TrenchKit/1.0.1"));
    request.setRawHeader("apikey", m_apiKey.toUtf8());
    request.setTransferTimeout(30000);
    return request;
}

QString NexusModsClient::formatNetworkError(const QString &context,
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

void NexusModsClient::handleRateLimitHeaders(QNetworkReply *reply) {
    if (!reply) {
        return;
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus == 429) {
        bool ok = false;
        int resetTime = reply->rawHeader("X-RL-Daily-Reset").toInt(&ok);
        if (ok) {
            emit rateLimitExceeded(resetTime);
        } else {
            emit rateLimitExceeded(3600);
        }
    }
}
