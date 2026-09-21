/**
 * @file ArchiveExtractor.h
 * @brief Extracts .pak files from mod archives (zip, rar, 7z, tar.gz, …).
 */
#ifndef ARCHIVEEXTRACTOR_H
#define ARCHIVEEXTRACTOR_H

#include "CancelToken.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

/**
 * @brief Extracts pak files from a mod archive into a temporary directory.
 *
 * Format detection uses both the file extension and magic bytes. After
 * processing the result, the caller is responsible for cleaning up by
 * calling @c cleanupTempDir(result.tempDir).
 */
class ArchiveExtractor : public QObject {
    Q_OBJECT

public:
    explicit ArchiveExtractor(QObject *parent = nullptr);

    /**
     * @brief Result of a @c extractPakFiles() call.
     */
    struct ExtractResult {
        bool success;
        QStringList pakFiles; ///< Absolute paths of the .pak files found inside the archive.
        QString tempDir;      ///< Temporary directory holding the extracted files; caller must call @c cleanupTempDir().
        QString error;        ///< Human-readable error description when @c success is false.
        bool cancelled = false; ///< True if the extraction was stopped through its @c CancelToken.
    };

    /**
     * @brief Extracts all .pak files from @p zipPath into a new temporary directory.
     */
    ExtractResult extractPakFiles(const QString &zipPath, const CancelToken *cancel = nullptr);

    /// Receives the result of @c extractPakFilesAsync(); the caller must still clean up @c tempDir.
    using ExtractCallback = std::function<void(const ExtractResult &)>;

    /**
     * @brief Runs @c extractPakFiles() on a worker thread so the UI stays responsive.
     *
     * @p onFinished is invoked on the thread that called this function, only while @p context
     * is alive. If @p context is destroyed first the callback is dropped, the work is cancelled
     * and any extracted files are removed, so nothing is leaked.
     * @param token Optional token to share with other stages; a new one is created if null.
     * @returns The token; cancelling it makes the callback receive a result with @c cancelled set.
     */
    static CancelTokenPtr extractPakFilesAsync(const QString &archivePath, QObject *context,
                                               ExtractCallback onFinished, CancelTokenPtr token = {});
    /**
     * @brief Returns true if @p filePath has a supported archive extension or magic signature.
     */
    static bool isArchiveFile(const QString &filePath);

    /**
     * @brief Deletes the temporary directory created by @c extractPakFiles().
     *
     * Does nothing for an empty path (a failed extraction) or for any directory that
     * @c extractPakFiles() did not create.
     */
    static void cleanupTempDir(const QString &tempDir);

signals:
    void errorOccurred(const QString &error);

private:
    enum class ArchiveFormat {
        Zip, Rar, SevenZip, TarGz, TarBz2, TarXz, Unknown
    };

    ArchiveFormat detectFormat(const QString &filePath) const;
    ArchiveFormat detectFormatBySignature(const QString &filePath) const;
    ExtractResult extractWithLibarchive(const QString &archivePath, const CancelToken *cancel);
    ExtractResult extractWithZip(const QString &archivePath, const CancelToken *cancel);
    bool isPakFile(const QString &fileName) const;
    QString createTempDir() const;
};

#endif // ARCHIVEEXTRACTOR_H
