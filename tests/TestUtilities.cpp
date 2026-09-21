#include <QtTest/QtTest>

#include "core/utils/ArchiveExtractor.h"
#include "core/utils/UpdateArchiveExtractor.h"
#include "core/utils/UpdateCleanup.h"
#include "core/services/UpdaterService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
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

class TestUtilities : public QObject {
    Q_OBJECT

private slots:
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
        pakFile.write("pak-data");
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
        ArchiveExtractor::cleanupTempDir(result.tempDir);
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
            ArchiveExtractor::cleanupTempDir(result.tempDir);
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

QTEST_APPLESS_MAIN(TestUtilities)

#include "TestUtilities.moc"
