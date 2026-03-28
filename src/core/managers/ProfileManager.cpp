#include "ProfileManager.h"
#include "ModManager.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QSet>
#include <QCryptographicHash>
#include <QByteArrayView>
#include <archive.h>
#include <archive_entry.h>

namespace {
constexpr const char* kProfileArchiveExtension = ".tkprofile";
constexpr const char* kProfileJsonName = "profile.json";
constexpr const char* kModsJsonName = "mods.json";
constexpr const char* kModsDirName = "mods";

bool hasArchiveExtension(const QString &filePath) {
    return filePath.endsWith(QLatin1String(kProfileArchiveExtension), Qt::CaseInsensitive);
}

QString ensureArchiveExtension(const QString &filePath) {
    if (hasArchiveExtension(filePath)) {
        return filePath;
    }
    return filePath + QLatin1String(kProfileArchiveExtension);
}

QByteArray computeFileHash(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        const qint64 readBytes = file.read(buffer.data(), buffer.size());
        if (readBytes > 0) {
            hash.addData(QByteArrayView(buffer.constData(), readBytes));
        }
    }

    return hash.result();
}

bool writeArchiveEntry(archive *a,
                       const QString &pathInArchive,
                       const QByteArray &data,
                       QString *error) {
    archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, pathInArchive.toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, data.size());

    if (archive_write_header(a, entry) != ARCHIVE_OK) {
        if (error) {
            *error = QString("Failed to write archive header: %1")
                .arg(archive_error_string(a));
        }
        archive_entry_free(entry);
        return false;
    }

    if (!data.isEmpty()) {
        if (archive_write_data(a, data.constData(), data.size()) < 0) {
            if (error) {
                *error = QString("Failed to write archive data: %1")
                    .arg(archive_error_string(a));
            }
            archive_entry_free(entry);
            return false;
        }
    }

    archive_entry_free(entry);
    return true;
}

bool writeArchiveFile(archive *a,
                      const QString &pathInArchive,
                      const QString &sourcePath,
                      QString *error) {
    QFileInfo fileInfo(sourcePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (error) {
            *error = "Missing mod file: " + sourcePath;
        }
        return false;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = "Failed to read mod file: " + sourcePath;
        }
        return false;
    }

    archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, pathInArchive.toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, fileInfo.size());

    if (archive_write_header(a, entry) != ARCHIVE_OK) {
        if (error) {
            *error = QString("Failed to write archive header: %1")
                .arg(archive_error_string(a));
        }
        archive_entry_free(entry);
        return false;
    }

    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (!source.atEnd()) {
        const qint64 readBytes = source.read(buffer.data(), buffer.size());
        if (readBytes <= 0) {
            break;
        }
        if (archive_write_data(a, buffer.constData(), readBytes) < 0) {
            if (error) {
                *error = QString("Failed to write archive data: %1")
                    .arg(archive_error_string(a));
            }
            archive_entry_free(entry);
            return false;
        }
    }

    archive_entry_free(entry);
    return true;
}

