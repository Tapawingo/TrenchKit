/**
 * @file ModConflictDetector.h
 * @brief Async service that detects pak file conflicts between enabled mods.
 */
#ifndef MODCONFLICTDETECTOR_H
#define MODCONFLICTDETECTOR_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QStringList>
#include <QSet>
#include <QFutureWatcher>
#include <QMutex>
#include <QDateTime>
#include "core/models/ModInfo.h"

/**
 * @brief Conflict summary for a single mod.
 *
 * @c conflictingFilePaths lists files that this mod shares with lower-priority
 * mods (i.e. files this mod overwrites). @c overwrittenFilePaths lists files
 * that are overwritten by a higher-priority mod, effectively hiding them.
 */
struct ConflictInfo {
    int modPriority = 0;
    QStringList conflictingModIds;     ///< IDs of mods that share at least one pak path with this mod.
    QStringList conflictingModNames;   ///< Display names corresponding to @c conflictingModIds.
    QList<int> conflictingModPriorities;
    QStringList conflictingFilePaths;  ///< Paths this mod shares with lower-priority mods.
    QStringList allConflictingFilePaths; ///< All shared paths regardless of direction.
    QSet<QString> overwrittenFilePaths; ///< Paths where a higher-priority mod wins over this mod.
    int fileConflictCount = 0;

    bool hasConflicts() const { return !conflictingModIds.isEmpty(); }
    /**
     * @brief Returns true when every conflicting file is overwritten by a higher-priority mod.
     */
    bool isEntirelyOverwritten() const {
        return !overwrittenFilePaths.isEmpty() &&
               overwrittenFilePaths.size() == fileConflictCount;
    }
};

class ModManager;

/**
 * @brief Scans enabled mods for pak path overlaps and caches the results.
 *
 * The scan runs on a background thread via @c QtConcurrent. Results are
 * persisted to @c conflict_cache.json so repeated launches are fast.
 */
class ModConflictDetector : public QObject {
    Q_OBJECT

public:
    explicit ModConflictDetector(ModManager *modManager, QObject *parent = nullptr);
    ~ModConflictDetector() override;

    /**
     * @brief Returns the cached conflict summary for @p modId, or a default if not yet scanned.
     */
    [[nodiscard]] ConflictInfo getConflictInfo(const QString &modId) const;
    /**
     * @brief Returns true if the cached data shows any conflicts for @p modId.
     */
    [[nodiscard]] bool hasConflicts(const QString &modId) const;
    /**
     * @brief Discards the in-memory conflict cache; next @c scanForConflicts() re-reads paks.
     */
    void invalidateCache();

public slots:
    /**
     * @brief Starts an async scan; emits @c scanComplete when finished.
     */
    void scanForConflicts();
    void cancelScan();

signals:
    void scanStarted();
    /**
     * @brief Emitted periodically during the scan; @p current and @p total are mod counts.
     */
    void scanProgress(int current, int total);
    /**
     * @brief Emitted when the scan finishes successfully with the full conflict map.
     */
    void scanComplete(QMap<QString, ConflictInfo> conflicts);
    void scanCancelled();
    void errorOccurred(QString message);

private slots:
    void onScanComplete();

private:
    struct ModFileCache {
        QString modId;
        QString modName;
        int priority;
        QStringList filePaths;
    };

    struct PersistentModCache {
        QString fileName;
        QDateTime lastModified;
        qint64 fileSize;
        QStringList filePaths;
    };

    struct ScanInput {
        QList<ModInfo> mods;
        QString modsStoragePath;
        QMap<QString, PersistentModCache> persistentCache;
    };

    struct ScanResult {
        QMap<QString, ConflictInfo> conflicts;
        QMap<QString, PersistentModCache> updatedCache;
    };

    static ScanResult detectConflictsWorker(const ScanInput &input);

    void loadPersistentCache();
    void savePersistentCache();
    QString getCacheFilePath() const;
    bool hasModChanged(const QString &pakPath, const PersistentModCache &cached) const;

    ModManager *m_modManager;
    QFutureWatcher<ScanResult> *m_scanWatcher;
    QMap<QString, ConflictInfo> m_conflictCache;
    QMap<QString, PersistentModCache> m_persistentCache;
    mutable QMutex m_cacheMutex;
    mutable QMutex m_persistentCacheMutex;
    bool m_isScanning = false;
};

#endif // MODCONFLICTDETECTOR_H
