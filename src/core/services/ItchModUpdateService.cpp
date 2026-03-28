#include "ItchModUpdateService.h"
#include "core/managers/ModManager.h"
#include "core/api/ItchClient.h"
#include "core/models/ItchUploadInfo.h"
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

ItchModUpdateService::ItchModUpdateService(ModManager *modManager,
                                         ItchClient *itchClient,
                                         QObject *parent)
    : QObject(parent)
    , m_modManager(modManager)
    , m_itchClient(itchClient)
{
    if (!m_modManager || !m_itchClient) {
        qWarning() << "ItchModUpdateService: Invalid modManager or itchClient";
        return;
    }

    m_rateLimitTimer.setSingleShot(true);
    connect(&m_rateLimitTimer, &QTimer::timeout, this, &ItchModUpdateService::processNextMod);

    connect(m_itchClient, &ItchClient::uploadsReceived,
            this, &ItchModUpdateService::onUploadsReceived);
    connect(m_itchClient, &ItchClient::errorOccurred,
            this, &ItchModUpdateService::onError);
}

void ItchModUpdateService::checkAllModsForUpdates() {
    if (m_isChecking) {
        qWarning() << "Update check already in progress";
        return;
    }

    if (!m_modManager) {
        emit errorOccurred("ModManager not available");
        return;
    }

    m_modsToCheck.clear();
    m_updateCache.clear();

    const QList<ModInfo> &allMods = m_modManager->getMods();
    for (const ModInfo &mod : allMods) {
        if (!mod.itchGameId.isEmpty()) {
            m_modsToCheck.append(mod);
        }
    }

    if (m_modsToCheck.isEmpty()) {
        emit checkComplete(0);
        return;
    }

    m_currentModIndex = 0;
    m_totalMods = m_modsToCheck.size();
    m_updatesFound = 0;
    m_isChecking = true;

    emit checkStarted();

    processNextMod();
}

void ItchModUpdateService::checkModForUpdate(const QString &modId) {
    if (m_isChecking) {
        qWarning() << "Update check already in progress";
        return;
    }

    if (!m_modManager) {
        emit errorOccurred("ModManager not available");
        return;
    }

    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty() || mod.itchGameId.isEmpty()) {
        emit errorOccurred("Mod not found or no itch game ID");
        return;
    }

    m_modsToCheck.clear();
    m_modsToCheck.append(mod);
    m_currentModIndex = 0;
    m_totalMods = 1;
    m_updatesFound = 0;
    m_isChecking = true;

    emit checkStarted();

    processNextMod();
}

void ItchModUpdateService::cancelCheck() {
    if (!m_isChecking) {
        return;
    }

    m_rateLimitTimer.stop();
    m_isChecking = false;
    m_modsToCheck.clear();
    m_currentModIndex = 0;

    emit checkComplete(m_updatesFound);
}

void ItchModUpdateService::processNextMod() {
    if (m_currentModIndex >= m_modsToCheck.size()) {
        m_isChecking = false;
        emit checkComplete(m_updatesFound);
        return;
    }

    const ModInfo &mod = m_modsToCheck[m_currentModIndex];
    emit checkProgress(m_currentModIndex + 1, m_totalMods);

    m_itchClient->getGameUploads(mod.itchGameId);
}

void ItchModUpdateService::onUploadsReceived(const QList<ItchUploadInfo> &uploads) {
    if (!m_isChecking || m_currentModIndex >= m_modsToCheck.size()) {
        return;
    }

    const ModInfo &mod = m_modsToCheck[m_currentModIndex];
    QDateTime currentDate = mod.uploadDate.isValid() ? mod.uploadDate : mod.installDate;

    QList<ItchUploadInfo> candidateUploads;
    if (!mod.itchUploadId.isEmpty()) {
        // Only flag an update if the specific installed upload has a newer date
        for (const ItchUploadInfo &upload : uploads) {
            if (upload.id == mod.itchUploadId && !mod.ignoredItchUploadIds.contains(upload.id)) {
                QDateTime uploadDate = upload.updatedAt.isValid() ? upload.updatedAt : upload.createdAt;
                if (isUpdateAvailable(currentDate, uploadDate)) {
                    candidateUploads.append(upload);
                }
                break;
            }
        }
    } else {
        // Legacy fallback: mod installed before upload ID tracking
        candidateUploads = findCandidateUploads(uploads, currentDate, mod.ignoredItchUploadIds);
    }

    if (!candidateUploads.isEmpty()) {
        ItchUpdateInfo updateInfo(mod.id, mod.version, currentDate, candidateUploads);
        m_updateCache[mod.id] = updateInfo;
        emit updateFound(mod.id, updateInfo);
        m_updatesFound++;
    }

    m_currentModIndex++;
    m_rateLimitTimer.start(RATE_LIMIT_DELAY_MS);
}

