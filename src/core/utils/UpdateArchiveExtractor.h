/**
 * @file UpdateArchiveExtractor.h
 * @brief Archive extractor used exclusively by the standalone updater helper executable.
 */
#pragma once

#include "CancelToken.h"

#include <QString>

/**
 * @brief Extracts a downloaded update archive to a destination directory.
 *
 * Used only by the @c updater/ standalone executable, not the main TrenchKit app.
 * Supports the same formats as @c ArchiveExtractor but without QObject overhead.
 */
class UpdateArchiveExtractor {
public:
    /**
     * @brief Extracts @p archivePath into @p destDir.
     * @param error Set to a human-readable message on failure.
     * @param cancel Optional; when it is cancelled the extraction stops and returns false.
     * @returns true on success.
     */
    static bool extractArchive(const QString &archivePath, const QString &destDir, QString *error,
                               const CancelToken *cancel = nullptr);

private:
    static bool extractWithLibarchive(const QString &archivePath,
                                     const QString &destDir,
                                     QString *error,
                                     const CancelToken *cancel);
    static bool extractWithZip(const QString &archivePath,
                              const QString &destDir,
                              QString *error,
                              const CancelToken *cancel);
};
