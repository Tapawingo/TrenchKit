/// @file UpdateCleanup.h
/// @brief Removes leftover update artifacts after a successful self-update.
#pragma once

#include <QString>

/// @brief Cleans up temporary files and backup directories left by the updater.
///
/// Called at application startup to remove any @c .old or @c .bak files that
/// the updater placed alongside the executable during the previous swap.
class UpdateCleanup {
public:
    /// @brief Cleans up in the directory containing the running executable.
    static void run();
    /// @brief Cleans up in the explicitly specified @p appDir.
    static void run(const QString &appDir);
};
