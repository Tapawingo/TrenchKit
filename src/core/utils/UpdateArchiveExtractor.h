/**
 * @file UpdateArchiveExtractor.h
 * @brief Archive extractor used exclusively by the standalone updater helper executable.
 */
#pragma once

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
     * @returns true on success.
     */
    static bool extractArchive(const QString &archivePath, const QString &destDir, QString *error);

private:
    static bool extractWithLibarchive(const QString &archivePath,
                                     const QString &destDir,
                                     QString *error);
    static bool extractWithZip(const QString &archivePath,
                              const QString &destDir,
                              QString *error);
};