bool extractArchiveToDir(const QString &archivePath, const QString &destDir, QString *error) {
    archive *a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if (archive_read_open_filename(a, archivePath.toUtf8().constData(), 10240) != ARCHIVE_OK) {
        if (error) {
            *error = QString("Failed to open archive: %1").arg(archive_error_string(a));
        }
        archive_read_free(a);
        return false;
    }

    QDir root(destDir);
    if (!root.exists() && !root.mkpath(".")) {
        if (error) {
            *error = "Failed to create extraction directory.";
        }
        archive_read_free(a);
        return false;
    }

    archive_entry *entry = nullptr;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *entryName = archive_entry_pathname(entry);
        if (!entryName) {
            continue;
        }

        QString entryPath = QDir::cleanPath(QString::fromUtf8(entryName));
        if (entryPath.isEmpty()
            || entryPath.startsWith("..")
            || entryPath.contains(":")
            || entryPath.startsWith("/")) {
            archive_read_data_skip(a);
            continue;
        }

        const QStringList parts = entryPath.split('/', Qt::SkipEmptyParts);
        bool hasTraversal = false;
        for (const QString &part : parts) {
            if (part == "..") {
                hasTraversal = true;
                break;
            }
        }
        if (hasTraversal) {
            archive_read_data_skip(a);
            continue;
        }

        const QString outputPath = root.filePath(entryPath);
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            root.mkpath(outputPath);
            continue;
        }

        QFileInfo outInfo(outputPath);
        if (!root.mkpath(outInfo.path())) {
            if (error) {
                *error = "Failed to create extraction directory.";
            }
            archive_read_free(a);
            return false;
        }

        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            if (error) {
                *error = "Failed to write extracted file.";
            }
            archive_read_free(a);
            return false;
        }

        const void *buff = nullptr;
        size_t size = 0;
        int64_t offset = 0;
        int readResult = ARCHIVE_OK;

        while ((readResult = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
            const qint64 written = outFile.write(static_cast<const char*>(buff),
                                                 static_cast<qint64>(size));
            if (written != static_cast<qint64>(size)) {
                if (error) {
                    *error = "Failed to write extracted file.";
                }
                outFile.close();
                archive_read_free(a);
                return false;
            }
        }

        outFile.close();

        if (readResult != ARCHIVE_EOF) {
            if (error) {
                *error = QString("Failed to read archive entry: %1")
                    .arg(archive_error_string(a));
            }
            archive_read_free(a);
            return false;
        }
    }

    archive_read_free(a);
    return true;
}
} // namespace

QString ProfileValidationResult::getMessage() const {
    if (isValid && !hasMissingMods()) {
        return "Profile is valid and all mods are available.";
    }

    QString msg;
    if (hasMissingMods()) {
        msg += QString("%1 of %2 mods are missing:\n")
            .arg(missingMods.size())
            .arg(totalModsCount);

        int autoInstallCount = 0;
        for (const MissingModInfo &info : missingMods) {
            if (info.hasNexusId()) {
                autoInstallCount++;
            }
        }

        if (autoInstallCount > 0) {
            msg += QString("\n%1 mod(s) have NexusMods IDs (available for auto-install).")
                .arg(autoInstallCount);
        }
    }

    return msg;
}

ProfileManager::ProfileManager(QObject *parent)
    : QObject(parent)
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_storagePath = appDataPath + "/mods";
    QDir().mkpath(m_storagePath);
}

void ProfileManager::setModManager(ModManager *modManager) {
    m_modManager = modManager;
}

void ProfileManager::setStoragePath(const QString &storagePath) {
    m_storagePath = storagePath;
    QDir().mkpath(m_storagePath);
}

bool ProfileManager::createProfile(const QString &name) {
    if (!m_modManager) {
        emit errorOccurred("ModManager not set");
        return false;
    }

    if (name.trimmed().isEmpty()) {
        emit errorOccurred("Profile name cannot be empty");
        return false;
    }

    for (const ProfileInfo &profile : m_profiles) {
        if (profile.name == name) {
            emit errorOccurred("Profile with this name already exists");
            return false;
        }
    }

    ProfileInfo profile = captureCurrentState();
    profile.id = ProfileInfo::generateId();
    profile.name = name;

    m_profiles.append(profile);
    saveProfiles();
    setActiveProfile(profile.id);

    emit profileCreated(profile.id);
    emit profilesChanged();

    qDebug() << "Created profile:" << name << "with" << profile.modConfigs.size() << "mods";
    return true;
}

bool ProfileManager::updateProfile(const QString &profileId) {
    if (!m_modManager) {
        emit errorOccurred("ModManager not set");
        return false;
    }

    auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                          [&profileId](const ProfileInfo &p) { return p.id == profileId; });

    if (it == m_profiles.end()) {
        emit errorOccurred("Profile not found: " + profileId);
        return false;
    }

    ProfileInfo currentState = captureCurrentState();
    it->modConfigs = currentState.modConfigs;
    it->modifiedDate = QDateTime::currentDateTime();

    saveProfiles();
    emit profilesChanged();

    qDebug() << "Updated profile:" << it->name;
    return true;
}

