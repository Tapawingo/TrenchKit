#include "ModManager.h"
#include <algorithm>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QUuid>
#include <QStandardPaths>
#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include "core/utils/ArchiveExtractor.h"
#include "core/utils/AsyncRunner.h"
#include "core/utils/ModManifestReader.h"
#include "core/utils/PakFileReader.h"

ModManager::ModManager(QObject *parent)
    : QObject(parent)
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_modsStoragePath = appDataPath + "/mods";
    QDir().mkpath(m_modsStoragePath);
}

void ModManager::setInstallPath(const QString &foxholeInstallPath) {
    m_foxholeInstallPath = foxholeInstallPath;
}

void ModManager::setModsStoragePath(const QString &modsPath) {
    m_modsStoragePath = modsPath;
    QDir().mkpath(m_modsStoragePath);
}

QString ModManager::getPaksPath() const {
    if (m_foxholeInstallPath.isEmpty()) {
        return QString();
    }

    // Foxhole paks are typically in War/Content/Paks
    return m_foxholeInstallPath + "/War/Content/Paks";
}

namespace {

/// Copies @p source to @p target in chunks, stopping early when @p cancel is set. Removes @p target on failure.
bool copyFileChunked(const QString &source, const QString &target, const CancelToken *cancel,
                     QString *error, bool *cancelled) {
    QFile in(source);
    QFile out(target);

    // Windows cannot delete a file that is still open, so close it before removing the partial copy.
    const auto fail = [&](const QString &message, bool wasCancelled = false) {
        out.close();
        QFile::remove(target);
        if (error) {
            *error = message;
        }
        if (cancelled) {
            *cancelled = wasCancelled;
        }
        return false;
    };

    if (!in.open(QIODevice::ReadOnly)) {
        return fail(in.errorString());
    }
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(out.errorString());
    }

    constexpr qint64 kChunkSize = 4 * 1024 * 1024;
    QByteArray chunk(kChunkSize, Qt::Uninitialized);
    qint64 copied = 0;
    for (;;) {
        if (cancel && cancel->isCancelled()) {
            return fail(QStringLiteral("Cancelled"), true);
        }
        const qint64 read = in.read(chunk.data(), chunk.size());
        if (read < 0) {
            return fail(in.errorString());
        }
        if (read == 0) {
            break;
        }
        if (out.write(chunk.constData(), read) != read) {
            return fail(out.errorString());
        }
        copied += read;
    }

    if (!out.flush() || out.error() != QFileDevice::NoError) {
        return fail(out.errorString());
    }
    out.close();
    if (copied != in.size() || QFileInfo(target).size() != copied) {
        return fail(QStringLiteral("the copy is incomplete"));
    }
    return true;
}

} // namespace

struct ModManager::StagedPak {
    bool ok = false;
    bool cancelled = false;
    QString error;
    ModManifest manifest;
    bool hasManifest = false;
};

struct ModManager::PendingAdd {
    ModInfo mod;
    QString sourcePath;
    QString destPath;
    QString stagedPath;
};

struct ModManager::PendingReplace {
    QString modId;
    QString sourcePath;
    QString stagedPath;
    QString newVersion;
    QString newFileId;
    QDateTime uploadDate;
};

ModManager::StagedPak ModManager::stagePak(const QString &source, const QString &stagedPath,
                                           const CancelToken *cancel) {
    StagedPak result;
    if (!copyFileChunked(source, stagedPath, cancel, &result.error, &result.cancelled)) {
        return result;
    }

    QString manifestError;
    result.hasManifest = ModManifestReader::readFromPak(source, &result.manifest, &manifestError);
    if (!result.hasManifest && !manifestError.isEmpty()) {
        qDebug() << "Manifest not loaded for" << source << ":" << manifestError;
    }

    result.ok = true;
    return result;
}

bool ModManager::prepareAdd(const QString &pakFilePath, const AddModParams &params, PendingAdd *pending) {
    QFileInfo fileInfo(pakFilePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        emit errorOccurred(tr("Mod file does not exist: %1").arg(pakFilePath));
        return false;
    }

    QString pakError;
    if (!PakFileReader::hasPakFooter(pakFilePath, &pakError)) {
        emit errorOccurred(tr("%1 is not a valid .pak file: %2").arg(fileInfo.fileName(), pakError));
        return false;
    }

    ModInfo &mod = pending->mod;
    mod.id = ModInfo::generateId();

    if (!params.name.isEmpty()) {
        mod.fileName = params.name + fileInfo.suffix().prepend('.');
        mod.name = cleanModName(params.name);
    } else {
        mod.fileName = fileInfo.fileName();
        mod.name = cleanModName(fileInfo.fileName());
    }

    mod.installDate = QDateTime::currentDateTime();
    mod.uploadDate = params.uploadDate;
    mod.enabled = false;
    mod.nexusModId = params.nexusModId;
    mod.nexusFileId = params.nexusFileId;
    mod.nexusUrl = params.nexusUrl;
    mod.itchGameId = params.itchGameId;
    mod.itchUrl = params.itchUrl;
    mod.itchUploadId = params.itchUploadId;
    mod.author = params.author;
    mod.description = params.description;
    mod.version = params.version;

    pending->sourcePath = pakFilePath;
    pending->destPath = m_modsStoragePath + "/" + mod.fileName;
    pending->stagedPath = m_modsStoragePath + "/" + mod.id + ".part";

    if (QFile::exists(pending->destPath)) {
        emit errorOccurred(tr("Mod already installed: \"%1\" is already in your mod library.").arg(mod.name));
        return false;
    }
    return true;
}

