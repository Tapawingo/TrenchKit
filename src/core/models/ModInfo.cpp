#include "ModInfo.h"
#include <QJsonArray>

QJsonObject ModInfo::toJson() const {
    QJsonObject json;
    json["id"] = id;
    json["name"] = name;
    json["fileName"] = fileName;
    json["numberedFileName"] = numberedFileName;
    json["installDate"] = installDate.toString(Qt::ISODate);
    json["enabled"] = enabled;
    json["priority"] = priority;

    // Optional fields
    if (uploadDate.isValid()) {
        json["uploadDate"] = uploadDate.toString(Qt::ISODate);
    }
    if (!nexusModId.isEmpty()) {
        json["nexusModId"] = nexusModId;
    }
    if (!nexusFileId.isEmpty()) {
        json["nexusFileId"] = nexusFileId;
    }
    if (!nexusUrl.isEmpty()) {
        json["nexusUrl"] = nexusUrl;
    }
    if (!itchGameId.isEmpty()) {
        json["itchGameId"] = itchGameId;
    }
    if (!itchUrl.isEmpty()) {
        json["itchUrl"] = itchUrl;
    }
    if (!itchUploadId.isEmpty()) {
        json["itchUploadId"] = itchUploadId;
    }
    if (!version.isEmpty()) {
        json["version"] = version;
    }
    if (!author.isEmpty()) {
        json["author"] = author;
    }
    if (!description.isEmpty()) {
        json["description"] = description;
    }
    if (!homepageUrl.isEmpty()) {
        json["homepageUrl"] = homepageUrl;
    }
    if (!manifestId.isEmpty()) {
        json["manifestId"] = manifestId;
    }
    if (!manifestAuthors.isEmpty()) {
        QJsonArray authorsArray;
        for (const QString &manifestAuthor : manifestAuthors) {
            authorsArray.append(manifestAuthor);
        }
        json["manifestAuthors"] = authorsArray;
    }
    if (!manifestDependencies.isEmpty()) {
        QJsonArray depsArray;
        for (const auto &dep : manifestDependencies) {
            QJsonObject depObj;
            depObj["id"] = dep.id;
            if (!dep.minVersion.isEmpty()) {
                depObj["minVersion"] = dep.minVersion;
            }
            if (!dep.maxVersion.isEmpty()) {
                depObj["maxVersion"] = dep.maxVersion;
            }
            depObj["required"] = dep.required;
            depsArray.append(depObj);
        }
        json["manifestDependencies"] = depsArray;
    }
    if (!manifestTags.isEmpty()) {
        QJsonArray tagsArray;
        for (const QString &tag : manifestTags) {
            tagsArray.append(tag);
        }
        json["manifestTags"] = tagsArray;
    }
    if (!noticeText.isEmpty()) {
        json["noticeText"] = noticeText;
    }
    if (!noticeIcon.isEmpty()) {
        json["noticeIcon"] = noticeIcon;
    }
    if (!ignoredItchUploadIds.isEmpty()) {
        QJsonArray ignoredArray;
        for (const QString &uploadId : ignoredItchUploadIds) {
            ignoredArray.append(uploadId);
        }
        json["ignoredItchUploadIds"] = ignoredArray;
    }

    return json;
}

ModInfo ModInfo::fromJson(const QJsonObject &json) {
    ModInfo mod;
    mod.id = json["id"].toString();
    mod.name = json["name"].toString();
    mod.fileName = json["fileName"].toString();
    mod.numberedFileName = json["numberedFileName"].toString();
    mod.installDate = QDateTime::fromString(json["installDate"].toString(), Qt::ISODate);
    mod.enabled = json["enabled"].toBool();
    mod.priority = json["priority"].toInt();

    // Optional fields
    if (json.contains("uploadDate")) {
        mod.uploadDate = QDateTime::fromString(json["uploadDate"].toString(), Qt::ISODate);
    }
    if (json.contains("nexusModId")) {
        mod.nexusModId = json["nexusModId"].toString();
    }
    if (json.contains("nexusFileId")) {
        mod.nexusFileId = json["nexusFileId"].toString();
    }
    if (json.contains("nexusUrl")) {
        mod.nexusUrl = json["nexusUrl"].toString();
    }
    if (json.contains("itchGameId")) {
        mod.itchGameId = json["itchGameId"].toString();
    }
    if (json.contains("itchUrl")) {
        mod.itchUrl = json["itchUrl"].toString();
    }
    if (json.contains("itchUploadId")) {
        mod.itchUploadId = json["itchUploadId"].toString();
    }
    if (json.contains("version")) {
        mod.version = json["version"].toString();
    }
    if (json.contains("author")) {
        mod.author = json["author"].toString();
    }
    if (json.contains("description")) {
        mod.description = json["description"].toString();
    }
    if (json.contains("homepageUrl")) {
        mod.homepageUrl = json["homepageUrl"].toString();
    }
    if (json.contains("manifestId")) {
        mod.manifestId = json["manifestId"].toString();
    }
    if (json.contains("manifestAuthors")) {
        QJsonArray authorsArray = json["manifestAuthors"].toArray();
        for (const QJsonValue &value : authorsArray) {
            mod.manifestAuthors.append(value.toString());
        }
    }
    if (json.contains("manifestDependencies")) {
        QJsonArray depsArray = json["manifestDependencies"].toArray();
        for (const QJsonValue &value : depsArray) {
            QJsonObject depObj = value.toObject();
            ModInfo::Dependency dep;
            dep.id = depObj["id"].toString();
            dep.minVersion = depObj["minVersion"].toString();
            dep.maxVersion = depObj["maxVersion"].toString();
            dep.required = depObj.contains("required") ? depObj["required"].toBool(true) : true;
            if (!dep.id.isEmpty()) {
                mod.manifestDependencies.append(dep);
            }
        }
    }
    if (json.contains("manifestTags")) {
        QJsonArray tagsArray = json["manifestTags"].toArray();
        for (const QJsonValue &value : tagsArray) {
            mod.manifestTags.append(value.toString());
        }
    }
    if (json.contains("noticeText")) {
        mod.noticeText = json["noticeText"].toString();
    }
    if (json.contains("noticeIcon")) {
        mod.noticeIcon = json["noticeIcon"].toString();
    }
    if (json.contains("ignoredItchUploadIds")) {
        QJsonArray ignoredArray = json["ignoredItchUploadIds"].toArray();
        for (const QJsonValue &value : ignoredArray) {
            mod.ignoredItchUploadIds.append(value.toString());
        }
    }

    return mod;
}