bool ProfileManager::renameProfile(const QString &profileId, const QString &newName) {
    if (newName.trimmed().isEmpty()) {
        emit errorOccurred("Profile name cannot be empty");
        return false;
    }

    auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                          [&profileId](const ProfileInfo &p) { return p.id == profileId; });

    if (it == m_profiles.end()) {
        emit errorOccurred("Profile not found");
        return false;
    }

    for (const ProfileInfo &profile : m_profiles) {
        if (profile.name == newName && profile.id != profileId) {
            emit errorOccurred("Profile with this name already exists");
            return false;
        }
    }

    it->name = newName;
    saveProfiles();
    emit profilesChanged();

    qDebug() << "Renamed profile to:" << newName;
    return true;
}

bool ProfileManager::deleteProfile(const QString &profileId) {
    auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                          [&profileId](const ProfileInfo &p) { return p.id == profileId; });

    if (it == m_profiles.end()) {
        emit errorOccurred("Profile not found: " + profileId);
        return false;
    }

    QString name = it->name;
    m_profiles.erase(it);

    if (m_activeProfileId == profileId) {
        clearActiveProfile();
    }

    saveProfiles();
    emit profileDeleted(profileId);
    emit profilesChanged();

    qDebug() << "Deleted profile:" << name;
    return true;
}

bool ProfileManager::reorderProfiles(const QList<QString> &orderedProfileIds) {
    if (orderedProfileIds.size() != m_profiles.size()) {
        emit errorOccurred("Profile count mismatch during reordering");
        return false;
    }

    QList<ProfileInfo> reorderedProfiles;
    for (const QString &profileId : orderedProfileIds) {
        auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                              [&profileId](const ProfileInfo &p) { return p.id == profileId; });
        if (it == m_profiles.end()) {
            emit errorOccurred("Profile not found during reordering: " + profileId);
            return false;
        }
        reorderedProfiles.append(*it);
    }

    m_profiles = reorderedProfiles;
    saveProfiles();
    emit profilesChanged();

    return true;
}

ProfileInfo ProfileManager::getProfile(const QString &profileId) const {
    auto it = std::find_if(m_profiles.begin(), m_profiles.end(),
                          [&profileId](const ProfileInfo &p) { return p.id == profileId; });
    return it != m_profiles.end() ? *it : ProfileInfo();
}

QList<ProfileInfo> ProfileManager::getProfiles() const {
    return m_profiles;
}

ProfileValidationResult ProfileManager::validateProfile(const QString &profileId) const {
    ProfileValidationResult result;
    result.isValid = false;

    if (!m_modManager) {
        return result;
    }

    ProfileInfo profile = getProfile(profileId);
    if (profile.id.isEmpty()) {
        return result;
    }

    QList<ModInfo> availableMods = m_modManager->getMods();
    result.totalModsCount = profile.modConfigs.size();
    result.availableModsCount = 0;

    for (const ModConfig &config : profile.modConfigs) {
        bool found = false;
        QString nexusId;

        for (const ModInfo &mod : availableMods) {
            if (mod.id == config.modId) {
                found = true;
                result.availableModsCount++;
                if (!mod.nexusModId.isEmpty()) {
                    nexusId = mod.nexusModId;
                }
                break;
            }
        }

        if (!found) {
            MissingModInfo missingInfo;
            missingInfo.modId = config.modId;
            missingInfo.nexusModId = nexusId;
            result.missingMods.append(missingInfo);
        }
    }

    result.isValid = !result.hasMissingMods();
    return result;
}

bool ProfileManager::applyProfile(const QString &profileId, bool ignoreWarnings) {
    if (!m_modManager) {
        emit errorOccurred("ModManager not set");
        return false;
    }

    ProfileInfo profile = getProfile(profileId);
    if (profile.id.isEmpty()) {
        emit errorOccurred("Profile not found: " + profileId);
        return false;
    }

    if (!ignoreWarnings) {
        ProfileValidationResult validation = validateProfile(profileId);
        if (validation.hasMissingMods()) {
            emit errorOccurred("Cannot apply profile: some mods are missing. "
                             "Use validation to see details.");
            return false;
        }
    }

    bool success = applyProfileInternal(profile);
    if (success) {
        setActiveProfile(profileId);
        emit profileApplied(profileId);
        qDebug() << "Applied profile:" << profile.name;
    }

    return success;
}

