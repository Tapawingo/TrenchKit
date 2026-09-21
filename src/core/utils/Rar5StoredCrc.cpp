#include "Rar5StoredCrc.h"

#include <QByteArray>
#include <QFile>
#include <array>

namespace {

const std::array<quint32, 256> &crcTable() {
    static const std::array<quint32, 256> table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            t[i] = c;
        }
        return t;
    }();
    return table;
}

constexpr char kRar5Signature[] = {'R', 'a', 'r', '!', 0x1A, 0x07, 0x01, 0x00};

constexpr quint64 kBlockHasExtra = 0x1;
constexpr quint64 kBlockHasData = 0x2;
constexpr quint64 kBlockSplitBefore = 0x8;
constexpr quint64 kBlockSplitAfter = 0x10;

constexpr quint64 kTypeFile = 2;
constexpr quint64 kTypeEncryption = 4;
constexpr quint64 kTypeEnd = 5;

constexpr quint64 kFileIsDirectory = 0x1;
constexpr quint64 kFileHasMtime = 0x2;
constexpr quint64 kFileHasCrc = 0x4;

bool readVint(QFile &file, quint64 &value) {
    value = 0;
    for (int shift = 0; shift < 64; shift += 7) {
        char byte = 0;
        if (!file.getChar(&byte)) {
            return false;
        }
        value |= static_cast<quint64>(static_cast<unsigned char>(byte) & 0x7F) << shift;
        if (!(static_cast<unsigned char>(byte) & 0x80)) {
            return true;
        }
    }
    return false;
}

} // namespace

bool isSolidRar(const QString &archivePath) {
    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray signature = file.read(sizeof(kRar5Signature));
    if (signature == QByteArray::fromRawData(kRar5Signature, sizeof(kRar5Signature))) {
        constexpr quint64 kArchiveSolid = 0x4;
        quint64 headerSize = 0;
        quint64 type = 0;
        quint64 flags = 0;
        if (file.read(4).size() != 4 || !readVint(file, headerSize) || !readVint(file, type)
            || !readVint(file, flags) || type != 1) {
            return false;
        }
        quint64 skipped = 0;
        if ((flags & kBlockHasExtra) && !readVint(file, skipped)) {
            return false;
        }
        if ((flags & kBlockHasData) && !readVint(file, skipped)) {
            return false;
        }
        quint64 archiveFlags = 0;
        return readVint(file, archiveFlags) && (archiveFlags & kArchiveSolid);
    }

    // RAR 1.5-4.x: "Rar!\x1A\x07\x00", then CRC16, type 0x73 (main header) and 16 bit flags.
    if (signature.size() >= 7 && signature.startsWith(QByteArray("Rar!\x1A\x07\x00", 7))) {
        constexpr unsigned char kMainHeaderType = 0x73;
        constexpr unsigned kMainHeaderSolid = 0x0008;
        const QByteArray header = signature.mid(7) + file.read(5 - (signature.size() - 7));
        if (header.size() < 5 || static_cast<unsigned char>(header[2]) != kMainHeaderType) {
            return false;
        }
        const unsigned flags = static_cast<unsigned char>(header[3])
                             | (static_cast<unsigned>(static_cast<unsigned char>(header[4])) << 8);
        return (flags & kMainHeaderSolid) != 0;
    }
    return false;
}

void Crc32::update(const char *data, qsizetype size) {
    const auto &table = crcTable();
    quint32 state = m_state;
    for (qsizetype i = 0; i < size; ++i) {
        state = table[(state ^ static_cast<unsigned char>(data[i])) & 0xFF] ^ (state >> 8);
    }
    m_state = state;
}

Rar5StoredCrc::Rar5StoredCrc(const QString &archivePath) {
    QFile file(archivePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    if (file.read(sizeof(kRar5Signature)) != QByteArray::fromRawData(kRar5Signature, sizeof(kRar5Signature))) {
        return;
    }

    const qint64 fileSize = file.size();
    QHash<QString, QList<std::optional<quint32>>> crcs;

    // Any surprise below means "cannot verify"; libarchive still validates what it can.
    while (file.pos() + 4 < fileSize) {
        if (file.read(4).size() != 4) {
            return;
        }

        quint64 headerSize = 0;
        if (!readVint(file, headerSize)) {
            return;
        }
        const qint64 headerStart = file.pos();

        quint64 type = 0;
        quint64 flags = 0;
        if (!readVint(file, type) || !readVint(file, flags)) {
            return;
        }
        quint64 extraSize = 0;
        if ((flags & kBlockHasExtra) && !readVint(file, extraSize)) {
            return;
        }
        quint64 dataSize = 0;
        if ((flags & kBlockHasData) && !readVint(file, dataSize)) {
            return;
        }

        if (type == kTypeEncryption) {
            return;
        }

        if (type == kTypeFile) {
            quint64 fileFlags = 0;
            quint64 unpackedSize = 0;
            quint64 attributes = 0;
            if (!readVint(file, fileFlags) || !readVint(file, unpackedSize) || !readVint(file, attributes)) {
                return;
            }
            if (fileFlags & kFileHasMtime) {
                if (file.read(4).size() != 4) {
                    return;
                }
            }

            std::optional<quint32> crc;
            if (fileFlags & kFileHasCrc) {
                const QByteArray raw = file.read(4);
                if (raw.size() != 4) {
                    return;
                }
                crc = static_cast<quint32>(static_cast<unsigned char>(raw[0]))
                    | static_cast<quint32>(static_cast<unsigned char>(raw[1])) << 8
                    | static_cast<quint32>(static_cast<unsigned char>(raw[2])) << 16
                    | static_cast<quint32>(static_cast<unsigned char>(raw[3])) << 24;
            }

            quint64 compressionInfo = 0;
            quint64 hostOs = 0;
            quint64 nameLength = 0;
            if (!readVint(file, compressionInfo) || !readVint(file, hostOs) || !readVint(file, nameLength)
                || nameLength == 0 || nameLength > 65536) {
                return;
            }
            const QByteArray name = file.read(static_cast<qint64>(nameLength));
            if (name.size() != static_cast<qint64>(nameLength)) {
                return;
            }

            if (!(fileFlags & kFileIsDirectory)) {
                const bool stored = ((compressionInfo >> 7) & 0x7) == 0;
                const bool split = (flags & (kBlockSplitBefore | kBlockSplitAfter)) != 0;
                const QString path = QString::fromUtf8(name).replace(u'\\', u'/');
                crcs[path].append((stored && !split) ? crc : std::nullopt);
            }
        }

        if (type == kTypeEnd) {
            break;
        }

        const quint64 next = static_cast<quint64>(headerStart) + headerSize + dataSize;
        if (next > static_cast<quint64>(fileSize) || !file.seek(static_cast<qint64>(next))) {
            break;
        }
    }

    m_crcs = std::move(crcs);
}

std::optional<quint32> Rar5StoredCrc::take(const QString &entryPath) {
    auto it = m_crcs.find(entryPath);
    if (it == m_crcs.end() || it->isEmpty()) {
        return std::nullopt;
    }
    return it->takeFirst();
}
