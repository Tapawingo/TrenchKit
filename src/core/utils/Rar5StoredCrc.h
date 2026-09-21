/**
 * @file Rar5StoredCrc.h
 * @brief CRC32 verification for stored (uncompressed) RAR5 entries, which libarchive does not check.
 */
#ifndef RAR5STOREDCRC_H
#define RAR5STOREDCRC_H

#include <QHash>
#include <QList>
#include <QString>
#include <QtGlobal>
#include <optional>

/**
 * @brief Incremental CRC32 (IEEE 802.3, as used by RAR and zip).
 */
class Crc32 {
public:
    void update(const char *data, qsizetype size);
    [[nodiscard]] quint32 value() const { return ~m_state; }

private:
    quint32 m_state = 0xFFFFFFFFu;
};

/**
 * @brief Reads the CRC32 RAR5 records for entries stored without compression.
 *
 * libarchive verifies checksums of compressed RAR5 entries but never of stored ones,
 * so a corrupt stored entry would extract "successfully". The CRCs are read straight
 * from the archive's file headers so the caller can verify the data it extracted.
 */
class Rar5StoredCrc {
public:
    /// Reads the headers of @p archivePath; stays empty for anything that is not a plain RAR5 file.
    explicit Rar5StoredCrc(const QString &archivePath);

    /// Removes and returns the next expected CRC for @p entryPath, if it is a stored entry with a CRC.
    std::optional<quint32> take(const QString &entryPath);

    [[nodiscard]] bool isEmpty() const { return m_crcs.isEmpty(); }

private:
    QHash<QString, QList<std::optional<quint32>>> m_crcs;
};

#endif // RAR5STOREDCRC_H
