#include "ArchiveExtractor.h"
#include "PakFileReader.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>
#include <zip.h>
#include <archive.h>
#include <archive_entry.h>
#include <memory>

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : QObject(parent)
{
}

ArchiveExtractor::ArchiveFormat ArchiveExtractor::detectFormat(const QString &filePath) const {
    QString lower = filePath.toLower();
    qDebug() << "ArchiveExtractor: Detecting format for:" << filePath;

    if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz")) {
        qDebug() << "ArchiveExtractor: Detected format: TarGz";
        return ArchiveFormat::TarGz;
    }
    if (lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2")) {
        qDebug() << "ArchiveExtractor: Detected format: TarBz2";
        return ArchiveFormat::TarBz2;
    }
    if (lower.endsWith(".tar.xz") || lower.endsWith(".txz")) {
        qDebug() << "ArchiveExtractor: Detected format: TarXz";
        return ArchiveFormat::TarXz;
    }
    if (lower.endsWith(".zip")) {
        qDebug() << "ArchiveExtractor: Detected format: Zip";
        return ArchiveFormat::Zip;
    }
    if (lower.endsWith(".rar")) {
        qDebug() << "ArchiveExtractor: Detected format: Rar";
        return ArchiveFormat::Rar;
    }
    if (lower.endsWith(".7z")) {
        qDebug() << "ArchiveExtractor: Detected format: 7z";
        return ArchiveFormat::SevenZip;
    }

    ArchiveFormat signatureFormat = detectFormatBySignature(filePath);
    if (signatureFormat != ArchiveFormat::Unknown) {
        qDebug() << "ArchiveExtractor: Detected format by signature";
        return signatureFormat;
    }

    qDebug() << "ArchiveExtractor: Unknown format";
    return ArchiveFormat::Unknown;
}

ArchiveExtractor::ArchiveFormat ArchiveExtractor::detectFormatBySignature(const QString &filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ArchiveFormat::Unknown;
    }

    QByteArray header = file.read(8);
    if (header.size() >= 4) {
        if (header.startsWith("PK\x03\x04") || header.startsWith("PK\x05\x06")
            || header.startsWith("PK\x07\x08")) {
            return ArchiveFormat::Zip;
        }
    }

    if (header.size() >= 7) {
        const QByteArray rar4("Rar!\x1A\x07\x00", 7);
        if (header.startsWith(rar4)) {
            return ArchiveFormat::Rar;
        }
    }

    if (header.size() >= 8) {
        const QByteArray rar5("Rar!\x1A\x07\x01\x00", 8);
        if (header.startsWith(rar5)) {
            return ArchiveFormat::Rar;
        }
    }

    if (header.size() >= 6) {
        const QByteArray sevenZip("\x37\x7A\xBC\xAF\x27\x1C", 6);
        if (header.startsWith(sevenZip)) {
            return ArchiveFormat::SevenZip;
        }
    }

    if (header.size() >= 2) {
        if (static_cast<unsigned char>(header[0]) == 0x1F
            && static_cast<unsigned char>(header[1]) == 0x8B) {
            return ArchiveFormat::TarGz;
        }
        if (header.size() >= 4 && header.startsWith("BZh")
            && header[3] >= '1' && header[3] <= '9') {
            return ArchiveFormat::TarBz2;
        }
    }

    if (header.size() >= 6) {
        const QByteArray xz("\xFD\x37\x7A\x58\x5A\x00", 6);
        if (header.startsWith(xz)) {
            return ArchiveFormat::TarXz;
        }
    }

    if (file.size() > 262 && file.seek(257)) {
        QByteArray tarMagic = file.read(5);
        if (tarMagic == "ustar") {
            return ArchiveFormat::TarGz;
        }
    }

    return ArchiveFormat::Unknown;
}

namespace {

struct ArchiveDeleter {
    void operator()(struct archive *a) const { archive_read_free(a); }
};

QString libarchiveError(struct archive *a) {
    const char *message = archive_error_string(a);
    return message ? QString::fromUtf8(message)
                   : QStringLiteral("the archive appears to be damaged or incomplete");
}

/// Returns a path inside @p dir for @p baseName that does not collide with an earlier entry.
QString uniqueDestPath(const QString &dir, const QString &baseName) {
    QString candidate = dir + "/" + baseName;
    const QFileInfo info(baseName);
    for (int i = 2; QFile::exists(candidate); ++i) {
        candidate = QStringLiteral("%1/%2_%3.%4").arg(dir, info.completeBaseName()).arg(i).arg(info.suffix());
    }
    return candidate;
}

} // namespace