bool ProfileManager::exportProfile(const QString &profileId, const QString &filePath) {
    ProfileInfo profile = getProfile(profileId);
    if (profile.id.isEmpty()) {
        emit errorOccurred("Profile not found: " + profileId);
        return false;
    }

    if (!m_modManager) {
        emit errorOccurred("ModManager not set");
        return false;
    }

    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        QJsonObject rootObj;
        rootObj["profile"] = profile.toJson();

        QJsonDocument doc(rootObj);
        QFile file(filePath);

        if (!file.open(QIODevice::WriteOnly)) {
            emit errorOccurred("Failed to open file for writing: " + filePath);
            return false;
        }

        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        qDebug() << "Exported profile to:" << filePath;
        return true;
    }

    ProfileInfo exportProfile = profile;
    QList<ModConfig> enabledConfigs;
    for (const ModConfig &config : profile.modConfigs) {
        if (config.enabled) {
            enabledConfigs.append(config);
        }
    }
    exportProfile.modConfigs = enabledConfigs;

    QJsonObject rootObj;
    rootObj["profile"] = exportProfile.toJson();
    QJsonDocument profileDoc(rootObj);

    QJsonArray modsArray;
    QMap<QString, ModInfo> modsById;
    QList<ModInfo> allMods = m_modManager->getMods();
    for (const ModConfig &config : exportProfile.modConfigs) {
        auto it = std::find_if(allMods.begin(), allMods.end(),
                               [&config](const ModInfo &mod) { return mod.id == config.modId; });
        if (it == allMods.end()) {
            emit errorOccurred("Missing mod for profile export: " + config.modId);
            return false;
        }
        modsById.insert(it->id, *it);
        modsArray.append(it->toJson());
    }

    QJsonObject modsObj;
    modsObj["mods"] = modsArray;
    QJsonDocument modsDoc(modsObj);

    QString error;
    QString archivePath = ensureArchiveExtension(filePath);

    archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, archivePath.toUtf8().constData()) != ARCHIVE_OK) {
        emit errorOccurred(QString("Failed to create archive: %1").arg(archive_error_string(a)));
        archive_write_free(a);
        return false;
    }

    if (!writeArchiveEntry(a, QLatin1String(kProfileJsonName),
                           profileDoc.toJson(QJsonDocument::Indented), &error)
        || !writeArchiveEntry(a, QLatin1String(kModsJsonName),
                              modsDoc.toJson(QJsonDocument::Indented), &error)) {
        emit errorOccurred(error);
        archive_write_close(a);
        archive_write_free(a);
        return false;
    }

    for (auto it = modsById.constBegin(); it != modsById.constEnd(); ++it) {
        const ModInfo &mod = it.value();
        const QString sourcePath = m_modManager->getModsStoragePath() + "/" + mod.fileName;
        const QString archiveName = QString("%1/%2")
            .arg(QLatin1String(kModsDirName))
            .arg(mod.fileName);
        if (!writeArchiveFile(a, archiveName, sourcePath, &error)) {
            emit errorOccurred(error);
            archive_write_close(a);
            archive_write_free(a);
            return false;
        }
    }

    archive_write_close(a);
    archive_write_free(a);

    qDebug() << "Exported profile to:" << archivePath;
    return true;
}

