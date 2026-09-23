#include "NexusUrlParser.h"
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>

NexusUrlParser::ParseResult NexusUrlParser::parseUrl(const QString &url) {
    ParseResult result;

    QUrl qurl(url);
    if (!qurl.isValid()) {
        result.error = QStringLiteral("Invalid URL format");
        return result;
    }

    if (qurl.host() != QStringLiteral("www.nexusmods.com")) {
        result.error = QStringLiteral("URL must be from www.nexusmods.com");
        return result;
    }

    QString path = qurl.path();
    QRegularExpression modPattern(QStringLiteral("^/([^/]+)/mods/(\\d+)"));
    QRegularExpressionMatch match = modPattern.match(path);

    if (!match.hasMatch()) {
        result.error = QStringLiteral("Invalid Nexus Mods URL format");
        return result;
    }

    result.gameDomain = match.captured(1);
    result.modId = match.captured(2);

    if (result.gameDomain != QStringLiteral("foxhole")) {
        result.error = QStringLiteral("URL must be for Foxhole game (got: ") + result.gameDomain + QStringLiteral(")");
        return result;
    }

    if (QUrlQuery query(qurl); query.hasQueryItem(QStringLiteral("file_id"))) {
        result.fileId = query.queryItemValue(QStringLiteral("file_id"));
    }

    result.isValid = true;
    return result;
}

NexusUrlParser::NxmResult NexusUrlParser::parseNxmUrl(const QString &url) {
    NxmResult result;

    QUrl qurl(url);
    if (!qurl.isValid() || qurl.scheme() != QStringLiteral("nxm")) {
        result.error = QStringLiteral("Not an nxm:// link");
        return result;
    }

    result.gameDomain = qurl.host();
    if (result.gameDomain != QStringLiteral("foxhole")) {
        result.error = QStringLiteral("nxm link is for a different game (got: ") + result.gameDomain + QStringLiteral(")");
        return result;
    }

    QRegularExpression pattern(QStringLiteral("^/mods/(\\d+)/files/(\\d+)"));
    QRegularExpressionMatch match = pattern.match(qurl.path());
    if (!match.hasMatch()) {
        result.error = QStringLiteral("Invalid nxm link format");
        return result;
    }

    result.modId = match.captured(1);
    result.fileId = match.captured(2);

    const QUrlQuery query(qurl);
    result.key = query.queryItemValue(QStringLiteral("key"));
    result.expires = query.queryItemValue(QStringLiteral("expires"));

    result.isValid = true;
    return result;
}
