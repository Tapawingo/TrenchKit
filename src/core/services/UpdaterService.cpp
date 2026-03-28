#include "UpdaterService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSslSocket>
#include <QMetaObject>
#include <algorithm>

namespace {
void emitDownloadFinishedQueued(UpdaterService *service, const QString &path) {
    if (!service) {
        return;
    }
    QMetaObject::invokeMethod(service, [service, path]() {
        emit service->downloadFinished(path);
    }, Qt::QueuedConnection);
}
}

QString UpdaterService::SemVer::toString() const {
    QString s = QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
    if (!preRelease.isEmpty()) {
        s += QLatin1Char('-') + preRelease;
    }
    return s;
}

UpdaterService::SemVer UpdaterService::SemVer::fromString(const QString& s) {
    QString t = s.trimmed();
    if (t.startsWith(QLatin1Char('v')) || t.startsWith(QLatin1Char('V'))) {
        t = t.mid(1);
    }

    SemVer result;
    const int dashIdx = t.indexOf(QLatin1Char('-'));
    const QString numericPart = dashIdx >= 0 ? t.left(dashIdx) : t;
    result.preRelease = dashIdx >= 0 ? t.mid(dashIdx + 1) : QString();

    const QStringList parts = numericPart.split(QLatin1Char('.'));
    bool ok = false;
    if (parts.size() >= 1) {
        result.major = parts[0].toInt(&ok);
        if (!ok) return {};
    }
    if (parts.size() >= 2) {
        result.minor = parts[1].toInt(&ok);
        if (!ok) return {};
    }
    if (parts.size() >= 3) {
        result.patch = parts[2].toInt(&ok);
        if (!ok) return {};
    }
    return result;
}

// Implements SemVer spec item 11 precedence rules.
int UpdaterService::SemVer::compare(const SemVer& a, const SemVer& b) {
    // 11.1 / 11.2: compare major, minor, patch numerically
    if (a.major != b.major) return a.major - b.major;
    if (a.minor != b.minor) return a.minor - b.minor;
    if (a.patch != b.patch) return a.patch - b.patch;

    // 11.3: a pre-release version has lower precedence than the associated normal version
    if (a.preRelease.isEmpty() && b.preRelease.isEmpty()) return 0;
    if (a.preRelease.isEmpty()) return 1;   // a is normal release, b is pre-release: a > b
    if (b.preRelease.isEmpty()) return -1;  // b is normal release, a is pre-release: a < b

    // 11.4: compare pre-release identifiers left-to-right, dot-separated
    const QStringList aIds = a.preRelease.split(QLatin1Char('.'));
    const QStringList bIds = b.preRelease.split(QLatin1Char('.'));
    const int minCount = std::min(aIds.size(), bIds.size());

    for (int i = 0; i < minCount; ++i) {
        const QString& aId = aIds[i];
        const QString& bId = bIds[i];

        bool aIsNum = false;
        bool bIsNum = false;
        const int aNum = aId.toInt(&aIsNum);
        const int bNum = bId.toInt(&bIsNum);

        if (aIsNum && bIsNum) {
            // 11.4.1.1: identifiers of only digits compared numerically
            if (aNum != bNum) return aNum - bNum;
        } else if (!aIsNum && !bIsNum) {
            // 11.4.1.2: identifiers with letters compared lexically in ASCII
            if (aId != bId) return aId < bId ? -1 : 1;
        } else {
            // 11.4.1.3: numeric identifiers have lower precedence than alphanumeric
            return aIsNum ? -1 : 1;
        }
    }

    // 11.4.4: larger set of fields has higher precedence
    return aIds.size() - bIds.size();
}

UpdaterService::UpdaterService(QString owner, QString repo, QObject* parent)
    : QObject(parent),
      m_owner(std::move(owner)),
      m_repo(std::move(repo)) {}

void UpdaterService::setAuthToken(const QString& token) {
    m_authToken = token;
}

void UpdaterService::setRepository(const QString& owner, const QString& repo) {
    if (owner.isEmpty() || repo.isEmpty()) {
        return;
    }
    m_owner = owner;
    m_repo = repo;
}

void UpdaterService::setIncludePrereleases(bool include) {
    m_includePrereleases = include;
}

UpdaterService::SemVer UpdaterService::currentVersion() const {
#ifdef TRENCHKIT_VERSION
    return SemVer::fromString(QStringLiteral(TRENCHKIT_VERSION));
#else
    return SemVer::fromString(QStringLiteral("0.0.0"));
#endif
}

