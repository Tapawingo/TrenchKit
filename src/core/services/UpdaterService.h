#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QPointer>
#include <QFile>
#include <QDateTime>

class UpdaterService final : public QObject {
    Q_OBJECT
public:
    // Semantic version per https://semver.org spec items 9 and 11.
    struct SemVer {
        int major = 0;
        int minor = 0;
        int patch = 0;
        QString preRelease; // e.g. "alpha", "rc.1", "beta.2" — empty for stable releases

        bool isNull() const { return major == 0 && minor == 0 && patch == 0 && preRelease.isEmpty(); }
        bool isPreRelease() const { return !preRelease.isEmpty(); }

        QString toString() const;
        static SemVer fromString(const QString& s);

        // Spec item 11: returns negative if a < b, 0 if equal, positive if a > b.
        static int compare(const SemVer& a, const SemVer& b);
    };

    struct Asset {
        QString name;
        QUrl downloadUrl;
        qint64 sizeBytes = -1;
        QString contentType;
    };

    struct ReleaseInfo {
        QString tagName;
        QString name;
        QString body;
        QUrl htmlUrl;
        QDateTime publishedAt;
        QList<Asset> assets;
        bool prerelease = false;
        bool draft = false;

        SemVer version;
    };

    explicit UpdaterService(QString owner,
                            QString repo,
                            QObject* parent = nullptr);

    void setAuthToken(const QString& token);
    void setRepository(const QString& owner, const QString& repo);
    QString owner() const { return m_owner; }
    QString repo() const { return m_repo; }
    void setIncludePrereleases(bool include);
    bool includePrereleases() const { return m_includePrereleases; }

    [[nodiscard]] SemVer currentVersion() const;
    [[nodiscard]] static SemVer parseVersionFromTag(const QString& tag);

public slots:
    void checkForUpdates();
    void downloadAsset(const Asset& asset, const QString& savePath);
    void cancelDownload();

signals:
    void checkingStarted();
    void updateAvailable(UpdaterService::ReleaseInfo release);
    void upToDate(UpdaterService::ReleaseInfo latest);
    void downloadProgress(qint64 received, qint64 total);
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

private:
    QString m_owner;
    QString m_repo;
    QString m_authToken;
    bool m_includePrereleases = false;

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_activeReply;
    QPointer<QNetworkReply> m_downloadReply;
};
