/**
 * @file ProfileManager.h
 * @brief Manager for creating, applying, importing, and exporting mod profiles.
 */
#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include "core/utils/CancelToken.h"
#include "core/models/ProfileInfo.h"
#include "core/models/ModInfo.h"
#include <QObject>
#include <QList>
#include <QString>
#include <functional>

class ModManager;

/**
 * @brief Identifies a mod that is referenced in a profile but not present locally.
 */
struct MissingModInfo {
    QString modId;
    QString nexusModId; ///< Non-empty if the mod can be retrieved from NexusMods.

    bool hasNexusId() const { return !nexusModId.isEmpty(); }
};

/**
 * @brief Result of validating a profile before applying it.
 */
struct ProfileValidationResult {
    bool isValid;
    QList<MissingModInfo> missingMods;
    int availableModsCount;
    int totalModsCount;

    bool hasMissingMods() const { return !missingMods.isEmpty(); }
    /**
     * @brief Returns a human-readable summary of the validation result.
     */
    QString getMessage() const;
};

/**
 * @brief Manages mod profiles (named snapshots of the mod list state).
 *
 * A profile stores the enabled/priority state of each mod at save time.
 * Applying a profile restores that state on the current mod list.
 * Profiles are persisted in AppData/TrenchKit/profiles/.
 */
class ProfileManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Action to take when an imported mod conflicts with an existing one.
     */
    enum class ImportConflictAction {
        Ignore,     ///< Keep the existing mod, skip the incoming one.
        Overwrite,  ///< Replace the existing mod with the incoming one.
        Duplicate   ///< Import as a new mod alongside the existing one.
    };

    /**
     * @brief Callback invoked for each conflicting mod during profile import.
     *
     * @param incoming  The mod from the imported profile archive.
     * @param existing  The locally installed mod with the same ID.
     * @param checksumMatch  True if both mods have identical pak file content.
     */
    using ImportConflictResolver = std::function<ImportConflictAction(const ModInfo &incoming,
                                                                      const ModInfo &existing,
                                                                      bool checksumMatch)>;

    explicit ProfileManager(QObject *parent = nullptr);
    ~ProfileManager() override = default;

    /**
     * @brief Injects the ModManager used to read and apply mod state.
     */
    void setModManager(ModManager *modManager);
    [[nodiscard]] ModManager *modManager() const { return m_modManager; }
    /**
     * @brief Sets the directory where profile files are stored.
     */
    void setStoragePath(const QString &storagePath);

    /**
     * @brief Creates a new empty profile with the given name.
     */
    bool createProfile(const QString &name);
    /**
     * @brief Overwrites an existing profile with the current mod list state.
     */
    bool updateProfile(const QString &profileId);
    /**
     * @brief Renames a profile.
     */
    bool renameProfile(const QString &profileId, const QString &newName);
    /**
     * @brief Deletes a profile and its stored file.
     */
    bool deleteProfile(const QString &profileId);
    /**
     * @brief Persists a new display order for the profile list.
     */
    bool reorderProfiles(const QList<QString> &orderedProfileIds);

    [[nodiscard]] ProfileInfo getProfile(const QString &profileId) const;
    [[nodiscard]] QList<ProfileInfo> getProfiles() const;

    /**
     * @brief Checks whether all mods referenced by a profile are present locally.
     */
    ProfileValidationResult validateProfile(const QString &profileId) const;

    /**
     * @brief Applies a profile, restoring the mod enabled/priority state it captured.
     *
     * The paks of the mods to enable are copied on a worker thread.
     * @param ignoreWarnings  If true, applies even when some mods are missing.
     *
     * Problems found up front are reported at once (@p onFinished(false) before this returns, null result).
     * Otherwise @p onFinished receives the outcome on the calling thread, and not at all if @p context
     * is destroyed first. A cancelled apply leaves the mods partly switched and the profile not active.
     * @returns A token that cancels the copying.
     */
    CancelTokenPtr applyProfileAsync(const QString &profileId, bool ignoreWarnings, QObject *context,
                                     std::function<void(bool)> onFinished);

    /**
     * @brief Exports a profile to a .tkprofile archive file.
     */
    bool exportProfile(const QString &profileId, const QString &filePath);

    /**
     * @brief Imports a profile from a .tkprofile archive file.
     * @param importedProfileId  Receives the ID of the newly created profile on success.
     * @param resolver  Optional callback to resolve conflicts; if null, conflicting mods are skipped.
     */
    bool importProfile(const QString &filePath, QString &importedProfileId,
                       ImportConflictResolver resolver = {});

    bool loadProfiles();
    bool saveProfiles();

    QString getActiveProfileId() const { return m_activeProfileId; }
    /**
     * @brief Marks a profile as the currently active one without applying it.
     */
    void setActiveProfile(const QString &profileId);
    void clearActiveProfile();
    bool isProfileActive(const QString &profileId) const;

signals:
    void profilesChanged();
    void profileCreated(const QString &profileId);
    void profileDeleted(const QString &profileId);
    /**
     * @brief Emitted after a profile has been successfully applied to the mod list.
     */
    void profileApplied(const QString &profileId);
    /**
     * @brief Emitted when the active (last applied) profile changes.
     */
    void activeProfileChanged(const QString &profileId);
    void errorOccurred(const QString &error);

private:
    QString getStorageFilePath() const;
    ProfileInfo captureCurrentState() const;
    /// Disables every mod and applies the profile's load order; returns the ids the profile wants enabled.
    QStringList prepareProfile(const ProfileInfo &profile);

    ModManager *m_modManager = nullptr;
    QString m_storagePath;
    QList<ProfileInfo> m_profiles;
    QString m_activeProfileId;
};

#endif // PROFILEMANAGER_H
