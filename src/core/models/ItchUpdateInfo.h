/**
 * @file ItchUpdateInfo.h
 * @brief Result of an itch.io update check for a single mod.
 */
#ifndef ITCHUPDATEINFO_H
#define ITCHUPDATEINFO_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QRegularExpression>
#include "ItchUploadInfo.h"

/**
 * @brief Cached result of an itch.io update check.
 *
 * Produced by ItchModUpdateService and consumed by the UI and
 * ItchModUpdateModalContent. When multiple candidate uploads are found,
 * @c candidateUploads is populated so the user can pick one.
 */
struct ItchUpdateInfo {
    QString modId;
    QString currentVersion;
    QString availableVersion;  ///< Extracted from filename via regex, or a formatted date string.
    QString availableUploadId;
    QDateTime currentUploadDate;
    QDateTime availableUploadDate;
    QList<ItchUploadInfo> candidateUploads; ///< Non-empty when the user must choose among uploads.
    QDateTime lastChecked;
    bool updateAvailable = false;

    ItchUpdateInfo() = default;

    /**
     * @brief Constructs a resolved single-upload update result.
     */
    ItchUpdateInfo(const QString &id, const QString &current, const QString &available,
                   const QString &uploadId, const QDateTime &currentDate, const QDateTime &availableDate)
        : modId(id)
        , currentVersion(current)
        , availableVersion(available)
        , availableUploadId(uploadId)
        , currentUploadDate(currentDate)
        , availableUploadDate(availableDate)
        , lastChecked(QDateTime::currentDateTime())
        , updateAvailable(true)
    {}

    /**
     * @brief Constructs an update result from a list of candidate uploads.
     *
     * Selects the first upload as the preferred candidate. The version string is
     * extracted from the filename via regex; if no version is found it falls back
     * to @c "Updated: YYYY-MM-DD". When @c uploads has more than one entry,
     * @c hasMultipleUploads() returns true and the UI should prompt the user.
     */
    ItchUpdateInfo(const QString &id, const QString &current,
                   const QDateTime &currentDate, const QList<ItchUploadInfo> &uploads)
        : modId(id)
        , currentVersion(current)
        , currentUploadDate(currentDate)
        , candidateUploads(uploads)
        , lastChecked(QDateTime::currentDateTime())
        , updateAvailable(!uploads.isEmpty())
    {
        if (!uploads.isEmpty()) {
            const ItchUploadInfo &latest = uploads.first();
            availableUploadId = latest.id;
            availableUploadDate = latest.updatedAt.isValid() ? latest.updatedAt : latest.createdAt;

            static const QRegularExpression versionRegex(QStringLiteral(R"(v?(\d+\.\d+(?:\.\d+)?))"));
            QRegularExpressionMatch match = versionRegex.match(latest.filename);

            if (match.hasMatch()) {
                availableVersion = match.captured(1);
            } else {
                availableVersion = QStringLiteral("Updated: %1").arg(availableUploadDate.toString(QStringLiteral("yyyy-MM-dd")));
            }
        }
    }

    /**
     * @brief Returns true when more than one upload is available to choose from.
     */
    bool hasMultipleUploads() const { return candidateUploads.size() > 1; }
};

#endif