ArchiveExtractor::ExtractResult ArchiveExtractor::extractWithLibarchive(
    const QString &archivePath) {

    qDebug() << "ArchiveExtractor: extractWithLibarchive called for:" << archivePath;

    const QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists() || !archiveInfo.isFile()) {
        return {false, {}, "", "Archive file does not exist"};
    }

    std::unique_ptr<struct archive, ArchiveDeleter> a(archive_read_new());
    archive_read_support_format_all(a.get());
    archive_read_support_filter_all(a.get());

#ifdef _WIN32
    const int openResult = archive_read_open_filename_w(
        a.get(), reinterpret_cast<const wchar_t *>(archivePath.utf16()), 10240);
#else
    const int openResult = archive_read_open_filename(a.get(), archivePath.toUtf8().constData(), 10240);
#endif
    if (openResult != ARCHIVE_OK) {
        const QString error = QString("Failed to open archive: %1").arg(libarchiveError(a.get()));
        qWarning() << "ArchiveExtractor:" << error;
        emit errorOccurred(error);
        return {false, {}, "", error};
    }

    const QString tempDir = createTempDir();
    if (!QDir().mkpath(tempDir)) {
        return {false, {}, "", "Failed to create temporary directory"};
    }

    // Windows cannot delete a directory holding an open file, so the pak being written is closed first.
    QFile *openOutput = nullptr;
    const auto discardOutput = [&] {
        if (openOutput) {
            openOutput->close();
        }
        cleanupTempDir(tempDir);
    };

    // Any failure discards everything extracted so far: a partial extraction must never look like success.
    const auto fail = [&](const QString &error) -> ExtractResult {
        qWarning() << "ArchiveExtractor: extraction of" << archivePath << "failed:" << error;
        discardOutput();
        emit errorOccurred(error);
        return {false, {}, "", error};
    };

    QStringList pakFiles;
    struct archive_entry *entry = nullptr;

    for (;;) {
        const int headerResult = archive_read_next_header(a.get(), &entry);
        if (headerResult == ARCHIVE_EOF) {
            break;
        }
        if (headerResult < ARCHIVE_WARN) {
            return fail(QString("Failed to read archive contents: %1").arg(libarchiveError(a.get())));
        }
        if (headerResult == ARCHIVE_WARN) {
            qWarning() << "ArchiveExtractor: header warning:" << libarchiveError(a.get());
        }

        const char *entryName = archive_entry_pathname(entry);
        if (!entryName) {
            continue;
        }

        const QString fileName = QString::fromUtf8(entryName).replace(u'\\', u'/');
        if (archive_entry_filetype(entry) != AE_IFREG || !isPakFile(fileName)) {
            if (archive_read_data_skip(a.get()) < ARCHIVE_WARN) {
                return fail(QString("Failed to read archive contents: %1").arg(libarchiveError(a.get())));
            }
            continue;
        }

        const QString baseName = QFileInfo(fileName).fileName();
        if (archive_entry_is_encrypted(entry)) {
            return fail(QString("%1 is password protected, which is not supported").arg(baseName));
        }

        const QString destPath = uniqueDestPath(tempDir, baseName);
        QFile outFile(destPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            return fail(QString("Failed to create %1: %2").arg(destPath, outFile.errorString()));
        }
        openOutput = &outFile;

        qint64 totalBytes = 0;
        for (;;) {
            const void *buff = nullptr;
            size_t size = 0;
            int64_t offset = 0;
            const int dataResult = archive_read_data_block(a.get(), &buff, &size, &offset);
            if (dataResult == ARCHIVE_EOF) {
                break;
            }
            if (dataResult < ARCHIVE_WARN) {
                return fail(QString("Failed to extract %1: %2").arg(baseName, libarchiveError(a.get())));
            }
            if (dataResult == ARCHIVE_WARN) {
                qWarning() << "ArchiveExtractor: data warning for" << baseName << ":" << libarchiveError(a.get());
            }
            if (offset != totalBytes) {
                return fail(QString("Failed to extract %1: unexpected data layout").arg(baseName));
            }
            if (outFile.write(static_cast<const char *>(buff), static_cast<qint64>(size))
                != static_cast<qint64>(size)) {
                return fail(QString("Failed to write %1: %2").arg(baseName, outFile.errorString()));
            }
            totalBytes += static_cast<qint64>(size);
        }

        if (!outFile.flush() || outFile.error() != QFileDevice::NoError) {
            return fail(QString("Failed to write %1: %2").arg(baseName, outFile.errorString()));
        }
        outFile.close();
        openOutput = nullptr;

        if (archive_entry_size_is_set(entry) && totalBytes != archive_entry_size(entry)) {
            return fail(QString("%1 is incomplete (expected %2 bytes, got %3)")
                            .arg(baseName).arg(archive_entry_size(entry)).arg(totalBytes));
        }

        QString pakError;
        if (!PakFileReader::hasPakFooter(destPath, &pakError)) {
            return fail(QString("%1 is not a valid .pak file: %2").arg(baseName, pakError));
        }

        qDebug() << "ArchiveExtractor: Extracted" << baseName << totalBytes << "bytes";
        pakFiles.append(destPath);
    }

    if (pakFiles.isEmpty()) {
        return fail("No .pak files found in archive");
    }

    return {true, pakFiles, tempDir, ""};
}

