/**
 * @file ModInstallHelper.h
 * @brief Installs a downloaded file (pak or archive) as a new mod, handling extraction.
 */
#ifndef MODINSTALLHELPER_H
#define MODINSTALLHELPER_H

#include <QString>
#include <QDateTime>
#include <functional>

class ModManager;
class ModalManager;
class QObject;

/// @brief Turns one downloaded file into an installed mod via @c ModManager::addModAsync().
/// Mirrors @c AddModModalContent's archive-extraction/multi-pak-selection pattern for a single file.
class ModInstallHelper {
public:
    /// Optional metadata to attach to the new mod; all fields are optional.
    struct Metadata {
        QString customModName;
        QString nexusModId;
        QString nexusFileId;
        QString nexusUrl;
        QString author;
        QString description;
        QString version;
        QString itchGameId;
        QString itchUrl;
        QString itchUploadId;
        QDateTime uploadDate;
    };

    /// @brief Installs @p filePath (a .pak or archive) as a new mod; @p onDone(anyInstalled) runs
    /// once finished, only while @p context is alive. @p deleteFileWhenDone removes @p filePath after.
    static void installFromFile(ModManager *modManager, ModalManager *modalManager,
                                const QString &filePath, const Metadata &meta,
                                bool deleteFileWhenDone, QObject *context,
                                std::function<void(bool)> onDone);

private:
    ModInstallHelper() = delete;

    static void installPaksSequentially(ModManager *modManager, ModalManager *modalManager,
                                        const QStringList &pakPaths, const Metadata &meta,
                                        QObject *context, bool anyInstalledSoFar,
                                        const std::function<void(bool)> &onDone);
    static void installOnePak(ModManager *modManager, ModalManager *modalManager,
                              const QString &pakPath, const Metadata &meta,
                              QObject *context, const std::function<void(bool)> &onDone);
};

#endif // MODINSTALLHELPER_H
