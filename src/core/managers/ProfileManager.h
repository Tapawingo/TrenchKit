#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include "core/models/ProfileInfo.h"
#include "core/models/ModInfo.h"
#include <QObject>
#include <QList>
#include <QString>
#include <functional>

class ModManager;

struct MissingModInfo {
    QString modId;
    QString nexusModId;

    bool hasNexusId() const { return !nexusModId.isEmpty(); }
};

struct ProfileValidationResult {
    bool isValid;
    QList<MissingModInfo> missingMods;
    int availableModsCount;
    int totalModsCount;

    bool hasMissingMods() const { return !missingMods.isEmpty(); }
    QString getMessage() const;
};

class ProfileManager : public QObject {
    Q_OBJECT

public:
    enum class ImportConflictAction {
        Ignore,
        Overwrite,
        Duplicate
    };
    using ImportConflictResolver = std::function<ImportConflictAction(const ModInfo &incoming,
                                                                      const ModInfo &existing,
                                                                      bool checksumMatch)>;

    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager() override = default;

    void setModManager(ModManager *modManager);
    void setStoragePath(const QString &storagePath);

    bool createProfile(const QString &name);
    bool updateProfile(const QString &profileId);
    bool renameProfile(const QString &profileId, const QString &newName);
    bool deleteProfile(const QString &profileId);
    bool reorderProfiles(const QList<QString> &orderedProfileIds);
    [[nodiscard]] ProfileInfo getProfile(const QString &profileId) const;
    [[nodiscard]] QList<ProfileInfo> getProfiles() const;

    ProfileValidationResult validateProfile(const QString &profileId) const;
    bool applyProfile(const QString &profileId, bool ignoreWarnings = false);

    bool exportProfile(const QString &profileId, const QString &filePath);
    bool importProfile(const QString &filePath, QString &importedProfileId,
                       ImportConflictResolver resolver = {});

    bool loadProfiles();
    bool saveProfiles();

    QString getActiveProfileId() const { return m_activeProfileId; }
    void setActiveProfile(const QString &profileId);
    void clearActiveProfile();
    bool isProfileActive(const QString &profileId) const;

signals:
    void profilesChanged();
    void profileCreated(const QString &profileId);
    void profileDeleted(const QString &profileId);
    void profileApplied(const QString &profileId);
    void activeProfileChanged(const QString &profileId);
    void errorOccurred(const QString &error);

private:
    QString getStorageFilePath() const;
    ProfileInfo captureCurrentState() const;
    bool applyProfileInternal(const ProfileInfo &profile);

    ModManager *m_modManager = nullptr;
    QString m_storagePath;
    QList<ProfileInfo> m_profiles;
    QString m_activeProfileId;
};

#endif // PROFILEMANAGER_H
