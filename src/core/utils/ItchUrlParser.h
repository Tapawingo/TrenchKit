/// @file ItchUrlParser.h
/// @brief Parses itch.io game page URLs into creator and game slug components.
#pragma once

#include <QString>

/// @brief Static utility for extracting creator and game name from itch.io URLs.
///
/// Expects URLs of the form @c creator.itch.io/game-slug. All methods are
/// static; instantiation is deleted.
class ItchUrlParser {
public:
    /// @brief Result of a URL parse attempt.
    struct ParseResult {
        bool isValid = false;
        QString creator;  ///< Creator/developer subdomain (e.g. "trenchgames").
        QString gameName; ///< Game slug from the URL path (e.g. "foxhole").
        QString error;    ///< Human-readable parse error when @c isValid is false.
    };

    /// @brief Parses @p url and returns a @c ParseResult; check @c isValid before using fields.
    static ParseResult parseUrl(const QString &url);

private:
    ItchUrlParser() = delete;
};
