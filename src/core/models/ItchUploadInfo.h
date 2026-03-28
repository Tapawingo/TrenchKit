/// @file ItchUploadInfo.h
/// @brief Metadata for a single upload entry returned by the itch.io API.
#ifndef ITCHUPLOADINFO_H
#define ITCHUPLOADINFO_H

#include <QString>
#include <QJsonObject>
#include <QDateTime>

/// @brief Describes one downloadable upload on an itch.io game page.
struct ItchUploadInfo {
    QString id;           ///< Numeric upload ID (as string).
    QString filename;     ///< Original filename of the upload.
    QString displayName;  ///< Human-readable name (may be empty; fall back to @c filename).
    qint64 sizeBytes = -1; ///< File size in bytes; -1 if unknown.
    QDateTime createdAt;  ///< When the upload was first published.
    QDateTime updatedAt;  ///< When the upload was last replaced; use @c createdAt if invalid.

    /// @brief Deserializes from an itch.io API JSON object.
    static ItchUploadInfo fromJson(const QJsonObject &json);
};

#endif // ITCHUPLOADINFO_H