bool ProfileManager::importProfile(const QString &filePath, QString &importedProfileId,
                                   ImportConflictResolver resolver) {
    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open file for reading: " + filePath);
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred("Invalid profile file format");
        return false;
    }

    QJsonObject rootObj = doc.object();
    if (!rootObj.contains("profile")) {
        emit errorOccurred("Invalid profile file: missing 'profile' key");
        return false;
    }

    ProfileInfo profile = ProfileInfo::fromJson(rootObj["profile"].toObject());

    profile.id = ProfileInfo::generateId();

    QString baseName = profile.name;
    int suffix = 1;
    while (true) {
        bool nameExists = false;
        for (const ProfileInfo &existing : m_profiles) {
            if (existing.name == profile.name) {
                nameExists = true;
                break;
            }
        }

        if (!nameExists) break;

        profile.name = baseName + QString(" (%1)").arg(suffix);
        suffix++;
    }

    m_profiles.append(profile);
    importedProfileId = profile.id;

    saveProfiles();
    emit profileCreated(profile.id);
    emit profilesChanged();

    qDebug() << "Imported profile:" << profile.name;
    return true;
    }

    if (!m_modManager) {
        emit errorOccurred("ModManager not set");
        return false;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        emit errorOccurred("Failed to create temporary directory");
        return false;
    }

    QString error;
    if (!extractArchiveToDir(filePath, tempDir.path(), &error)) {
        emit errorOccurred(error);
        return false;
    }

    QFile profileFile(tempDir.path() + "/" + QLatin1String(kProfileJsonName));
    if (!profileFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Profile archive missing profile.json");
        return false;
    }

    QJsonDocument profileDoc = QJsonDocument::fromJson(profileFile.readAll());
    profileFile.close();
    if (!profileDoc.isObject()) {
        emit errorOccurred("Invalid profile archive format");
        return false;
    }

    QJsonObject profileRoot = profileDoc.object();
    if (!profileRoot.contains("profile")) {
        emit errorOccurred("Invalid profile archive: missing profile metadata");
        return false;
    }

    ProfileInfo profile = ProfileInfo::fromJson(profileRoot["profile"].toObject());

    QFile modsFile(tempDir.path() + "/" + QLatin1String(kModsJsonName));
    if (!modsFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Profile archive missing mods.json");
        return false;
    }

    QJsonDocument modsDoc = QJsonDocument::fromJson(modsFile.readAll());
    modsFile.close();
    if (!modsDoc.isObject()) {
        emit errorOccurred("Invalid mods metadata in profile archive");
        return false;
    }

    QJsonArray modsArray = modsDoc.object()["mods"].toArray();
    QMap<QString, ModInfo> modsById;
    for (const QJsonValue &value : modsArray) {
        ModInfo mod = ModInfo::fromJson(value.toObject());
        if (!mod.id.isEmpty()) {
            modsById.insert(mod.id, mod);
        }
    }

    const QString modsDirPath = tempDir.path() + "/" + QLatin1String(kModsDirName);
    QDir modsDir(modsDirPath);
    if (!modsDir.exists()) {
        emit errorOccurred("Profile archive missing mods directory");
        return false;
    }

    QMap<QString, ModInfo> existingByFileName;
    QMap<QByteArray, ModInfo> existingByHash;
    QSet<QString> existingFileNames;
    const QList<ModInfo> existingMods = m_modManager->getMods();
    const QString modsStoragePath = m_modManager->getModsStoragePath();

    for (const ModInfo &mod : existingMods) {
        const QString key = mod.fileName.toLower();
        existingByFileName.insert(key, mod);
        existingFileNames.insert(key);

        const QString existingPath = modsStoragePath + "/" + mod.fileName;
        if (QFile::exists(existingPath)) {
            const QByteArray hash = computeFileHash(existingPath);
            if (!hash.isEmpty()) {
                existingByHash.insert(hash, mod);
            }
        }
    }

    QMap<QString, QString> idMap;
    for (const ModConfig &config : profile.modConfigs) {
        if (!modsById.contains(config.modId)) {
            continue;
        }
        const ModInfo &meta = modsById[config.modId];
        const QString pakPath = modsDir.filePath(meta.fileName);
        if (!QFile::exists(pakPath)) {
            emit errorOccurred("Missing mod file in profile archive: " + meta.fileName);
            return false;
        }

        const QString fileNameKey = meta.fileName.toLower();
        ModInfo existingMod;
        bool checksumMatch = false;

        if (existingByFileName.contains(fileNameKey)) {
            existingMod = existingByFileName.value(fileNameKey);
        } else {
            const QByteArray incomingHash = computeFileHash(pakPath);
            if (!incomingHash.isEmpty() && existingByHash.contains(incomingHash)) {
                existingMod = existingByHash.value(incomingHash);
                checksumMatch = true;
            }
        }

        ImportConflictAction action = ImportConflictAction::Ignore;
        if (!existingMod.id.isEmpty() && resolver) {
            action = resolver(meta, existingMod, checksumMatch);
        }

        if (!existingMod.id.isEmpty()) {
            if (action == ImportConflictAction::Ignore) {
                idMap.insert(config.modId, existingMod.id);
                continue;
            }

            if (action == ImportConflictAction::Overwrite) {
                if (!checksumMatch) {
                    if (!m_modManager->replaceMod(existingMod.id, pakPath,
                                                  meta.version, meta.nexusFileId,
                                                  meta.uploadDate)) {
                        return false;
                    }
                }

                ModInfo updated = existingMod;
                if (!meta.name.isEmpty()) {
                    updated.name = meta.name;
                }
                updated.description = meta.description;
                updated.nexusModId = meta.nexusModId;
                updated.nexusFileId = meta.nexusFileId;
                updated.nexusUrl = meta.nexusUrl;
                updated.itchGameId = meta.itchGameId;
                updated.itchUrl = meta.itchUrl;
                updated.version = meta.version;
                updated.author = meta.author;
                updated.uploadDate = meta.uploadDate;
                m_modManager->updateModMetadata(updated);

                idMap.insert(config.modId, existingMod.id);
                continue;
            }
        }

        if (existingMod.id.isEmpty()) {
        const QList<ModInfo> beforeMods = m_modManager->getMods();
        if (!m_modManager->addMod(pakPath, {
                .nexusModId = meta.nexusModId,
                .nexusFileId = meta.nexusFileId,
                .nexusUrl = meta.nexusUrl,
                .author = meta.author,
                .description = meta.description,
                .version = meta.version,
                .itchGameId = meta.itchGameId,
                .itchUrl = meta.itchUrl,
                .uploadDate = meta.uploadDate
            })) {
            return false;
        }

        QList<ModInfo> afterMods = m_modManager->getMods();
        ModInfo added;
        if (afterMods.size() > beforeMods.size()) {
            QSet<QString> beforeIds;
            for (const ModInfo &mod : beforeMods) {
                beforeIds.insert(mod.id);
            }
            for (const ModInfo &mod : afterMods) {
                if (!beforeIds.contains(mod.id)) {
                    added = mod;
                    break;
                }
            }
        }
        if (added.id.isEmpty()) {
            emit errorOccurred("Failed to map imported mod metadata");
            return false;
        }

        ModInfo updated = added;
        if (!meta.name.isEmpty()) {
            updated.name = meta.name;
        }
        updated.description = meta.description;
        updated.nexusModId = meta.nexusModId;
        updated.nexusFileId = meta.nexusFileId;
        updated.nexusUrl = meta.nexusUrl;
        updated.itchGameId = meta.itchGameId;
        updated.itchUrl = meta.itchUrl;
        updated.version = meta.version;
        updated.author = meta.author;
        updated.uploadDate = meta.uploadDate;
        m_modManager->updateModMetadata(updated);

        existingFileNames.insert(added.fileName.toLower());
        idMap.insert(config.modId, added.id);
        continue;
    }

    QString modName = QFileInfo(meta.fileName).completeBaseName();
        QString extension = QFileInfo(meta.fileName).suffix();
        int suffix = 1;
        QString candidateName;
        do {
            candidateName = QString("%1_dup%2").arg(modName).arg(suffix);
            const QString candidateFile = (candidateName + "." + extension).toLower();
            if (!existingFileNames.contains(candidateFile)) {
                break;
            }
            suffix++;
        } while (true);

        const QList<ModInfo> beforeMods = m_modManager->getMods();
        if (!m_modManager->addMod(pakPath, {
                .name = candidateName,
                .nexusModId = meta.nexusModId,
                .nexusFileId = meta.nexusFileId,
                .nexusUrl = meta.nexusUrl,
                .author = meta.author,
                .description = meta.description,
                .version = meta.version,
                .itchGameId = meta.itchGameId,
                .itchUrl = meta.itchUrl,
                .uploadDate = meta.uploadDate
            })) {
            return false;
        }

        QList<ModInfo> afterMods = m_modManager->getMods();
        ModInfo added;
        if (afterMods.size() > beforeMods.size()) {
            QSet<QString> beforeIds;
            for (const ModInfo &mod : beforeMods) {
                beforeIds.insert(mod.id);
            }
            for (const ModInfo &mod : afterMods) {
                if (!beforeIds.contains(mod.id)) {
                    added = mod;
                    break;
                }
            }
        }
        if (added.id.isEmpty()) {
            emit errorOccurred("Failed to map imported mod metadata");
            return false;
        }

        ModInfo updated = added;
        updated.name = candidateName;
        updated.description = meta.description;
        updated.nexusModId = meta.nexusModId;
        updated.nexusFileId = meta.nexusFileId;
        updated.nexusUrl = meta.nexusUrl;
        updated.itchGameId = meta.itchGameId;
        updated.itchUrl = meta.itchUrl;
        updated.version = meta.version;
        updated.author = meta.author;
        updated.uploadDate = meta.uploadDate;
        m_modManager->updateModMetadata(updated);

        existingFileNames.insert(added.fileName.toLower());
        idMap.insert(config.modId, added.id);
    }

    QList<ModConfig> remappedConfigs;
    for (ModConfig config : profile.modConfigs) {
        if (idMap.contains(config.modId)) {
            config.modId = idMap.value(config.modId);
            remappedConfigs.append(config);
        }
    }
    profile.modConfigs = remappedConfigs;
    profile.id = ProfileInfo::generateId();

    QString baseName = profile.name;
    int suffix = 1;
    while (true) {
        bool nameExists = false;
        for (const ProfileInfo &existing : m_profiles) {
            if (existing.name == profile.name) {
                nameExists = true;
                break;
            }
        }

        if (!nameExists) break;

        profile.name = baseName + QString(" (%1)").arg(suffix);
        suffix++;
    }

    m_profiles.append(profile);
    importedProfileId = profile.id;

    saveProfiles();
    emit profileCreated(profile.id);
    emit profilesChanged();

    qDebug() << "Imported profile:" << profile.name;
    return true;
}