ArchiveExtractor::ExtractResult ArchiveExtractor::extractPakFiles(
    const QString &archivePath) {

    qDebug() << "ArchiveExtractor: extractPakFiles called for:" << archivePath;

    ArchiveFormat format = detectFormat(archivePath);

    if (format == ArchiveFormat::Zip) {
        ExtractResult result = extractWithLibarchive(archivePath);
        if (result.success) {
            return result;
        }

        qDebug() << "ArchiveExtractor: libarchive failed for zip, trying zip library:" << result.error;
        ExtractResult fallback = extractWithZip(archivePath);
        if (!fallback.success) {
            fallback.error = QString("%1 (fallback: %2)").arg(result.error, fallback.error);
        }
        return fallback;
    }
    if (format == ArchiveFormat::Unknown) {
        return {false, {}, "", "Unknown or unsupported archive format"};
    }
    return extractWithLibarchive(archivePath);
}

bool ArchiveExtractor::isArchiveFile(const QString &filePath) {
    ArchiveExtractor extractor;
    return extractor.detectFormat(filePath) != ArchiveFormat::Unknown;
}

ArchiveExtractor::ExtractResult ArchiveExtractor::extractWithZip(const QString &zipPath) {
    QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists() || !zipInfo.isFile()) {
        return {false, {}, "", "Archive file does not exist"};
    }

    zip_t *zip = zip_open(zipPath.toUtf8().constData(), 0, 'r');
    if (!zip) {
        QString error = "Failed to open archive";
        emit errorOccurred(error);
        return {false, {}, "", error};
    }

    QString tempDir = createTempDir();
    if (!QDir().mkpath(tempDir)) {
        zip_close(zip);
        return {false, {}, "", "Failed to create temporary directory"};
    }

    QStringList pakFiles;
    QString failure;
    const int totalEntries = static_cast<int>(zip_entries_total(zip));

    for (int i = 0; i < totalEntries && failure.isEmpty(); ++i) {
        if (zip_entry_openbyindex(zip, i) < 0) {
            failure = QString("Failed to read archive entry %1").arg(i);
            break;
        }

        const QString fileName = QString::fromUtf8(zip_entry_name(zip)).replace(u'\\', u'/');

        if (zip_entry_isdir(zip) == 0 && isPakFile(fileName)) {
            const QString baseName = QFileInfo(fileName).fileName();
            const QString destPath = uniqueDestPath(tempDir, baseName);
            const auto expectedSize = zip_entry_size(zip);

            void *buf = nullptr;
            size_t bufsize = 0;
            const ssize_t readResult = zip_entry_read(zip, &buf, &bufsize);

            if (readResult < 0 || bufsize != expectedSize) {
                failure = QString("Failed to extract %1").arg(baseName);
            } else {
                QFile outFile(destPath);
                if (!outFile.open(QIODevice::WriteOnly)
                    || (bufsize > 0 && outFile.write(static_cast<const char *>(buf),
                                                     static_cast<qint64>(bufsize))
                                           != static_cast<qint64>(bufsize))
                    || !outFile.flush()) {
                    failure = QString("Failed to write %1: %2").arg(baseName, outFile.errorString());
                } else {
                    outFile.close();
                    QString pakError;
                    if (PakFileReader::hasPakFooter(destPath, &pakError)) {
                        pakFiles.append(destPath);
                    } else {
                        failure = QString("%1 is not a valid .pak file: %2").arg(baseName, pakError);
                    }
                }
            }
            if (buf) {
                free(buf);
            }
        }

        zip_entry_close(zip);
    }

    zip_close(zip);

    if (failure.isEmpty() && pakFiles.isEmpty()) {
        failure = "No .pak files found in archive";
    }
    if (!failure.isEmpty()) {
        cleanupTempDir(tempDir);
        return {false, {}, "", failure};
    }

    return {true, pakFiles, tempDir, ""};
}

void ArchiveExtractor::cleanupTempDir(const QString &tempDir) {
    // Failed extractions have no temp dir, and QDir("") means the current directory: never delete that.
    if (tempDir.isEmpty() || !QFileInfo(tempDir).fileName().startsWith(QLatin1String("TrenchKit_extract_"))) {
        return;
    }

    QDir dir(tempDir);
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

bool ArchiveExtractor::isPakFile(const QString &fileName) const {
    return fileName.endsWith(".pak", Qt::CaseInsensitive);
}

QString ArchiveExtractor::createTempDir() const {
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return tempPath + "/TrenchKit_extract_" + uuid;
}
