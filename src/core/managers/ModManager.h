/**
 * @file ModManager.h
 * @brief Central manager for all mod storage, load-order, and lifecycle operations.
 */
#ifndef MODMANAGER_H
#define MODMANAGER_H

#include "core/models/ModInfo.h"
#include "core/utils/CancelToken.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>

/**
 * @brief Manages the mod list, load order, and pak file deployment.
 *
 * Mods are stored as .pak files under AppData/TrenchKit/mods/ with metadata in
 * mods.json. Enabled mods are copied into the Foxhole paks directory using
 * zero-padded numbered filenames (e.g. "001_modname.pak") that determine load
 * order: lower priority number loads first and is overwritten by higher-priority mods.
 *
 * All public methods are thread-safe via an internal QRecursiveMutex.
 * Results are returned synchronously; UI feedback is delivered via signals.
 */
class ModManager : public QObject {
    Q_OBJECT

public:
    explicit ModManager(QObject *parent = nullptr);
    /**
     * @brief Stops a running enable job and keeps the mods it had already finished.
     *
     * The pak being copied is abandoned, the mods copied before it stay enabled and are saved, and mods
     * that had not been started stay disabled, so closing the app never leaves unregistered paks behind.
     */
    ~ModManager() override;

    /**
     * @brief Sets the Foxhole installation directory used to locate the paks folder.
     */
    void setInstallPath(const QString &foxholeInstallPath);
    /**
     * @brief Overrides the default mod storage directory.
     */
    void setModsStoragePath(const QString &modsPath);

