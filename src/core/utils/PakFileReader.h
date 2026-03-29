/**
 * @file PakFileReader.h
 * @brief Parses Unreal Engine pak file headers to list or extract embedded files.
 */
#ifndef PAKFILEREADER_H
#define PAKFILEREADER_H

#include <QString>
#include <QStringList>
#include <QIODevice>
#include <QFile>
#include <QVector>
#include <QPair>

/**
 * @brief Reads Unreal Engine pak file headers without decompressing content.
 *
 * @c extractFilePaths() reads only the index and is fast; @c extractFile()
 * decompresses a single entry on demand.
 */
class PakFileReader {
public:
    /**
     * @brief Pak footer structure at the end of every Unreal pak file.
     */
    struct PakFooter {
        quint32 magic;
        quint32 version;
        quint64 indexOffset;
        quint64 indexSize;
        quint8 indexHash[20];

        static constexpr quint32 MAGIC = 0x5A6F12E1; ///< Unreal pak file magic constant.
        static constexpr int SIZE = 44;
    };

    /**
     * @brief Result of @c extractFilePaths().
     */
    struct ParseResult {
        bool success;
        QString error;       ///< Human-readable error when @c success is false.
        QStringList filePaths; ///< All internal file paths listed in the pak index.
        QString mountPoint;  ///< Pak mount point prefix (e.g. "../../../").
    };

    /**
     * @brief Metadata for a single file entry in the pak index.
     */
    struct FileEntry {
        QString path;
        quint64 offset = 0;
        quint64 compressedSize = 0;
        quint64 uncompressedSize = 0;
        quint32 compressionMethod = 0;
        bool encrypted = false;
        quint32 compressionBlockSize = 0;
        QVector<QPair<quint64, quint64>> compressionBlocks;
    };

    /**
     * @brief Lists all file paths in the pak index; does not decompress any content.
     */
    static ParseResult extractFilePaths(const QString &pakFilePath);

    /**
     * @brief Extracts and decompresses the first file whose path matches one of @p candidatePaths.
     * @param data Receives the decompressed file bytes on success.
     * @param error Set to a human-readable message on failure; may be null.
     * @returns true on success.
     */
    static bool extractFile(const QString &pakFilePath,
                            const QStringList &candidatePaths,
                            QByteArray *data,
                            QString *error = nullptr);

private:
    static bool readFooter(QFile &file, PakFooter &footer);
    static bool readIndex(QFile &file, quint64 indexOffset, quint32 version, QStringList &filePaths, QString &mountPoint);
    static bool readEntry(QFile &file, quint32 version, FileEntry &entry);
    static bool matchesCandidate(const QString &fileName, const QStringList &candidatesLower);
    static QString readString(QIODevice &device);
    static quint64 calculateRecordMetadataSize(quint32 version);
};

#endif // PAKFILEREADER_H
