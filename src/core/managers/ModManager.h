/// @file ModManager.h
/// @brief Central manager for all mod storage, load-order, and lifecycle operations.
#ifndef MODMANAGER_H
#define MODMANAGER_H

#include "core/models/ModInfo.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

/// @brief Manages the mod list, load order, and pak file deployment.
///
/// Mods are stored as .pak files under AppData/TrenchKit/mods/ with metadata in
/// mods.json. Enabled mods are copied into the Foxhole paks directory using
/// zero-padded numbered filenames (e.g. "001_modname.pak") that determine load
/// order: lower priority number loads first and is overwritten by higher-priority mods.
///
/// All public methods are thread-safe via an internal QRecursiveMutex.
/// Results are returned synchronously; UI feedback is delivered via signals.
class ModManager : public QObject {
    Q_OBJECT

public:
    explicit ModManager(QObject *parent = nullptr);
    ~ModManager() override = default;

    /// @brief Sets the Foxhole installation directory used to locate the paks folder.
    void setInstallPath(const QString &foxholeInstallPath);
    /// @brief Overrides the default mod storage directory.
    void setModsStoragePath(const QString &modsPath);

    /// @brief Parameters for addMod(). All fields are optional except the pak file path.
    ///
    /// Use C++20 designated initializers: `addMod(path, {.name = "My Mod", .nexusModId = "123"})`.
    struct AddModParams {
        QString name;
        QString nexusModId;
        QString nexusFileId;
        QString nexusUrl;
        QString author;
        QString description;
        QString version;
        QString itchGameId;
        QString itchUrl;
        QDateTime uploadDate;
    };

    /// @brief Registers a pak file and copies it into managed storage.
    /// @returns false if the file cannot be read or a mod with the same ID already exists.
    bool addMod(const QString &pakFilePath, const AddModParams &params = {});

    /// @brief Removes a mod and deletes its pak from storage and the paks folder.
    bool removeMod(const QString &modId);

    /// @brief Replaces the pak file for an existing mod and updates its metadata.
    bool replaceMod(const QString &modId, const QString &newPakPath,
                   const QString &newVersion, const QString &newFileId,
                   const QDateTime &uploadDate = QDateTime());

    /// @brief Copies the mod's pak into the Foxhole paks folder.
    bool enableMod(const QString &modId);
    /// @brief Removes the mod's pak from the Foxhole paks folder.
    bool disableMod(const QString &modId);

    /// @brief Enables or disables all mods in a single batch operation.
    bool setAllModsEnabled(bool enabled);
    /// @brief Enables or disables a specific set of mods by ID.
    bool setModsEnabled(const QStringList &modIds, bool enabled);

    /// @brief Changes the load-order priority of a mod and renumbers enabled mods.
    bool setModPriority(const QString &modId, int priority);
    /// @brief Applies a map of modId → priority in one atomic operation.
    bool batchSetModPriorities(const QMap<QString, int> &priorityMap);

    /// @brief Updates stored metadata fields without touching the pak file.
    bool updateModMetadata(const ModInfo &updatedMod);

    /// @brief Returns a snapshot of the full mod list (thread-safe copy).
    [[nodiscard]] QList<ModInfo> getMods() const;
    /// @brief Returns a single mod by ID; returns a default-constructed ModInfo if not found.
    [[nodiscard]] ModInfo getMod(const QString &modId) const;
    /// @brief Returns the directory where mod pak files are stored.
    [[nodiscard]] QString getModsStoragePath() const { return m_modsStoragePath; }
    /// @brief Returns the Foxhole paks directory where enabled mods are deployed.
    [[nodiscard]] QString getPaksPath() const;

    /// @brief Loads mod metadata from mods.json on disk.
    bool loadMods();
    /// @brief Persists mod metadata to mods.json on disk.
    bool saveMods();

    /// @brief Scans the paks folder for pak files not registered in the mod list.
    void detectUnregisteredMods();
    /// @brief Reconciles the enabled state of all mods against the paks folder contents.
    void syncEnabledModsWithPaks();

signals:
    /// @brief Emitted after any change to the mod list or load order.
    void modsChanged();
    void modEnabled(const QString &modId);
    void modDisabled(const QString &modId);
    void modAdded(const QString &modId);
    void modRemoved(const QString &modId);
    void errorOccurred(const QString &error);

private:
    QString getModFilePath(const QString &modId) const;
    QString getMetadataFilePath() const;
    bool copyModToPaks(const ModInfo &mod);
    bool removeModFromPaks(const ModInfo &mod);
    void sortModsByPriority();
    /// @brief Generates the zero-padded numbered filename for a mod (e.g. "001_modname.pak").
    QString generateNumberedFileName(int priority, const QString &originalFileName) const;
    void updateNumberedFileNames();
    void renumberEnabledMods();
    QString cleanModName(const QString &fileName) const;
    bool isBaseGamePak(const QString &fileName) const;

    /// @brief Width of the zero-padded priority prefix (e.g. 3 → "001_").
    static constexpr int PriorityPadWidth = 3;

    QString m_foxholeInstallPath;
    QString m_modsStoragePath;
    QList<ModInfo> m_mods;
    mutable QRecursiveMutex m_modsMutex;
};

#endif // MODMANAGER_H
