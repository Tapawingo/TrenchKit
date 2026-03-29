/**
 * @file ModInfo.h
 * @brief Persistent metadata for a single installed mod.
 */
#ifndef MODINFO_H
#define MODINFO_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QUuid>

/**
 * @brief Metadata for a single installed mod.
 *
 * Tracks identity, load order, source URLs (NexusMods and/or itch.io),
 * manifest-derived data, and update-suppression state.
 */
struct ModInfo {
    /**
     * @brief Manifest-declared dependency on another mod.
     */
    struct Dependency {
        QString id;               ///< Target mod identifier.
        QString minVersion;       ///< Minimum compatible version (empty = any).
        QString maxVersion;       ///< Maximum compatible version (empty = any).
        bool required = true;     ///< Whether the dependency is mandatory.
    };

    QString id;                   ///< Unique identifier (UUID).
    QString name;                 ///< Display name.
    QString fileName;             ///< Original .pak filename in the mods storage directory.
    QString numberedFileName;     ///< Load-order filename in the paks folder (e.g. "001_modname.pak").
    QDateTime installDate;        ///< When the mod was added to TrenchKit.
    QDateTime uploadDate;         ///< Upload timestamp from the source platform.
    bool enabled;                 ///< Whether the mod is currently deployed to the paks folder.
    int priority;                 ///< Load order index (lower value = loads first = overwritten by higher).
    QString nexusModId;           ///< NexusMods mod ID (empty if not linked).
    QString nexusFileId;          ///< NexusMods file ID for update tracking (empty if not linked).
    QString nexusUrl;             ///< NexusMods mod page URL.
    QString itchGameId;           ///< itch.io game ID (empty if not linked).
    QString itchUrl;              ///< itch.io game page URL.
    QString itchUploadId;         ///< itch.io upload ID of the installed file (empty if not linked).
    QString version;              ///< Mod version string.
    QString author;               ///< Mod author name.
    QString description;          ///< Short mod description.
    QString homepageUrl;          ///< External homepage URL.
    QString manifestId;           ///< ID declared inside the pak manifest.xml.
    QStringList manifestAuthors;  ///< Authors declared in the manifest.
    QList<Dependency> manifestDependencies; ///< Dependencies declared in the manifest.
    QStringList manifestTags;     ///< Tags declared in the manifest.
    QString noticeText;           ///< User-visible notice text from the manifest.
    QString noticeIcon;           ///< Icon type for the notice (e.g. "warning").
    QStringList ignoredItchUploadIds; ///< itch.io upload IDs skipped in future update checks.

    ModInfo()
        : enabled(false)
        , priority(0)
    {}

    /**
     * @brief Serializes this mod to a JSON object.
     */
    QJsonObject toJson() const;
    /**
     * @brief Deserializes a mod from a JSON object.
     */
    static ModInfo fromJson(const QJsonObject &json);

    /**
     * @brief Generates a new random UUID string suitable for use as a mod ID.
     */
    static QString generateId() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
};

#endif // MODINFO_H
