#ifndef MODMANAGER_H
#define MODMANAGER_H

#include "core/models/ModInfo.h"
#include <QObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>

class ModManager : public QObject {
    Q_OBJECT

public:
    explicit ModManager(QObject *parent = nullptr);
    ~ModManager() override = default;

    // Initialization
    void setInstallPath(const QString &foxholeInstallPath);
    void setModsStoragePath(const QString &modsPath);

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

    // Mod management
    bool addMod(const QString &pakFilePath, const AddModParams &params = {});
    bool removeMod(const QString &modId);
    bool replaceMod(const QString &modId, const QString &newPakPath,
                   const QString &newVersion, const QString &newFileId,
                   const QDateTime &uploadDate = QDateTime());
    bool enableMod(const QString &modId);
    bool disableMod(const QString &modId);
    bool setAllModsEnabled(bool enabled);
    bool setModsEnabled(const QStringList &modIds, bool enabled);
    bool setModPriority(const QString &modId, int priority);
    bool batchSetModPriorities(const QMap<QString, int> &priorityMap);
    bool updateModMetadata(const ModInfo &updatedMod);

    // Getters
    QList<ModInfo> getMods() const;
    ModInfo getMod(const QString &modId) const;
    QString getModsStoragePath() const { return m_modsStoragePath; }
    QString getPaksPath() const;

    // Persistence
    bool loadMods();
    bool saveMods();

    // Unregistered mod detection
    void detectUnregisteredMods();
    void syncEnabledModsWithPaks();

signals:
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
    QString generateNumberedFileName(int priority, const QString &originalFileName) const;
    void updateNumberedFileNames();
    void renumberEnabledMods();
    QString cleanModName(const QString &fileName) const;
    bool isBaseGamePak(const QString &fileName) const;

    QString m_foxholeInstallPath;
    QString m_modsStoragePath;
    QList<ModInfo> m_mods;
    mutable QRecursiveMutex m_modsMutex;
};

#endif // MODMANAGER_H