bool ModManager::finishAdd(PendingAdd &pending, const StagedPak &staged) {
    ModInfo &mod = pending.mod;

    if (!staged.ok) {
        QFile::remove(pending.stagedPath);
        if (!staged.cancelled) {
            emit errorOccurred(tr("Failed to copy mod file to storage: %1").arg(staged.error));
        }
        return false;
    }

    if (staged.hasManifest) {
        const ModManifest &manifest = staged.manifest;
        mod.manifestId = manifest.id;
        mod.manifestAuthors = manifest.authors;
        mod.manifestTags = manifest.tags;
        mod.noticeText = manifest.noticeText;
        mod.noticeIcon = manifest.noticeIcon;
        mod.homepageUrl = manifest.homepageUrl;
        mod.manifestDependencies.clear();
        for (const auto &dep : manifest.dependencies) {
            ModInfo::Dependency modDep;
            modDep.id = dep.id;
            modDep.minVersion = dep.minVersion;
            modDep.maxVersion = dep.maxVersion;
            modDep.required = dep.required;
            mod.manifestDependencies.append(modDep);
        }

        if (!manifest.name.isEmpty()) {
            mod.name = manifest.name;
        }
        if (mod.author.isEmpty() && !manifest.authors.isEmpty()) {
            mod.author = manifest.authors.join(", ");
        }
        if (mod.description.isEmpty() && !manifest.description.isEmpty()) {
            mod.description = manifest.description;
        }
        if (mod.version.isEmpty() && !manifest.version.isEmpty()) {
            mod.version = manifest.version;
        }
        if (mod.nexusUrl.isEmpty() && !manifest.nexusUrl.isEmpty()) {
            mod.nexusUrl = manifest.nexusUrl;
        }
        if (mod.itchUrl.isEmpty() && !manifest.itchUrl.isEmpty()) {
            mod.itchUrl = manifest.itchUrl;
        }
    }

    // Another add may have claimed the name while this copy was running.
    if (QFile::exists(pending.destPath)) {
        QFile::remove(pending.stagedPath);
        emit errorOccurred(tr("Mod already installed: \"%1\" is already in your mod library.").arg(mod.name));
        return false;
    }
    if (!QFile::rename(pending.stagedPath, pending.destPath)) {
        QFile::remove(pending.stagedPath);
        emit errorOccurred(tr("Failed to move mod file into storage."));
        return false;
    }

    {
        QMutexLocker locker(&m_modsMutex);
        mod.priority = static_cast<int>(m_mods.size());
        m_mods.append(mod);
    }
    saveMods();
    emit modAdded(mod.id);
    emit modsChanged();

    qDebug() << "Added mod:" << mod.name << "ID:" << mod.id;
    return true;
}

bool ModManager::addMod(const QString &pakFilePath, const AddModParams &params) {
    PendingAdd pending;
    if (!prepareAdd(pakFilePath, params, &pending)) {
        return false;
    }
    return finishAdd(pending, stagePak(pending.sourcePath, pending.stagedPath, nullptr));
}

CancelTokenPtr ModManager::addModAsync(const QString &pakFilePath, const AddModParams &params,
                                       QObject *context, std::function<void(bool)> onFinished) {
    auto pending = std::make_shared<PendingAdd>();
    if (!prepareAdd(pakFilePath, params, pending.get())) {
        onFinished(false);
        return nullptr;
    }

    QPointer<ModManager> self(this);
    return runCancellable<StagedPak>(
        context,
        [pending](const CancelToken &cancel) {
            return stagePak(pending->sourcePath, pending->stagedPath, &cancel);
        },
        [self, pending, onFinished = std::move(onFinished)](const StagedPak &staged) {
            if (!self) {
                QFile::remove(pending->stagedPath);
                return;
            }
            onFinished(self->finishAdd(*pending, staged));
        },
        [pending](const StagedPak &) { QFile::remove(pending->stagedPath); });
}

bool ModManager::removeMod(const QString &modId) {
    bool wasEnabled;
    QString filePath;

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            qWarning() << "Mod not found:" << modId;
            emit errorOccurred(tr("Mod not found."));
            return false;
        }

        wasEnabled = it->enabled;
        filePath = getModFilePath(modId);
    }

    if (wasEnabled) {
        disableMod(modId);
    }

    if (QFile::exists(filePath)) {
        QFile::remove(filePath);
    }

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });
        if (it != m_mods.end()) {
            m_mods.erase(it);
        }
    }

    saveMods();
    emit modRemoved(modId);
    emit modsChanged();

    qDebug() << "Removed mod:" << modId;
    return true;
}

bool ModManager::prepareReplace(const QString &modId, const QString &newPakPath, const QString &newVersion,
                                const QString &newFileId, const QDateTime &uploadDate, PendingReplace *pending) {
    QString savedName;
    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            qWarning() << "Mod not found:" << modId;
            emit errorOccurred(tr("Mod not found."));
            return false;
        }
        savedName = it->name;
    }

    const QFileInfo newFileInfo(newPakPath);
    if (!newFileInfo.exists() || !newFileInfo.isFile()) {
        emit errorOccurred(tr("New mod file does not exist: %1").arg(newPakPath));
        return false;
    }

    QString pakError;
    if (!PakFileReader::hasPakFooter(newPakPath, &pakError)) {
        emit errorOccurred(tr("The update for \"%1\" is not a valid .pak file: %2").arg(savedName, pakError));
        return false;
    }

    pending->modId = modId;
    pending->sourcePath = newPakPath;
    pending->stagedPath = m_modsStoragePath + "/" + modId + "." + QUuid::createUuid().toString(QUuid::Id128)
                          + ".update.part";
    pending->newVersion = newVersion;
    pending->newFileId = newFileId;
    pending->uploadDate = uploadDate;
    return true;
}

