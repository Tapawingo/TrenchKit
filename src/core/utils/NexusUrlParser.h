/**
 * @file NexusUrlParser.h
 * @brief Parses NexusMods mod page and file download URLs.
 */
#pragma once

#include <QString>

/**
 * @brief Static utility for extracting mod and file IDs from NexusMods URLs.
 *
 * Handles both mod page URLs (nexusmods.com/foxhole/mods/123) and file
 * download URLs that carry an explicit file ID. All methods are static;
 * instantiation is deleted.
 */
class NexusUrlParser {
public:
    /**
     * @brief Result of a URL parse attempt.
     */
    struct ParseResult {
        bool isValid = false;
        QString gameDomain; ///< Game slug extracted from the URL (e.g. "foxhole").
        QString modId;      ///< Numeric mod ID as a string.
        QString fileId;     ///< Numeric file ID; may be empty if not present in the URL.
        QString error;      ///< Human-readable parse error when @c isValid is false.

        bool hasFileId() const { return !fileId.isEmpty(); }
    };

    /**
     * @brief Parses @p url and returns a @c ParseResult; check @c isValid before using fields.
     */
    static ParseResult parseUrl(const QString &url);

private:
    NexusUrlParser() = delete;
};
