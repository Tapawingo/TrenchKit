/// @file ModUpdateInfo.h
/// @brief Result of a NexusMods update check for a single mod.
#ifndef MODUPDATEINFO_H
#define MODUPDATEINFO_H

#include <QString>
#include <QDateTime>

/// @brief Cached result of a NexusMods update check.
///
/// Produced by ModUpdateService and consumed by the UI and ModUpdateModalContent.
struct ModUpdateInfo {
    QString modId;
    QString currentVersion;   ///< Version string from ModInfo at check time.
    QString availableVersion; ///< Latest version string found on NexusMods.
    QString availableFileId;  ///< NexusMods file ID of the latest available file.
    QDateTime lastChecked;
    bool updateAvailable = false;

    ModUpdateInfo() = default;

    /// @brief Convenience constructor for a confirmed update.
    ModUpdateInfo(const QString &id, const QString &current, const QString &available,
                  const QString &fileId)
        : modId(id)
        , currentVersion(current)
        , availableVersion(available)
        , availableFileId(fileId)
        , lastChecked(QDateTime::currentDateTime())
        , updateAvailable(true)
    {}
};

#endif // MODUPDATEINFO_H
