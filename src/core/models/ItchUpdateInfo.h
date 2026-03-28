#ifndef ITCHUPDATEINFO_H
#define ITCHUPDATEINFO_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QRegularExpression>
#include "ItchUploadInfo.h"

struct ItchUpdateInfo {
    QString modId;
    QString currentVersion;
    QString availableVersion;
    QString availableUploadId;
    QDateTime currentUploadDate;
    QDateTime availableUploadDate;
    QList<ItchUploadInfo> candidateUploads;
    QDateTime lastChecked;
    bool updateAvailable = false;

    ItchUpdateInfo() = default;

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

    bool hasMultipleUploads() const { return candidateUploads.size() > 1; }
};

#endif