void ItchModUpdateService::onError(const QString &error) {
    if (!m_isChecking) {
        return;
    }

    m_isChecking = false;
    m_modsToCheck.clear();
    m_currentModIndex = 0;

    emit errorOccurred(error);
}

bool ItchModUpdateService::hasUpdate(const QString &modId) const {
    return m_updateCache.contains(modId);
}

ItchUpdateInfo ItchModUpdateService::getUpdateInfo(const QString &modId) const {
    return m_updateCache.value(modId);
}

void ItchModUpdateService::clearUpdateForMod(const QString &modId) {
    m_updateCache.remove(modId);
}

void ItchModUpdateService::ignoreUpdatesForMod(const QString &modId, const QStringList &uploadIds) {
    ModInfo mod = m_modManager->getMod(modId);
    if (mod.id.isEmpty()) {
        return;
    }

    // Add upload IDs to ignore list (avoid duplicates)
    for (const QString &uploadId : uploadIds) {
        if (!mod.ignoredItchUploadIds.contains(uploadId)) {
            mod.ignoredItchUploadIds.append(uploadId);
        }
    }

    // Save the updated mod
    m_modManager->updateModMetadata(mod);

    // Remove from update cache so it won't show as having an update
    m_updateCache.remove(modId);
}

bool ItchModUpdateService::isUpdateAvailable(const QDateTime &currentDate, const QDateTime &availableDate) const {
    if (!currentDate.isValid() || !availableDate.isValid()) {
        return false;
    }
    return availableDate > currentDate;
}

void ItchModUpdateService::findLatestUpload(const QList<ItchUploadInfo> &uploads,
                                           QString &latestVersion, QString &latestUploadId,
                                           QDateTime &latestDate) const {
    if (uploads.isEmpty()) {
        return;
    }

    const ItchUploadInfo *latest = nullptr;

    for (const ItchUploadInfo &upload : uploads) {
        QDateTime uploadDate = upload.updatedAt.isValid() ? upload.updatedAt : upload.createdAt;

        if (!latest || uploadDate > (latest->updatedAt.isValid() ? latest->updatedAt : latest->createdAt)) {
            latest = &upload;
        }
    }

    if (latest) {
        latestUploadId = latest->id;
        latestDate = latest->updatedAt.isValid() ? latest->updatedAt : latest->createdAt;

        latestVersion = extractVersionFromFilename(latest->filename);
        if (latestVersion.isEmpty()) {
            latestVersion = QStringLiteral("Updated: %1").arg(latestDate.toString(QStringLiteral("yyyy-MM-dd")));
        }
    }
}

QList<ItchUploadInfo> ItchModUpdateService::findCandidateUploads(
    const QList<ItchUploadInfo> &uploads,
    const QDateTime &currentDate,
    const QStringList &ignoredUploadIds) const
{
    QList<ItchUploadInfo> candidates;

    for (const ItchUploadInfo &upload : uploads) {
        // Skip ignored uploads
        if (ignoredUploadIds.contains(upload.id)) {
            continue;
        }

        QDateTime uploadDate = upload.updatedAt.isValid() ? upload.updatedAt : upload.createdAt;

        if (isUpdateAvailable(currentDate, uploadDate)) {
            candidates.append(upload);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const ItchUploadInfo &a, const ItchUploadInfo &b) {
        QDateTime dateA = a.updatedAt.isValid() ? a.updatedAt : a.createdAt;
        QDateTime dateB = b.updatedAt.isValid() ? b.updatedAt : b.createdAt;
        return dateA > dateB;
    });

    return candidates;
}

QString ItchModUpdateService::extractVersionFromFilename(const QString &filename) const {
    static const QRegularExpression versionRegex(QStringLiteral(R"(v?(\d+\.\d+(?:\.\d+)?))"));
    QRegularExpressionMatch match = versionRegex.match(filename);

    if (match.hasMatch()) {
        return match.captured(1);
    }

    return QString();
}