QNetworkRequest UpdaterService::makeRequest(const QUrl& url) const {
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TrenchKit-Updater"));
    req.setHeader(QNetworkRequest::KnownHeaders::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    if (!m_authToken.isEmpty()) {
        req.setRawHeader("Authorization", QByteArray("Bearer ") + m_authToken.toUtf8());
    }
    req.setTransferTimeout(30000);
    return req;
}

UpdaterService::SemVer UpdaterService::parseVersionFromTag(const QString& tag) {
    return SemVer::fromString(tag);
}

void UpdaterService::checkForUpdates() {
    emit checkingStarted();

    if (!QSslSocket::supportsSsl()) {
        const QString message = QStringLiteral("TLS initialization failed. No TLS backend is available.");
        qWarning() << "Updater:" << message
                   << "Build SSL:" << QSslSocket::sslLibraryBuildVersionString()
                   << "Runtime SSL:" << QSslSocket::sslLibraryVersionString();
        emit errorOccurred(message);
        return;
    }

    qInfo() << "Updater: checking for updates.";
    const QUrl url = m_includePrereleases
        ? QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases")
                   .arg(m_owner, m_repo))
        : QUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
                   .arg(m_owner, m_repo));

    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
    }

    m_activeReply = m_nam.get(makeRequest(url));
    connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
        if (!m_activeReply) return;

        QNetworkReply* reply = m_activeReply;
        const auto err = reply->error();
        const QByteArray body = reply->readAll();
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;

        if (err != QNetworkReply::NoError || (httpStatus != 200 && httpStatus != 0)) {
            if (!m_includePrereleases && httpStatus == 404) {
                const QUrl fallbackUrl(QStringLiteral("https://api.github.com/repos/%1/%2/releases")
                                           .arg(m_owner, m_repo));
                qInfo() << "Updater: latest release not found, falling back to releases list.";
                m_activeReply = m_nam.get(makeRequest(fallbackUrl));
                connect(m_activeReply, &QNetworkReply::finished, this, [this]() {
                    if (!m_activeReply) return;

                    QNetworkReply* fallbackReply = m_activeReply;
                    const auto fallbackErr = fallbackReply->error();
                    const QByteArray fallbackBody = fallbackReply->readAll();
                    const int fallbackStatus =
                        fallbackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    m_activeReply->deleteLater();
                    m_activeReply = nullptr;

                    if (fallbackErr != QNetworkReply::NoError || (fallbackStatus != 200 && fallbackStatus != 0)) {
                        emit errorOccurred(formatNetworkError(
                            QStringLiteral("Update check failed"), fallbackReply, fallbackBody));
                        return;
                    }

                    handleReleaseJson(fallbackBody);
                });
                return;
            }

            emit errorOccurred(formatNetworkError(
                QStringLiteral("Update check failed"), reply, body));
            return;
        }

        handleReleaseJson(body);
    });
}

void UpdaterService::handleReleaseJson(const QByteArray& json) {
    const auto doc = QJsonDocument::fromJson(json);
    if (doc.isNull()) {
        emit errorOccurred(QStringLiteral("Update check failed: invalid JSON response."));
        return;
    }

    QJsonObject relObj;
    if (doc.isObject()) {
        relObj = doc.object();
    } else if (doc.isArray()) {
        const auto arr = doc.array();
        for (const auto& v : arr) {
            if (!v.isObject()) continue;
            const auto o = v.toObject();
            const bool draft = o.value("draft").toBool();
            const bool prerelease = o.value("prerelease").toBool();
            if (draft) continue;
            if (!m_includePrereleases && prerelease) continue;
            relObj = o;
            break;
        }
        if (relObj.isEmpty()) {
            emit errorOccurred(QStringLiteral("No suitable releases found."));
            return;
        }
    } else {
        emit errorOccurred(QStringLiteral("Update check failed: unexpected JSON format."));
        return;
    }

    ReleaseInfo info;
    info.tagName = relObj.value("tag_name").toString();
    info.name = relObj.value("name").toString();
    info.body = relObj.value("body").toString();
    info.prerelease = relObj.value("prerelease").toBool();
    info.draft = relObj.value("draft").toBool();
    info.htmlUrl = QUrl(relObj.value("html_url").toString());
    info.publishedAt = QDateTime::fromString(relObj.value("published_at").toString(), Qt::ISODate);

    info.version = parseVersionFromTag(info.tagName);

    const auto assets = relObj.value("assets").toArray();
    for (const auto& a : assets) {
        if (!a.isObject()) continue;
        const auto ao = a.toObject();

        Asset asset;
        asset.name = ao.value("name").toString();
        asset.downloadUrl = QUrl(ao.value("browser_download_url").toString());
        asset.sizeBytes = static_cast<qint64>(ao.value("size").toDouble(-1));
        asset.contentType = ao.value("content_type").toString();
        info.assets.push_back(asset);
    }

    const auto cur = currentVersion();
    if (!info.version.isNull() && SemVer::compare(info.version, cur) > 0) {
        emit updateAvailable(info);
    } else {
        emit upToDate(info);
    }
}

