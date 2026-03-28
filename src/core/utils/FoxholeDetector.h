/// @file FoxholeDetector.h
/// @brief Utility for locating the Foxhole Steam installation directory.
#ifndef FOXHOLEDETECTOR_H
#define FOXHOLEDETECTOR_H

#include <QString>
#include <QStringList>

/// @brief Detects and validates the Foxhole installation path.
///
/// On Windows, checks the registry for Steam library paths. On all platforms,
/// falls back to scanning common Steam library locations. Manual path selection
/// is still available as a fallback in the UI.
class FoxholeDetector {
public:
    /// @brief Attempts to auto-detect the Foxhole install directory; returns empty string on failure.
    static QString detectInstallPath();

    /// @brief Returns true if @p path contains a recognizable Foxhole installation.
    static bool isValidInstallPath(const QString &path);

private:
    static QStringList getSteamLibraryPaths();
    static QString checkPath(const QString &basePath);

    static constexpr const char* STEAM_APP_PATH = "steamapps/common/Foxhole";
    static constexpr int FOXHOLE_APP_ID = 505460; ///< Steam App ID for Foxhole.
};

#endif // FOXHOLEDETECTOR_H
