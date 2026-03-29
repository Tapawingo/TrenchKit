/**
 * @file ModUpdateService.h
 * @brief NexusMods update checker for installed mods.
 */
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QTimer>
#include "core/models/ModUpdateInfo.h"
#include "core/models/ModInfo.h"

class ModManager;
class NexusModsClient;
struct NexusFileInfo;

/**
 * @brief Checks NexusMods for newer file versions of installed mods.
 *
 * Mods are checked one at a time with a 500 ms rate-limit delay between
 * requests. Results are cached in memory; call @c checkAllModsForUpdates()
 * to refresh.
 */
class ModUpdateService final : public QObject {
    Q_OBJECT

public:
    explicit ModUpdateService(ModManager *modManager,
                             NexusModsClient *nexusClient,
                             QObject *parent = nullptr);
    ~ModUpdateService() override = default;

    /**
     * @brief Returns true if a cached update result exists for @p modId.
     */
    bool hasUpdate(const QString &modId) const;
    /**
     * @brief Returns the cached update info for @p modId; result is default-constructed if absent.
     */
    ModUpdateInfo getUpdateInfo(const QString &modId) const;

public slots:
    /**
     * @brief Queues all NexusMods-linked mods for update checking.
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
     * @brief Emitted for each mod where a newer version is found.
     */
    void updateFound(QString modId, ModUpdateInfo updateInfo);
    /**
     * @brief Emitted when all queued mods have been checked; @p updatesFound is the total count.
     */
    void checkComplete(int updatesFound);
    void errorOccurred(QString message);

private slots:
    void onModFilesReceived(const QList<NexusFileInfo> &files);
    void onError(const QString &error);

private:
    void processNextMod();
    bool isUpdateAvailable(const QString &currentVersion, const QString &availableVersion) const;
    void findLatestVersion(const QList<NexusFileInfo> &files, const QString &currentFileId,
                          QString &latestVersion, QString &latestFileId, QDateTime &latestDate,
                          bool &isExplicit) const;

    ModManager *m_modManager;
    NexusModsClient *m_nexusClient;

    QMap<QString, ModUpdateInfo> m_updateCache;
    QList<ModInfo> m_modsToCheck;
    int m_currentModIndex = 0;
    int m_totalMods = 0;
    int m_updatesFound = 0;
    bool m_isChecking = false;

    QTimer m_rateLimitTimer;
    static constexpr int RATE_LIMIT_DELAY_MS = 500; ///< Delay between NexusMods API requests (ms).
};