    /**
     * @brief Parameters for addMod(). All fields are optional except the pak file path.
     *
     * Use C++20 designated initializers: `addMod(path, {.name = "My Mod", .nexusModId = "123"})`.
     */
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
        QString itchUploadId;
        QDateTime uploadDate;
    };

    /**
     * @brief Registers a pak file and copies it into managed storage.
     * @returns false if the file is not a valid pak, cannot be read, or a mod with the same ID already exists.
     */
    bool addMod(const QString &pakFilePath, const AddModParams &params = {});

    /**
     * @brief Like @c addMod(), but copies the pak into storage on a worker thread.
     *
     * Validation problems are reported at once: @p onFinished(false) is called before this returns and
     * the result is null. Otherwise @p onFinished receives the outcome on the calling thread, and not at
     * all if @p context is destroyed first (the copy is then cancelled and nothing is added).
     * @returns A token that cancels the copy; the callback then receives false without an error signal.
     */
    CancelTokenPtr addModAsync(const QString &pakFilePath, const AddModParams &params,
                               QObject *context, std::function<void(bool)> onFinished);

    /**
     * @brief Removes a mod and deletes its pak from storage and the paks folder.
     */
    bool removeMod(const QString &modId);

    /**
     * @brief Replaces the pak file for an existing mod and updates its metadata.
     *
     * The existing mod is left untouched unless @p newPakPath is a valid pak.
     * @returns false (after emitting @c errorOccurred) if the file is not a valid pak or cannot be installed.
     */
    bool replaceMod(const QString &modId, const QString &newPakPath,
                   const QString &newVersion, const QString &newFileId,
                   const QDateTime &uploadDate = QDateTime());

    /**
     * @brief Like @c replaceMod(), but copies the new pak on a worker thread; see @c addModAsync() for the callback rules.
     */
    CancelTokenPtr replaceModAsync(const QString &modId, const QString &newPakPath,
                                   const QString &newVersion, const QString &newFileId,
                                   const QDateTime &uploadDate, QObject *context,
                                   std::function<void(bool)> onFinished);

    /**
     * @brief Like @c replaceMod(), but accepts a downloaded archive as well as a bare pak.
     *
     * An archive is extracted on a worker thread first; if it holds several paks the one named
     * like the mod's current file is used. Fails (after emitting @c errorOccurred) rather than guessing.
     * @p onFinished receives the outcome on the calling thread: immediately for a bare pak, later for
     * an archive, and not at all if @p context is destroyed first (the mod is then left untouched).
     * @returns A token that cancels unpacking and copying; null if the outcome was already reported.
     */
    CancelTokenPtr replaceModFromFile(const QString &modId, const QString &filePath,
                            const QString &newVersion, const QString &newFileId,
                            const QDateTime &uploadDate, QObject *context,
                            std::function<void(bool)> onFinished);

    /**
     * @brief Removes the mod's pak from the Foxhole paks folder.
     *
     * Enabling has no blocking counterpart on purpose: copying a pak into the game folder takes long
     * enough to freeze the window, so it only exists as setModsEnabledAsync().
     */
    bool disableMod(const QString &modId);

    /**
     * @brief Disables every enabled mod in a single batch operation.
     */
    bool disableAllMods();
    /**
     * @brief Disables a specific set of mods by ID.
     */
    bool disableMods(const QStringList &modIds);

    /**
     * @brief What an asynchronous enable job achieved.
     */
    struct EnableOutcome {
        int enabled = 0;        ///< Mods that are enabled because of this job.
        int failed = 0;         ///< Mods that could not be enabled (an error signal was emitted for each).
        bool cancelled = false; ///< Stopped through the token before every mod was done.
        [[nodiscard]] bool ok() const { return failed == 0 && !cancelled; }
    };

    /**
     * @brief True from the moment a mod is handed to setModsEnabledAsync() until that job is over.
     *
     * Such a mod cannot be disabled, removed or replaced yet, because the job would enable it a moment
     * later; those calls are refused with an error instead of silently losing the request.
     */
    [[nodiscard]] bool isEnabling(const QString &modId) const;

    /**
     * @brief Enables mods by copying their paks into the game folder on a worker thread.
     *
     * Each pak is copied to a temporary name and renamed into place, so the game never sees a partial file.
     * Jobs run one after another. Disabling is cheap and happens at once. @p onFinished receives the outcome on
     * the calling thread (possibly before this returns when there is nothing to do), and not at all if
     * @p context is destroyed first; the job still finishes and keeps the mod list consistent.
     * Progress is reported through @c enableProgress().
     * @returns A token that stops the job after the pak being copied; finished mods stay enabled.
     */
    CancelTokenPtr setModsEnabledAsync(const QStringList &modIds, bool enabled, QObject *context,
                                       std::function<void(const EnableOutcome &)> onFinished);

    /**
     * @brief Changes the load-order priority of a mod and renumbers enabled mods.
     */
    bool setModPriority(const QString &modId, int priority);
    /**
     * @brief Applies a map of modId → priority in one atomic operation.
     */
    bool batchSetModPriorities(const QMap<QString, int> &priorityMap);

    /**
     * @brief Updates stored metadata fields without touching the pak file.
     */
    bool updateModMetadata(const ModInfo &updatedMod);

    /**
     * @brief Returns a snapshot of the full mod list (thread-safe copy).
     */
    [[nodiscard]] QList<ModInfo> getMods() const;
    /**
     * @brief Returns a single mod by ID; returns a default-constructed ModInfo if not found.
     */
    [[nodiscard]] ModInfo getMod(const QString &modId) const;
    /**
     * @brief Returns the directory where mod pak files are stored.
     */
    [[nodiscard]] QString getModsStoragePath() const { return m_modsStoragePath; }
    /**
     * @brief Returns the Foxhole paks directory where enabled mods are deployed.
     */
    [[nodiscard]] QString getPaksPath() const;

    /**
     * @brief Loads mod metadata from mods.json on disk.
     */
    bool loadMods();
    /**
     * @brief Persists mod metadata to mods.json on disk.
     */
    bool saveMods();

    /**
     * @brief Scans the paks folder for pak files not registered in the mod list.
     */
    void detectUnregisteredMods();
    /**
     * @brief Reconciles the enabled state of all mods against the paks folder contents.
     */
    void syncEnabledModsWithPaks();
    /**
     * @brief Produces a human-readable display name from a raw pak filename.
     */
    QString cleanModName(const QString &fileName) const;

    /**
     * @brief A single issue found by verifyMods().
     */
    struct VerificationIssue {
        QString modId;
        QString modName;
        QString issue; ///< Human-readable description of the problem.
    };

    /**
     * @brief Checks every mod's pak file exists in storage. Thread-safe; safe to call from any thread.
     * @returns A list of issues; empty means all mods are healthy.
     */
    [[nodiscard]] QList<VerificationIssue> verifyMods() const;

