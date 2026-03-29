/// @file UpdaterService.h
/// @brief Self-update service that checks GitHub releases and downloads update assets.
#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QPointer>
#include <QFile>
#include <QDateTime>

/// @brief Checks for new application releases on GitHub and downloads update assets.
///
/// Uses the GitHub Releases API. Comparison follows semantic versioning
/// (spec items 9 and 11): pre-release versions sort lower than their
/// associated release.
class UpdaterService final : public QObject {
    Q_OBJECT
public:
    /// @brief Semantic version per https://semver.org spec items 9 and 11.
    struct SemVer {
        int major = 0;
        int minor = 0;
        int patch = 0;
        QString preRelease; ///< Pre-release label (e.g. "alpha", "rc.1"); empty for stable releases.

        bool isNull() const { return major == 0 && minor == 0 && patch == 0 && preRelease.isEmpty(); }
        bool isPreRelease() const { return !preRelease.isEmpty(); }

        QString toString() const;
        /// @brief Parses a version string or tag (e.g. "v1.2.0-rc.1").
        static SemVer fromString(const QString& s);

        /// @brief Spec item 11: returns negative if a < b, 0 if equal, positive if a > b.
        static int compare(const SemVer& a, const SemVer& b);
    };

    /// @brief A single downloadable file attached to a GitHub release.
    struct Asset {
        QString name;
        QUrl downloadUrl;
        qint64 sizeBytes = -1; ///< -1 if not reported by the API.
        QString contentType;
    };

    /// @brief Metadata for one GitHub release.
    struct ReleaseInfo {
        QString tagName;
        QString name;
        QString body;        ///< Release notes in Markdown.
        QUrl htmlUrl;
        QDateTime publishedAt;
        QList<Asset> assets;
        bool prerelease = false;
        bool draft = false;

        SemVer version; ///< Parsed from @c tagName.
    };

    explicit UpdaterService(QString owner,
                            QString repo,
                            QObject* parent = nullptr);

    void setAuthToken(const QString& token);
    void setRepository(const QString& owner, const QString& repo);
    QString owner() const { return m_owner; }
    QString repo() const { return m_repo; }
    /// @brief Controls whether pre-release tags are considered when checking for updates.
    void setIncludePrereleases(bool include);
    bool includePrereleases() const { return m_includePrereleases; }

    /// @brief Returns the running application's version parsed from the TRENCHKIT_VERSION macro.
    [[nodiscard]] SemVer currentVersion() const;
    /// @brief Parses a SemVer from a git tag string (strips leading "v").
    [[nodiscard]] static SemVer parseVersionFromTag(const QString& tag);

public slots:
    /// @brief Fetches the latest release from GitHub and emits @c updateAvailable or @c upToDate.
    void checkForUpdates();
    /// @brief Downloads @p asset to @p savePath; emits @c downloadProgress and @c downloadFinished.
    void downloadAsset(const Asset& asset, const QString& savePath);
    void cancelDownload();

signals:
    void checkingStarted();
    /// @brief Emitted when a newer release exists; carries full release metadata.
    void updateAvailable(UpdaterService::ReleaseInfo release);
    /// @brief Emitted when the running version is current; carries the latest release for reference.
    void upToDate(UpdaterService::ReleaseInfo latest);
    void downloadProgress(qint64 received, qint64 total);
    /// @brief Emitted on successful download completion; @p savePath is the local file path.
    void downloadFinished(QString savePath);
    void errorOccurred(QString message);

private:
    QNetworkRequest makeRequest(const QUrl& url) const;
    void handleReleaseJson(const QByteArray& json);
    void startDownload(bool allowResume, bool forceRestart);
    void restartDownloadFromScratch();
    QString formatNetworkError(const QString& context,
                               QNetworkReply* reply,
                               const QByteArray& body) const;

    QFile m_downloadFile;
    qint64 m_resumeFrom = 0;
    bool m_restartAttempted = false;
    Asset m_currentAsset;
    QString m_currentSavePath;

    QString m_owner;
    QString m_repo;
    QString m_authToken;
    bool m_includePrereleases = false;

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_activeReply;
    QPointer<QNetworkReply> m_downloadReply;
};