bool ModManager::finishReplace(const PendingReplace &pending, const StagedPak &staged) {
    if (!staged.ok) {
        QFile::remove(pending.stagedPath);
        if (!staged.cancelled) {
            emit errorOccurred(tr("Failed to copy new mod file to storage: %1").arg(staged.error));
        }
        return false;
    }

    // The mod may have changed while the copy ran, so its state is read now, not earlier.
    const QString &modId = pending.modId;
    bool wasEnabled;
    int savedPriority;
    QString savedName;
    QString fileName;
    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            QFile::remove(pending.stagedPath);
            emit errorOccurred(tr("Mod not found."));
            return false;
        }

        wasEnabled = it->enabled;
        savedPriority = it->priority;
        savedName = it->name;
        fileName = it->fileName;
    }

    const QString destPath = m_modsStoragePath + "/" + fileName;

    if (wasEnabled) {
        if (!disableMod(modId)) {
            QFile::remove(pending.stagedPath);
            emit errorOccurred(tr("Failed to disable mod before replacement"));
            return false;
        }
    }

    const auto restoreEnabled = [&]() {
        if (!wasEnabled) {
            return;
        }
        {
            QMutexLocker locker(&m_modsMutex);
            auto it = std::ranges::find_if(m_mods,
                                   [&modId](const ModInfo &mod) { return mod.id == modId; });
            if (it != m_mods.end()) {
                it->priority = savedPriority;
            }
        }
        enableMod(modId);
    };

    if (QFile::exists(destPath) && !QFile::remove(destPath)) {
        QFile::remove(pending.stagedPath);
        emit errorOccurred(tr("Failed to remove old mod file"));
        restoreEnabled();
        return false;
    }

    if (!QFile::rename(pending.stagedPath, destPath)) {
        QFile::remove(pending.stagedPath);
        emit errorOccurred(tr("Failed to move new mod file into storage"));
        return false;
    }

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });
        if (it != m_mods.end()) {
            it->version = pending.newVersion;
            it->nexusFileId = pending.newFileId;
            it->uploadDate = pending.uploadDate;
            it->installDate = QDateTime::currentDateTime();
            if (staged.hasManifest && !staged.manifest.name.isEmpty()) {
                it->name = staged.manifest.name;
            }

            if (wasEnabled) {
                it->priority = savedPriority;
            }
        }
    }

    if (wasEnabled) {
        if (!enableMod(modId)) {
            emit errorOccurred(tr("Failed to re-enable mod after replacement"));
        }
    }

    saveMods();
    emit modsChanged();

    qDebug() << "Replaced mod:" << savedName << "version" << pending.newVersion;
    return true;
}

bool ModManager::replaceMod(const QString &modId, const QString &newPakPath,
                            const QString &newVersion, const QString &newFileId,
                            const QDateTime &uploadDate) {
    PendingReplace pending;
    if (!prepareReplace(modId, newPakPath, newVersion, newFileId, uploadDate, &pending)) {
        return false;
    }
    return finishReplace(pending, stagePak(pending.sourcePath, pending.stagedPath, nullptr));
}

CancelTokenPtr ModManager::replaceStaged(const QString &modId, const QString &newPakPath,
                                         const QString &newVersion, const QString &newFileId,
                                         const QDateTime &uploadDate, QObject *context,
                                         std::function<void(bool)> onFinished, CancelTokenPtr token,
                                         std::shared_ptr<void> keepAlive) {
    auto pending = std::make_shared<PendingReplace>();
    if (!prepareReplace(modId, newPakPath, newVersion, newFileId, uploadDate, pending.get())) {
        onFinished(false);
        return nullptr;
    }

    // keepAlive (e.g. the unpacked archive) is held by both handlers, so it outlives the copy even
    // when the context is destroyed mid-way.
    QPointer<ModManager> self(this);
    return runCancellable<StagedPak>(
        context,
        [pending](const CancelToken &cancel) {
            return stagePak(pending->sourcePath, pending->stagedPath, &cancel);
        },
        [self, pending, keepAlive, onFinished = std::move(onFinished)](const StagedPak &staged) {
            if (!self) {
                QFile::remove(pending->stagedPath);
                return;
            }
            onFinished(self->finishReplace(*pending, staged));
        },
        [pending, keepAlive](const StagedPak &) { QFile::remove(pending->stagedPath); },
        std::move(token));
}

CancelTokenPtr ModManager::replaceModAsync(const QString &modId, const QString &newPakPath,
                                           const QString &newVersion, const QString &newFileId,
                                           const QDateTime &uploadDate, QObject *context,
                                           std::function<void(bool)> onFinished) {
    return replaceStaged(modId, newPakPath, newVersion, newFileId, uploadDate, context,
                         std::move(onFinished), {}, {});
}

namespace {

/// Removes an unpacked archive once every user of it has finished.
struct TempDirGuard {
    explicit TempDirGuard(QString dir) : path(std::move(dir)) {}
    ~TempDirGuard() { ArchiveExtractor::cleanupTempDir(path); }
    QString path;
};

} // namespace