bool ProfileManager::loadProfiles() {
    QString filePath = getStorageFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No profiles file found, starting fresh";
        return true;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred("Invalid profiles file format");
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray profilesArray = rootObj["profiles"].toArray();

    m_profiles.clear();
    for (const QJsonValue &value : profilesArray) {
        ProfileInfo profile = ProfileInfo::fromJson(value.toObject());
        m_profiles.append(profile);
    }

    m_activeProfileId = rootObj["activeProfileId"].toString();

    qDebug() << "Loaded" << m_profiles.size() << "profiles";
    return true;
}

bool ProfileManager::saveProfiles() {
    QJsonArray profilesArray;
    for (const ProfileInfo &profile : m_profiles) {
        profilesArray.append(profile.toJson());
    }

    QJsonObject rootObj;
    rootObj["profiles"] = profilesArray;
    rootObj["activeProfileId"] = m_activeProfileId;

    QJsonDocument doc(rootObj);
    QString filePath = getStorageFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Failed to save profiles");
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Saved" << m_profiles.size() << "profiles";
    return true;
}

void ProfileManager::setActiveProfile(const QString &profileId) {
    if (m_activeProfileId != profileId) {
        m_activeProfileId = profileId;
        saveProfiles();
        emit activeProfileChanged(profileId);
    }
}

