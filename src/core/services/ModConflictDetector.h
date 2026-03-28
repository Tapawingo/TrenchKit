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

struct ConflictInfo {
    int modPriority = 0;
    QStringList conflictingModIds;
    QStringList conflictingModNames;
    QList<int> conflictingModPriorities;
    QStringList conflictingFilePaths;
    QStringList allConflictingFilePaths;
    QSet<QString> overwrittenFilePaths;
    int fileConflictCount = 0;

    bool hasConflicts() const { return !conflictingModIds.isEmpty(); }
    bool isEntirelyOverwritten() const {
        return !overwrittenFilePaths.isEmpty() &&
               overwrittenFilePaths.size() == fileConflictCount;
    }
};

class ModManager;

class ModConflictDetector : public QObject {
    Q_OBJECT

public:
    explicit ModConflictDetector(ModManager *modManager, QObject *parent = nullptr);
    ~ModConflictDetector() override;

    [[nodiscard]] ConflictInfo getConflictInfo(const QString &modId) const;
    [[nodiscard]] bool hasConflicts(const QString &modId) const;
    void invalidateCache();

public slots:
    void scanForConflicts();
    void cancelScan();

signals:
    void scanStarted();
    void scanProgress(int current, int total);
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
