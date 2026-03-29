/**
 * @file ItchModUpdateService.h
 * @brief itch.io update checker for installed mods.
 */
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QTimer>
#include "core/models/ItchUpdateInfo.h"
#include "core/models/ModInfo.h"

class ModManager;
class ItchClient;
struct ItchUploadInfo;

/**
 * @brief Checks itch.io for newer uploads of installed mods.
 *
 * Update detection compares upload dates rather than version strings.
 * When multiple candidate uploads exist, the result carries all of them
 * so the UI can prompt the user to choose. Mods are checked one at a time
 * with a 500 ms rate-limit delay between requests.
 */
class ItchModUpdateService final : public QObject {
    Q_OBJECT

public:
    explicit ItchModUpdateService(ModManager *modManager,
                                 ItchClient *itchClient,
                                 QObject *parent = nullptr);
    ~ItchModUpdateService() override = default;

    /**
     * @brief Returns true if a cached update result exists for @p modId.
     */
    bool hasUpdate(const QString &modId) const;
    /**
     * @brief Returns the cached update info for @p modId; result is default-constructed if absent.
     */
    ItchUpdateInfo getUpdateInfo(const QString &modId) const;
    /**
     * @brief Marks @p uploadIds as ignored so they are skipped in future update checks for @p modId.
     */
    void ignoreUpdatesForMod(const QString &modId, const QStringList &uploadIds);
    /**
     * @brief Clears any cached update result for @p modId.
     */
    void clearUpdateForMod(const QString &modId);

public slots:
    /**
     * @brief Queues all itch.io-linked mods for update checking.
     */
    void checkAllModsForUpdates();
    /**
     * @brief Checks a single mod immediately, bypassing the queue.
     */
    void checkModForUpdate(const QString &modId);
    void cancelCheck();

signals:
    void checkStarted();
    /**
     * @brief Emitted per mod as the queue is processed; @p current and @p total are queue positions.
     */
    void checkProgress(int current, int total);
    /**
     * @brief Emitted for each mod where a newer upload is found.
     */
    void updateFound(QString modId, ItchUpdateInfo updateInfo);
    /**
     * @brief Emitted when all queued mods have been checked; @p updatesFound is the total count.
     */
    void checkComplete(int updatesFound);
    void errorOccurred(QString message);

private slots:
    void onUploadsReceived(const QList<ItchUploadInfo> &uploads);
    void onError(const QString &error);

private:
    void processNextMod();
    bool isUpdateAvailable(const QDateTime &currentDate, const QDateTime &availableDate) const;
    void findLatestUpload(const QList<ItchUploadInfo> &uploads,
                         QString &latestVersion, QString &latestUploadId,
                         QDateTime &latestDate) const;
    QList<ItchUploadInfo> findCandidateUploads(const QList<ItchUploadInfo> &uploads,
                                               const QDateTime &currentDate,
                                               const QStringList &ignoredUploadIds) const;
    QString extractVersionFromFilename(const QString &filename) const;

    ModManager *m_modManager;
    ItchClient *m_itchClient;

    QMap<QString, ItchUpdateInfo> m_updateCache;
    QList<ModInfo> m_modsToCheck;
    int m_currentModIndex = 0;
    int m_totalMods = 0;
    int m_updatesFound = 0;
    bool m_isChecking = false;

    QTimer m_rateLimitTimer;
    static constexpr int RATE_LIMIT_DELAY_MS = 500; ///< Delay between itch.io API requests (ms).
};
