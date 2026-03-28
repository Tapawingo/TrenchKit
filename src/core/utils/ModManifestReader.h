/// @file ModManifestReader.h
/// @brief Reads mod metadata from a @c manifest.xml file embedded inside a pak.
#ifndef MODMANIFESTREADER_H
#define MODMANIFESTREADER_H

#include <QString>
#include <QStringList>
#include <QList>

/// @brief Mirrors the schema of the @c manifest.xml embedded in a mod pak file.
struct ModManifest {
    /// @brief A declared dependency on another mod.
    struct Dependency {
        QString id;
        QString minVersion; ///< Empty means no minimum version constraint.
        QString maxVersion; ///< Empty means no maximum version constraint.
        bool required = true;
    };

    QString id;
    QString name;
    QString version;
    QString description;
    QString homepageUrl;
    QString nexusUrl;
    QString itchUrl;
    QString noticeText;
    QString noticeIcon;
    QStringList authors;
    QList<Dependency> dependencies;
    QStringList tags;
};

/// @brief Extracts and parses @c manifest.xml from a pak file.
class ModManifestReader {
public:
    /// @brief Opens @p pakPath, locates @c manifest.xml inside it, and fills @p manifest.
    /// @param error Set to a human-readable message on failure; may be null.
    /// @returns true on success.
    static bool readFromPak(const QString &pakPath, ModManifest *manifest, QString *error = nullptr);

    /// @brief Parses raw @c manifest.xml bytes into @p manifest without opening a pak file.
    /// @param error Set to a human-readable message on failure; may be null.
    /// @returns true on success.
    static bool parseXml(const QByteArray &data, ModManifest *manifest, QString *error = nullptr);
};

#endif // MODMANIFESTREADER_H