CancelTokenPtr ModManager::replaceModFromFile(const QString &modId, const QString &filePath,
                                              const QString &newVersion, const QString &newFileId,
                                              const QDateTime &uploadDate, QObject *context,
                                              std::function<void(bool)> onFinished) {
    if (!ArchiveExtractor::isArchiveFile(filePath)) {
        return replaceModAsync(modId, filePath, newVersion, newFileId, uploadDate, context, std::move(onFinished));
    }

    QString currentFileName;
    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });
        if (it == m_mods.end()) {
            emit errorOccurred(tr("Mod not found."));
            onFinished(false);
            return nullptr;
        }
        currentFileName = it->fileName;
    }

    // One token covers unpacking and the copy that follows, so a single cancel stops both.
    const auto token = std::make_shared<CancelToken>();
    QPointer<ModManager> self(this);
    ArchiveExtractor::extractPakFilesAsync(filePath, context,
        [self, context = QPointer<QObject>(context), token, modId, currentFileName, newVersion, newFileId,
         uploadDate, onFinished = std::move(onFinished)](const ArchiveExtractor::ExtractResult &extracted) {
        if (!self || !context) {
            ArchiveExtractor::cleanupTempDir(extracted.tempDir);
            return;
        }
        if (extracted.cancelled) {
            onFinished(false);
            return;
        }
        if (!extracted.success) {
            emit self->errorOccurred(tr("Could not unpack the update: %1").arg(extracted.error));
            onFinished(false);
            return;
        }

        const auto unpacked = std::make_shared<TempDirGuard>(extracted.tempDir);

        QString chosen;
        if (extracted.pakFiles.size() == 1) {
            chosen = extracted.pakFiles.first();
        } else {
            for (const QString &pak : extracted.pakFiles) {
                if (QFileInfo(pak).fileName().compare(currentFileName, Qt::CaseInsensitive) == 0) {
                    chosen = pak;
                    break;
                }
            }
        }

        if (chosen.isEmpty()) {
            QStringList names;
            for (const QString &pak : extracted.pakFiles) {
                names.append(QFileInfo(pak).fileName());
            }
            emit self->errorOccurred(tr("The update contains several .pak files (%1) and none matches \"%2\". "
                                        "Remove the mod and add it again to choose which one to install.")
                                         .arg(names.join(QStringLiteral(", ")), currentFileName));
            onFinished(false);
            return;
        }

        self->replaceStaged(modId, chosen, newVersion, newFileId, uploadDate, context.data(), onFinished, token,
                            unpacked);
    }, token);
    return token;
}

bool ModManager::enableMod(const QString &modId) {
    ModInfo modCopy;
    bool alreadyEnabled = false;

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            qWarning() << "Mod not found:" << modId;
            emit errorOccurred(tr("Mod not found."));
            return false;
        }

        if (it->enabled) {
            alreadyEnabled = true;
        } else {
            modCopy = *it;
        }
    }

    if (alreadyEnabled) {
        return true;
    }

    if (!copyModToPaks(modCopy)) {
        emit errorOccurred(tr("Failed to enable mod: %1").arg(modCopy.name));
        return false;
    }

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });
        if (it != m_mods.end()) {
            it->enabled = true;
        }
    }

    saveMods();
    emit modEnabled(modId);
    emit modsChanged();

    qDebug() << "Enabled mod:" << modCopy.name;
    return true;
}

bool ModManager::disableMod(const QString &modId) {
    ModInfo modCopy;
    bool alreadyDisabled = false;

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            qWarning() << "Mod not found:" << modId;
            emit errorOccurred(tr("Mod not found."));
            return false;
        }

        if (!it->enabled) {
            alreadyDisabled = true;
        } else {
            modCopy = *it;
        }
    }

    if (alreadyDisabled) {
        return true;
    }

    if (!removeModFromPaks(modCopy)) {
        emit errorOccurred(tr("Failed to disable mod: %1").arg(modCopy.name));
        return false;
    }

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });
        if (it != m_mods.end()) {
            it->enabled = false;
        }
    }

    saveMods();
    emit modDisabled(modId);
    emit modsChanged();

    qDebug() << "Disabled mod:" << modCopy.name;
    return true;
}

bool ModManager::setAllModsEnabled(bool enabled) {
    QList<ModInfo> modsToProcess;
    {
        QMutexLocker locker(&m_modsMutex);
        for (const ModInfo &mod : m_mods) {
            if (mod.enabled != enabled) {
                modsToProcess.append(mod);
            }
        }
    }

    if (modsToProcess.isEmpty()) {
        return true;
    }

    bool anyFailed = false;
    for (const ModInfo &mod : modsToProcess) {
        bool ok = enabled ? copyModToPaks(mod) : removeModFromPaks(mod);
        if (!ok) {
            emit errorOccurred(tr("Failed to %1 mod: %2")
                .arg(enabled ? tr("enable") : tr("disable"), mod.name));
            anyFailed = true;
            continue;
        }

        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&mod](const ModInfo &item) { return item.id == mod.id; });
        if (it != m_mods.end()) {
            it->enabled = enabled;
        }
    }

    if (enabled) {
        renumberEnabledMods();
    }

    saveMods();
    emit modsChanged();

    return !anyFailed;
}

