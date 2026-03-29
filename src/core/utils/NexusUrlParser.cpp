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
