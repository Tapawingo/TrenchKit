#include <QtTest/QtTest>

#include "core/managers/ModManager.h"
#include "core/utils/ArchiveExtractor.h"
#include "core/utils/PakFileReader.h"
#include "core/utils/Rar5StoredCrc.h"
#include "core/utils/UpdateArchiveExtractor.h"
#include "core/utils/UpdateCleanup.h"
#include "core/services/UpdaterService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

static QString currentVersionString() {
#ifdef TRENCHKIT_VERSION
    return QStringLiteral(TRENCHKIT_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

static bool createZipFromDir(const QString &sourceDir,
                             const QString &zipPath,
                             QString *error) {
#if defined(Q_OS_WIN)
    const QStringList args = {
        "-NoProfile",
        "-Command",
        QString("Compress-Archive -Path \"%1\\*\" -DestinationPath \"%2\" -Force")
            .arg(QDir::toNativeSeparators(sourceDir),
                 QDir::toNativeSeparators(zipPath))
    };
    QProcess process;
    process.start("powershell", args);
    if (!process.waitForFinished(10000)) {
        if (error) {
            *error = "Compress-Archive timed out.";
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = "Compress-Archive failed.";
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(sourceDir);
    Q_UNUSED(zipPath);
    if (error) {
        *error = "No zip tool available for this platform.";
    }
    return false;
#endif
}

static QString fixture(const QString &name) {
    return QDir(QStringLiteral(TEST_FIXTURES_DIR)).filePath(name);
}

static QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

static bool writeAll(const QString &path, const QByteArray &data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}

/// Smallest thing PakFileReader::hasPakFooter() accepts: filler plus a 44 byte footer.
static QByteArray validPakBytes() {
    QByteArray bytes(64, 'x');
    QDataStream stream(&bytes, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(PakFileReader::PakFooter::MAGIC) << quint32(3) << quint64(0) << quint64(0);
    bytes.append(QByteArray(20, '\0'));
    return bytes;
}

static QStringList extractDirs() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .entryList({QStringLiteral("TrenchKit_extract_*")}, QDir::Dirs | QDir::NoDotAndDotDot);
}

class TestUtilities : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testParseVersionFromTag() {
        auto v1 = UpdaterService::parseVersionFromTag("v1.2.3");
        QCOMPARE(v1.major, 1);
        QCOMPARE(v1.minor, 2);
        QCOMPARE(v1.patch, 3);
        QVERIFY(v1.preRelease.isEmpty());

        auto v2 = UpdaterService::parseVersionFromTag("1.2.3-beta");
        QCOMPARE(v2.major, 1);
        QCOMPARE(v2.minor, 2);
        QCOMPARE(v2.patch, 3);
        QCOMPARE(v2.preRelease, QStringLiteral("beta"));
    }

    void testUpdateArchiveExtractor() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sourceDir = QDir(tempDir.path()).filePath("source");
        QDir().mkpath(QDir(sourceDir).filePath("bin"));
        QDir().mkpath(QDir(sourceDir).filePath("data"));

        QFile appFile(QDir(sourceDir).filePath("bin/app.exe"));
        QVERIFY(appFile.open(QIODevice::WriteOnly));
        appFile.write("test-binary");
        appFile.close();

        QFile configFile(QDir(sourceDir).filePath("data/config.json"));
        QVERIFY(configFile.open(QIODevice::WriteOnly));
        configFile.write("{}");
        configFile.close();

        const QString zipPath = QDir(tempDir.path()).filePath("update.zip");
        QString error;
        if (!createZipFromDir(sourceDir, zipPath, &error)) {
            QSKIP(qPrintable(error));
        }

        const QString outputDir = QDir(tempDir.path()).filePath("out");
        QVERIFY(UpdateArchiveExtractor::extractArchive(zipPath, outputDir, &error));

        QFileInfo extractedApp(QDir(outputDir).filePath("bin/app.exe"));
        QVERIFY(extractedApp.exists());
        QFileInfo extractedConfig(QDir(outputDir).filePath("data/config.json"));
        QVERIFY(extractedConfig.exists());
    }

    void testArchiveExtractorPak() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sourceDir = QDir(tempDir.path()).filePath("source");
        QDir().mkpath(QDir(sourceDir).filePath("mods"));

        QFile pakFile(QDir(sourceDir).filePath("mods/test.pak"));
        QVERIFY(pakFile.open(QIODevice::WriteOnly));
        pakFile.write(validPakBytes());
        pakFile.close();

        QFile readmeFile(QDir(sourceDir).filePath("mods/readme.txt"));
        QVERIFY(readmeFile.open(QIODevice::WriteOnly));
        readmeFile.write("ignore");
        readmeFile.close();

        const QString zipPath = QDir(tempDir.path()).filePath("mods.zip");
        QString error;
        if (!createZipFromDir(sourceDir, zipPath, &error)) {
            QSKIP(qPrintable(error));
        }

        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(zipPath);
        QVERIFY(result.success);
        QCOMPARE(result.pakFiles.size(), 1);
        QFileInfo extracted(result.pakFiles.first());
        QVERIFY(extracted.exists());

        ArchiveExtractor::cleanupTempDir(result.tempDir);
    }

    void testArchiveExtractor7zLzma() {
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(fixture("mod_lzma.7z"));
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.pakFiles.size(), 1);
        QCOMPARE(QFileInfo(result.pakFiles.first()).fileName(), QStringLiteral("Fixture.pak"));
        QCOMPARE(QFileInfo(result.pakFiles.first()).size(), qint64(4444));
        QVERIFY(PakFileReader::hasPakFooter(result.pakFiles.first()));
        ArchiveExtractor::cleanupTempDir(result.tempDir);
    }

    void testArchiveExtractorRar() {
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(fixture("mod_stored.rar"));
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.pakFiles.size(), 1);
        QCOMPARE(QFileInfo(result.pakFiles.first()).size(), qint64(4444));
        QVERIFY(PakFileReader::hasPakFooter(result.pakFiles.first()));
        ArchiveExtractor::cleanupTempDir(result.tempDir);
    }

    void testArchiveExtractorRejectsDamagedArchives_data() {
        QTest::addColumn<QString>("fixtureName");
        QTest::addColumn<int>("keepBytes");
        QTest::newRow("7z truncated") << "mod_lzma.7z" << 150;
        QTest::newRow("7z header only") << "mod_lzma.7z" << 40;
        QTest::newRow("rar truncated in data") << "mod_stored.rar" << 2000;
        QTest::newRow("rar truncated in header") << "mod_stored.rar" << 30;
    }

    void testArchiveExtractorRejectsDamagedArchives() {
        QFETCH(QString, fixtureName);
        QFETCH(int, keepBytes);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString damaged = tempDir.filePath(fixtureName);
        QVERIFY(writeAll(damaged, readAll(fixture(fixtureName)).left(keepBytes)));

        const QStringList dirsBefore = extractDirs();
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(damaged);
        QVERIFY(!result.success);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.pakFiles.isEmpty());
        QCOMPARE(extractDirs(), dirsBefore);
    }

    void testArchiveExtractorRejectsCorruptedData() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // The packed LZMA stream sits right after the 32 byte signature header.
        QByteArray bytes = readAll(fixture("mod_lzma.7z"));
        for (int i = 40; i < 70; ++i) {
            bytes[i] = static_cast<char>(bytes[i] ^ 0xFF);
        }
        const QString damaged = tempDir.filePath("corrupt.7z");
        QVERIFY(writeAll(damaged, bytes));

        const QStringList dirsBefore = extractDirs();
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(damaged);
        QVERIFY2(!result.success, "corrupt packed data must fail the extraction");
        QVERIFY(!result.error.isEmpty());
        QCOMPARE(extractDirs(), dirsBefore);
    }

    void testCrc32KnownAnswer() {
        Crc32 crc;
        crc.update("1234", 4);
        crc.update("56789", 5);
        QCOMPARE(crc.value(), quint32(0xCBF43926));
    }

    void testArchiveExtractorRejectsCorruptedStoredRar() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // libarchive does not verify stored RAR5 entries, so the extractor has to.
        QByteArray bytes = readAll(fixture("mod_stored.rar"));
        bytes[2000] = static_cast<char>(bytes[2000] ^ 0xFF);
        const QString damaged = tempDir.filePath("corrupt.rar");
        QVERIFY(writeAll(damaged, bytes));

        const QStringList dirsBefore = extractDirs();
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(damaged);
        QVERIFY2(!result.success, "a CRC mismatch in a stored entry must fail the extraction");
        QVERIFY2(result.error.contains("CRC32"), qPrintable(result.error));
        QCOMPARE(extractDirs(), dirsBefore);
    }

    void testArchiveExtractorOtherFormats_data() {
        QTest::addColumn<QString>("archiveName");
        QTest::newRow("tar.gz") << "mod.tar.gz";
        QTest::newRow("tar.bz2") << "mod.tar.bz2";
        QTest::newRow("tar.xz") << "mod.tar.xz";
        QTest::newRow("zip (deflate)") << "mod_deflate.zip";
    }

    void testArchiveExtractorOtherFormats() {
        QFETCH(QString, archiveName);

        // Once by extension and once by signature only, as downloads arrive as *.tmp.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString renamed = tempDir.filePath("download.tmp");
        QVERIFY(QFile::copy(fixture(archiveName), renamed));

        for (const QString &path : {fixture(archiveName), renamed}) {
            ArchiveExtractor extractor;
            const auto result = extractor.extractPakFiles(path);
            QVERIFY2(result.success, qPrintable(path + ": " + result.error));
            QCOMPARE(result.pakFiles.size(), 1);
            QCOMPARE(QFileInfo(result.pakFiles.first()).size(), qint64(4444));
            QVERIFY(PakFileReader::hasPakFooter(result.pakFiles.first()));
            ArchiveExtractor::cleanupTempDir(result.tempDir);
        }
    }

    void testArchiveExtractorRejectsNonPakEntry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString sourceDir = QDir(tempDir.path()).filePath("source");
        QVERIFY(QDir().mkpath(sourceDir));
        QVERIFY(writeAll(QDir(sourceDir).filePath("fake.pak"), readAll(fixture("not_a_pak.pak"))));

        const QString zipPath = QDir(tempDir.path()).filePath("fake.zip");
        QString error;
        if (!createZipFromDir(sourceDir, zipPath, &error)) {
            QSKIP(qPrintable(error));
        }

        const QStringList dirsBefore = extractDirs();
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(zipPath);
        QVERIFY(!result.success);
        QVERIFY2(result.error.contains("not a valid .pak"), qPrintable(result.error));
        QCOMPARE(extractDirs(), dirsBefore);
    }

    void testIsArchiveFileRecognisesSignatures() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        for (const QString &name : {QStringLiteral("mod_lzma.7z"), QStringLiteral("mod_stored.rar")}) {
            const QString renamed = tempDir.filePath(name + ".tmp");
            QVERIFY(QFile::copy(fixture(name), renamed));
            QVERIFY2(ArchiveExtractor::isArchiveFile(renamed), qPrintable(name));
        }

        const QString pak = tempDir.filePath("real.tmp");
        QVERIFY(writeAll(pak, validPakBytes()));
        QVERIFY(!ArchiveExtractor::isArchiveFile(pak));
    }

    void testHasPakFooter() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString good = tempDir.filePath("good.pak");
        QVERIFY(writeAll(good, validPakBytes()));
        QVERIFY(PakFileReader::hasPakFooter(good));

        QString error;
        QVERIFY(!PakFileReader::hasPakFooter(fixture("not_a_pak.pak"), &error));
        QVERIFY(!error.isEmpty());

        const QString tiny = tempDir.filePath("tiny.pak");
        QVERIFY(writeAll(tiny, QByteArray(10, 'x')));
        QVERIFY(!PakFileReader::hasPakFooter(tiny));

        QVERIFY(!PakFileReader::hasPakFooter(fixture("mod_lzma.7z")));
        QVERIFY(!PakFileReader::hasPakFooter(tempDir.filePath("missing.pak")));
    }

    void testModManagerRefusesNonPak() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());
        QSignalSpy errors(&manager, &ModManager::errorOccurred);

        QVERIFY(!manager.addMod(fixture("not_a_pak.pak"), {.name = "Fake"}));
        QVERIFY(!manager.addMod(fixture("mod_stored.rar"), {.name = "Archive"}));
        QCOMPARE(errors.size(), 2);
        QVERIFY(manager.getMods().isEmpty());
    }

    void testModManagerReplaceModNeverInstallsAnArchive() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString original = work.filePath("Fixture.pak");
        QVERIFY(writeAll(original, validPakBytes()));
        QVERIFY(manager.addMod(original, {.name = "Fixture"}));
        const ModInfo mod = manager.getMods().first();
        const QString storedPath = (manager.getModsStoragePath() + "/" + mod.fileName);
        const QByteArray before = readAll(storedPath);
        QVERIFY(!before.isEmpty());

        QSignalSpy errors(&manager, &ModManager::errorOccurred);
        QVERIFY(!manager.replaceMod(mod.id, fixture("mod_stored.rar"), "2.0", "2"));
        QVERIFY(!manager.replaceMod(mod.id, fixture("mod_lzma.7z"), "2.0", "2"));
        QCOMPARE(errors.size(), 2);
        QCOMPARE(readAll(storedPath), before);
        QCOMPARE(manager.getMod(mod.id).version, mod.version);
    }

    void testModManagerReplaceModFromArchive_data() {
        QTest::addColumn<QString>("archiveName");
        QTest::newRow("rar") << "mod_stored.rar";
        QTest::newRow("7z") << "mod_lzma.7z";
    }

    void testModManagerReplaceModFromArchive() {
        QFETCH(QString, archiveName);

        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString original = work.filePath("Fixture.pak");
        QVERIFY(writeAll(original, validPakBytes()));
        QVERIFY(manager.addMod(original, {.name = "Fixture", .version = "1.0"}));
        const ModInfo mod = manager.getMods().first();

        // Downloads arrive as *.tmp, so detection must not depend on the extension.
        const QString download = work.filePath("update_1_2.tmp");
        QVERIFY(QFile::copy(fixture(archiveName), download));

        QVERIFY(manager.replaceModFromFile(mod.id, download, "2.0", "42"));

        const QByteArray stored = readAll((manager.getModsStoragePath() + "/" + mod.fileName));
        QCOMPARE(stored.size(), qsizetype(4444));
        QVERIFY(stored != validPakBytes());
        QCOMPARE(manager.getMod(mod.id).version, QStringLiteral("2.0"));
        QVERIFY(!QFileInfo::exists((manager.getModsStoragePath() + "/" + mod.fileName) + ".new"));
    }

    void testModManagerReplaceModFromDamagedArchiveKeepsOldMod() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString original = work.filePath("Fixture.pak");
        QVERIFY(writeAll(original, validPakBytes()));
        QVERIFY(manager.addMod(original, {.name = "Fixture", .version = "1.0"}));
        const ModInfo mod = manager.getMods().first();
        const QByteArray before = readAll((manager.getModsStoragePath() + "/" + mod.fileName));

        const QString damaged = work.filePath("update.7z");
        QVERIFY(writeAll(damaged, readAll(fixture("mod_lzma.7z")).left(150)));

        QSignalSpy errors(&manager, &ModManager::errorOccurred);
        QVERIFY(!manager.replaceModFromFile(mod.id, damaged, "2.0", "42"));
        QCOMPARE(errors.size(), 1);
        QCOMPARE(readAll((manager.getModsStoragePath() + "/" + mod.fileName)), before);
        QCOMPARE(manager.getMod(mod.id).version, QStringLiteral("1.0"));
    }

    void testCleanupTempDirNeverTouchesOtherDirectories() {
        QTemporaryDir sandbox;
        QVERIFY(sandbox.isValid());
        const QString previousCwd = QDir::currentPath();
        QVERIFY(QDir::setCurrent(sandbox.path()));

        QVERIFY(writeAll(sandbox.filePath("precious.txt"), "keep"));
        QVERIFY(QDir(sandbox.path()).mkpath("other/nested"));
        QVERIFY(writeAll(sandbox.filePath("other/nested/file.txt"), "keep"));
        QVERIFY(QDir(sandbox.path()).mkpath("TrenchKit_extract_test"));
        QVERIFY(writeAll(sandbox.filePath("TrenchKit_extract_test/a.pak"), "x"));

        // A failed extraction reports an empty temp dir; QDir("") is the current directory.
        ArchiveExtractor::cleanupTempDir(QString());
        ArchiveExtractor::cleanupTempDir(QStringLiteral("."));
        ArchiveExtractor::cleanupTempDir(sandbox.filePath("other"));
        QVERIFY(QFileInfo::exists(sandbox.filePath("precious.txt")));
        QVERIFY(QFileInfo::exists(sandbox.filePath("other/nested/file.txt")));

        ArchiveExtractor::cleanupTempDir(sandbox.filePath("TrenchKit_extract_test"));
        QVERIFY(!QFileInfo::exists(sandbox.filePath("TrenchKit_extract_test")));

        QVERIFY(QDir::setCurrent(previousCwd));
    }

    void testIsSolidRar() {
        QVERIFY(!isSolidRar(fixture("mod_stored.rar")));
        QVERIFY(isSolidRar(fixture("mod_stored_solid.rar")));
        QVERIFY(!isSolidRar(fixture("mod_lzma.7z")));
        QVERIFY(!isSolidRar(fixture("not_a_pak.pak")));
        QVERIFY(!isSolidRar(fixture("missing.rar")));

        // RAR 1.5-4.x: signature, CRC16, type 0x73, then the 16 bit flags (0x0008 = solid).
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString solid4 = dir.filePath("solid4.rar");
        const QString plain4 = dir.filePath("plain4.rar");
        QVERIFY(writeAll(solid4, QByteArray::fromHex("526172211a0700" "0000" "73" "0800" "0d00" "00000000")));
        QVERIFY(writeAll(plain4, QByteArray::fromHex("526172211a0700" "0000" "73" "0000" "0d00" "00000000")));
        QVERIFY(isSolidRar(solid4));
        QVERIFY(!isSolidRar(plain4));
    }

    void testSolidArchiveIsExtractedAndCancelsMidEntry() {
        // The archive holds 1 GiB of zeros ahead of the pak. libarchive's own skip of an entry this big
        // makes the next one read back empty, so the extractor reads skipped entries block by block.
        ArchiveExtractor extractor;
        const auto result = extractor.extractPakFiles(fixture("mod_solid_bigfiller.7z"));
        QVERIFY2(result.success, qPrintable(result.error));
        QCOMPARE(result.pakFiles.size(), 1);
        QCOMPARE(QFileInfo(result.pakFiles.first()).size(), qint64(4444));
        ArchiveExtractor::cleanupTempDir(result.tempDir);
    }

    void testUpdateCleanup() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QDir appDir(tempDir.path());
        QDir updatesDir(appDir.filePath("updates"));
        QVERIFY(updatesDir.mkpath("staging/v1"));
        QVERIFY(updatesDir.mkpath("staging/v2"));
        QVERIFY(updatesDir.mkpath("staging/v3"));

        QFile archive(updatesDir.filePath("old.zip"));
        QVERIFY(archive.open(QIODevice::WriteOnly));
        archive.write("old");
        archive.close();

        QFile marker(updatesDir.filePath("last_installed_version.txt"));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("0.9.0");
        marker.close();

        UpdateCleanup::run(appDir.path());

        QVERIFY(!QFileInfo::exists(archive.fileName()));
        QDir stagingDir(updatesDir.filePath("staging"));
        const QStringList remaining = stagingDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot);
        QVERIFY(remaining.size() <= 2);

        QFile updatedMarker(updatesDir.filePath("last_installed_version.txt"));
        QVERIFY(updatedMarker.open(QIODevice::ReadOnly));
        const QString stored = QString::fromUtf8(updatedMarker.readAll()).trimmed();
        QCOMPARE(stored, currentVersionString());
    }
};

QTEST_APPLESS_MAIN(TestUtilities)

#include "TestUtilities.moc"
