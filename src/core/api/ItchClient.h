/// @file ItchClient.h
/// @brief itch.io REST API v1 client for game info, upload listings, and downloads.
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QFile>
#include "core/models/ItchUploadInfo.h"

/// @brief HTTP client for the itch.io API v1.
///
/// All authenticated requests require an API key set via @c setApiKey(). A
/// typical download flow chains: @c gameIdReceived → @c uploadsReceived →
/// @c downloadLinkReceived → @c downloadFinished.
class ItchClient final : public QObject {
    Q_OBJECT

public:
    explicit ItchClient(QObject *parent = nullptr);
    ~ItchClient() override = default;

    bool hasApiKey() const;
    void setApiKey(const QString &key);
    void clearApiKey();
    QString getApiKey() const { return m_apiKey; }

public slots:
    /// @brief Resolves a creator/game slug to a numeric game ID; emits @c gameIdReceived.
    void getGameId(const QString &creator, const QString &gameName);
    /// @brief Fetches the upload list for a game; emits @c uploadsReceived.
    void getGameUploads(const QString &gameId);
    /// @brief Requests a download URL for an upload; emits @c downloadLinkReceived.
    void getDownloadLink(const QString &uploadId);
    /// @brief Downloads a file to @p savePath; emits @c downloadProgress and @c downloadFinished.
    void downloadFile(const QUrl &url, const QString &savePath);
    void cancelDownload();

signals:
    /// @brief Emitted after @c getGameId() with the numeric ID, title, and author.
    void gameIdReceived(QString gameId, QString gameTitle, QString author);
    /// @brief Emitted after @c getGameUploads() with the full upload list.
    void uploadsReceived(QList<ItchUploadInfo> uploads);
    /// @brief Emitted after @c getDownloadLink() with the CDN URL.
    void downloadLinkReceived(QString downloadUrl);
    void downloadProgress(qint64 received, qint64 total);
    /// @brief Emitted on successful download; @p savePath is the local file path.
    void downloadFinished(QString savePath);
    void errorOccurred(QString message);

private:
    QNetworkRequest makeRequest(const QUrl &url, bool useAuth = true) const;
    QString formatNetworkError(const QString &context,
                              QNetworkReply *reply,
                              const QByteArray &body) const;
    void saveApiKey();
    void loadApiKey();

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_gameIdReply;
    QPointer<QNetworkReply> m_uploadsReply;
    QPointer<QNetworkReply> m_linkReply;
    QPointer<QNetworkReply> m_downloadReply;

    QString m_apiKey;
    QFile m_downloadFile;

    static constexpr const char* API_BASE = "https://itch.io/api/1";
};