signals:
    /**
     * @brief Emitted after any change to the mod list or load order.
     */
    void modsChanged();
    void modEnabled(const QString &modId);
    void modDisabled(const QString &modId);
    void modAdded(const QString &modId);
    void modRemoved(const QString &modId);
    void errorOccurred(const QString &error);

    /**
     * @brief Progress of the running @c setModsEnabledAsync() job: @p done of @p total mods copied.
     */
    void enableProgress(int done, int total);

private:
    struct EnableJob;
    struct EnableWork;
    void startNextEnableJob();
    void releaseEnableClaims(const std::shared_ptr<EnableJob> &job);
    /// Emits an error and returns true when @p modId is still waiting for an enable job.
    bool refuseWhileEnabling(const QString &modId);
    void runEnableJob(const std::shared_ptr<EnableJob> &job);
    void finishEnableJob(const std::shared_ptr<EnableJob> &job, const EnableWork &work, bool notify = true);
    void shutdownEnableJob();

    struct StagedPak;
    struct PendingAdd;
    struct PendingReplace;

    /// Copies @p source to @p stagedPath and reads its manifest; safe to run on any thread.
    static StagedPak stagePak(const QString &source, const QString &stagedPath, const CancelToken *cancel);
    bool prepareAdd(const QString &pakFilePath, const AddModParams &params, PendingAdd *pending);
    bool finishAdd(PendingAdd &pending, const StagedPak &staged);
    bool prepareReplace(const QString &modId, const QString &newPakPath, const QString &newVersion,
                        const QString &newFileId, const QDateTime &uploadDate, PendingReplace *pending);
    /// With @p deferReenable a previously enabled mod is not re-enabled here; @p needsReenable tells the caller to.
    bool finishReplace(const PendingReplace &pending, const StagedPak &staged, bool deferReenable = false,
                       bool *needsReenable = nullptr);
    CancelTokenPtr replaceStaged(const QString &modId, const QString &newPakPath, const QString &newVersion,
                                 const QString &newFileId, const QDateTime &uploadDate, QObject *context,
                                 std::function<void(bool)> onFinished, CancelTokenPtr token,
                                 std::shared_ptr<void> keepAlive);

    QString getModFilePath(const QString &modId) const;
    QString getMetadataFilePath() const;
    bool copyModToPaks(const ModInfo &mod);
    /// Copies the pak on the calling thread. Only the synchronous replaceMod() may use it.
    bool enableModBlocking(const QString &modId);
    bool removeModFromPaks(const ModInfo &mod);
    void sortModsByPriority();
    /**
     * @brief Generates the zero-padded numbered filename for a mod (e.g. "001_modname.pak").
     */
    QString generateNumberedFileName(int priority, const QString &originalFileName) const;
    void updateNumberedFileNames();
    void renumberEnabledMods();
    bool isBaseGamePak(const QString &fileName) const;

    /**
     * @brief Width of the zero-padded priority prefix (e.g. 3 → "001_").
     */
    static constexpr int PriorityPadWidth = 3;

    QString m_foxholeInstallPath;
    QString m_modsStoragePath;
    QList<ModInfo> m_mods;
    mutable QRecursiveMutex m_modsMutex;

    QList<std::shared_ptr<EnableJob>> m_enableQueue;
    bool m_enableRunning = false;
    QSet<QString> m_enablingIds;
    std::shared_ptr<EnableJob> m_runningEnable;
};

#endif // MODMANAGER_H
