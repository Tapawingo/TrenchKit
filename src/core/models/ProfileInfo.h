/// @file ProfileInfo.h
/// @brief Data structures for mod load-out profiles.
#ifndef PROFILEINFO_H
#define PROFILEINFO_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QList>

/// @brief Lightweight snapshot of one mod's state within a profile.
///
/// Stores only the state needed to restore a profile — does not duplicate
/// full ModInfo metadata.
struct ModConfig {
    QString modId;   ///< References ModInfo::id.
    bool enabled;    ///< Enabled state captured at profile save time.
    int priority;    ///< Priority captured at profile save time.

    /// @brief Serializes to JSON.
    QJsonObject toJson() const;
    /// @brief Deserializes from JSON.
    static ModConfig fromJson(const QJsonObject &json);
};

/// @brief A named snapshot of the mod list state.
///
/// Applying a profile replaces the current mod enable/priority state
/// with the values stored in @c modConfigs.
struct ProfileInfo {
    QString id;
    QString name;
    QDateTime createdDate  = QDateTime::currentDateTime();
    QDateTime modifiedDate = QDateTime::currentDateTime();
    QList<ModConfig> modConfigs; ///< One entry per mod in the list at save time.

    /// @brief Serializes to JSON.
    QJsonObject toJson() const;
    /// @brief Deserializes from JSON.
    static ProfileInfo fromJson(const QJsonObject &json);

    /// @brief Generates a new random UUID string suitable for use as a profile ID.
    static QString generateId() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
};

#endif // PROFILEINFO_H
