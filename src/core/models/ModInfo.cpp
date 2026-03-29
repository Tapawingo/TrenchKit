#include "ModInfo.h"
#include <QJsonArray>

static void writeNexusFields(const ModInfo &mod, QJsonObject &json) {
    if (!mod.nexusModId.isEmpty())  json["nexusModId"]  = mod.nexusModId;
    if (!mod.nexusFileId.isEmpty()) json["nexusFileId"] = mod.nexusFileId;
    if (!mod.nexusUrl.isEmpty())    json["nexusUrl"]    = mod.nexusUrl;
}

static void writeItchFields(const ModInfo &mod, QJsonObject &json) {
    if (!mod.itchGameId.isEmpty())   json["itchGameId"]   = mod.itchGameId;
    if (!mod.itchUrl.isEmpty())      json["itchUrl"]      = mod.itchUrl;
    if (!mod.itchUploadId.isEmpty()) json["itchUploadId"] = mod.itchUploadId;
    if (!mod.ignoredItchUploadIds.isEmpty()) {
        QJsonArray arr;
        for (const QString &id : mod.ignoredItchUploadIds) arr.append(id);
        json["ignoredItchUploadIds"] = arr;
    }
}

static void writeMetadataFields(const ModInfo &mod, QJsonObject &json) {
    if (mod.uploadDate.isValid())    json["uploadDate"]   = mod.uploadDate.toString(Qt::ISODate);
    if (!mod.version.isEmpty())      json["version"]      = mod.version;
    if (!mod.author.isEmpty())       json["author"]       = mod.author;
    if (!mod.description.isEmpty())  json["description"]  = mod.description;
    if (!mod.homepageUrl.isEmpty())  json["homepageUrl"]  = mod.homepageUrl;
    if (!mod.noticeText.isEmpty())   json["noticeText"]   = mod.noticeText;
    if (!mod.noticeIcon.isEmpty())   json["noticeIcon"]   = mod.noticeIcon;
}

static void writeManifestFields(const ModInfo &mod, QJsonObject &json) {
    if (!mod.manifestId.isEmpty()) json["manifestId"] = mod.manifestId;
    if (!mod.manifestAuthors.isEmpty()) {
        QJsonArray arr;
        for (const QString &a : mod.manifestAuthors) arr.append(a);
        json["manifestAuthors"] = arr;
    }
    if (!mod.manifestTags.isEmpty()) {
        QJsonArray arr;
        for (const QString &tag : mod.manifestTags) arr.append(tag);
        json["manifestTags"] = arr;
    }
    if (!mod.manifestDependencies.isEmpty()) {
        QJsonArray arr;
        for (const auto &dep : mod.manifestDependencies) {
            QJsonObject depObj;
            depObj["id"] = dep.id;
            if (!dep.minVersion.isEmpty()) depObj["minVersion"] = dep.minVersion;
            if (!dep.maxVersion.isEmpty()) depObj["maxVersion"] = dep.maxVersion;
            depObj["required"] = dep.required;
            arr.append(depObj);
        }
        json["manifestDependencies"] = arr;
    }
}

static void readNexusFields(const QJsonObject &json, ModInfo &mod) {
    if (json.contains("nexusModId"))  mod.nexusModId  = json["nexusModId"].toString();
    if (json.contains("nexusFileId")) mod.nexusFileId = json["nexusFileId"].toString();
    if (json.contains("nexusUrl"))    mod.nexusUrl    = json["nexusUrl"].toString();
}

static void readItchFields(const QJsonObject &json, ModInfo &mod) {
    if (json.contains("itchGameId"))   mod.itchGameId   = json["itchGameId"].toString();
    if (json.contains("itchUrl"))      mod.itchUrl      = json["itchUrl"].toString();
    if (json.contains("itchUploadId")) mod.itchUploadId = json["itchUploadId"].toString();
    if (json.contains("ignoredItchUploadIds")) {
        for (const QJsonValue &v : json["ignoredItchUploadIds"].toArray())
            mod.ignoredItchUploadIds.append(v.toString());
    }
}

static void readMetadataFields(const QJsonObject &json, ModInfo &mod) {
    if (json.contains("uploadDate"))  mod.uploadDate  = QDateTime::fromString(json["uploadDate"].toString(), Qt::ISODate);
    if (json.contains("version"))     mod.version     = json["version"].toString();
    if (json.contains("author"))      mod.author      = json["author"].toString();
    if (json.contains("description")) mod.description = json["description"].toString();
    if (json.contains("homepageUrl")) mod.homepageUrl = json["homepageUrl"].toString();
    if (json.contains("noticeText"))  mod.noticeText  = json["noticeText"].toString();
    if (json.contains("noticeIcon"))  mod.noticeIcon  = json["noticeIcon"].toString();
}

static void readManifestFields(const QJsonObject &json, ModInfo &mod) {
    if (json.contains("manifestId")) mod.manifestId = json["manifestId"].toString();
    if (json.contains("manifestAuthors")) {
        for (const QJsonValue &v : json["manifestAuthors"].toArray())
            mod.manifestAuthors.append(v.toString());
    }
    if (json.contains("manifestTags")) {
        for (const QJsonValue &v : json["manifestTags"].toArray())
            mod.manifestTags.append(v.toString());
    }
    if (json.contains("manifestDependencies")) {
        for (const QJsonValue &v : json["manifestDependencies"].toArray()) {
            QJsonObject depObj = v.toObject();
            ModInfo::Dependency dep;
            dep.id         = depObj["id"].toString();
            dep.minVersion = depObj["minVersion"].toString();
            dep.maxVersion = depObj["maxVersion"].toString();
            dep.required   = depObj.contains("required") ? depObj["required"].toBool(true) : true;
            if (!dep.id.isEmpty()) mod.manifestDependencies.append(dep);
        }
    }
}

QJsonObject ModInfo::toJson() const {
    QJsonObject json;
    json["id"]              = id;
    json["name"]            = name;
    json["fileName"]        = fileName;
    json["numberedFileName"] = numberedFileName;
    json["installDate"]     = installDate.toString(Qt::ISODate);
    json["enabled"]         = enabled;
    json["priority"]        = priority;

    writeNexusFields(*this, json);
    writeItchFields(*this, json);
    writeMetadataFields(*this, json);
    writeManifestFields(*this, json);

    return json;
}

ModInfo ModInfo::fromJson(const QJsonObject &json) {
    ModInfo mod;
    mod.id              = json["id"].toString();
    mod.name            = json["name"].toString();
    mod.fileName        = json["fileName"].toString();
    mod.numberedFileName = json["numberedFileName"].toString();
    mod.installDate     = QDateTime::fromString(json["installDate"].toString(), Qt::ISODate);
    mod.enabled         = json["enabled"].toBool();
    mod.priority        = json["priority"].toInt();

    readNexusFields(json, mod);
    readItchFields(json, mod);
    readMetadataFields(json, mod);
    readManifestFields(json, mod);

    return mod;
}
