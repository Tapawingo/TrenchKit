/**
 * @file NexusFileInfo.h
 * @brief Metadata for a single file entry returned by the NexusMods API.
 */
#pragma once

#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QList>
#include <QPair>

/**
 * @brief Describes one downloadable file on a NexusMods mod page.
 */
struct NexusFileInfo {
    QString id;
    QString name;
    QString version;
    QString description;
    qint64 sizeBytes = -1;
    QString categoryName;
    QDateTime uploadedTime;
    /**
     * @brief Version-update pairs associated with this file (old → new version strings).
     */
    QList<QPair<QString, QString>> fileUpdates;

    /**
     * @brief Deserializes from a NexusMods API JSON object.
     */
    static NexusFileInfo fromJson(const QJsonObject &json);
};
