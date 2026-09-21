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
#include <QElapsedTimer>
#include <QTimer>
#include <QSignalSpy>
#include <QThread>
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

/// A valid pak of roughly @p fillerSize bytes.
static QByteArray pakBytesOfSize(qsizetype fillerSize) {
    return QByteArray(fillerSize, 'y') + validPakBytes().right(44);
}

static QStringList partFiles(const QString &dir) {
    return QDir(dir).entryList({QStringLiteral("*.part")}, QDir::Files);
}

/// A mod library plus a fake Foxhole install with an empty paks folder.
struct ModLibrary {
    QTemporaryDir storage;
    QTemporaryDir install;
    QTemporaryDir work;
    ModManager manager;

    ModLibrary() {
        manager.setModsStoragePath(storage.path());
        QDir().mkpath(paksPath());
        manager.setInstallPath(install.path());
    }

    QString paksPath() const { return install.filePath("War/Content/Paks"); }

    /// Adds a mod (not enabled) and returns its id.
    QString addMod(const QString &name, const QByteArray &bytes) {
        const QString source = work.filePath(name + ".pak");
        if (!writeAll(source, bytes) || !manager.addMod(source, {.name = name})) {
            return QString();
        }
        return manager.getMods().last().id;
    }

    QStringList paksFiles(const QString &pattern = QStringLiteral("*")) const {
        return QDir(paksPath()).entryList({pattern}, QDir::Files);
    }
};

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

    void testExtractAsyncDeliversOnCallingThread() {
        QObject context;
        bool finished = false;
        bool success = false;
        QThread *callbackThread = nullptr;
        QString tempDir;

        ArchiveExtractor::extractPakFilesAsync(fixture("mod_lzma.7z"), &context,
            [&](const ArchiveExtractor::ExtractResult &result) {
            finished = true;
            success = result.success;
            tempDir = result.tempDir;
            callbackThread = QThread::currentThread();
        });

        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QVERIFY(success);
        QCOMPARE(callbackThread, QThread::currentThread());
        ArchiveExtractor::cleanupTempDir(tempDir);
    }

    void testExtractAsyncReportsFailures() {
        QObject context;
        bool finished = false;
        bool success = true;
        QString error;

        ArchiveExtractor::extractPakFilesAsync(fixture("not_a_pak.pak"), &context,
            [&](const ArchiveExtractor::ExtractResult &result) {
            finished = true;
            success = result.success;
            error = result.error;
        });

        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QVERIFY(!success);
        QVERIFY(!error.isEmpty());
    }

    void testExtractAsyncCleansUpWhenContextIsDestroyed() {
        const QStringList before = extractDirs();

        bool called = false;
        auto *context = new QObject;
        ArchiveExtractor::extractPakFilesAsync(fixture("mod_lzma.7z"), context,
            [&](const ArchiveExtractor::ExtractResult &) { called = true; });
        delete context;

        QTest::qWait(300);
        QVERIFY2(!called, "the callback must not run once its context is gone");
        QTRY_COMPARE_WITH_TIMEOUT(extractDirs(), before, 5000);
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

        bool finished = false;
        bool installed = false;
        manager.replaceModFromFile(mod.id, download, "2.0", "42", QDateTime(), &manager, [&](bool ok) {
            finished = true;
            installed = ok;
        });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QVERIFY(installed);

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
        bool finished = false;
        bool installed = true;
        manager.replaceModFromFile(mod.id, damaged, "2.0", "42", QDateTime(), &manager, [&](bool ok) {
            finished = true;
            installed = ok;
        });
        QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
        QVERIFY(!installed);
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
        // The archive holds 1 GiB of zeros ahead of the pak. Skipping it means decoding all of it.
        const QString path = fixture("mod_solid_bigfiller.7z");

        QElapsedTimer full;
        full.start();
        {
            ArchiveExtractor extractor;
            const auto result = extractor.extractPakFiles(path);
            QVERIFY2(result.success, qPrintable(result.error));
            QCOMPARE(result.pakFiles.size(), 1);
            // libarchive's own skip of an entry this big makes the next one read back empty.
            QCOMPARE(QFileInfo(result.pakFiles.first()).size(), qint64(4444));
            ArchiveExtractor::cleanupTempDir(result.tempDir);
        }
        const qint64 fullMs = full.elapsed();

        QObject context;
        bool finished = false;
        bool wasCancelled = false;
        qint64 finishedAt = 0;
        QElapsedTimer clock;
        clock.start();
        const auto token = ArchiveExtractor::extractPakFilesAsync(path, &context,
            [&](const ArchiveExtractor::ExtractResult &result) {
            finished = true;
            wasCancelled = result.cancelled;
            finishedAt = clock.elapsed();
        });

        QTest::qWait(static_cast<int>(qMin<qint64>(150, fullMs / 4)));
        const qint64 cancelledAt = clock.elapsed();
        token->cancel();
        QTRY_VERIFY_WITH_TIMEOUT(finished, 30000);

        QVERIFY(wasCancelled);
        // Without block-wise skipping the cancel would only be seen after the whole 1 GiB was decoded.
        if (fullMs > 1000) {
            QVERIFY2(finishedAt - cancelledAt < 500,
                     qPrintable(QStringLiteral("stopped %1 ms after cancel (full extraction: %2 ms)")
                                    .arg(finishedAt - cancelledAt).arg(fullMs)));
        }
    }

    void testExtractorHonoursCancellation() {
        CancelToken token;
        token.cancel();

        for (const QString &name : {QStringLiteral("mod_lzma.7z"), QStringLiteral("mod_stored.rar"),
                                    QStringLiteral("mod.tar.gz"), QStringLiteral("mod_deflate.zip")}) {
            ArchiveExtractor extractor;
            const auto result = extractor.extractPakFiles(fixture(name), &token);
            QVERIFY2(!result.success, qPrintable(name));
            QVERIFY2(result.cancelled, qPrintable(name));
            QVERIFY2(result.tempDir.isEmpty(), qPrintable(name));
        }
    }

    void testUpdateExtractorHonoursCancellation() {
        QTemporaryDir out;
        QVERIFY(out.isValid());
        CancelToken token;
        token.cancel();

        for (const QString &name : {QStringLiteral("mod_deflate.zip"), QStringLiteral("mod_lzma.7z")}) {
            QString error;
            QVERIFY2(!UpdateArchiveExtractor::extractArchive(fixture(name), out.filePath("staging"), &error, &token),
                     qPrintable(name));
            QCOMPARE(error, QStringLiteral("Cancelled."));
        }
    }

    void testModManagerAddModAsync() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString pak = work.filePath("Async.pak");
        QVERIFY(writeAll(pak, pakBytesOfSize(1 << 20)));

        bool done = false;
        bool added = false;
        QThread *callbackThread = nullptr;
        const auto token = manager.addModAsync(pak, {.name = "Async"}, &manager, [&](bool ok) {
            done = true;
            added = ok;
            callbackThread = QThread::currentThread();
        });
        QVERIFY(token);
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QVERIFY(added);
        QCOMPARE(callbackThread, QThread::currentThread());
        QCOMPARE(int(manager.getMods().size()), 1);
        QCOMPARE(readAll(storage.filePath("Async.pak")), readAll(pak));
        QVERIFY(partFiles(storage.path()).isEmpty());
    }

    void testModManagerAddModAsyncReportsInvalidFilesAtOnce() {
        QTemporaryDir storage;
        QVERIFY(storage.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());
        QSignalSpy errors(&manager, &ModManager::errorOccurred);

        bool done = false;
        bool added = true;
        const auto token = manager.addModAsync(fixture("not_a_pak.pak"), {.name = "Fake"}, &manager, [&](bool ok) {
            done = true;
            added = ok;
        });

        QVERIFY2(done, "validation failures are reported before the call returns");
        QVERIFY(!added);
        QVERIFY(!token);
        QCOMPARE(errors.size(), 1);
        QVERIFY(manager.getMods().isEmpty());
    }

    void testModManagerAddModAsyncDroppedContextLeavesNothing() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString pak = work.filePath("Gone.pak");
        QVERIFY(writeAll(pak, pakBytesOfSize(8 << 20)));

        bool called = false;
        auto *context = new QObject;
        manager.addModAsync(pak, {.name = "Gone"}, context, [&](bool) { called = true; });
        delete context;

        QTest::qWait(300);
        QVERIFY2(!called, "the callback must not run once its context is gone");
        QVERIFY(manager.getMods().isEmpty());
        QVERIFY(!QFile::exists(storage.filePath("Gone.pak")));
        QTRY_VERIFY_WITH_TIMEOUT(partFiles(storage.path()).isEmpty(), 5000);
    }

    void testModManagerAddModAsyncCancelStaysConsistent() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());
        QSignalSpy errors(&manager, &ModManager::errorOccurred);

        const QString pak = work.filePath("Big.pak");
        QVERIFY(writeAll(pak, pakBytesOfSize(64 << 20)));

        bool done = false;
        bool added = false;
        const auto token = manager.addModAsync(pak, {.name = "Big"}, &manager, [&](bool ok) {
            done = true;
            added = ok;
        });
        QVERIFY(token);
        token->cancel();
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);

        // Whether the cancel landed in time or not, the outcome must be all or nothing.
        QCOMPARE(int(manager.getMods().size()), added ? 1 : 0);
        QCOMPARE(QFile::exists(storage.filePath("Big.pak")), added);
        QVERIFY(partFiles(storage.path()).isEmpty());
        QCOMPARE(errors.size(), 0);
    }

    void testModManagerReplaceModAsync() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString original = work.filePath("Fixture.pak");
        QVERIFY(writeAll(original, validPakBytes()));
        QVERIFY(manager.addMod(original, {.name = "Fixture", .version = "1.0"}));
        const ModInfo mod = manager.getMods().first();

        const QString update = work.filePath("Update.pak");
        const QByteArray updateBytes = pakBytesOfSize(2 << 20);
        QVERIFY(writeAll(update, updateBytes));

        bool done = false;
        bool replaced = false;
        manager.replaceModAsync(mod.id, update, "2.0", "42", QDateTime(), &manager, [&](bool ok) {
            done = true;
            replaced = ok;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QVERIFY(replaced);
        QCOMPARE(readAll(storage.filePath(mod.fileName)), updateBytes);
        QCOMPARE(manager.getMod(mod.id).version, QStringLiteral("2.0"));
        QVERIFY(partFiles(storage.path()).isEmpty());
    }

    void testModManagerReplaceFromArchiveCancelStaysConsistent() {
        QTemporaryDir storage;
        QTemporaryDir work;
        QVERIFY(storage.isValid() && work.isValid());
        ModManager manager;
        manager.setModsStoragePath(storage.path());

        const QString original = work.filePath("Fixture.pak");
        QVERIFY(writeAll(original, validPakBytes()));
        QVERIFY(manager.addMod(original, {.name = "Fixture", .version = "1.0"}));
        const ModInfo mod = manager.getMods().first();
        const QStringList dirsBefore = extractDirs();
        QSignalSpy errors(&manager, &ModManager::errorOccurred);

        bool done = false;
        bool replaced = false;
        const auto token = manager.replaceModFromFile(mod.id, fixture("mod_lzma.7z"), "2.0", "42", QDateTime(),
                                                      &manager, [&](bool ok) {
            done = true;
            replaced = ok;
        });
        QVERIFY(token);
        token->cancel();
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        const QByteArray stored = readAll(storage.filePath(mod.fileName));
        if (replaced) {
            QCOMPARE(stored.size(), qsizetype(4444));
            QCOMPARE(manager.getMod(mod.id).version, QStringLiteral("2.0"));
        } else {
            QCOMPARE(stored, validPakBytes());
            QCOMPARE(manager.getMod(mod.id).version, QStringLiteral("1.0"));
            QCOMPARE(errors.size(), 0);
        }
        QVERIFY(partFiles(storage.path()).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(extractDirs(), dirsBefore, 5000);
    }

    void testEnableModsAsync() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QByteArray bytesA = pakBytesOfSize(1 << 20);
        const QByteArray bytesB = pakBytesOfSize(3 << 20);
        const QString idA = lib.addMod("Alpha", bytesA);
        const QString idB = lib.addMod("Bravo", bytesB);
        QVERIFY(!idA.isEmpty() && !idB.isEmpty());

        QSignalSpy enabledSignals(&lib.manager, &ModManager::modEnabled);
        QSignalSpy progress(&lib.manager, &ModManager::enableProgress);
        QSignalSpy errors(&lib.manager, &ModManager::errorOccurred);

        bool done = false;
        ModManager::EnableOutcome outcome;
        QThread *callbackThread = nullptr;
        const auto token = lib.manager.setModsEnabledAsync({idA, idB}, true, &lib.manager,
            [&](const ModManager::EnableOutcome &result) {
            done = true;
            outcome = result;
            callbackThread = QThread::currentThread();
        });
        QVERIFY(token);
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QVERIFY(outcome.ok());
        QCOMPARE(outcome.enabled, 2);
        QCOMPARE(callbackThread, QThread::currentThread());
        QCOMPARE(enabledSignals.size(), 2);
        QCOMPARE(errors.size(), 0);
        QVERIFY(!progress.isEmpty());
        QCOMPARE(progress.last().at(0).toInt(), 2);
        QCOMPARE(progress.last().at(1).toInt(), 2);

        const ModInfo a = lib.manager.getMod(idA);
        const ModInfo b = lib.manager.getMod(idB);
        QVERIFY(a.enabled && b.enabled);
        QCOMPARE(readAll(lib.paksPath() + "/" + a.numberedFileName), bytesA);
        QCOMPARE(readAll(lib.paksPath() + "/" + b.numberedFileName), bytesB);
        QCOMPARE(lib.paksFiles().size(), 2);
        QVERIFY(lib.paksFiles("*.part").isEmpty());
    }

    void testEnableModsAsyncReportsFailuresAndKeepsTheRest() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QString good = lib.addMod("Good", pakBytesOfSize(1 << 20));
        const QString broken = lib.addMod("Broken", pakBytesOfSize(1 << 20));
        QVERIFY(QFile::remove(lib.storage.filePath(lib.manager.getMod(broken).fileName)));
        QSignalSpy errors(&lib.manager, &ModManager::errorOccurred);

        bool done = false;
        ModManager::EnableOutcome outcome;
        lib.manager.setModsEnabledAsync({broken, good}, true, &lib.manager,
                                        [&](const ModManager::EnableOutcome &result) {
            done = true;
            outcome = result;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QVERIFY(!outcome.ok());
        QCOMPARE(outcome.enabled, 1);
        QCOMPARE(outcome.failed, 1);
        QCOMPARE(errors.size(), 1);
        QVERIFY(lib.manager.getMod(good).enabled);
        QVERIFY(!lib.manager.getMod(broken).enabled);
        QCOMPARE(lib.paksFiles().size(), 1);
    }

    void testEnableModsAsyncWithoutPaksFolderFails() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QString id = lib.addMod("Solo", pakBytesOfSize(1 << 16));
        QVERIFY(QDir(lib.paksPath()).removeRecursively());
        QSignalSpy errors(&lib.manager, &ModManager::errorOccurred);

        bool done = false;
        ModManager::EnableOutcome outcome;
        lib.manager.setModsEnabledAsync({id}, true, &lib.manager, [&](const ModManager::EnableOutcome &result) {
            done = true;
            outcome = result;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QCOMPARE(outcome.failed, 1);
        QCOMPARE(outcome.enabled, 0);
        QCOMPARE(errors.size(), 1);
        QVERIFY(!lib.manager.getMod(id).enabled);
    }

    void testEnableModsAsyncCancelStaysConsistent() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        QStringList ids;
        for (const QString &name : {QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")}) {
            ids << lib.addMod(name, pakBytesOfSize(48 << 20));
        }
        QSignalSpy errors(&lib.manager, &ModManager::errorOccurred);

        bool done = false;
        ModManager::EnableOutcome outcome;
        const auto token = lib.manager.setModsEnabledAsync(ids, true, &lib.manager,
            [&](const ModManager::EnableOutcome &result) {
            done = true;
            outcome = result;
        });
        QVERIFY(token);
        token->cancel();
        QTRY_VERIFY_WITH_TIMEOUT(done, 20000);

        // Whatever the cancel caught, state and the paks folder must agree, with nothing half-written.
        int enabledMods = 0;
        for (const QString &id : ids) {
            const ModInfo mod = lib.manager.getMod(id);
            QCOMPARE(QFile::exists(lib.paksPath() + "/" + mod.numberedFileName) && mod.enabled, mod.enabled);
            enabledMods += mod.enabled ? 1 : 0;
        }
        QCOMPARE(lib.paksFiles().size(), enabledMods);
        QCOMPARE(outcome.enabled, enabledMods);
        QVERIFY(lib.paksFiles("*.part").isEmpty());
        QCOMPARE(errors.size(), 0);
        if (enabledMods < ids.size()) {
            QVERIFY(outcome.cancelled);
        }
    }

    void testEnableJobsRunOneAfterAnother() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QString id = lib.addMod("Queued", pakBytesOfSize(8 << 20));

        QList<int> enabledCounts;
        for (int i = 0; i < 2; ++i) {
            lib.manager.setModsEnabledAsync({id}, true, &lib.manager, [&](const ModManager::EnableOutcome &result) {
                enabledCounts.append(result.enabled);
            });
        }
        QTRY_COMPARE_WITH_TIMEOUT(enabledCounts.size(), 2, 10000);

        QCOMPARE(enabledCounts, (QList<int>{1, 0}));
        QVERIFY(lib.manager.getMod(id).enabled);
        QCOMPARE(lib.paksFiles().size(), 1);
    }

    void testEnableJobFinishesEvenIfItsContextIsDestroyed() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QString id = lib.addMod("Orphan", pakBytesOfSize(8 << 20));

        bool called = false;
        auto *context = new QObject;
        lib.manager.setModsEnabledAsync({id}, true, context, [&](const ModManager::EnableOutcome &) { called = true; });
        delete context;

        QTRY_VERIFY_WITH_TIMEOUT(lib.manager.getMod(id).enabled, 10000);
        QVERIFY2(!called, "the callback must not run once its context is gone");
        QVERIFY(QFile::exists(lib.paksPath() + "/" + lib.manager.getMod(id).numberedFileName));
        QVERIFY(lib.paksFiles("*.part").isEmpty());
    }

    void testReplaceModAsyncKeepsAnEnabledModEnabled() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QString id = lib.addMod("Live", validPakBytes());
        QVERIFY(lib.manager.enableMod(id));
        QVERIFY(lib.manager.getMod(id).enabled);

        const QByteArray updateBytes = pakBytesOfSize(4 << 20);
        const QString update = lib.work.filePath("LiveUpdate.pak");
        QVERIFY(writeAll(update, updateBytes));

        bool done = false;
        bool replaced = false;
        lib.manager.replaceModAsync(id, update, "2.0", "42", QDateTime(), &lib.manager, [&](bool ok) {
            done = true;
            replaced = ok;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);

        QVERIFY(replaced);
        const ModInfo mod = lib.manager.getMod(id);
        QVERIFY2(mod.enabled, "an enabled mod must be enabled again once the update is installed");
        QCOMPARE(mod.version, QStringLiteral("2.0"));
        QCOMPARE(readAll(lib.storage.filePath(mod.fileName)), updateBytes);
        QCOMPARE(readAll(lib.paksPath() + "/" + mod.numberedFileName), updateBytes);
        QCOMPARE(lib.paksFiles().size(), 1);
        QVERIFY(lib.paksFiles("*.part").isEmpty());
    }

    void testReorderingRenamesPaksInsteadOfCopyingThem() {
        ModLibrary lib;
        QVERIFY(lib.storage.isValid() && lib.install.isValid() && lib.work.isValid());
        const QByteArray bytesA = pakBytesOfSize(2 << 20);
        const QByteArray bytesB = pakBytesOfSize(3 << 20);
        const QString idA = lib.addMod("Alpha", bytesA);
        const QString idB = lib.addMod("Bravo", bytesB);
        QVERIFY(lib.manager.enableMod(idA));
        QVERIFY(lib.manager.enableMod(idB));

        const ModInfo before = lib.manager.getMod(idA);
        const QDateTime born = QFileInfo(lib.paksPath() + "/" + before.numberedFileName).birthTime();
        QTest::qWait(50);

        QVERIFY(lib.manager.setModPriority(idA, 5));
        const ModInfo after = lib.manager.getMod(idA);
        QVERIFY(after.numberedFileName != before.numberedFileName);
        QVERIFY(!QFile::exists(lib.paksPath() + "/" + before.numberedFileName));
        QCOMPARE(readAll(lib.paksPath() + "/" + after.numberedFileName), bytesA);
        QCOMPARE(readAll(lib.paksPath() + "/" + lib.manager.getMod(idB).numberedFileName), bytesB);
        QCOMPARE(lib.paksFiles().size(), 2);
        if (born.isValid()) {
            QCOMPARE(QFileInfo(lib.paksPath() + "/" + after.numberedFileName).birthTime(), born);
        }
    }

    void testUpdateArchiveExtractorRejectsTruncatedArchive() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // A truncated update must fail with an error, never leave a half-extracted staging directory that looks valid.
        for (const QString &name : {QStringLiteral("mod_deflate.zip"), QStringLiteral("mod_lzma.7z")}) {
            const QByteArray bytes = readAll(fixture(name));
            const QString truncated = dir.filePath("truncated_" + name);
            QVERIFY(writeAll(truncated, bytes.left(bytes.size() / 2)));

            QString error;
            QVERIFY2(!UpdateArchiveExtractor::extractArchive(truncated, dir.filePath("staging_" + name), &error),
                     qPrintable(name));
            QVERIFY(!error.isEmpty());
        }
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

QTEST_GUILESS_MAIN(TestUtilities)

#include "TestUtilities.moc"
