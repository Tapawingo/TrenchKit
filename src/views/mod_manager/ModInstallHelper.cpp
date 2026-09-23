#include "ModInstallHelper.h"
#include "modals/mod_manager/FileSelectionModalContent.h"
#include "common/modals/MessageModal.h"
#include "common/modals/ModalManager.h"
#include "core/managers/ModManager.h"
#include "core/utils/ArchiveExtractor.h"
#include "core/utils/PakFileReader.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

void ModInstallHelper::installFromFile(ModManager *modManager, ModalManager *modalManager,
                                       const QString &filePath, const Metadata &meta,
                                       bool deleteFileWhenDone, QObject *context,
                                       std::function<void(bool)> onDone) {
    qDebug() << "ModInstallHelper::installFromFile" << filePath
             << "name:" << meta.customModName << "author:" << meta.author
             << "nexusModId:" << meta.nexusModId << "itchGameId:" << meta.itchGameId;

    if (!ArchiveExtractor::isArchiveFile(filePath)) {
        // A bare .pak (or something claiming to be one; addModAsync() rejects anything that isn't).
        installPaksSequentially(modManager, modalManager, {filePath}, meta, context, false,
            [onDone, filePath, deleteFileWhenDone](bool installed) {
                if (deleteFileWhenDone) {
                    QFile::remove(filePath);
                }
                onDone(installed);
            });
        return;
    }

    ArchiveExtractor::extractPakFilesAsync(filePath, context,
        [=](const ArchiveExtractor::ExtractResult &result) {
            if (deleteFileWhenDone) {
                QFile::remove(filePath);
            }

            if (result.cancelled) {
                ArchiveExtractor::cleanupTempDir(result.tempDir);
                return;
            }

            if (!result.success) {
                MessageModal::warning(modalManager, QObject::tr("Error"), result.error);
                ArchiveExtractor::cleanupTempDir(result.tempDir);
                onDone(false);
                return;
            }

            if (result.pakFiles.isEmpty()) {
                MessageModal::warning(modalManager, QObject::tr("Error"),
                                      QObject::tr("No .pak files found in archive"));
                ArchiveExtractor::cleanupTempDir(result.tempDir);
                onDone(false);
                return;
            }

            const auto finish = [=](bool anyInstalled) {
                ArchiveExtractor::cleanupTempDir(result.tempDir);
                onDone(anyInstalled);
            };

            if (result.pakFiles.size() == 1) {
                installPaksSequentially(modManager, modalManager, result.pakFiles, meta, context, false, finish);
                return;
            }

            QStringList fileNames;
            for (const QString &path : result.pakFiles) {
                fileNames.append(QFileInfo(path).fileName());
            }

            auto *fileModal = new FileSelectionModalContent(fileNames, QFileInfo(filePath).fileName(), true);
            QObject::connect(fileModal, &FileSelectionModalContent::accepted, context,
                             [=]() {
                QStringList selectedPaks;
                for (const QString &fileName : fileModal->getSelectedFiles()) {
                    for (const QString &pakPath : result.pakFiles) {
                        if (QFileInfo(pakPath).fileName() == fileName) {
                            selectedPaks.append(pakPath);
                            break;
                        }
                    }
                }
                installPaksSequentially(modManager, modalManager, selectedPaks, meta, context, false, finish);
            });
            QObject::connect(fileModal, &FileSelectionModalContent::rejected, context,
                             [finish]() { finish(false); });

            modalManager->showModal(fileModal);
        });
}

void ModInstallHelper::installPaksSequentially(ModManager *modManager, ModalManager *modalManager,
                                               const QStringList &pakPaths, const Metadata &meta,
                                               QObject *context, bool anyInstalledSoFar,
                                               const std::function<void(bool)> &onDone) {
    if (pakPaths.isEmpty()) {
        onDone(anyInstalledSoFar);
        return;
    }

    const QString pakPath = pakPaths.first();
    const QStringList rest = pakPaths.mid(1);
    installOnePak(modManager, modalManager, pakPath, meta, context, [=](bool installed) {
        installPaksSequentially(modManager, modalManager, rest, meta, context,
                                anyInstalledSoFar || installed, onDone);
    });
}

void ModInstallHelper::installOnePak(ModManager *modManager, ModalManager *modalManager,
                                     const QString &pakPath, const Metadata &meta,
                                     QObject *context, const std::function<void(bool)> &onDone) {
    QString normalizedPath = pakPath;
    if (!normalizedPath.endsWith(".pak", Qt::CaseInsensitive)) {
        auto parseResult = PakFileReader::extractFilePaths(normalizedPath);
        if (parseResult.success) {
            const QFileInfo fileInfo(normalizedPath);
            QString newPath = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".pak";
            if (QFile::exists(newPath)) {
                QFile::remove(newPath);
            }
            if (QFile::rename(normalizedPath, newPath)) {
                normalizedPath = newPath;
            }
        }
    }

    const QString modName = !meta.customModName.isEmpty() ? meta.customModName
                                                          : QFileInfo(normalizedPath).completeBaseName();

    qDebug() << "ModInstallHelper::installOnePak" << normalizedPath << "name:" << modName
             << "author:" << meta.author;

    modManager->addModAsync(normalizedPath, {
            .name = modName,
            .nexusModId = meta.nexusModId,
            .nexusFileId = meta.nexusFileId,
            .nexusUrl = meta.nexusUrl,
            .author = meta.author,
            .description = meta.description,
            .version = meta.version,
            .itchGameId = meta.itchGameId,
            .itchUrl = meta.itchUrl,
            .itchUploadId = meta.itchUploadId,
            .uploadDate = meta.uploadDate
        }, context, [modalManager, modName, onDone](bool added) {
        if (!added) {
            MessageModal::warning(modalManager, QObject::tr("Error"),
                                  QObject::tr("Failed to add mod: %1").arg(modName));
        }
        onDone(added);
    });
}