void UpdaterService::downloadAsset(const Asset& asset, const QString& savePath) {
    if (!asset.downloadUrl.isValid()) {
        emit errorOccurred(QStringLiteral("Invalid download URL."));
        return;
    }

    m_currentAsset = asset;
    m_currentSavePath = savePath;
    m_restartAttempted = false;

    startDownload(true, false);
}

void UpdaterService::startDownload(bool allowResume, bool forceRestart) {
    if (!m_currentAsset.downloadUrl.isValid()) {
        emit errorOccurred(QStringLiteral("Invalid download URL."));
        return;
    }

    qInfo() << "Updater: starting download for" << m_currentAsset.name;
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }

    m_downloadFile.close();
    m_resumeFrom = 0;

    QFileInfo fi(m_currentSavePath);
    if (!fi.dir().exists()) {
        fi.dir().mkpath(".");
    }

    m_downloadFile.setFileName(m_currentSavePath);
    if (!forceRestart && m_downloadFile.exists() && m_currentAsset.sizeBytes > 0) {
        const qint64 existingSize = m_downloadFile.size();
        if (existingSize == m_currentAsset.sizeBytes && existingSize > 0) {
            emitDownloadFinishedQueued(this, m_currentSavePath);
            return;
        }
        if (existingSize > m_currentAsset.sizeBytes) {
            forceRestart = true;
            allowResume = false;
        }
    }

    if (!forceRestart && allowResume && m_downloadFile.exists()) {
        m_resumeFrom = m_downloadFile.size();
        if (!m_downloadFile.open(QIODevice::Append)) {
            emit errorOccurred(QStringLiteral("Failed to open file for resume."));
            return;
        }
    } else {
        if (!m_downloadFile.open(QIODevice::WriteOnly)) {
            emit errorOccurred(QStringLiteral("Failed to create file."));
            return;
        }
    }

    QNetworkRequest req = makeRequest(m_currentAsset.downloadUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    if (m_resumeFrom > 0) {
        req.setRawHeader("Range",
            QByteArray("bytes=") + QByteArray::number(m_resumeFrom) + "-");
    }

    m_downloadReply = m_nam.get(req);

    connect(m_downloadReply, &QNetworkReply::metaDataChanged, this, [this]() {
        if (!m_downloadReply || m_resumeFrom <= 0 || m_restartAttempted) return;
        const int httpStatus =
            m_downloadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 200) {
            qInfo() << "Updater: server ignored Range, restarting download.";
            restartDownloadFromScratch();
        }
    });

    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            emit downloadProgress(m_resumeFrom + received,
                                  m_resumeFrom + total);
        }
    });

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (!m_downloadReply || !m_downloadFile.isOpen()) return;
        m_downloadFile.write(m_downloadReply->readAll());
    });

    connect(m_downloadReply, &QNetworkReply::finished, this, [this]() {
        if (!m_downloadReply) return;

        QNetworkReply* reply = m_downloadReply;
        const auto err = reply->error();
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        reply->deleteLater();
        m_downloadReply = nullptr;

        m_downloadFile.flush();
        m_downloadFile.close();

        if (httpStatus == 416 && m_resumeFrom > 0 && !m_restartAttempted) {
            QFileInfo fi(m_currentSavePath);
            if (m_currentAsset.sizeBytes > 0 && fi.exists() && fi.size() == m_currentAsset.sizeBytes) {
                emitDownloadFinishedQueued(this, m_currentSavePath);
                return;
            }
            restartDownloadFromScratch();
            return;
        }

        if (err != QNetworkReply::NoError) {
            emit errorOccurred(formatNetworkError(
                QStringLiteral("Download failed"), reply, body));
            return;
        }

        if (httpStatus != 200 && httpStatus != 206) {
            emit errorOccurred(QStringLiteral("Unexpected HTTP status: %1").arg(httpStatus));
            return;
        }

        if (m_resumeFrom > 0 && httpStatus == 200 && !m_restartAttempted) {
            emit errorOccurred(QStringLiteral("Server returned full response during resume."));
            return;
        }

        emitDownloadFinishedQueued(this, m_currentSavePath);
    });
}

void UpdaterService::restartDownloadFromScratch() {
    if (m_restartAttempted) return;
    m_restartAttempted = true;
    startDownload(false, true);
}

void UpdaterService::cancelDownload() {
    if (m_downloadReply) {
        m_downloadReply->abort();
        m_downloadReply->deleteLater();
        m_downloadReply = nullptr;
    }
    if (m_downloadFile.isOpen()) {
        m_downloadFile.flush();
        m_downloadFile.close();
    }
}

QString UpdaterService::formatNetworkError(const QString& context,
                                           QNetworkReply* reply,
                                           const QByteArray& body) const {
    QStringList parts;
    parts << context;
    if (reply) {
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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