bool ModManager::setModsEnabled(const QStringList &modIds, bool enabled) {
    if (modIds.isEmpty()) {
        return true;
    }

    QSet<QString> idSet(modIds.begin(), modIds.end());
    QList<ModInfo> modsToProcess;
    {
        QMutexLocker locker(&m_modsMutex);
        for (const ModInfo &mod : m_mods) {
            if (idSet.contains(mod.id) && mod.enabled != enabled) {
                modsToProcess.append(mod);
            }
        }
    }

    if (modsToProcess.isEmpty()) {
        return true;
    }

    bool anyFailed = false;
    for (const ModInfo &mod : modsToProcess) {
        bool ok = enabled ? copyModToPaks(mod) : removeModFromPaks(mod);
        if (!ok) {
            emit errorOccurred(tr("Failed to %1 mod: %2")
                .arg(enabled ? tr("enable") : tr("disable"), mod.name));
            anyFailed = true;
            continue;
        }

        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&mod](const ModInfo &item) { return item.id == mod.id; });
        if (it != m_mods.end()) {
            it->enabled = enabled;
        }
    }

    if (enabled) {
        renumberEnabledMods();
    }

    saveMods();
    emit modsChanged();

    return !anyFailed;
}

bool ModManager::setModPriority(const QString &modId, int priority) {
    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&modId](const ModInfo &mod) { return mod.id == modId; });

        if (it == m_mods.end()) {
            return false;
        }

        it->priority = priority;
        sortModsByPriority();
        renumberEnabledMods();
    }

    saveMods();
    emit modsChanged();

    return true;
}

bool ModManager::batchSetModPriorities(const QMap<QString, int> &priorityMap) {
    if (priorityMap.isEmpty()) {
        return false;
    }

    bool wasBlocked = blockSignals(true);

    bool anyChanged = false;
    {
        QMutexLocker locker(&m_modsMutex);
        for (auto it = priorityMap.constBegin(); it != priorityMap.constEnd(); ++it) {
            const QString &modId = it.key();
            int newPriority = it.value();

            auto modIt = std::ranges::find_if(m_mods,
                                       [&modId](const ModInfo &mod) { return mod.id == modId; });

            if (modIt != m_mods.end() && modIt->priority != newPriority) {
                modIt->priority = newPriority;
                anyChanged = true;
            }
        }

        if (anyChanged) {
            sortModsByPriority();
            renumberEnabledMods();
        }
    }

    if (anyChanged) {
        saveMods();
    }

    blockSignals(wasBlocked);

    if (anyChanged) {
        emit modsChanged();
    }

    return anyChanged;
}

bool ModManager::updateModMetadata(const ModInfo &updatedMod) {
    bool fileNameChanged = false;
    QString oldPath;
    QString newPath;

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&updatedMod](const ModInfo &mod) { return mod.id == updatedMod.id; });

        if (it == m_mods.end()) {
            qWarning() << "Mod not found:" << updatedMod.id;
            emit errorOccurred(tr("Mod not found."));
            return false;
        }

        QString oldFileName = it->fileName;
        fileNameChanged = (oldFileName != updatedMod.fileName);

        it->name = updatedMod.name;
        it->description = updatedMod.description;
        it->nexusModId = updatedMod.nexusModId;
        it->nexusFileId = updatedMod.nexusFileId;
        it->nexusUrl = updatedMod.nexusUrl;
        it->itchGameId = updatedMod.itchGameId;
        it->itchUrl = updatedMod.itchUrl;
        it->version = updatedMod.version;
        it->author = updatedMod.author;
        it->installDate = updatedMod.installDate;
        it->uploadDate = updatedMod.uploadDate;
        it->fileName = updatedMod.fileName;
        it->ignoredItchUploadIds = updatedMod.ignoredItchUploadIds;

        if (fileNameChanged) {
            oldPath = m_modsStoragePath + "/" + oldFileName;
            newPath = m_modsStoragePath + "/" + updatedMod.fileName;
        }
    }

    if (fileNameChanged) {
        if (QFile::exists(oldPath) && oldPath != newPath) {
            QFile::rename(oldPath, newPath);
        }

        QMutexLocker locker(&m_modsMutex);
        auto it2 = std::ranges::find_if(m_mods,
                               [&updatedMod](const ModInfo &mod) { return mod.id == updatedMod.id; });
        if (it2 != m_mods.end() && it2->enabled) {
            it2->numberedFileName = generateNumberedFileName(it2->priority, updatedMod.fileName);
            renumberEnabledMods();
        }
    }

    saveMods();
    emit modsChanged();

    qDebug() << "Updated mod metadata:" << updatedMod.name;
    return true;
}

QList<ModInfo> ModManager::getMods() const {
    QMutexLocker locker(&m_modsMutex);
    return m_mods;
}

ModInfo ModManager::getMod(const QString &modId) const {
    QMutexLocker locker(&m_modsMutex);
    auto it = std::ranges::find_if(m_mods,
                           [&modId](const ModInfo &mod) { return mod.id == modId; });
    return it != m_mods.end() ? *it : ModInfo();
}

bool ModManager::loadMods() {
    qDebug() << "=== loadMods START";

    QString metadataPath = getMetadataFilePath();
    QFile file(metadataPath);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No mods metadata file found, starting fresh";
        qDebug() << "=== loadMods END, loaded: 0 mods";
        return true;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        emit errorOccurred(tr("Invalid mods metadata format"));
        return false;
    }

    QMutexLocker locker(&m_modsMutex);
    m_mods.clear();
    QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        ModInfo mod = ModInfo::fromJson(value.toObject());
        m_mods.append(mod);
    }

    sortModsByPriority();
    qDebug() << "=== loadMods END, loaded:" << m_mods.size() << "mods";
    return true;
}

