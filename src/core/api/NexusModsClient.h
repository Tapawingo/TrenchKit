/**
 * @file NexusModsClient.h
 * @brief NexusMods REST API v1 client for mod info, file listings, and downloads.
 */
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QFile>
#include "core/models/NexusFileInfo.h"

/**
 * @brief HTTP client for the NexusMods v1 API targeting the Foxhole game domain.
 *
 * All operations require a valid API key set via @c setApiKey(). A typical
 * download flow chains four signals: @c modInfoReceived → @c modFilesReceived
 * → @c downloadLinkReceived → @c downloadFinished.
 */
class NexusModsClient final : public QObject {
    Q_OBJECT

public:
    explicit NexusModsClient(QObject *parent = nullptr);
    ~NexusModsClient() override = default;

    bool hasApiKey() const;
    void setApiKey(const QString &key);
    void clearApiKey();
    QString getApiKey() const { return m_apiKey; }

public slots:
    /**
     * @brief Fetches mod author, description, and version; emits @c modInfoReceived.
     */
    void getModInfo(const QString &modId);
    /**
     * @brief Fetches all downloadable files for a mod; emits @c modFilesReceived.
     */
    void getModFiles(const QString &modId);
    /**
     * @brief Requests a CDN download URL for a specific file; emits @c downloadLinkReceived.
     */
    void getDownloadLink(const QString &modId, const QString &fileId);
    /**
     * @brief Downloads a file to @p savePath; emits @c downloadProgress and @c downloadFinished.
     */
    void downloadFile(const QUrl &url, const QString &savePath);
    void cancelDownload();

signals:
    /**
     * @brief Emitted after @c getModInfo() completes.
     */
    void modInfoReceived(QString author, QString description, QString version);
    /**
     * @brief Emitted after @c getModFiles() completes with the full file list.
     */
    void modFilesReceived(QList<NexusFileInfo> files);
    /**
     * @brief Emitted after @c getDownloadLink() with the CDN URL.
     */
    void downloadLinkReceived(QString downloadUrl);
    void downloadProgress(qint64 received, qint64 total);
    /**
     * @brief Emitted on successful download; @p savePath is the local file path.
     */
    void downloadFinished(QString savePath);
    void errorOccurred(QString message);
    /**
     * @brief Emitted when the API rate limit is hit; @p secondsUntilReset is the wait time.
     */
    void rateLimitExceeded(int secondsUntilReset);

private:
    QNetworkRequest makeRequest(const QUrl &url) const;
    QString formatNetworkError(const QString &context,
                              QNetworkReply *reply,
                              const QByteArray &body) const;
    void saveApiKey();
    void loadApiKey();
    void handleRateLimitHeaders(QNetworkReply *reply);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_modInfoReply;
    QPointer<QNetworkReply> m_filesReply;
    QPointer<QNetworkReply> m_linkReply;
    QPointer<QNetworkReply> m_downloadReply;

    QString m_apiKey;
    QFile m_downloadFile;

    static constexpr const char* GAME_DOMAIN = "foxhole"; ///< NexusMods game domain slug.
    static constexpr const char* API_BASE = "https://api.nexusmods.com/v1";
};