void ProfileManager::clearActiveProfile() {
    if (!m_activeProfileId.isEmpty()) {
        m_activeProfileId.clear();
        saveProfiles();
        emit activeProfileChanged(QString());
    }
}

bool ProfileManager::isProfileActive(const QString &profileId) const {
    return m_activeProfileId == profileId;
}

QString ProfileManager::getStorageFilePath() const {
    return m_storagePath + "/profiles.json";
}

ProfileInfo ProfileManager::captureCurrentState() const {
    ProfileInfo profile;

    if (!m_modManager) {
        return profile;
    }

    QList<ModInfo> mods = m_modManager->getMods();
    for (const ModInfo &mod : mods) {
        ModConfig config;
        config.modId = mod.id;
        config.enabled = mod.enabled;
        config.priority = mod.priority;
        profile.modConfigs.append(config);
    }

    return profile;
}

bool ProfileManager::applyProfileInternal(const ProfileInfo &profile) {
    if (!m_modManager) {
        return false;
    }

    QList<ModInfo> allMods = m_modManager->getMods();
    for (const ModInfo &mod : allMods) {
        if (mod.enabled) {
            m_modManager->disableMod(mod.id);
        }
    }

    QMap<QString, int> priorityMap;
    for (const ModConfig &config : profile.modConfigs) {
        priorityMap.insert(config.modId, config.priority);
    }

    m_modManager->batchSetModPriorities(priorityMap);

    for (const ModConfig &config : profile.modConfigs) {
        if (config.enabled) {
            m_modManager->enableMod(config.modId);
        }
    }

    return true;
}