bool ModManager::saveMods() {
    QJsonArray array;
    {
        QMutexLocker locker(&m_modsMutex);
        for (const ModInfo &mod : m_mods) {
            array.append(mod.toJson());
        }
    }

    QJsonDocument doc(array);
    QString metadataPath = getMetadataFilePath();
    QFile file(metadataPath);

    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred(tr("Failed to save mods metadata"));
        return false;
    }

    file.write(doc.toJson());
    file.close();

    qDebug() << "Saved" << m_mods.size() << "mods";
    return true;
}

QString ModManager::getModFilePath(const QString &modId) const {
    QMutexLocker locker(&m_modsMutex);
    auto it = std::ranges::find_if(m_mods,
                           [&modId](const ModInfo &mod) { return mod.id == modId; });

    if (it != m_mods.end()) {
        return m_modsStoragePath + "/" + it->fileName;
    }
    return QString();
}

QString ModManager::getMetadataFilePath() const {
    return m_modsStoragePath + "/mods.json";
}

bool ModManager::copyModToPaks(const ModInfo &mod) {
    QString paksPath = getPaksPath();
    if (paksPath.isEmpty()) {
        return false;
    }

    QDir paksDir(paksPath);
    if (!paksDir.exists()) {
        emit errorOccurred(tr("Paks directory not found: %1").arg(paksPath));
        return false;
    }

    QString sourcePath = m_modsStoragePath + "/" + mod.fileName;
    if (!QFile::exists(sourcePath)) {
        emit errorOccurred(tr("Mod file not found in storage: %1").arg(sourcePath));
        return false;
    }

    if (!mod.numberedFileName.isEmpty()) {
        QString oldPath = paksPath + "/" + mod.numberedFileName;
        if (QFile::exists(oldPath)) {
            QFile::remove(oldPath);
        }
    }

    QString numberedName = generateNumberedFileName(mod.priority, mod.fileName);
    QString destPath = paksPath + "/" + numberedName;

    if (mod.fileName != numberedName) {
        QString originalPath = paksPath + "/" + mod.fileName;
        if (QFile::exists(originalPath)) {
            QFile::remove(originalPath);
        }
    }

    if (!QFile::copy(sourcePath, destPath)) {
        return false;
    }

#ifdef Q_OS_LINUX
    QFile::setPermissions(destPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner |
        QFileDevice::ReadGroup | QFileDevice::ReadOther);
#endif

    {
        QMutexLocker locker(&m_modsMutex);
        auto it = std::ranges::find_if(m_mods,
                               [&mod](const ModInfo &m) { return m.id == mod.id; });
        if (it != m_mods.end()) {
            it->numberedFileName = numberedName;
        }
    }

    qDebug() << "Copied mod to paks:" << numberedName;
    return true;
}

bool ModManager::removeModFromPaks(const ModInfo &mod) {
    QString paksPath = getPaksPath();
    if (paksPath.isEmpty()) {
        return false;
    }

    if (!mod.numberedFileName.isEmpty()) {
        QString filePath = paksPath + "/" + mod.numberedFileName;
        if (QFile::exists(filePath)) {
            bool removed = QFile::remove(filePath);
            if (removed) {
                qDebug() << "Removed mod from paks:" << mod.numberedFileName;
            }
            return removed;
        }
    }

    QString filePath = paksPath + "/" + mod.fileName;
    if (QFile::exists(filePath)) {
        bool removed = QFile::remove(filePath);
        if (removed) {
            qDebug() << "Removed mod from paks:" << mod.fileName;
        }
        return removed;
    }

    return true;
}

void ModManager::sortModsByPriority() {
    QMutexLocker locker(&m_modsMutex);
    std::sort(m_mods.begin(), m_mods.end(),
              [](const ModInfo &a, const ModInfo &b) {
                  return a.priority < b.priority;
              });
}

QString ModManager::generateNumberedFileName(int priority, const QString &originalFileName) const {
    QString number = QString("%1").arg(priority, PriorityPadWidth, 10, QChar('0'));

    if (originalFileName.startsWith("War-WindowsNoEditor", Qt::CaseInsensitive)) {
        QString remaining = originalFileName.mid(19);
        if (remaining.startsWith("_") || remaining.startsWith("-")) {
            return "War-WindowsNoEditor_" + number + remaining;
        } else {
            return "War-WindowsNoEditor_" + number + "_" + remaining;
        }
    } else {
        return number + "_" + originalFileName;
    }
}

void ModManager::updateNumberedFileNames() {
    QMutexLocker locker(&m_modsMutex);
    for (int i = 0; i < static_cast<int>(m_mods.size()); ++i) {
        m_mods[i].numberedFileName = generateNumberedFileName(i, m_mods[i].fileName);
    }
}

