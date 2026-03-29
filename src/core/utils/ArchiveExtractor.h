/**
 * @file ArchiveExtractor.h
 * @brief Extracts .pak files from mod archives (zip, rar, 7z, tar.gz, …).
 */
#ifndef ARCHIVEEXTRACTOR_H
#define ARCHIVEEXTRACTOR_H

#include <QObject>
#include <QString>
#include <QStringList>

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
    };

    /**
     * @brief Extracts all .pak files from @p zipPath into a new temporary directory.
     */
    ExtractResult extractPakFiles(const QString &zipPath);
    /**
     * @brief Returns true if @p filePath has a supported archive extension or magic signature.
     */
    static bool isArchiveFile(const QString &filePath);

    /**
     * @brief Deletes the temporary directory created by @c extractPakFiles().
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
    ExtractResult extractWithLibarchive(const QString &archivePath);
    ExtractResult extractWithZip(const QString &archivePath);
    bool isPakFile(const QString &fileName) const;
    QString createTempDir() const;
};

#endif // ARCHIVEEXTRACTOR_H