void ModManager::renumberEnabledMods() {
    QString paksPath = getPaksPath();
    if (paksPath.isEmpty()) {
        return;
    }

    struct EnabledModSnapshot {
        QString id;
        QString fileName;
        QString numberedFileName;
        int priority;
    };

    QList<EnabledModSnapshot> enabledMods;
    {
        QMutexLocker locker(&m_modsMutex);
        enabledMods.reserve(m_mods.size());
        for (const ModInfo &mod : m_mods) {
            if (!mod.enabled) {
                continue;
            }
            enabledMods.append({mod.id, mod.fileName, mod.numberedFileName, mod.priority});
        }
    }

    for (const auto &mod : enabledMods) {
        QString newNumberedName = generateNumberedFileName(mod.priority, mod.fileName);
        QString expectedPath = paksPath + "/" + newNumberedName;
        QString sourcePath = m_modsStoragePath + "/" + mod.fileName;

        if (mod.numberedFileName == newNumberedName) {
            if (!QFile::exists(expectedPath) && QFile::exists(sourcePath)) {
                QFile::copy(sourcePath, expectedPath);
#ifdef Q_OS_LINUX
                QFile::setPermissions(expectedPath,
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                    QFileDevice::ReadGroup | QFileDevice::ReadOther);
#endif
                QMutexLocker locker(&m_modsMutex);
                auto it = std::ranges::find_if(m_mods,
                                       [&mod](const ModInfo &item) { return item.id == mod.id; });
                if (it != m_mods.end()) {
                    it->numberedFileName = newNumberedName;
                }
                qDebug() << "Restored mod to paks:" << newNumberedName;
            }
            continue;
        }

        if (!mod.fileName.isEmpty() && mod.fileName != newNumberedName) {
            QString originalPath = paksPath + "/" + mod.fileName;
            if (QFile::exists(originalPath)) {
                QFile::remove(originalPath);
            }
        }

        if (!mod.numberedFileName.isEmpty()) {
            QString oldPath = paksPath + "/" + mod.numberedFileName;
            if (QFile::exists(oldPath)) {
                QFile::remove(oldPath);
            }
        }

        if (QFile::exists(sourcePath)) {
            QFile::copy(sourcePath, expectedPath);
#ifdef Q_OS_LINUX
            QFile::setPermissions(expectedPath,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                QFileDevice::ReadGroup | QFileDevice::ReadOther);
#endif
            QMutexLocker locker(&m_modsMutex);
            auto it = std::ranges::find_if(m_mods,
                                   [&mod](const ModInfo &item) { return item.id == mod.id; });
            if (it != m_mods.end()) {
                it->numberedFileName = newNumberedName;
            }
            qDebug() << "Renumbered mod:" << mod.numberedFileName << "->" << newNumberedName;
        }
    }
}

void ModManager::detectUnregisteredMods() {
    qDebug() << "=== detectUnregisteredMods START, current size:" << m_mods.size();

    QString paksPath = getPaksPath();
    if (paksPath.isEmpty()) {
        qDebug() << "Cannot detect unregistered mods: paks path not set";
        return;
    }

    QDir paksDir(paksPath);
    if (!paksDir.exists()) {
        qDebug() << "Paks directory does not exist:" << paksPath;
        return;
    }

    QStringList pakFiles = paksDir.entryList(QStringList() << "*.pak", QDir::Files);

    for (const QString &pakFile : pakFiles) {
        if (isBaseGamePak(pakFile)) {
            qDebug() << "Skipping base game pak:" << pakFile;
            continue;
        }

        bool isRegistered = false;
        QString originalFileName = pakFile;

        QRegularExpression warPrefixRegex(R"(^War-WindowsNoEditor_\d{3}_(.+)$)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch warMatch = warPrefixRegex.match(pakFile);
        if (warMatch.hasMatch()) {
            originalFileName = "War-WindowsNoEditor_" + warMatch.captured(1);
        } else {
            QRegularExpression numPrefixRegex(R"(^\d{3}_(.+)$)");
            QRegularExpressionMatch numMatch = numPrefixRegex.match(pakFile);
            if (numMatch.hasMatch()) {
                originalFileName = numMatch.captured(1);
            }
        }

        // Check if already registered (case-insensitive)
        {
            QMutexLocker locker(&m_modsMutex);
            for (const ModInfo &mod : m_mods) {
                if (mod.fileName.compare(originalFileName, Qt::CaseInsensitive) == 0 ||
                    mod.numberedFileName.compare(pakFile, Qt::CaseInsensitive) == 0) {
                    isRegistered = true;
                    break;
                }
            }
        }

        if (!isRegistered) {
            QString pakFilePath = paksPath + "/" + pakFile;

            ModInfo newMod;
            newMod.id = ModInfo::generateId();
            newMod.fileName = originalFileName;
            newMod.numberedFileName = pakFile;
            newMod.name = cleanModName(originalFileName);
            newMod.installDate = QFileInfo(pakFilePath).lastModified();
            newMod.enabled = true;

            ModManifest manifest;
            QString manifestError;
            const bool hasManifest = ModManifestReader::readFromPak(pakFilePath, &manifest, &manifestError);
            if (!hasManifest && !manifestError.isEmpty()) {
                qDebug() << "Manifest not loaded for" << pakFilePath << ":" << manifestError;
            }
            if (hasManifest) {
                newMod.manifestId = manifest.id;
                newMod.manifestAuthors = manifest.authors;
                newMod.manifestTags = manifest.tags;
                newMod.noticeText = manifest.noticeText;
                newMod.noticeIcon = manifest.noticeIcon;
                newMod.homepageUrl = manifest.homepageUrl;
                newMod.manifestDependencies.clear();
                for (const auto &dep : manifest.dependencies) {
                    ModInfo::Dependency modDep;
                    modDep.id = dep.id;
                    modDep.minVersion = dep.minVersion;
                    modDep.maxVersion = dep.maxVersion;
                    modDep.required = dep.required;
                    newMod.manifestDependencies.append(modDep);
                }

                if (!manifest.name.isEmpty()) {
                    newMod.name = manifest.name;
                }
                if (!manifest.authors.isEmpty()) {
                    newMod.author = manifest.authors.join(", ");
                }
                if (!manifest.description.isEmpty()) {
                    newMod.description = manifest.description;
                }
                if (!manifest.version.isEmpty()) {
                    newMod.version = manifest.version;
                }
                if (!manifest.nexusUrl.isEmpty()) {
                    newMod.nexusUrl = manifest.nexusUrl;
                }
                if (!manifest.itchUrl.isEmpty()) {
                    newMod.itchUrl = manifest.itchUrl;
                }
            }

            QString destPath = m_modsStoragePath + "/" + originalFileName;

            // Double-check registration status (mod may have been loaded between initial check and now)
            bool stillUnregistered = true;
            {
                QMutexLocker locker(&m_modsMutex);
                for (const ModInfo &existingMod : m_mods) {
                    if (existingMod.fileName.compare(originalFileName, Qt::CaseInsensitive) == 0 ||
                        existingMod.numberedFileName.compare(pakFile, Qt::CaseInsensitive) == 0) {
                        stillUnregistered = false;
                        break;
                    }
                }

                if (!stillUnregistered) {
                    qDebug() << "Mod already registered (caught in double-check):" << originalFileName;
                    continue;
                }

                newMod.priority = m_mods.size();
            }

            // Copy file if needed (outside lock for I/O)
            bool needsCopy = !QFile::exists(destPath);
            if (needsCopy) {
                if (!QFile::copy(pakFilePath, destPath)) {
                    qDebug() << "Failed to copy mod file:" << originalFileName;
                    continue;
                }
                qDebug() << "Detected and registered unregistered mod:" << originalFileName;
            } else {
                qDebug() << "Registered existing mod from paks:" << originalFileName;
            }

            // Add to list with lock
            {
                QMutexLocker locker(&m_modsMutex);
                m_mods.append(newMod);
            }
        }
    }

    if (!pakFiles.isEmpty()) {
        // Deduplication safety net: remove any duplicates that might have slipped through
        {
            QMutexLocker locker(&m_modsMutex);

            QMap<QString, int> seenMods;  // fileName.toLower() -> first index
            QList<int> indicesToRemove;

            for (int i = 0; i < static_cast<int>(m_mods.size()); ++i) {
                QString lowerFileName = m_mods[i].fileName.toLower();
                if (seenMods.contains(lowerFileName)) {
                    int originalIndex = seenMods[lowerFileName];
                    qWarning() << "Duplicate mod detected:" << m_mods[i].fileName
                              << "(keeping first at index" << originalIndex << ", removing duplicate at" << i << ")";
                    indicesToRemove.append(i);
                } else {
                    seenMods[lowerFileName] = i;
                }
            }

            // Remove duplicates in reverse order to maintain indices
            std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<int>());
            for (int idx : indicesToRemove) {
                m_mods.removeAt(idx);
            }

            if (!indicesToRemove.isEmpty()) {
                qWarning() << "Removed" << indicesToRemove.size() << "duplicate mod(s)";
            }
        }

        sortModsByPriority();
        saveMods();
        emit modsChanged();
    }

    qDebug() << "=== detectUnregisteredMods END, final size:" << m_mods.size();
}

void ModManager::syncEnabledModsWithPaks() {
    // This ensures all enabled mods are properly numbered in the paks folder
    renumberEnabledMods();
    saveMods();
}

QList<ModManager::VerificationIssue> ModManager::verifyMods() const {
    QList<VerificationIssue> issues;
    QMutexLocker locker(&m_modsMutex);
    for (const ModInfo &mod : m_mods) {
        const QString path = m_modsStoragePath + "/" + mod.fileName;
        if (!QFileInfo::exists(path)) {
            issues.append({mod.id, mod.name,
                tr("Pak file missing from storage: %1").arg(mod.fileName)});
        }
    }
    return issues;
}

QString ModManager::cleanModName(const QString &fileName) const {
    QString baseName = QFileInfo(fileName).completeBaseName();

    baseName.replace(QStringLiteral("War-WindowsNoEditor"), QString(), Qt::CaseInsensitive);
    baseName.replace(QStringLiteral("WindowsNoEditor"), QString(), Qt::CaseInsensitive);

    // Split CamelCase: "MyAwesomeMod" → "My Awesome Mod"
    static const QRegularExpression camelCase(QStringLiteral("([a-z])([A-Z])"));
    baseName.replace(camelCase, QStringLiteral("\\1 \\2"));

    baseName.replace('_', ' ');
    baseName.replace('-', ' ');
    baseName = baseName.simplified();

    if (!baseName.isEmpty()) {
        baseName[0] = baseName[0].toUpper();
    }

    return baseName.isEmpty() ? fileName : baseName;
}

bool ModManager::isBaseGamePak(const QString &fileName) const {
    QString lowerFileName = fileName.toLower();

    QRegularExpression pakChunkRegex(R"(^pakchunk\d+.*\.pak$)", QRegularExpression::CaseInsensitiveOption);
    if (pakChunkRegex.match(lowerFileName).hasMatch()) {
        return true;
    }

    if (lowerFileName == "war-windowsnoeditor.pak" || lowerFileName == "war.pak") {
        return true;
    }

    return false;
}
